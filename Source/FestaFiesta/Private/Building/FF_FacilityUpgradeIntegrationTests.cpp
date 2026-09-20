// Facility upgrade purchase, authoritative state, and Party queue integration contracts.
#include "Building/FF_FacilityUpgradeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_FacilityUpgradeTypes.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/FF_ProgramSchedulingSubsystem.h"
#include "Economy/Rules/FF_BoothRevenueRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FacilityUpgradeTestPolicy.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

namespace FFFacilityUpgradeIntegrationTest
{
	constexpr int32 InstallCostGold = 1000;
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	const FName CostKey(TEXT("Cost.Booth.FoodStall.UpgradeAutomation"));
	// The FoodTruck's footprint + clearance + queue costs 796,250 cells at this size,
	// below the production 1,000,000-cell cap. A 1.5x fixture would cost 1,075,000 cells.
	constexpr float TestResizeMultiplier = 1.25f;

	FFFZSFacilitySpec MakeUpgradeableSpec()
	{
		FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_UpgradeAutomation");
		Spec.InstallCostKey = CostKey;
		Spec.QueueRules.ServiceDurationSeconds = 10.f;
		Spec.QueueRules.ServiceCapacity = 1;
		Spec.bUsesFacilityLevels = true;
		FFFZSFacilityUpgradeTier Level2;
		Level2.TargetLevel = 2;
		Level2.CostMultiplier = .3f;
		Level2.ServiceDurationMultiplier = .8f;
		Level2.RevenueMultiplier = 1.5f;
		FFFZSFacilityUpgradeTier Level3;
		Level3.TargetLevel = 3;
		Level3.CostMultiplier = .6f;
		Level3.ServiceDurationMultiplier = .65f;
		Level3.RevenueMultiplier = 2.25f;
		Spec.UpgradeTiers = {Level2, Level3};
		return Spec;
	}

	class FScopedSettings
	{
	public:
		FScopedSettings()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, Ground(Settings->bEnableGroundChecks)
			, Collision(Settings->bEnableWorldCollisionChecks)
			, Navigation(Settings->bEnableNavigationProjectionChecks)
			, Policies(Settings->bRequireProductionPolicyDomains)
			, Presets(Settings->bRestrictZoneIdsToProjectPresets)
			, Phase(Settings->InitialPhase)
			, MaxCells(Settings->MaxEnumeratedCellsPerFacility)
		{
			Settings->bEnableGroundChecks = false;
			Settings->bEnableWorldCollisionChecks = false;
			Settings->bEnableNavigationProjectionChecks = false;
			Settings->bRequireProductionPolicyDomains = false;
			Settings->bRestrictZoneIdsToProjectPresets = false;
			Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
			Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
		}
		~FScopedSettings()
		{
			Settings->bEnableGroundChecks = Ground;
			Settings->bEnableWorldCollisionChecks = Collision;
			Settings->bEnableNavigationProjectionChecks = Navigation;
			Settings->bRequireProductionPolicyDomains = Policies;
			Settings->bRestrictZoneIdsToProjectPresets = Presets;
			Settings->InitialPhase = Phase;
			Settings->MaxEnumeratedCellsPerFacility = MaxCells;
		}
	private:
		UFFZSDeveloperSettings* Settings;
		bool Ground, Collision, Navigation, Policies, Presets;
		EFFZSChangePhase Phase;
		int32 MaxCells;
	};

	class FHarness
	{
	public:
		bool Initialize(FAutomationTestBase* Test, int32 Gold = 5000)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(Test);
				return false;
			}
			World = TestWorld.GetTestWorld();
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			if (!GameInstance)
			{
				return false;
			}
			Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
			Queues = World->GetSubsystem<UFF_QueueSubsystem>();
			QueueBridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
			Upgrades = World->GetSubsystem<UFF_FacilityUpgradeSubsystem>();
			Progression = GameInstance->GetSubsystem<UFF_FestivalProgressionSubsystem>();
			if (!Zones || !Queues || !QueueBridge || !Upgrades || !Progression)
			{
				return false;
			}
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			UDataTable* Costs = NewObject<UDataTable>(GameInstance);
			Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
			FFF_FacilityCostRow Cost;
			Cost.InstallCost = InstallCostGold;
			Costs->AddRow(CostKey, Cost);
			Definition->FestivalId = TEXT("FacilityUpgradeAutomation");
			Definition->FacilityCostTable = Costs;
			// These isolated quotes exercise legacy prices, independent of production balance edits.
			Definition->FacilityRevenueTable.Reset();
			Definition->InitialGold = Gold;
			Definition->DailyVenueRentGold = 0;
			Progression->StartFestival(Definition);
			Queues->SetPaused(true);

			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
				AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
			return Zone && Zone->ApplyAcceptedZonePreset(ZoneId) && Zones->RegisterZone(Zone);
		}

		FGuid Install(FAutomationTestBase* Test, int32 X = 1000, const FFFZSFacilitySpec* AuthoredSpec = nullptr)
		{
			const FFFZSFacilitySpec Spec = AuthoredSpec ? *AuthoredSpec : MakeUpgradeableSpec();
			FFFZSGridPlacement Placement;
			Placement.OriginCell = FIntPoint(X, 1000);
			const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(
				Spec, ZoneId, Placement, Zones->GetWorldRevision());
			const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
			Test->TestTrue(*FString::Printf(TEXT("Install succeeds: %s"), *Result.Message.ToString()), Result.bSucceeded);
			return Result.bSucceeded ? Command.FacilityInstanceId : FGuid();
		}

		FFFZSCommand Command(FGuid Id, EFFZSCommandType Type) const
		{
			FFFZSCommand Result;
			Result.CommandId = FGuid::NewGuid();
			Result.TransactionId = FGuid::NewGuid();
			Result.ExpectedWorldRevision = Zones->GetWorldRevision();
			Result.Type = Type;
			Result.FacilityInstanceId = Id;
			return Result;
		}

		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		UFFZSWorldSubsystem* Zones = nullptr;
		UFF_QueueSubsystem* Queues = nullptr;
		UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = nullptr;
		UFF_FacilityUpgradeSubsystem* Upgrades = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeActualServiceTest,
	"FestaFiesta.Building.FacilityUpgrade.ActualServiceAndIndependentSibling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeActualServiceTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!TestTrue(TEXT("Upgrade integration world initializes"), H.Initialize(this))) return false;
	const FGuid First = H.Install(this);
	const FGuid Sibling = H.Install(this, 3500);
	if (!First.IsValid() || !Sibling.IsValid()) return false;
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(First);
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(First, Preview);
	TestTrue(TEXT("Lv1 quote is actionable"), Preview.bCanUpgrade);
	TestEqual(TEXT("The default first tier costs 30% of install"), Preview.CostGold, 300);
	TestEqual(TEXT("The first tier quotes eight seconds"), Preview.NextServiceDurationSeconds, 8.f);
	const int32 GoldBefore = H.Progression->GetGold();
	FText Failure;
	if (!TestTrue(TEXT("The first facility upgrades"), H.Upgrades->TryUpgradeFacility(First, 1, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	TestEqual(TEXT("Gold is charged once"), H.Progression->GetGold(), GoldBefore - 300);
	TestEqual(TEXT("The project Queue keeps its stable ID"), H.QueueBridge->FindQueueFacilityId(First), QueueId);
	AFFZSFacilityActor* Actor = H.Zones->FindFacility(First);
	TestEqual(TEXT("Upgrade is stored on this instance"), Actor->GetUpgradeLevel(), 2);
	TestEqual(TEXT("Frozen installation spec stays unchanged"), Actor->GetBaseFacilitySpec().QueueRules.ServiceDurationSeconds, 10.f);
	TestEqual(TEXT("Plugin queue has the effective duration"), Actor->GetQueueComponent()->GetRules().ServiceDurationSeconds, 8.f);
	TestEqual(TEXT("Party queue has the effective throughput"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 7.5f);
	TestEqual(TEXT("Same-type sibling remains level one"), H.Zones->FindFacility(Sibling)->GetUpgradeLevel(), 1);
	TestFalse(TEXT("Replaying a stale level cannot buy the next tier"), H.Upgrades->TryUpgradeFacility(First, 1, Failure));
	TestEqual(TEXT("Stale action spends nothing"), H.Progression->GetGold(), GoldBefore - 300);

	if (!TestTrue(TEXT("Upgraded facility opens"), H.Zones->ExecuteCommand(H.Command(First, EFFZSCommandType::Open)).bSucceeded)
		|| !TestTrue(TEXT("Sibling opens"), H.Zones->ExecuteCommand(H.Command(Sibling, EFFZSCommandType::Open)).bSucceeded)) return false;
	FFF_JoinRequest Join;
	Join.PartyId = 98701;
	Join.MemberCount = 1;
	if (!TestTrue(TEXT("Customer joins the upgraded facility"), H.QueueBridge->TryJoinZoneFacility(First, Join).bAccepted)) return false;
	Join.PartyId = 98702;
	if (!TestTrue(TEXT("Customer joins the sibling"), H.QueueBridge->TryJoinZoneFacility(Sibling, Join).bAccepted)) return false;
	H.Queues->AdvanceFixedSteps(270); // Nine simulated seconds, between the two actual completion times.
	TestEqual(TEXT("Upgraded service has actually completed"), H.QueueBridge->GetZoneFacilityQueueState(First).OccupiedMemberCount, 0);
	TestEqual(TEXT("Unupgraded service is still running"), H.QueueBridge->GetZoneFacilityQueueState(Sibling).OccupiedMemberCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeRejectionTest,
	"FestaFiesta.Building.FacilityUpgrade.InsufficientFundsAndForgedCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeRejectionTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this, InstallCostGold + 299)) return false;
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	const int64 Revision = H.Zones->GetWorldRevision();
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	FText Failure;
	TestFalse(TEXT("One Gold short rejects purchase"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestFalse(TEXT("Purchase rejection gives a reason"), Failure.IsEmpty());
	TestEqual(TEXT("Rejected purchase keeps Gold"), H.Progression->GetGold(), 299);
	TestEqual(TEXT("Rejected purchase keeps the level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Rejected purchase keeps revision"), H.Zones->GetWorldRevision(), Revision);
	TestEqual(TEXT("Rejected purchase keeps actual queue rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	TestEqual(TEXT("Rejected purchase is not a refund"), H.Progression->GetTotalRefundForDay(1), 0);

	H.Progression->Refund(1000);
	FFFZSCommand Forged = H.Command(Id, EFFZSCommandType::Upgrade);
	Forged.TargetUpgradeLevel = 2;
	Forged.UpgradeServiceDurationMultiplier = 0.4f;
	TestFalse(TEXT("Direct command cannot forge a stronger tier"), H.Zones->ExecuteCommand(Forged).bSucceeded);
	TestEqual(TEXT("Forged request does not spend Gold"), H.Progression->GetGold(), 1299);
	TestEqual(TEXT("Forged request does not change rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	Forged = H.Command(Id, EFFZSCommandType::Upgrade);
	Forged.TargetUpgradeLevel = 2;
	Forged.UpgradeServiceDurationMultiplier = .8f;
	Forged.UpgradeRevenueMultiplier = 3.f;
	TestFalse(TEXT("Direct command cannot forge a richer tier"), H.Zones->ExecuteCommand(Forged).bSucceeded);
	TestEqual(TEXT("Forged revenue does not spend Gold"), H.Progression->GetGold(), 1299);
	TestEqual(TEXT("Forged revenue does not change income"), H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier(), 1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeSnapshotTest,
	"FestaFiesta.Building.FacilityUpgrade.SnapshotRestoresEffectiveService",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeSnapshotTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	FText Failure;
	if (!TestTrue(TEXT("Upgrade to level two succeeds"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure))
		|| !TestTrue(TEXT("Upgrade to level three succeeds"), H.Upgrades->TryUpgradeFacility(Id, 2, Failure))) return false;
	const FFFZSWorldSnapshot Snapshot = H.Zones->CaptureWorldSnapshot();
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const int32 Gold = H.Progression->GetGold();
	TArray<FText> Errors;
	if (!TestTrue(TEXT("Snapshot restore and queue synchronization succeed"),
		H.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, Errors)))
	{
		for (const FText& Error : Errors) AddError(Error.ToString());
		return false;
	}
	const AFFZSFacilityActor* Restored = H.Zones->FindFacility(Id);
	if (!TestNotNull(TEXT("Restored instance retains its ID"), Restored)) return false;
	TestEqual(TEXT("Snapshot retains level three"), Restored->GetUpgradeLevel(), 3);
	TestEqual(TEXT("Snapshot retains absolute revenue without applying it twice"), Restored->GetUpgradeRevenueMultiplier(), 2.25f);
	TestTrue(TEXT("Level three uses an absolute baseline multiplier"),
		FMath::IsNearlyEqual(Restored->GetEffectiveQueueRules().ServiceDurationSeconds, 6.5f));
	TestEqual(TEXT("Restore retains the stable Party queue ID"), H.QueueBridge->FindQueueFacilityId(Id), QueueId);
	TestTrue(TEXT("Restore rebuilds the upgraded Party throughput"),
		FMath::IsNearlyEqual(H.Queues->GetFacilityServiceRatePerMinute(QueueId), 60.f / 6.5f));
	TestEqual(TEXT("Zone restore does not charge another upgrade fee"), H.Progression->GetGold(), Gold);
	TestFalse(TEXT("Maximum level rejects another purchase"), H.Upgrades->TryUpgradeFacility(Id, 3, Failure));
	TestEqual(TEXT("Max-level rejection keeps Gold"), H.Progression->GetGold(), Gold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeRefundTest,
	"FestaFiesta.Building.FacilityUpgrade.UpgradeFeesAreNotDemolitionRefunds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeRefundTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	FText Failure;
	if (!H.Upgrades->TryUpgradeFacility(Id, 1, Failure) || !H.Upgrades->TryUpgradeFacility(Id, 2, Failure)) return false;
	const int32 BeforeDemolition = H.Progression->GetGold();
	if (!TestTrue(TEXT("Upgraded facility can be removed"), H.Zones->ExecuteCommand(H.Command(Id, EFFZSCommandType::Remove)).bSucceeded)) return false;
	const int32 RefundGold = FMath::RoundToInt(InstallCostGold * H.Definition->FacilityDemolitionRefundRate);
	TestEqual(TEXT("Only the installation cost determines refund"), H.Progression->GetGold(), BeforeDemolition + RefundGold);
	TestEqual(TEXT("Both upgrade fees remain spent"), H.Progression->GetGold(), 5000 - InstallCostGold - 300 - 600 + RefundGold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeQueueRollbackTest,
	"FestaFiesta.Building.FacilityUpgrade.QueueTentativeRollbackAndOccupancy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeQueueRollbackTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	FFFZSFacilitySpec ResizedSpec = MakeUpgradeableSpec();
	ResizedSpec.UpgradeTiers[0].SizeMultiplier = TestResizeMultiplier;
	ResizedSpec.UpgradeTiers[1].SizeMultiplier = TestResizeMultiplier;
	const FGuid Id = H.Install(this, 1000, &ResizedSpec);
	if (!Id.IsValid()) return false;
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const FVector PreviousExit = H.Queues->GetFacilityExitLocation(QueueId);
	FFFZSPolicyContext Context;
	Context.Command = H.Command(Id, EFFZSCommandType::Upgrade);
	Context.Command.TargetUpgradeLevel = 2;
	Context.Command.UpgradeServiceDurationMultiplier = 0.8f;
	Context.Command.UpgradeSizeMultiplier = TestResizeMultiplier;
	Context.Command.UpgradeRevenueMultiplier = 1.5f;
	Context.FacilityActor = H.Zones->FindFacility(Id);
	Context.Command.FacilitySpec = FFZSFacilityUpgradeRules::BuildEffectiveSpec(
		Context.FacilityActor->GetBaseFacilitySpec(), 2, TestResizeMultiplier);
	Context.Validation = H.Zones->ValidateUpgradePlacement(Id, TestResizeMultiplier);
	if (!TestTrue(TEXT("Tentative fixture has a valid candidate placement"), Context.Validation.bIsValid))
	{
		AddError(Context.Validation.Summary.ToString());
		return false;
	}
	Context.CurrentWorldRevision = H.Zones->GetWorldRevision();
	if (!TestTrue(TEXT("Queue can tentatively apply the candidate"),
		IFFZSExternalPolicy::Execute_ApplyTentativeFFZoneSystemChange(H.QueueBridge, Context).bAllowed)) return false;
	TestEqual(TEXT("Candidate Party rate exists before final commit"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 7.5f);
	TestFalse(TEXT("Tentative reconfiguration keeps the queue closed"), H.Queues->GetQueueState(QueueId).bIsOpen);
	TestFalse(TEXT("Tentative resize moves the actual Party exit"),
		H.Queues->GetFacilityExitLocation(QueueId).Equals(PreviousExit));
	IFFZSExternalPolicy::Execute_RollbackFFZoneSystemChange(H.QueueBridge, Context);
	IFFZSExternalPolicy::Execute_RollbackFFZoneSystemChange(H.QueueBridge, Context);
	TestEqual(TEXT("Rollback restores the complete old rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	TestFalse(TEXT("Rollback restores closed state"), H.Queues->GetQueueState(QueueId).bIsOpen);
	TestEqual(TEXT("Rollback never changes actor level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Rollback retains stable queue ID"), H.QueueBridge->FindQueueFacilityId(Id), QueueId);
	TestTrue(TEXT("Rollback restores the old Party exit geometry"),
		H.Queues->GetFacilityExitLocation(QueueId).Equals(PreviousExit));

	if (!H.Zones->ExecuteCommand(H.Command(Id, EFFZSCommandType::Open)).bSucceeded) return false;
	FFF_JoinRequest Join;
	Join.PartyId = 98801;
	Join.MemberCount = 1;
	if (!H.QueueBridge->TryJoinZoneFacility(Id, Join).bAccepted) return false;
	const FFFZSPolicyDecision Occupied = IFFZSExternalPolicy::Execute_PreValidateFFZoneSystemChange(H.QueueBridge, Context);
	TestFalse(TEXT("Project Party occupancy rejects upgrade reconfiguration"), Occupied.bAllowed);
	TestEqual(TEXT("Occupancy has a stable rejection code"), Occupied.FailureReason, EFFZSPlacementFailure::FacilityOccupied);
	TestEqual(TEXT("Rejected upgrade never evacuates a customer"), H.Queues->GetQueueState(QueueId).OccupiedMemberCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradePhaseAndProgramTest,
	"FestaFiesta.Building.FacilityUpgrade.StageProgramLocksAndDayPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradePhaseAndProgramTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	FText Failure;
	if (!TestTrue(TEXT("First upgrade succeeds during Setup"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure))) return false;
	UFF_FestivalStageSubsystem* Stage = H.World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("Stage subsystem exists"), Stage)) return false;

	// Author a compatible program only in the isolated test definition.
	FFF_ProgramCard& Program = H.Definition->ProgramCatalog[0];
	Program.AllowedFacilityTypes = { TEXT("Booth_FoodStall") };
	UFF_ProgramSchedulingSubsystem* Scheduling = H.World->GetSubsystem<UFF_ProgramSchedulingSubsystem>();
	if (!TestNotNull(TEXT("Program scheduling subsystem exists"), Scheduling)
		|| !TestEqual(TEXT("A compatible program is scheduled"), Scheduling->TryScheduleProgramAtFacility(Program.ProgramId, Id),
			EFF_PaidProgramScheduleResult::Success)) return false;
	const int32 BeforeProgramAttempt = H.Progression->GetGold();
	TestFalse(TEXT("Today's scheduled program locks upgrading"), H.Upgrades->TryUpgradeFacility(Id, 2, Failure));
	TestEqual(TEXT("Program lock does not charge Gold"), H.Progression->GetGold(), BeforeProgramAttempt);
	TestEqual(TEXT("Program lock is not a refund"), H.Progression->GetTotalRefundForDay(1), 0);

	Stage->CurrentStage = EFF_FestivalStage::Live;
	H.Zones->SetChangePhase(EFFZSChangePhase::LiveOperation);
	if (!TestTrue(TEXT("The facility opens normally for Live"),
		H.Zones->ExecuteCommand(H.Command(Id, EFFZSCommandType::Open)).bSucceeded)) return false;
	TestEqual(TEXT("Normal Open makes the facility Active"), H.Zones->FindFacility(Id)->GetFacilityState(), EFFZSFacilityState::Active);
	const int32 BeforeLiveAttempt = H.Progression->GetGold();
	TestFalse(TEXT("Live rejects facility upgrade"), H.Upgrades->TryUpgradeFacility(Id, 2, Failure));
	TestEqual(TEXT("Live rejection preserves Gold"), H.Progression->GetGold(), BeforeLiveAttempt);
	TestEqual(TEXT("Live rejection preserves level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 2);
	if (!TestTrue(TEXT("The facility closes normally after service"),
		H.Zones->ExecuteCommand(H.Command(Id, EFFZSCommandType::Close)).bSucceeded)) return false;
	TestEqual(TEXT("A normally drained facility enters MovePending"),
		H.Zones->FindFacility(Id)->GetFacilityState(), EFFZSFacilityState::MovePending);
	TestEqual(TEXT("Normal Close leaves the plugin queue Closed"),
		H.Zones->FindFacility(Id)->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
	TestFalse(TEXT("Normal Close also closes the Party queue"),
		H.QueueBridge->GetZoneFacilityQueueState(Id).bIsOpen);
	H.Progression->BeginNewDay(2);
	Stage->CurrentStage = EFF_FestivalStage::MorningCheck;
	H.Zones->SetChangePhase(EFFZSChangePhase::MorningCheck);
	TestEqual(TEXT("Beginning a new day preserves the instance upgrade"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 2);
	TestEqual(TEXT("Beginning a new day preserves upgraded Party throughput"),
		H.Queues->GetFacilityServiceRatePerMinute(H.QueueBridge->FindQueueFacilityId(Id)), 7.5f);
	TestFalse(TEXT("Previous day's program no longer locks the facility"), Scheduling->HasScheduledProgramForFacility(Id));
	TestTrue(TEXT("MorningCheck upgrades the normally closed MovePending facility"), H.Upgrades->TryUpgradeFacility(Id, 2, Failure));
	TestEqual(TEXT("The next morning purchase reaches level three"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeAtomicRollbackTest,
	"FestaFiesta.Building.FacilityUpgrade.LatePolicyFailureRollsBackWithoutNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeAtomicRollbackTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	const int32 BeforeGold = H.Progression->GetGold();
	const int64 BeforeRevision = H.Zones->GetWorldRevision();
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	UFF_FacilityUpgradeTestPolicy* Policy = NewObject<UFF_FacilityUpgradeTestPolicy>(H.World);
	int32 ReservedGold = INDEX_NONE;
	float TentativeRate = 0.f;
	Policy->ObserveTentative = [&]()
	{
		ReservedGold = H.Progression->GetGold();
		TentativeRate = H.Queues->GetFacilityServiceRatePerMinute(QueueId);
	};
	H.Progression->OnGoldChanged.AddDynamic(Policy, &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	if (!TestTrue(TEXT("Late rejection policy registers"), H.Zones->RegisterExternalPolicy(Policy))) return false;
	FText Failure;
	TestFalse(TEXT("The injected late failure rejects the complete purchase"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestEqual(TEXT("The rejection occurs after tentative policies"), Policy->TentativeCallCount, 1);
	TestEqual(TEXT("Gold was reserved before the late policy"), ReservedGold, BeforeGold - 300);
	TestEqual(TEXT("Party rate was staged before the late policy"), TentativeRate, 7.5f);
	TestEqual(TEXT("Failure returns reserved Gold"), H.Progression->GetGold(), BeforeGold);
	TestEqual(TEXT("Failure restores the Party queue"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	TestEqual(TEXT("Failure preserves the authoritative level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Failure preserves the authoritative revenue"), H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier(), 1.f);
	TestEqual(TEXT("Failure does not advance world revision"), H.Zones->GetWorldRevision(), BeforeRevision);
	TestEqual(TEXT("Reservation rollback creates no refund ledger entry"), H.Progression->GetTotalRefundForDay(1), 0);
	TestEqual(TEXT("Reservation rollback creates no upgrade expense"),
		H.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FacilityUpgrade), 0);
	TestEqual(TEXT("Failure never broadcasts a partial balance"), Policy->GoldNotificationCount, 0);
	H.Zones->UnregisterExternalPolicy(Policy);

	bool bObservedCoherentSuccess = false;
	Policy->ObserveGoldChanged = [&]()
	{
		bObservedCoherentSuccess = H.Zones->FindFacility(Id)->GetUpgradeLevel() == 2
			&& H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier() == 1.5f
			&& FMath::IsNearlyEqual(H.Queues->GetFacilityServiceRatePerMinute(QueueId), 7.5f)
			&& H.Progression->GetGold() == BeforeGold - 300;
	};
	TestTrue(TEXT("A fresh purchase succeeds after rollback"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestEqual(TEXT("Successful purchase broadcasts Gold exactly once"), Policy->GoldNotificationCount, 1);
	TestTrue(TEXT("Gold observer sees committed level and actual queue rate"), bObservedCoherentSuccess);
	H.Progression->OnGoldChanged.RemoveDynamic(Policy, &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradePerDefinitionTest,
	"FestaFiesta.Building.FacilityUpgrade.PerDefinitionCostsAndLevelPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradePerDefinitionTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this, 10000)) return false;
	FFFZSFacilitySpec PremiumSpec = MakeUpgradeableSpec();
	PremiumSpec.FacilityDefinitionId = TEXT("Booth_FoodStall_PremiumUpgradeAutomation");
	PremiumSpec.UpgradeTiers[0].CostMultiplier = .5f;
	PremiumSpec.UpgradeTiers[0].ServiceDurationMultiplier = .75f;
	const FGuid Basic = H.Install(this);
	const FGuid Premium = H.Install(this, 3500, &PremiumSpec);
	if (!Basic.IsValid() || !Premium.IsValid()) return false;
	FFF_FacilityUpgradePreview BasicQuote, PremiumQuote;
	H.Upgrades->GetUpgradePreview(Basic, BasicQuote);
	H.Upgrades->GetUpgradePreview(Premium, PremiumQuote);
	TestEqual(TEXT("First facility owns its 30 percent upgrade price"), BasicQuote.CostGold, 300);
	TestEqual(TEXT("Second facility owns its different 50 percent upgrade price"), PremiumQuote.CostGold, 500);
	TestEqual(TEXT("Second facility owns its different service improvement"), PremiumQuote.NextServiceDurationSeconds, 7.5f);
	FText Failure;
	const int32 GoldBefore = H.Progression->GetGold();
	TestTrue(TEXT("Per-definition tier purchases through the common facade"), H.Upgrades->TryUpgradeFacility(Premium, 1, Failure));
	TestEqual(TEXT("Purchase uses this definition's price"), H.Progression->GetGold(), GoldBefore - 500);
	TestEqual(TEXT("Other definition remains level one"), H.Zones->FindFacility(Basic)->GetUpgradeLevel(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeNoLevelsTest,
	"FestaFiesta.Building.FacilityUpgrade.NoLevelsAndLevelOneMaximum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeNoLevelsTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	FFFZSFacilitySpec NoLevels = MakeUpgradeableSpec();
	NoLevels.bUsesFacilityLevels = false;
	NoLevels.UpgradeTiers.Reset();
	FFFZSFacilitySpec LevelOneOnly = NoLevels;
	LevelOneOnly.bUsesFacilityLevels = true;
	const FGuid Plain = H.Install(this, 1000, &NoLevels);
	const FGuid FixedLevel = H.Install(this, 3500, &LevelOneOnly);
	if (!Plain.IsValid() || !FixedLevel.IsValid()) return false;
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Plain, Preview);
	TestFalse(TEXT("Facilities without levels explicitly hide their level UI"), Preview.bHasLevels);
	TestFalse(TEXT("Facilities without levels cannot buy upgrades"), Preview.bCanUpgrade);
	const int32 Gold = H.Progression->GetGold();
	FText Failure;
	TestFalse(TEXT("No-level policy also rejects direct purchase calls"), H.Upgrades->TryUpgradeFacility(Plain, 1, Failure));
	H.Upgrades->GetUpgradePreview(FixedLevel, Preview);
	TestTrue(TEXT("A level-one-only facility still has a displayed level"), Preview.bHasLevels);
	TestEqual(TEXT("An empty per-facility tier list means maximum level one"), Preview.MaxLevel, 1);
	TestFalse(TEXT("An empty tier list cannot be purchased"), Preview.bCanUpgrade);
	TestEqual(TEXT("Neither policy creates an expense"), H.Progression->GetGold(), Gold);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeResizeQueueTest,
	"FestaFiesta.Building.FacilityUpgrade.ResizeRebuildsQueueAndRestores",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeResizeQueueTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	FFFZSFacilitySpec Spec = MakeUpgradeableSpec();
	Spec.UpgradeTiers[0].SizeMultiplier = TestResizeMultiplier;
	Spec.UpgradeTiers[1].SizeMultiplier = TestResizeMultiplier;
	const FGuid Id = H.Install(this, 1000, &Spec);
	if (!Id.IsValid()) return false;
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const FVector OldExit = H.Queues->GetFacilityExitLocation(QueueId);
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestEqual(TEXT("Preview shows original physical footprint"), Preview.CurrentFootprintCentimeters, FIntPoint(600, 300));
	TestEqual(TEXT("Preview shows the proposed larger footprint"), Preview.NextFootprintCentimeters, FIntPoint(750, 375));
	FText Failure;
	if (!TestTrue(TEXT("A resize with sufficient space succeeds"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	const AFFZSFacilityActor* Actor = H.Zones->FindFacility(Id);
	TestEqual(TEXT("Resize is stored as an absolute baseline multiplier"), Actor->GetUpgradeSizeMultiplier(), TestResizeMultiplier);
	TestEqual(TEXT("Authoritative physical footprint changes"), Actor->GetFacilitySpec().FootprintSizeCells, FIntPoint(750, 375));
	TestEqual(TEXT("Original installation footprint remains frozen"), Actor->GetBaseFacilitySpec().FootprintSizeCells, FIntPoint(600, 300));
	TestTrue(TEXT("Party exit uses the same resized anchor and shifted center as the plugin queue"),
		H.Queues->GetFacilityExitLocation(QueueId).Equals(Actor->GetQueueComponent()->GetExitWorldLocation(0), .01));
	TestFalse(TEXT("The old exit location is not left behind"), H.Queues->GetFacilityExitLocation(QueueId).Equals(OldExit, .01));
	const FFFZSWorldSnapshot Snapshot = H.Zones->CaptureWorldSnapshot();
	TArray<FText> Errors;
	if (!TestTrue(TEXT("Resized snapshot restores queues together"), H.QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(Snapshot, Errors))) return false;
	Actor = H.Zones->FindFacility(Id);
	TestEqual(TEXT("Restoring does not multiply footprint twice"), Actor->GetFacilitySpec().FootprintSizeCells, FIntPoint(750, 375));
	TestTrue(TEXT("Restored Party exit still matches plugin geometry"),
		H.Queues->GetFacilityExitLocation(QueueId).Equals(Actor->GetQueueComponent()->GetExitWorldLocation(0), .01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeResizeCollisionTest,
	"FestaFiesta.Building.FacilityUpgrade.ResizeCollisionNeverCharges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeResizeCollisionTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	FFFZSFacilitySpec Spec = MakeUpgradeableSpec();
	Spec.UpgradeTiers[0].SizeMultiplier = TestResizeMultiplier;
	Spec.UpgradeTiers[1].SizeMultiplier = TestResizeMultiplier;
	const FGuid Id = H.Install(this, 1000, &Spec);
	if (!Id.IsValid()) return false;
	FFF_FacilityUpgradePreview UnblockedPreview;
	H.Upgrades->GetUpgradePreview(Id, UnblockedPreview);
	if (!TestTrue(TEXT("The authored expansion is valid before a neighbor blocks it"), UnblockedPreview.bCanUpgrade))
	{
		AddError(UnblockedPreview.FailureReason.ToString());
		return false;
	}
	// Baseline reserved bounds end at x=1700. The neighbor starts at x=1750,
	// while this facility's expanded bounds reach x=1850 and must collide.
	const FGuid Neighbor = H.Install(this, 1850);
	if (!Id.IsValid() || !Neighbor.IsValid()) return false;
	const int32 Gold = H.Progression->GetGold();
	const int64 Revision = H.Zones->GetWorldRevision();
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const FVector Exit = H.Queues->GetFacilityExitLocation(QueueId);
	const FTransform Transform = H.Zones->FindFacility(Id)->GetActorTransform();
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestFalse(TEXT("Collision is reported by the purchase preview"), Preview.bCanUpgrade);
	TestFalse(TEXT("Collision gives a player-facing reason"), Preview.FailureReason.IsEmpty());
	FText Failure;
	TestFalse(TEXT("An overlapping expansion cannot be bought"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestEqual(TEXT("Collision rejection keeps Gold"), H.Progression->GetGold(), Gold);
	TestEqual(TEXT("Collision rejection keeps world revision"), H.Zones->GetWorldRevision(), Revision);
	TestEqual(TEXT("Collision rejection keeps level one"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Collision rejection keeps original footprint"), H.Zones->FindFacility(Id)->GetFacilitySpec().FootprintSizeCells, FIntPoint(600, 300));
	TestTrue(TEXT("Collision rejection keeps the actor center"), H.Zones->FindFacility(Id)->GetActorTransform().Equals(Transform));
	TestTrue(TEXT("Collision rejection keeps the real exit anchor"), H.Queues->GetFacilityExitLocation(QueueId).Equals(Exit));
	TestEqual(TEXT("Collision rejection keeps actual service rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeRevenuePreviewTest,
	"FestaFiesta.Building.FacilityUpgrade.RevenueOnlyQuoteAndIndependentIncome",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeRevenuePreviewTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this)) return false;
	H.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 26);
	H.Definition->BoothCommissionRate = .4f;
	FFFZSFacilitySpec Spec = MakeUpgradeableSpec();
	for (FFFZSFacilityUpgradeTier& Tier : Spec.UpgradeTiers)
	{
		Tier.ServiceDurationMultiplier = 1.f;
	}
	FFFZSFacilitySpec PlainSpec = Spec;
	PlainSpec.bUsesFacilityLevels = false;
	const FGuid Id = H.Install(this, 1000, &Spec);
	const FGuid Sibling = H.Install(this, 3500, &PlainSpec);
	if (!Id.IsValid() || !Sibling.IsValid()) return false;
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestTrue(TEXT("Revenue-only upgrade produces an actionable quote"), Preview.bCanUpgrade);
	TestTrue(TEXT("Revenue uses the parsed minor token and festival rules"), Preview.bRevenueAvailable);
	TestEqual(TEXT("Base one-person income rounds only at settlement"), Preview.BaseRevenuePerUseGold, 10);
	TestEqual(TEXT("Current one-person income starts at the baseline"), Preview.CurrentRevenuePerUseGold, 10);
	TestEqual(TEXT("Next income multiplies before rounding, not rounded baseline times 1.5"), Preview.NextRevenuePerUseGold, 16);
	TestEqual(TEXT("Quote calls the actual commission rule"), Preview.NextRevenuePerUseGold,
		FFF_BoothRevenueRules::ResolveCommission(26, 1, .4f, 1.5f));
	TestEqual(TEXT("Revenue-only tier retains service time"), Preview.NextServiceDurationSeconds, Preview.CurrentServiceDurationSeconds);
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const float OriginalRate = H.Queues->GetFacilityServiceRatePerMinute(QueueId);
	FText Failure;
	if (!TestTrue(TEXT("Revenue-only purchase succeeds through the facade"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	TestEqual(TEXT("Purchase applies the authored revenue multiplier"), H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier(), 1.5f);
	TestEqual(TEXT("Revenue-only purchase preserves Party service rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), OriginalRate);
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestEqual(TEXT("Refreshed current income reflects the purchased tier"), Preview.CurrentRevenuePerUseGold, 16);
	TestEqual(TEXT("Next tier uses the absolute 2.25 baseline multiplier"), Preview.NextRevenuePerUseGold, 23);
	H.Upgrades->GetUpgradePreview(Sibling, Preview);
	TestFalse(TEXT("The independent sibling has no level UI"), Preview.bHasLevels);
	TestTrue(TEXT("Facilities without levels still report current income"), Preview.bRevenueAvailable);
	TestEqual(TEXT("The independent sibling retains baseline income"), Preview.CurrentRevenuePerUseGold, 10);
	H.Definition->RevenuePerUseByMinorToken.Add(TEXT("FoodStall"), 0);
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestTrue(TEXT("A free facility is an available zero quote, not a missing revenue rule"), Preview.bRevenueAvailable);
	TestEqual(TEXT("A free facility still earns zero after upgrading"), Preview.CurrentRevenuePerUseGold, 0);
	TestEqual(TEXT("Multipliers cannot turn a free facility into a paid facility"), Preview.NextRevenuePerUseGold, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeFreeSparseTest,
	"FestaFiesta.Building.FacilityUpgrade.FreeSparseTiersRemainAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeFreeSparseTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this, InstallCostGold)) return false;
	FFFZSFacilitySpec Spec = MakeUpgradeableSpec();
	FFFZSFacilityUpgradeTier SameEffect;
	SameEffect.TargetLevel = 5;
	SameEffect.CostMultiplier = 0.f;
	SameEffect.ServiceDurationMultiplier = 1.f;
	SameEffect.RevenueMultiplier = 1.f;
	FFFZSFacilityUpgradeTier Slower = SameEffect;
	Slower.TargetLevel = 20;
	Slower.ServiceDurationMultiplier = 2.f;
	Slower.RevenueMultiplier = .5f;
	Slower.SizeMultiplier = TestResizeMultiplier;
	FFFZSFacilityUpgradeTier Shrink = SameEffect;
	Shrink.TargetLevel = 99;
	Shrink.RevenueMultiplier = 0.f;
	Spec.UpgradeTiers = {Shrink, SameEffect, Slower};
	const FGuid Id = H.Install(this, 1000, &Spec);
	if (!Id.IsValid()) return false;
	TestEqual(TEXT("The installation leaves no Gold"), H.Progression->GetGold(), 0);
	// An explicitly free tier does not need an installation-price lookup after installation.
	H.Definition->FacilityCostTable = nullptr;
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestTrue(TEXT("A free, identical-effect tier is a resolved actionable quote"), Preview.bHasUpgradeQuote && Preview.bCanUpgrade);
	TestEqual(TEXT("Unsorted sparse tiers choose the smallest next level"), Preview.TargetLevel, 5);
	TestEqual(TEXT("Maximum level does not assume the last array entry"), Preview.MaxLevel, 99);
	TestEqual(TEXT("A zero multiplier quotes zero Gold"), Preview.CostGold, 0);
	const int32 QueueId = H.QueueBridge->FindQueueFacilityId(Id);
	const int64 BeforeRevision = H.Zones->GetWorldRevision();
	UFF_FacilityUpgradeTestPolicy* Policy = NewObject<UFF_FacilityUpgradeTestPolicy>(H.World);
	H.Progression->OnGoldChanged.AddDynamic(Policy, &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	if (!TestTrue(TEXT("A late failure can also inspect a free purchase"), H.Zones->RegisterExternalPolicy(Policy))) return false;
	FText Failure;
	TestFalse(TEXT("A free tentative purchase rolls back on late failure"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestEqual(TEXT("The free reservation reaches the late policy"), Policy->TentativeCallCount, 1);
	TestEqual(TEXT("Rollback leaves the level unchanged"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 1);
	TestEqual(TEXT("Rollback leaves the world revision unchanged"), H.Zones->GetWorldRevision(), BeforeRevision);
	TestEqual(TEXT("Rollback leaves the original queue rate"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 6.f);
	TestEqual(TEXT("A free rollback makes no Gold notification"), Policy->GoldNotificationCount, 0);
	H.Zones->UnregisterExternalPolicy(Policy);

	FFFZSCommand FreeCommand = H.Command(Id, EFFZSCommandType::Upgrade);
	FreeCommand.TargetUpgradeLevel = 5;
	FreeCommand.UpgradeServiceDurationMultiplier = 1.f;
	FreeCommand.UpgradeRevenueMultiplier = 1.f;
	if (!TestTrue(TEXT("A fresh free command succeeds after rollback"), H.Zones->ExecuteCommand(FreeCommand).bSucceeded)) return false;
	const int64 PurchasedRevision = H.Zones->GetWorldRevision();
	TestTrue(TEXT("Replaying a free command returns its receipt"), H.Zones->ExecuteCommand(FreeCommand).bSucceeded);
	TestEqual(TEXT("Replaying never advances the revision again"), H.Zones->GetWorldRevision(), PurchasedRevision);
	TestEqual(TEXT("Replaying never buys the next level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 5);
	TestFalse(TEXT("A stale free quote cannot buy the next tier"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestEqual(TEXT("A sparse next tier may reduce revenue"), Preview.NextRevenueMultiplier, .5f);
	TestEqual(TEXT("A sparse next tier may be slower"), Preview.NextServiceDurationSeconds, 20.f);
	if (!TestTrue(TEXT("The slower, reduced-revenue expansion is purchasable"), H.Upgrades->TryUpgradeFacility(Id, 5, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	TestEqual(TEXT("The authored slower rate reaches the real Party queue"), H.Queues->GetFacilityServiceRatePerMinute(QueueId), 3.f);
	TestEqual(TEXT("The authored reduced revenue reaches the actor"), H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier(), .5f);
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestEqual(TEXT("The final tier may shrink back to the baseline"), Preview.NextFootprintCentimeters, FIntPoint(600, 300));
	if (!TestTrue(TEXT("Shrinking with sufficient queue geometry succeeds"), H.Upgrades->TryUpgradeFacility(Id, 20, Failure)))
	{
		AddError(Failure.ToString());
		return false;
	}
	TestEqual(TEXT("Sparse levels can exceed the former maximum"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 99);
	TestEqual(TEXT("An authored zero revenue multiplier is preserved"), H.Zones->FindFacility(Id)->GetUpgradeRevenueMultiplier(), 0.f);
	TestEqual(TEXT("Shrinking rebuilds the original physical footprint"), H.Zones->FindFacility(Id)->GetFacilitySpec().FootprintSizeCells, FIntPoint(600, 300));
	TestEqual(TEXT("Free purchases never change Gold"), H.Progression->GetGold(), 0);
	TestEqual(TEXT("Free purchases create no upgrade expense"), H.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FacilityUpgrade), 0);
	TestEqual(TEXT("Free rollback is not a refund"), H.Progression->GetTotalRefundForDay(1), 0);
	TestEqual(TEXT("Free success does not broadcast unchanged Gold"), Policy->GoldNotificationCount, 0);
	H.Progression->OnGoldChanged.RemoveDynamic(Policy, &UFF_FacilityUpgradeTestPolicy::HandleGoldChanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFFacilityUpgradeFreeInstallationTest,
	"FestaFiesta.Building.FacilityUpgrade.ZeroInstallationCostAllowsFreeUpgrade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFFacilityUpgradeFreeInstallationTest::RunTest(const FString& Parameters)
{
	using namespace FFFacilityUpgradeIntegrationTest;
	FScopedSettings Settings;
	FHarness H;
	if (!H.Initialize(this, InstallCostGold)) return false;
	// Repricing an existing facility to zero also makes its later upgrade quote free,
	// with the wallet already emptied by the original paid installation.
	const FGuid Id = H.Install(this);
	if (!Id.IsValid()) return false;
	TestEqual(TEXT("A normally priced installation empties the wallet"), H.Progression->GetGold(), 0);
	FFF_FacilityCostRow FreeCost;
	FreeCost.InstallCost = 0;
	H.Definition->FacilityCostTable->AddRow(CostKey, FreeCost);
	FFF_FacilityUpgradePreview Preview;
	H.Upgrades->GetUpgradePreview(Id, Preview);
	TestTrue(TEXT("The existing facility can quote an upgrade from the current zero base price"), Preview.bHasUpgradeQuote && Preview.bCanUpgrade);
	TestEqual(TEXT("Multiplying the zero base price stays free"), Preview.CostGold, 0);
	FText Failure;
	TestTrue(TEXT("The paid installation upgrades for free with an empty wallet"), H.Upgrades->TryUpgradeFacility(Id, 1, Failure));
	TestEqual(TEXT("Free upgrade preserves empty wallet"), H.Progression->GetGold(), 0);
	TestEqual(TEXT("Free upgrade applies the target level"), H.Zones->FindFacility(Id)->GetUpgradeLevel(), 2);
	TestEqual(TEXT("The original installation remains a paid ledger entry"),
		H.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FacilityInstall), InstallCostGold);
	TestEqual(TEXT("The free upgrade creates no upgrade expense"),
		H.Progression->GetDayLedger().GetSpendByReason(EFF_SpendReason::FacilityUpgrade), 0);
	return true;
}

#endif
