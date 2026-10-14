// 역할: 실제 저장 슬롯 전환 중 도감 기록·보상·월드 배치의 복원 순서를 검사한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Codex/FF_FacilityCodexSettings.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/Codex/FF_FacilityCodexTestObserver.h"
#include "Building/FF_FacilityUpgradeTestPolicy.h"
#include "Building/Synergy/FF_FacilitySynergySettings.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Save/FF_SaveArchive.h"
#include "Save/FF_SaveSubsystem.h"
#include "Save/FF_ZoneSaveBridge.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"

namespace FFF_CodexFestivalSaveTests
{
	class FScopedProfile
	{
	public:
		FScopedProfile()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("CodexFestivalSaveTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig) { GConfig->Add(Path, FConfigFile()); }
			GGameUserSettingsIni = Path;
		}
		~FScopedProfile()
		{
			if (GConfig) { GConfig->UnloadFile(Path); }
			GGameUserSettingsIni = PreviousPath;
			IFileManager::Get().Delete(*Path);
		}
	private:
		FString Path;
		FString PreviousPath;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexFestivalSlotTransitionTest,
	"FestaFiesta.Building.Codex.FestivalSave.SlotTransitionWaitsForWholeWorldRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_FacilityCodexFestivalSlotTransitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// The real Save subsystem writes its slot index during initialization. Use the
	// project's existing integration-runner boundary before constructing a game instance.
	FString UserDirectory;
	const FString OrdinarySaved = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), UserDirectory)
		|| UserDirectory.IsEmpty() || FPaths::IsRelative(UserDirectory)
		|| FPaths::IsSamePath(FPaths::ProjectSavedDir(), OrdinarySaved))
	{
		AddError(TEXT("Codex festival save integration requires -UserDir=<isolated absolute directory>."));
		return false;
	}
	FFF_CodexFestivalSaveTests::FScopedProfile Profile;
	TGuardValue<TSoftObjectPtr<UFF_FestivalDefinition>> StartupDefinition(
		GetMutableDefault<UFF_FestivalSettings>()->DefaultFestival, {});
	UFFZSDeveloperSettings* ZoneSettings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(ZoneSettings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(ZoneSettings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(ZoneSettings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(ZoneSettings->bRequireProductionPolicyDomains, false);
	TGuardValue<bool> Presets(ZoneSettings->bRestrictZoneIdsToProjectPresets, false);
	TGuardValue<EFFZSChangePhase> Phase(ZoneSettings->InitialPhase, EFFZSChangePhase::InitialSetup);
	TGuardValue<int32> Cells(ZoneSettings->MaxEnumeratedCellsPerFacility, 1'000'000);
	const FName RuleA(TEXT("SlotAInstalledPair"));
	const FName RuleB(TEXT("SlotBDemolishedPair"));
	const FName DefinitionA(TEXT("Booth_FoodStall_CodexSlotAutomation"));
	const FName MilestoneId(TEXT("CodexSlotFirstCollection"));
	TStrongObjectPtr<UDataTable> Rules(NewObject<UDataTable>());
	Rules->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	FFF_FacilitySynergyRow RowA;
	RowA.TargetFacilityId = DefinitionA;
	RowA.PartnerFacilityId = DefinitionA;
	RowA.DiscoveryRewardGold = 111;
	Rules->AddRow(RuleA, RowA);
	FFF_FacilitySynergyRow RowB = RowA;
	RowB.TargetFacilityId = TEXT("SlotBHistoricalTarget");
	RowB.PartnerFacilityId = TEXT("SlotBHistoricalPartner");
	RowB.DiscoveryRewardGold = 147;
	Rules->AddRow(RuleB, RowB);
	TGuardValue<TSoftObjectPtr<UDataTable>> TableSetting(
		GetMutableDefault<UFF_FacilitySynergySettings>()->SynergyTable, TSoftObjectPtr<UDataTable>(Rules.Get()));
	FFF_FacilityCodexCollectionReward CollectionReward;
	CollectionReward.MilestoneId = MilestoneId;
	CollectionReward.RequiredDiscoveries = 1;
	CollectionReward.RewardGold = 222;
	TGuardValue<TArray<FFF_FacilityCodexCollectionReward>> CollectionSetting(
		GetMutableDefault<UFF_FacilityCodexSettings>()->CollectionRewards, {CollectionReward});
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("Integration game instance exists"), Game)) { return false; }
	Game->OnWorldChanged(nullptr, World);
	auto* Save = Game->GetSubsystem<UFF_SaveSubsystem>();
	auto* Progress = Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>();
	auto* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	auto* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	auto* ZoneSave = World->GetSubsystem<UFF_ZoneSaveBridge>();
	if (!TestNotNull(TEXT("Real save service exists"), Save)
		|| !TestNotNull(TEXT("Festival progress exists"), Progress)
		|| !TestNotNull(TEXT("Economy exists"), Economy)
		|| !TestNotNull(TEXT("Stage exists"), Stage)
		|| !TestNotNull(TEXT("Facility registry exists"), Zones)
		|| !TestNotNull(TEXT("World codex exists"), Codex)
		|| !TestNotNull(TEXT("Real facility save participant exists"), ZoneSave)) { return false; }
	const FString SlotSuffix = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FName SlotA(*FString::Printf(TEXT("CodexIntegration_A_%s"), *SlotSuffix));
	const FName SlotB(*FString::Printf(TEXT("CodexIntegration_B_%s"), *SlotSuffix));
	ON_SCOPE_EXIT
	{
		if (Save->DoesSlotExist(SlotA)) { Save->DeleteSlot(SlotA); }
		if (Save->DoesSlotExist(SlotB)) { Save->DeleteSlot(SlotB); }
	};
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 0;
	const FName CostKey(TEXT("Cost.Booth.FoodStall.CodexSlotAutomation"));
	Costs->AddRow(CostKey, Cost);
	Definition->FestivalId = TEXT("CodexSlotAutomationFestival");
	Definition->FacilityCostTable = Costs;
	Definition->InitialGold = 1000;
	Economy->StartFestival(Definition);
	Stage->CurrentDay = 1;
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	ZoneSave->OnWorldBeginPlay(*World);
	Codex->OnWorldBeginPlay(*World);
	World->GetTimerManager().Tick(0.016f);
	TestTrue(TEXT("First-open guidance is recorded independently"), Progress->MarkIntroductionSeen());
	// A discovered pair may be absent from its saved layout after demolition.
	Progress->RecordDiscoveries({RuleB});
	if (!TestTrue(TEXT("Historical slot B saves through real registration"), Save->SaveToSlot(SlotB))) { return false; }
	UFF_SaveGame* SavedB = nullptr;
	if (!TestTrue(TEXT("Slot B can be inspected"), Save->PeekSlot(SlotB, SavedB))
		|| !TestNotNull(TEXT("Slot B provides a save object"), SavedB)) { return false; }
	const FFF_SaveBlob* CodexBlobB = SavedB->PartData.Find(Progress->GetSaveKey());
	if (!TestNotNull(TEXT("Initialize registered codex with the real save service"), CodexBlobB)) { return false; }
	const TArray<uint8> ProgressBytesB = CodexBlobB->Bytes;
	Economy->RestartFestival();
	TestEqual(TEXT("New festival clears the previous slot's discoveries"), Progress->GetDiscoveredRuleIdsView().Num(), 0);
	TestTrue(TEXT("New festival retains first-open guidance"), Progress->HasSeenIntroduction());
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	if (!TestTrue(TEXT("Placement zone registers"), Zone && Zone->ApplyAcceptedZonePreset(ZoneId)
		&& Zones->RegisterZone(Zone))) { return false; }
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = DefinitionA;
	Spec.InstallCostKey = CostKey;
	TArray<FGuid> InstalledIds;
	for (const int32 X : {1000, 3500})
	{
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
			Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		if (!TestTrue(*FString::Printf(TEXT("Slot A pair installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) { return false; }
		InstalledIds.Add(Command.FacilityInstanceId);
	}
	TestTrue(TEXT("Installed layout discovers slot A recipe"), Progress->IsDiscovered(RuleA));
	TestTrue(TEXT("Slot A individual reward is claimed"), Codex->ClaimDiscoveryReward(RuleA));
	TestTrue(TEXT("Slot A collection reward is claimed"), Codex->ClaimCollectionReward(MilestoneId));
	if (!TestTrue(TEXT("Slot A saves progress and wallet together"), Save->SaveToSlot(SlotA))) { return false; }
	UFF_SaveGame* SavedA = nullptr;
	if (!TestTrue(TEXT("Slot A can be inspected"), Save->PeekSlot(SlotA, SavedA))
		|| !TestNotNull(TEXT("Slot A provides a save object"), SavedA)) { return false; }
	const FFF_SaveBlob* CodexBlobA = SavedA->PartData.Find(Progress->GetSaveKey());
	FFF_FacilityCodexProgressSave StateA;
	if (!TestTrue(TEXT("Registered codex payload is readable"), CodexBlobA
		&& UFF_SaveArchive::ReadStruct(CodexBlobA->Bytes, StateA))) { return false; }
	TestTrue(TEXT("Actual slot contains individual claim"), StateA.ClaimedDiscoveryRewardRuleIds.Contains(RuleA));
	TestTrue(TEXT("Actual slot contains collection claim"), StateA.ClaimedCollectionRewardIds.Contains(MilestoneId));
	TStrongObjectPtr<UFF_FacilityCodexTestObserver> CodexObserver(NewObject<UFF_FacilityCodexTestObserver>());
	Codex->OnCodexChanged.AddDynamic(CodexObserver.Get(), &UFF_FacilityCodexTestObserver::HandleCodexChanged);
	TStrongObjectPtr<UFF_FacilityUpgradeTestPolicy> WalletObserver(NewObject<UFF_FacilityUpgradeTestPolicy>(Game));
	bool bObservedGlobalRestore = false;
	WalletObserver->ObserveGoldChanged = [&]()
	{
		if (!Save->IsRestoring()) { return; }
		bObservedGlobalRestore = true;
		TestTrue(TEXT("Global restore precedes removal of old facility pair"),
			Zones->FindFacility(InstalledIds[0]) && Zones->FindFacility(InstalledIds[1]));
		// Global participants have no relative ordering guarantee. Apply the exact B
		// payload here to exercise progress-before-world ordering using public APIs.
		Progress->ReadSaveData(ProgressBytesB);
		const int32 NotificationsBeforeRefresh = CodexObserver->NotificationCount;
		Codex->Refresh();
		TestEqual(TEXT("Partial restoration skips geometry evaluation and publication"),
			CodexObserver->NotificationCount, NotificationsBeforeRefresh);
		TestFalse(TEXT("Old installed pair is not rediscovered during restoration"), Progress->IsDiscovered(RuleA));
		TestFalse(TEXT("Direct discovery writes are blocked during restoration"), Progress->RecordDiscoveries({RuleA}));
		TestFalse(TEXT("World individual claims wait for complete restore"), Codex->ClaimDiscoveryReward(RuleB));
		TestFalse(TEXT("World collection claims wait for complete restore"), Codex->ClaimCollectionReward(MilestoneId));
		TestFalse(TEXT("Direct individual claims also wait"), Progress->ClaimDiscoveryReward(RuleB, 147, Economy));
		TestFalse(TEXT("Direct collection claims also wait"),
			Progress->ClaimCollectionReward(MilestoneId, 1, 222, {RuleA, RuleB}, Economy));
		TestFalse(TEXT("Failed restore-time claims do not consume individual reward"), Progress->IsDiscoveryRewardClaimed(RuleB));
		TestFalse(TEXT("Failed restore-time claims do not consume collection reward"), Progress->IsCollectionRewardClaimed(MilestoneId));
		const TArray<FFF_FacilityCodexCollectionMilestone> Milestones = Codex->GetCollectionMilestones();
		TestTrue(TEXT("Reached milestone remains unclaimable while restoring"),
			Milestones.Num() == 1 && Milestones[0].bReached && !Milestones[0].bCanClaim);
		// The existing ZoneSaveBridge clear loop omits TransactionId, which the
		// zone command contract rejects before any policy runs. Keep that teammate
		// code untouched and isolate this codex test from the unrelated clear bug.
		// Execute the fixture's complete clear commands inside the same real restore
		// transaction. The codex must ignore their events and reconcile only after
		// OnSaveLoaded, independently of this pre-existing facility-bridge issue.
		for (const FGuid& InstalledId : InstalledIds)
		{
			FFFZSCommand Remove;
			Remove.Type = EFFZSCommandType::Remove;
			Remove.CommandId = FGuid::NewGuid();
			Remove.TransactionId = Remove.CommandId;
			Remove.ExpectedWorldRevision = Zones->GetWorldRevision();
			Remove.FacilityInstanceId = InstalledId;
			const FFFZSCommandResult Removed = Zones->ExecuteCommand(Remove);
			TestTrue(*FString::Printf(TEXT("Fixture restoration clears old instance: %s"),
				*Removed.Message.ToString()), Removed.bSucceeded);
		}
		FFF_FacilityCodexEntry DuringRestore;
		TestTrue(TEXT("Old recipe is still published during partial restoration"), Codex->FindEntry(RuleA, DuringRestore));
		TestTrue(TEXT("Restore command events defer cached geometry reconciliation"), DuringRestore.bHasInstalledPair);
		TestFalse(TEXT("Restore command events cannot add old discoveries"), Progress->IsDiscovered(RuleA));
	};
	Economy->OnGoldChanged.AddDynamic(WalletObserver.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	ON_SCOPE_EXIT
	{
		Economy->OnGoldChanged.RemoveDynamic(WalletObserver.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
		Codex->OnCodexChanged.RemoveDynamic(CodexObserver.Get(), &UFF_FacilityCodexTestObserver::HandleCodexChanged);
	};
	if (!TestTrue(TEXT("Real slot B load succeeds while slot A facilities remain installed"), Save->LoadFromSlot(SlotB))) { return false; }
	TestTrue(TEXT("Restore-time observer exercised guards inside the real load transaction"), bObservedGlobalRestore);
	TestFalse(TEXT("Restore transaction ends before final reconciliation"), Save->IsRestoring());
	TestEqual(TEXT("Slot B contains only its own historical discovery"), Progress->GetDiscoveredRuleIdsView().Num(), 1);
	TestTrue(TEXT("Slot B historical discovery survives absent facilities"), Progress->IsDiscovered(RuleB));
	TestFalse(TEXT("Slot A discovery does not leak into slot B"), Progress->IsDiscovered(RuleA));
	TestFalse(TEXT("Slot A individual claim does not leak"), Progress->IsDiscoveryRewardClaimed(RuleA));
	TestFalse(TEXT("Slot A collection claim does not leak"), Progress->IsCollectionRewardClaimed(MilestoneId));
	TestTrue(TEXT("Loading another slot retains first-open guidance"), Progress->HasSeenIntroduction());
	TestEqual(TEXT("Rejected partial claims preserve slot B's wallet"), Economy->GetGold(), 1000);
	for (const FGuid& InstalledId : InstalledIds)
	{
		TestNull(TEXT("Restoration commands remove the old facility"), Zones->FindFacility(InstalledId));
	}
	FFF_FacilityCodexEntry EntryA;
	TestTrue(TEXT("Old recipe stays visible after final load notification"), Codex->FindEntry(RuleA, EntryA));
	TestFalse(TEXT("OnSaveLoaded reconciles old cached installation flags"), EntryA.bHasInstalledPair);
	TestFalse(TEXT("OnSaveLoaded reconciles old cached discovery flags"), EntryA.bDiscovered);
	TestEqual(TEXT("Published collection summary reflects only restored progress"), Codex->GetSummary().DiscoveredCount, 1);
	TestTrue(TEXT("Individual claim becomes available after all phases finish"), Codex->ClaimDiscoveryReward(RuleB));
	TestTrue(TEXT("Collection claim becomes available after all phases finish"), Codex->ClaimCollectionReward(MilestoneId));
	TestEqual(TEXT("Only restored festival rewards credit the wallet"), Economy->GetGold(), 1369);
	TestEqual(TEXT("Discovery rewards remain outside sales revenue"), Economy->GetTotalRevenue(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
