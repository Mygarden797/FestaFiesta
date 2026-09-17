#include "Building/Codex/FF_FacilityCodexSubsystem.h"

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodex"
DEFINE_LOG_CATEGORY_STATIC(LogFacilityCodex, Log, All);

void UFF_FacilityCodexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ZoneSystem = Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
	const UFF_FacilitySynergySettings* Settings = GetDefault<UFF_FacilitySynergySettings>();
	LoadedTable = Settings->SynergyTable.LoadSynchronous();
	TArray<FFF_FacilitySynergyRule> Rules;
	if (LoadedTable && LoadedTable->GetRowStruct() == FFF_FacilitySynergyRow::StaticStruct())
	{
		for (const auto& Pair : LoadedTable->GetRowMap())
		{
			const FFF_FacilitySynergyRow& Row = *reinterpret_cast<const FFF_FacilitySynergyRow*>(Pair.Value);
			if (Row.bEnabled && Row.IsUsable()) { Rules.Add({Pair.Key, Row}); }
		}
	}
	else if (!Settings->SynergyTable.IsNull())
	{
		UE_LOG(LogFacilityCodex, Warning, TEXT("시설 도감의 조합 데이터표를 읽을 수 없습니다: %s"), *Settings->SynergyTable.ToString());
	}
	Model.SetRules(MoveTemp(Rules), Settings->MaxTotalBonusPercent);
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::Refresh);
	}
	Refresh();
}

void UFF_FacilityCodexSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Some test/transition worlds receive their GameInstance after subsystem initialization.
	Refresh();
}

bool UFF_FacilityCodexSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_FacilityCodexSubsystem::BindGameInstance()
{
	UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_BuildCatalogSubsystem* CurrentCatalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	UFF_FacilityCodexProgressSubsystem* CurrentProgress = Game ? Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>() : nullptr;
	if (Catalog != CurrentCatalog)
	{
		if (Catalog) { Catalog->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged); }
		Catalog = CurrentCatalog;
		if (Catalog) { Catalog->OnCatalogChanged.AddUniqueDynamic(this, &ThisClass::HandleCatalogChanged); }
	}
	if (Progress != CurrentProgress)
	{
		if (Progress) { Progress->OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleProgressChanged); }
		Progress = CurrentProgress;
		if (Progress) { Progress->OnProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleProgressChanged); }
	}
}

void UFF_FacilityCodexSubsystem::Refresh()
{
	if (bRefreshing) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	BindGameInstance();
	if (Progress) { SessionDiscoveries.Append(Progress->GetDiscoveredRuleIdsView()); }
	const FFFZSReadModel WorldModel = ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel();
	CachedRegistryRevision = ZoneSystem ? ZoneSystem->GetFacilityRegistryRevision() : MAX_uint64;
	EligibleInstanceIds.Reset();
	for (const FFFZSFacilityReadModel& Facility : WorldModel.Facilities)
	{
		if (FFF_FacilityCodexModel::IsEligibleFacility(Facility)) { EligibleInstanceIds.Add(Facility.FacilityInstanceId); }
	}
	Model.Evaluate(WorldModel, SessionDiscoveries);
	SessionDiscoveries.Append(Model.GetNewDiscoveries());
	if (Progress) { Progress->RecordDiscoveries(SessionDiscoveries); }
	PublishEntries();
}

void UFF_FacilityCodexSubsystem::PublishEntries()
{
	CachedEntries = Model.GetEntries();
	EntryByRuleId.Reset();
	for (int32 Index = 0; Index < CachedEntries.Num(); ++Index)
	{
		FFF_FacilityCodexEntry& Entry = CachedEntries[Index];
		EntryByRuleId.Add(Entry.RuleId, Index);
		Entry.bDiscovered |= SessionDiscoveries.Contains(Entry.RuleId);
		Entry.TargetName = Catalog ? Catalog->ResolveFacilityDisplayName(Entry.TargetFacilityId) : LOCTEXT("LoadingFacility", "시설 이름 불러오는 중");
		Entry.PartnerName = Catalog ? Catalog->ResolveFacilityDisplayName(Entry.PartnerFacilityId) : LOCTEXT("LoadingFacility", "시설 이름 불러오는 중");
		FFF_BuildCatalogEntry Facility;
		if (Catalog && Catalog->FindEntry(Entry.TargetFacilityId, Facility))
		{
			Entry.TargetThumbnail = Facility.Thumbnail;
			Entry.TargetCategory = Facility.Category;
		}
		if (Catalog && Catalog->FindEntry(Entry.PartnerFacilityId, Facility)) { Entry.PartnerThumbnail = Facility.Thumbnail; }
	}
	CachedSummary = FFF_FacilityCodexModel::Summarize(CachedEntries);
	OnCodexChanged.Broadcast();
}

void UFF_FacilityCodexSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	// Open/Close changes no geometry once an instance is eligible. Planned-to-installed transitions still refresh.
	if (Event.Type == EFFZSCommandType::Install || Event.Type == EFFZSCommandType::Move
		|| Event.Type == EFFZSCommandType::Remove || Event.Type == EFFZSCommandType::Upgrade
		|| !EligibleInstanceIds.Contains(Event.FacilityInstanceId)
		|| (ZoneSystem && CachedRegistryRevision != ZoneSystem->GetFacilityRegistryRevision()))
	{
		Refresh();
	}
}

void UFF_FacilityCodexSubsystem::HandleCatalogChanged()
{
	// A name or thumbnail metadata update cannot change live revenue or discovery.
	PublishEntries();
}

void UFF_FacilityCodexSubsystem::HandleProgressChanged()
{
	if (bRefreshing || !Progress) { return; }
	SessionDiscoveries.Append(Progress->GetDiscoveredRuleIdsView());
	PublishEntries();
}

bool UFF_FacilityCodexSubsystem::FindEntry(const FName RuleId, FFF_FacilityCodexEntry& OutEntry) const
{
	const int32* Index = EntryByRuleId.Find(RuleId);
	if (Index && CachedEntries.IsValidIndex(*Index)) { OutEntry = CachedEntries[*Index]; return true; }
	OutEntry = FFF_FacilityCodexEntry();
	return false;
}

TArray<FFF_FacilityCodexEntry> UFF_FacilityCodexSubsystem::GetFilteredEntries(const FFF_FacilityCodexFilter& Filter) const
{
	TArray<FFF_FacilityCodexEntry> Result;
	for (const FFF_FacilityCodexEntry& Entry : CachedEntries)
	{
		if (FFF_FacilityCodexModel::MatchesFilter(Entry, Filter)) { Result.Add(Entry); }
	}
	return Result;
}

void UFF_FacilityCodexSubsystem::Deinitialize()
{
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::Refresh);
	}
	if (Catalog) { Catalog->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged); }
	if (Progress) { Progress->OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleProgressChanged); }
	OnCodexChanged.Clear();
	CachedEntries.Reset();
	EntryByRuleId.Reset();
	EligibleInstanceIds.Reset();
	SessionDiscoveries.Reset();
	ZoneSystem = nullptr;
	Catalog = nullptr;
	Progress = nullptr;
	LoadedTable = nullptr;
	Super::Deinitialize();
}

#undef LOCTEXT_NAMESPACE
