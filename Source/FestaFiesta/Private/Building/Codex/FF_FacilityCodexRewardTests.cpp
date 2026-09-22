#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Codex/FF_FacilityCodexModel.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
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
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Subsystems/SubsystemCollection.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/StrongObjectPtr.h"

namespace FFF_CodexRewardTests
{
	/** Tests read/write an isolated profile, never the player's discovery history. */
	class FScopedProfile
	{
	public:
		FScopedProfile()
			: Path(FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("CodexRewardTest"), TEXT(".ini")))
			, PreviousPath(GGameUserSettingsIni)
		{
			Path = FPaths::ConvertRelativePathToFull(Path);
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
		FString Path;
		FString PreviousPath;
	};

	struct FHarness
	{
		FHarness()
			: Game(NewObject<UGameInstance>())
		{
			Economy = NewObject<UFF_FestivalProgressionSubsystem>(Game.Get());
			Progress = NewObject<UFF_FacilityCodexProgressSubsystem>(Game.Get());
			Definition = NewObject<UFF_FestivalDefinition>(Game.Get());
			UDataTable* Costs = NewObject<UDataTable>(Game.Get());
			Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
			Costs->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = Costs;
			Definition->FestivalId = TEXT("CodexRewardAutomation");
			Definition->InitialGold = 1000;
			Progress->Initialize(Collection);
		}
		~FHarness() { Progress->Deinitialize(); }
		FScopedProfile Profile;
		TStrongObjectPtr<UGameInstance> Game;
		FSubsystemCollection<UGameInstanceSubsystem> Collection;
		UFF_FestivalProgressionSubsystem* Economy = nullptr;
		UFF_FacilityCodexProgressSubsystem* Progress = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
	};

	FFF_FacilitySynergyRule Rule(FName Id, FName Partner, int32 Reward)
	{
		FFF_FacilitySynergyRule Result;
		Result.Id = Id;
		Result.Row.TargetFacilityId = TEXT("Food");
		Result.Row.PartnerFacilityId = Partner;
		Result.Row.DiscoveryRewardGold = Reward;
		return Result;
	}

	FFFZSFacilityReadModel Facility(int32 Id, FName Definition, EFFZSFacilityState State)
	{
		FFFZSFacilityReadModel Result;
		Result.FacilityInstanceId = FGuid(0, 0, 0, Id);
		Result.FacilityDefinitionId = Definition;
		Result.WorldLocation = FVector(Id * 10000., 0., 0.);
		Result.State = State;
		return Result;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexAuthoredRewardTest,
	"FestaFiesta.Building.Codex.Rewards.DiscoveryRequiresExplicitOneTimeClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexAuthoredRewardTest::RunTest(const FString& Parameters)
{
	FFF_CodexRewardTests::FHarness H;
	H.Economy->StartFestival(H.Definition);
	FFF_FacilityCodexModel Model;
	Model.SetRules({FFF_CodexRewardTests::Rule(TEXT("FoodRest"), TEXT("Rest"), 375),
		FFF_CodexRewardTests::Rule(TEXT("FoodShade"), TEXT("Shade"), 0),
		FFF_CodexRewardTests::Rule(TEXT("FoodSupport"), TEXT("Support"), 25)}, 0.f);
	FFFZSReadModel World;
	World.Facilities = {FFF_CodexRewardTests::Facility(1, TEXT("Food"), EFFZSFacilityState::Validated),
		FFF_CodexRewardTests::Facility(2, TEXT("Rest"), EFFZSFacilityState::Planned)};
	Model.Evaluate(World, H.Progress->GetDiscoveredRuleIdsView());
	TestFalse(TEXT("A planned partner does not record or reward a discovery"),
		H.Progress->RecordDiscoveries(Model.GetNewDiscoveries()));
	TestFalse(TEXT("An undiscovered recipe cannot be claimed"),
		H.Progress->ClaimDiscoveryReward(TEXT("FoodRest"), 375, H.Economy));
	TestEqual(TEXT("Preview and catalog evaluation leave gold alone"), H.Economy->GetGold(), 1000);
	World.Facilities[1].State = EFFZSFacilityState::Validated;
	World.Facilities.Add(FFF_CodexRewardTests::Facility(3, TEXT("Shade"), EFFZSFacilityState::Closed));
	World.Facilities.Add(FFF_CodexRewardTests::Facility(4, TEXT("Support"), EFFZSFacilityState::Validated));
	Model.Evaluate(World, H.Progress->GetDiscoveredRuleIdsView());
	const FFF_FacilityCodexEntry* RewardEntry = Model.GetEntries().FindByPredicate(
		[](const FFF_FacilityCodexEntry& Entry) { return Entry.RuleId == TEXT("FoodRest"); });
	if (!TestNotNull(TEXT("Codex projects the authored recipe"), RewardEntry)) { return false; }
	TestEqual(TEXT("Codex projection preserves the table-authored reward"), RewardEntry->DiscoveryRewardGold, 375);
	TestTrue(TEXT("Completed pairs commit all discoveries as one batch"), H.Progress->RecordDiscoveries(Model.GetNewDiscoveries()));
	TestEqual(TEXT("Discovering recipes never automatically credits gold"), H.Economy->GetGold(), 1000);
	TestFalse(TEXT("New discovery has an unclaimed reward"), H.Progress->IsDiscoveryRewardClaimed(TEXT("FoodRest")));
	TestTrue(TEXT("Explicit claim credits the authored reward"),
		H.Progress->ClaimDiscoveryReward(TEXT("FoodRest"), RewardEntry->DiscoveryRewardGold, H.Economy));
	TestEqual(TEXT("Claim credits exactly one recipe"), H.Economy->GetGold(), 1375);
	TestTrue(TEXT("Claim is recorded separately"), H.Progress->IsDiscoveryRewardClaimed(TEXT("FoodRest")));
	TestFalse(TEXT("Other discovered recipes stay unclaimed"), H.Progress->IsDiscoveryRewardClaimed(TEXT("FoodSupport")));
	TestTrue(TEXT("Another recipe can be claimed independently"), H.Progress->ClaimDiscoveryReward(TEXT("FoodSupport"), 25, H.Economy));
	TestTrue(TEXT("A zero-gold recipe is still discovered"), H.Progress->IsDiscovered(TEXT("FoodShade")));
	TestEqual(TEXT("Distance and revenue cap do not block the first discovery reward"),
		FFF_FacilityCodexModel::Summarize(Model.GetEntries()).EffectAppliedCount, 0);
	TestEqual(TEXT("Discovery is not total sales revenue"), H.Economy->GetTotalRevenue(), 0);
	TestEqual(TEXT("Discovery is not daily sales revenue"), H.Economy->GetTodayRevenue(), 0);
	TestEqual(TEXT("Discovery is not demolition refund income"), H.Economy->GetDayLedger().GetTotalRefund(), 0);
	TestFalse(TEXT("Repeated refresh cannot change discovery history"), H.Progress->RecordDiscoveries(Model.GetNewDiscoveries()));
	TestFalse(TEXT("Repeated clicks cannot pay the same recipe"), H.Progress->ClaimDiscoveryReward(TEXT("FoodRest"), 375, H.Economy));
	TestFalse(TEXT("An authored zero reward has nothing to claim"), H.Progress->ClaimDiscoveryReward(TEXT("FoodShade"), 0, H.Economy));
	World.Facilities.Reset();
	Model.Evaluate(World, H.Progress->GetDiscoveredRuleIdsView());
	World.Facilities = {FFF_CodexRewardTests::Facility(5, TEXT("Food"), EFFZSFacilityState::Validated),
		FFF_CodexRewardTests::Facility(6, TEXT("Rest"), EFFZSFacilityState::Validated)};
	Model.Evaluate(World, H.Progress->GetDiscoveredRuleIdsView());
	TestTrue(TEXT("Reinstalling a pair has no new discovery reward"), Model.GetNewDiscoveries().IsEmpty());
	TestEqual(TEXT("Repeat and reinstall preserve the claimed wallet balance"), H.Economy->GetGold(), 1400);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardPersistenceTest,
	"FestaFiesta.Building.Codex.Rewards.ProfileReloadPreservesUnclaimedAndMigratesPaidHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexRewardPersistenceTest::RunTest(const FString& Parameters)
{
	FFF_CodexRewardTests::FHarness H;
	if (!TestNotNull(TEXT("Config persistence is available"), GConfig)) { return false; }
	const TArray<FString> LegacyHistory = {TEXT("LegacyDiscovery")};
	GConfig->SetArray(TEXT("FestaFiesta.FacilityCodex"), TEXT("DiscoveredRuleIds"), LegacyHistory, GGameUserSettingsIni);
	GConfig->RemoveKey(TEXT("FestaFiesta.FacilityCodex"), TEXT("DiscoveryRewardClaimVersion"), GGameUserSettingsIni);
	H.Progress->Initialize(H.Collection);
	H.Economy->StartFestival(H.Definition);
	TestEqual(TEXT("Loading old profile history never grants gold"), H.Economy->GetGold(), 1000);
	TestTrue(TEXT("Legacy automatic rewards migrate as claimed"), H.Progress->IsDiscoveryRewardClaimed(TEXT("LegacyDiscovery")));
	TestFalse(TEXT("Legacy automatic payment cannot be manually collected again"),
		H.Progress->ClaimDiscoveryReward(TEXT("LegacyDiscovery"), 999, H.Economy));
	const TSet<FName> Discoveries = {TEXT("LegacyDiscovery"), TEXT("NewDiscovery"), TEXT("ClaimedDiscovery")};
	TestTrue(TEXT("A batch can contain old and new history"), H.Progress->RecordDiscoveries(Discoveries));
	TestTrue(TEXT("One new discovery can be claimed before reload"), H.Progress->ClaimDiscoveryReward(TEXT("ClaimedDiscovery"), 47, H.Economy));
	TestEqual(TEXT("Only the explicit claim receives gold"), H.Economy->GetGold(), 1047);
	FString SavedProfile;
	TestTrue(TEXT("Discovery history is written to disk"), FFileHelper::LoadFileToString(SavedProfile, *H.Profile.Path));
	TestTrue(TEXT("Persisted history includes the new rule ID"), SavedProfile.Contains(TEXT("NewDiscovery")));
	GConfig->UnloadFile(GGameUserSettingsIni);
	UFF_FacilityCodexProgressSubsystem* Reloaded = NewObject<UFF_FacilityCodexProgressSubsystem>(H.Game.Get());
	Reloaded->Initialize(H.Collection);
	TestTrue(TEXT("A fresh subsystem reads the saved discovery"), Reloaded->IsDiscovered(TEXT("NewDiscovery")));
	TestFalse(TEXT("Unclaimed rewards remain unclaimed across reload"), Reloaded->IsDiscoveryRewardClaimed(TEXT("NewDiscovery")));
	TestTrue(TEXT("Claimed rewards remain claimed across reload"), Reloaded->IsDiscoveryRewardClaimed(TEXT("ClaimedDiscovery")));
	TestTrue(TEXT("Legacy migration survives reload"), Reloaded->IsDiscoveryRewardClaimed(TEXT("LegacyDiscovery")));
	TestFalse(TEXT("Restoring the same layout cannot pay the saved rules"), Reloaded->RecordDiscoveries(Discoveries));
	H.Economy->StartFestival(H.Definition);
	TestFalse(TEXT("Starting a new festival keeps a claimed profile reward spent"),
		Reloaded->ClaimDiscoveryReward(TEXT("ClaimedDiscovery"), 47, H.Economy));
	TestEqual(TEXT("New festival starts with only its configured gold"), H.Economy->GetGold(), 1000);
	TestTrue(TEXT("Previously earned unclaimed reward can be collected after restart"),
		Reloaded->ClaimDiscoveryReward(TEXT("NewDiscovery"), 125, H.Economy));
	TestEqual(TEXT("Deferred claim credits only its authored reward"), H.Economy->GetGold(), 1125);
	Reloaded->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardReadinessAndOverflowTest,
	"FestaFiesta.Building.Codex.Rewards.FailedClaimsRemainAvailableAndClampOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexRewardReadinessAndOverflowTest::RunTest(const FString& Parameters)
{
	FFF_CodexRewardTests::FHarness H;
	const TSet<FName> Discoveries = {TEXT("First"), TEXT("Second"), NAME_None};
	TestTrue(TEXT("Discovery persists before economy startup"), H.Progress->RecordDiscoveries(Discoveries));
	TestEqual(TEXT("Invalid rule IDs are not recorded"), H.Progress->GetDiscoveredRuleIdsView().Num(), 2);
	TestFalse(TEXT("Missing economy rejects a claim"), H.Progress->ClaimDiscoveryReward(TEXT("First"), MAX_int32, nullptr));
	TestFalse(TEXT("Unstarted economy rejects a claim"), H.Progress->ClaimDiscoveryReward(TEXT("First"), MAX_int32, H.Economy));
	TestFalse(TEXT("Failed claim does not consume a reward"), H.Progress->IsDiscoveryRewardClaimed(TEXT("First")));
	H.Economy->StartFestival(H.Definition);
	TestFalse(TEXT("Negative reward is rejected"), H.Progress->ClaimDiscoveryReward(TEXT("Second"), -1, H.Economy));
	TestFalse(TEXT("Zero reward is rejected"), H.Progress->ClaimDiscoveryReward(TEXT("Second"), 0, H.Economy));
	TestFalse(TEXT("Invalid amounts do not consume a reward"), H.Progress->IsDiscoveryRewardClaimed(TEXT("Second")));
	TestTrue(TEXT("Failed claim can be retried once economy is ready"), H.Progress->ClaimDiscoveryReward(TEXT("First"), MAX_int32, H.Economy));
	TestEqual(TEXT("Wallet addition saturates safely"), H.Economy->GetGold(), MAX_int32);
	TestFalse(TEXT("Saturated rewards cannot be claimed repeatedly"), H.Progress->ClaimDiscoveryReward(TEXT("First"), MAX_int32, H.Economy));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardReentrancyTest,
	"FestaFiesta.Building.Codex.Rewards.WalletNotificationCannotReenterClaim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexRewardReentrancyTest::RunTest(const FString& Parameters)
{
	FFF_CodexRewardTests::FHarness H;
	H.Economy->StartFestival(H.Definition);
	H.Progress->RecordDiscoveries({TEXT("ReentrantReward")});
	// Reuse the existing wallet-notification observer; no placement policy is registered here.
	TStrongObjectPtr<UFF_FacilityUpgradeTestPolicy> Observer(NewObject<UFF_FacilityUpgradeTestPolicy>(H.Game.Get()));
	Observer->ObserveGoldChanged = [&]()
	{
		TestTrue(TEXT("Claim is committed before the wallet notifies observers"),
			H.Progress->IsDiscoveryRewardClaimed(TEXT("ReentrantReward")));
		TestFalse(TEXT("A wallet listener cannot claim the same reward recursively"),
			H.Progress->ClaimDiscoveryReward(TEXT("ReentrantReward"), 80, H.Economy));
	};
	H.Economy->OnGoldChanged.AddDynamic(Observer.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	TestTrue(TEXT("The original claim succeeds"), H.Progress->ClaimDiscoveryReward(TEXT("ReentrantReward"), 80, H.Economy));
	TestEqual(TEXT("Exactly one wallet notification is published"), Observer->GoldNotificationCount, 1);
	TestEqual(TEXT("Exactly one reward reaches the wallet"), H.Economy->GetGold(), 1080);
	H.Economy->OnGoldChanged.RemoveDynamic(Observer.Get(), &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexRewardStartupWorldTest,
	"FestaFiesta.Building.Codex.Rewards.StartupRestoreAndCodexClaimPreserveRewardState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFF_FacilityCodexRewardStartupWorldTest::RunTest(const FString& Parameters)
{
	FFF_CodexRewardTests::FScopedProfile Profile;
	UFFZSDeveloperSettings* ZoneSettings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(ZoneSettings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(ZoneSettings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(ZoneSettings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(ZoneSettings->bRequireProductionPolicyDomains, false);
	TGuardValue<bool> Presets(ZoneSettings->bRestrictZoneIdsToProjectPresets, false);
	TGuardValue<EFFZSChangePhase> Phase(ZoneSettings->InitialPhase, EFFZSChangePhase::InitialSetup);
	TGuardValue<int32> Cells(ZoneSettings->MaxEnumeratedCellsPerFacility, 1'000'000);
	TStrongObjectPtr<UDataTable> Rules(NewObject<UDataTable>());
	Rules->RowStruct = FFF_FacilitySynergyRow::StaticStruct();
	FFF_FacilitySynergyRow Row;
	Row.TargetFacilityId = TEXT("Booth_FoodStall_CodexAutomation");
	Row.PartnerFacilityId = Row.TargetFacilityId;
	Row.DiscoveryRewardGold = 333;
	Rules->AddRow(TEXT("CodexStartupPair"), Row);
	TGuardValue<TSoftObjectPtr<UDataTable>> TableSetting(
		GetMutableDefault<UFF_FacilitySynergySettings>()->SynergyTable, TSoftObjectPtr<UDataTable>(Rules.Get()));
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* Game = World->GetGameInstance();
	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	UFF_FacilityCodexSubsystem* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	UFF_FacilityCodexProgressSubsystem* Progress = Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>();
	UFF_FestivalProgressionSubsystem* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("World provides authoritative facilities"), Zones)
		|| !TestNotNull(TEXT("World provides codex projection"), Codex)
		|| !TestNotNull(TEXT("Game provides profile persistence"), Progress)
		|| !TestNotNull(TEXT("Game provides wallet"), Economy)) { return false; }
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(Game);
	UDataTable* Costs = NewObject<UDataTable>(Game);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 10;
	const FName CostKey(TEXT("Cost.Booth.FoodStall.CodexAutomation"));
	Costs->AddRow(CostKey, Cost);
	Definition->FestivalId = TEXT("CodexStartupAutomation");
	Definition->FacilityCostTable = Costs;
	Definition->InitialGold = 1000;
	Economy->StartFestival(Definition);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	if (!TestTrue(TEXT("Isolated placement zone registers"),
		Zone && Zone->ApplyAcceptedZonePreset(ZoneId) && Zones->RegisterZone(Zone))) { return false; }
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = Row.TargetFacilityId;
	Spec.InstallCostKey = CostKey;
	for (const int32 X : {1000, 3500})
	{
		FFFZSGridPlacement Placement;
		Placement.OriginCell = FIntPoint(X, 1000);
		const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement, Zones->GetWorldRevision());
		const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
		if (!TestTrue(*FString::Printf(TEXT("Pre-startup pair installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) { return false; }
	}
	TestEqual(TEXT("Installed startup pair is visible as discovered"), Codex->GetSummary().DiscoveredCount, 1);
	TestTrue(TEXT("Startup discovery persists independently of the economy reset"), Progress->IsDiscovered(TEXT("CodexStartupPair")));
	TestEqual(TEXT("Installation spends only build costs and does not automatically pay discovery"), Economy->GetGold(), 980);
	TestFalse(TEXT("Startup cannot consume a claim before the player-login reset"), Codex->ClaimDiscoveryReward(TEXT("CodexStartupPair")));
	// Exercise the real world-subsystem startup callback and next-tick timer, while avoiding unrelated GameMode startup.
	Codex->OnWorldBeginPlay(*World);
	Economy->RestartFestival();
	TestEqual(TEXT("Player-login economy reset starts with authored gold"), Economy->GetGold(), 1000);
	TestFalse(TEXT("BeginPlay leaves the profile reward unclaimed"), Progress->IsDiscoveryRewardClaimed(TEXT("CodexStartupPair")));
	World->GetTimerManager().Tick(0.016f);
	TestEqual(TEXT("Startup completion never automatically grants discovery gold"), Economy->GetGold(), 1000);
	FFF_FacilityCodexEntry Entry;
	TestTrue(TEXT("Codex publishes the discovered recipe"), Codex->FindEntry(TEXT("CodexStartupPair"), Entry));
	TestTrue(TEXT("Published recipe is discovered"), Entry.bDiscovered);
	TestFalse(TEXT("Published recipe exposes a pending reward"), Entry.bDiscoveryRewardClaimed);
	TestFalse(TEXT("Unknown recipes cannot be claimed through the codex"), Codex->ClaimDiscoveryReward(TEXT("MissingRule")));
	TestTrue(TEXT("Player explicitly claims the discovered recipe"), Codex->ClaimDiscoveryReward(TEXT("CodexStartupPair")));
	TestEqual(TEXT("Claim uses the table-authored reward"), Economy->GetGold(), 1333);
	TestTrue(TEXT("The reward claim is persisted"), Progress->IsDiscoveryRewardClaimed(TEXT("CodexStartupPair")));
	Codex->FindEntry(TEXT("CodexStartupPair"), Entry);
	TestTrue(TEXT("Claim immediately refreshes the published button state"), Entry.bDiscoveryRewardClaimed);
	TestFalse(TEXT("Double clicking the codex claim does not pay again"), Codex->ClaimDiscoveryReward(TEXT("CodexStartupPair")));
	Codex->Refresh();
	Codex->GetEntries();
	Codex->GetFilteredEntries(FFF_FacilityCodexFilter());
	TestEqual(TEXT("Refresh and UI getters do not repay"), Economy->GetGold(), 1333);
	const FFFZSWorldSnapshot Snapshot = Zones->CaptureWorldSnapshot();
	TArray<FText> Errors;
	TestTrue(TEXT("The installed layout can be restored"), Zones->RestoreWorldSnapshot(Snapshot, Errors));
	TestEqual(TEXT("Snapshot restore cannot grant a second reward"), Economy->GetGold(), 1333);
	return true;
}

#endif
