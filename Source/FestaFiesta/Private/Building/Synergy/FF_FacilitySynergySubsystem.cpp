#include "Building/Synergy/FF_FacilitySynergySubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Engine/DataTable.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

DEFINE_LOG_CATEGORY_STATIC(LogFacilitySynergy, Log, All);

void UFF_FacilitySynergySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ZoneSystem = Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
	const UFF_FacilitySynergySettings* Settings = GetDefault<UFF_FacilitySynergySettings>();
	// Startup-only load; no disk work during pointer movement or a customer payment.
	LoadedTable = Settings->SynergyTable.LoadSynchronous();
	TArray<FFF_FacilitySynergyRule> Rules;
	if (LoadedTable && LoadedTable->GetRowStruct() == FFF_FacilitySynergyRow::StaticStruct())
	{
		for (const auto& Pair : LoadedTable->GetRowMap())
		{
			const FFF_FacilitySynergyRow& Row = *reinterpret_cast<const FFF_FacilitySynergyRow*>(Pair.Value);
			if (!Row.bEnabled) continue;
			FString Error;
			if (Row.IsUsable(&Error)) Rules.Add({Pair.Key, Row});
			else UE_LOG(LogFacilitySynergy, Warning, TEXT("시설 조합 '%s' 사용 안 함: %s"), *Pair.Key.ToString(), *Error);
		}
	}
	else if (!Settings->SynergyTable.IsNull())
	{
		UE_LOG(LogFacilitySynergy, Warning, TEXT("시설 조합 데이터표를 읽을 수 없거나 FF_FacilitySynergyRow 형식이 아닙니다. 조합 보너스는 적용되지 않습니다: %s"), *Settings->SynergyTable.ToString());
	}
	Evaluator.SetRules(MoveTemp(Rules), Settings->MaxTotalBonusPercent);
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
}

void UFF_FacilitySynergySubsystem::Deinitialize()
{
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
	RevenueCache.Reset();
	Evaluator.SetFacilities({});
	LoadedTable = nullptr;
	ZoneSystem = nullptr;
	Super::Deinitialize();
}

bool UFF_FacilitySynergySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_FacilitySynergySubsystem::EnsureIndex() const
{
	if (!ZoneSystem || !Evaluator.HasRules()) return;
	const int64 WorldRevision = ZoneSystem->GetWorldRevision();
	const uint64 RegistryRevision = ZoneSystem->GetFacilityRegistryRevision();
	if (!bIndexDirty && CachedWorldRevision == WorldRevision && CachedRegistryRevision == RegistryRevision) return;
	TArray<FFF_FacilitySynergyPoint> Points;
	const FFFZSReadModel ReadModel = ZoneSystem->BuildReadModel();
	Points.Reserve(ReadModel.Facilities.Num());
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.bNeedsRelocation || Facility.State == EFFZSFacilityState::Planned || Facility.State == EFFZSFacilityState::Removed) continue;
		Points.Add({Facility.FacilityInstanceId, Facility.FacilityDefinitionId, Facility.WorldLocation});
	}
	Evaluator.SetFacilities(MoveTemp(Points));
	RevenueCache.Reset();
	CachedWorldRevision = WorldRevision;
	CachedRegistryRevision = RegistryRevision;
	bIndexDirty = false;
	++Revision;
}

TArray<FName> UFF_FacilitySynergySubsystem::GetRelatedFacilityDefinitionIds(const FName DefinitionId) const
{
	// Rules are indexed once at startup. Catalog highlighting must never rebuild the world index.
	return Evaluator.GetRelatedFacilityDefinitionIds(DefinitionId);
}

float UFF_FacilitySynergySubsystem::GetBonusPercent(const FGuid FacilityInstanceId) const
{
	EnsureIndex();
	if (const float* Cached = RevenueCache.Find(FacilityInstanceId)) return *Cached;
	const FFF_FacilitySynergyPoint* Target = Evaluator.FindFacility(FacilityInstanceId);
	if (!Target) return 0.f;
	const float Percent = Evaluator.Evaluate(*Target).TotalPercent;
	RevenueCache.Add(FacilityInstanceId, Percent);
	return Percent;
}

float UFF_FacilitySynergySubsystem::GetRevenueMultiplier(const FGuid FacilityInstanceId) const
{
	return 1.f + GetBonusPercent(FacilityInstanceId) * 0.01f;
}

float UFF_FacilitySynergySubsystem::GetRevenueMultiplierAt(const FGuid FacilityInstanceId, const FVector& ProposedWorldLocation) const
{
	EnsureIndex();
	const FFF_FacilitySynergyPoint* Current = Evaluator.FindFacility(FacilityInstanceId);
	if (!Current || ProposedWorldLocation.ContainsNaN()) return 1.f;
	FFF_FacilitySynergyPoint Candidate = *Current;
	Candidate.WorldLocation = ProposedWorldLocation;
	return 1.f + Evaluator.Evaluate(Candidate, &Candidate, FacilityInstanceId).TotalPercent * 0.01f;
}

void UFF_FacilitySynergySubsystem::BuildPlacementPreview(const FName DefinitionId, const FVector& WorldLocation,
	const FGuid MovingInstanceId, FFF_FacilitySynergyPreview& OutPreview) const
{
	EnsureIndex();
	Evaluator.BuildPreview(DefinitionId, WorldLocation, MovingInstanceId, OutPreview);
}

uint64 UFF_FacilitySynergySubsystem::GetRevision() const
{
	EnsureIndex();
	return Revision;
}

void UFF_FacilitySynergySubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	// Closed partners keep their installed layout benefit. Any lifecycle command can turn
	// a previously Planned (unindexed) facility into an eligible facility, including Close.
	// Queue occupancy and income never invalidate the index.
	if (Event.Type == EFFZSCommandType::Install || Event.Type == EFFZSCommandType::Move ||
		Event.Type == EFFZSCommandType::Remove || Event.Type == EFFZSCommandType::Upgrade ||
		!Evaluator.FindFacility(Event.FacilityInstanceId))
		bIndexDirty = true;
	else CachedWorldRevision = Event.WorldRevision;
}

void UFF_FacilitySynergySubsystem::HandleSnapshotRestored()
{
	// A restored save may reuse a previous world revision and GUIDs with new actors/locations.
	bIndexDirty = true;
}
