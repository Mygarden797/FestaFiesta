// FF_FacilityCodexSubsystem.cpp
// 역할: 월드·카탈로그·발견 기록 변경을 도감 화면과 보상 청구로 연결한다.

#include "Building/Codex/FF_FacilityCodexSubsystem.h"

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "Building/Synergy/FF_FacilitySynergyRulePolicy.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodex"
DEFINE_LOG_CATEGORY_STATIC(LogFacilityCodex, Log, All);

void UFF_FacilityCodexSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	ZoneSystem = Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
	const UFF_FacilitySynergySettings* Settings = GetDefault<UFF_FacilitySynergySettings>();
	LoadedTable = Settings->SynergyTable.LoadSynchronous();
	TArray<FString> Warnings;
	TArray<FFF_FacilitySynergyRule> Rules = FFFacilitySynergyRulePolicy::ReadTable(LoadedTable, Warnings);
	if (!LoadedTable && !Settings->SynergyTable.IsNull())
	{
		Warnings.Add(FString::Printf(TEXT("시설 조합 데이터표를 읽을 수 없습니다: %s"), *Settings->SynergyTable.ToString()));
	}
	for (const FString& Warning : Warnings)
	{
		UE_LOG(LogFacilityCodex, Warning, TEXT("%s"), *Warning);
	}
	Model.SetRules(MoveTemp(Rules), Settings->MaxTotalBonusPercent);
	ReferencedDefinitionIds.Reset();
	for (const FFF_FacilityCodexEntry& Entry : Model.GetEntries())
	{
		ReferencedDefinitionIds.Add(Entry.TargetFacilityId);
		ReferencedDefinitionIds.Add(Entry.PartnerFacilityId);
	}
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
	// Player login resets the configured festival during startup. Accept claims only after
	// that reset finishes; discovery itself can safely persist before the economy is ready.
	StartupRewardTimer = InWorld.GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::CompleteStartup));
}

void UFF_FacilityCodexSubsystem::CompleteStartup()
{
	bReadyToClaimRewards = true;
	Refresh();
}

bool UFF_FacilityCodexSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

// 카탈로그·발견 기록·경제의 실제 소유자를 연결하며 인스턴스가 바뀌면 이전 이벤트 구독을 해제한다.
void UFF_FacilityCodexSubsystem::BindGameInstance()
{
	UGameInstance* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_BuildCatalogSubsystem* CurrentCatalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	UFF_FacilityCodexProgressSubsystem* CurrentProgress = Game ? Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>() : nullptr;
	UFF_FestivalProgressionSubsystem* CurrentEconomy = Game ? Game->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
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
	if (Economy != CurrentEconomy)
	{
		if (Economy) { Economy->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::Refresh); }
		Economy = CurrentEconomy;
		if (Economy) { Economy->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::Refresh); }
	}
}

// 월드의 현재 배치로 도감을 다시 계산하고 새 발견을 저장한 뒤 UI 읽기 전용 목록을 갱신한다.
void UFF_FacilityCodexSubsystem::Refresh()
{
	if (bRefreshing) { return; }
	TGuardValue<bool> RefreshGuard(bRefreshing, true);
	BindGameInstance();
	if (Progress) { SessionDiscoveries.Append(Progress->GetDiscoveredRuleIdsView()); }
	const FFFZSReadModel WorldModel = ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel();
	CachedRegistryRevision = ZoneSystem ? ZoneSystem->GetFacilityRegistryRevision() : MAX_uint64;
	EligibleInstanceIds.Reset();
	UnreferencedInstanceIds.Reset();
	for (const FFFZSFacilityReadModel& Facility : WorldModel.Facilities)
	{
		if (FFF_FacilityCodexModel::IsEligibleFacility(Facility)) { EligibleInstanceIds.Add(Facility.FacilityInstanceId); }
		if (!ReferencedDefinitionIds.Contains(Facility.FacilityDefinitionId))
		{
			UnreferencedInstanceIds.Add(Facility.FacilityInstanceId);
		}
	}
	Model.Evaluate(WorldModel, SessionDiscoveries);
	SessionDiscoveries.Append(Model.GetNewDiscoveries());
	if (Progress)
	{
		TSet<FName> PendingDiscoveries;
		for (const FFF_FacilityCodexEntry& Entry : Model.GetEntries())
		{
			if (SessionDiscoveries.Contains(Entry.RuleId) && !Progress->IsDiscovered(Entry.RuleId))
			{
				PendingDiscoveries.Add(Entry.RuleId);
			}
		}
		Progress->RecordDiscoveries(PendingDiscoveries);
	}
	PublishEntries();
}

// 계산 결과에 사람이 읽는 시설 이름·썸네일·보상 수령 상태를 붙여 화면에 전달한다.
void UFF_FacilityCodexSubsystem::PublishEntries()
{
	CachedEntries = Model.GetEntries();
	EntryByRuleId.Reset();
	for (int32 Index = 0; Index < CachedEntries.Num(); ++Index)
	{
		FFF_FacilityCodexEntry& Entry = CachedEntries[Index];
		EntryByRuleId.Add(Entry.RuleId, Index);
		Entry.bDiscovered |= SessionDiscoveries.Contains(Entry.RuleId);
		Entry.bDiscoveryRewardClaimed = Progress && Progress->IsDiscoveryRewardClaimed(Entry.RuleId);
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

// 조합 조건에 영향을 줄 수 있는 배치·등록 변경만 다시 계산한다. 단순 영업 열기·닫기는 재계산을 생략한다.
void UFF_FacilityCodexSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	if (ZoneSystem)
	{
		const uint64 RegistryRevision = ZoneSystem->GetFacilityRegistryRevision();
		const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Event.FacilityInstanceId);
		const bool bUnreferencedNow = Facility
			&& !ReferencedDefinitionIds.Contains(Facility->GetFacilitySpec().FacilityDefinitionId);
		const bool bWasUnreferenced = UnreferencedInstanceIds.Contains(Event.FacilityInstanceId);
		const bool bRegistryMutation = Event.Type == EFFZSCommandType::Install || Event.Type == EFFZSCommandType::Remove;
		// Install/remove changes the registry once. Any other revision change may include an
		// external registration or revalidation, so retain the full reconciliation in that case.
		const bool bExpectedRegistryRevision = CachedRegistryRevision != MAX_uint64
			&& RegistryRevision == CachedRegistryRevision + (bRegistryMutation ? 1 : 0);
		const bool bUnreferencedChange = Event.Type == EFFZSCommandType::Remove
			? bWasUnreferenced && !Facility
			: bUnreferencedNow && (bWasUnreferenced || Event.Type == EFFZSCommandType::Install);
		if (bExpectedRegistryRevision && bUnreferencedChange)
		{
			// Most dragged surfaces never occur in a recipe. Avoid copying and evaluating the
			// entire world for each tile, while still supporting recipes that explicitly use roads.
			if (Event.Type == EFFZSCommandType::Remove)
			{
				UnreferencedInstanceIds.Remove(Event.FacilityInstanceId);
				EligibleInstanceIds.Remove(Event.FacilityInstanceId);
			}
			else
			{
				UnreferencedInstanceIds.Add(Event.FacilityInstanceId);
			}
			CachedRegistryRevision = RegistryRevision;
			return;
		}
	}
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

bool UFF_FacilityCodexSubsystem::ClaimDiscoveryReward(const FName RuleId)
{
	if (!bReadyToClaimRewards) { return false; }
	BindGameInstance();
	FFF_FacilityCodexEntry Entry;
	if (!Progress || !FindEntry(RuleId, Entry) || !Entry.bDiscovered || Entry.bDiscoveryRewardClaimed)
	{
		return false;
	}
	return Progress->ClaimDiscoveryReward(RuleId, Entry.DiscoveryRewardGold, Economy);
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
	if (UWorld* World = GetWorld()) { World->GetTimerManager().ClearTimer(StartupRewardTimer); }
	bReadyToClaimRewards = false;
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::Refresh);
	}
	if (Catalog) { Catalog->OnCatalogChanged.RemoveDynamic(this, &ThisClass::HandleCatalogChanged); }
	if (Progress) { Progress->OnProgressChanged.RemoveDynamic(this, &ThisClass::HandleProgressChanged); }
	if (Economy) { Economy->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::Refresh); }
	OnCodexChanged.Clear();
	CachedEntries.Reset();
	EntryByRuleId.Reset();
	EligibleInstanceIds.Reset();
	UnreferencedInstanceIds.Reset();
	ReferencedDefinitionIds.Reset();
	SessionDiscoveries.Reset();
	ZoneSystem = nullptr;
	Catalog = nullptr;
	Progress = nullptr;
	Economy = nullptr;
	LoadedTable = nullptr;
	Super::Deinitialize();
}

#undef LOCTEXT_NAMESPACE
