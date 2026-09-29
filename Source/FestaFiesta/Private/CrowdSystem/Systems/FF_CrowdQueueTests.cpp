#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

struct FFF_CrowdQueueTestAccess
{
	static FUnitData& Unit(UFF_CrowdSimulationSubsystem& Crowd, int32 Index) { return Crowd.UnitDatas[Index]; }
	static FPartyDecisionState& State(UFF_CrowdSimulationSubsystem& Crowd, int32 Index) { return Crowd.PartyDecisionStates[Index]; }
	static FFF_CrowdPathFollower& Path(UFF_CrowdSimulationSubsystem& Crowd, int32 Index) { return Crowd.PathFollowers[Index]; }
	static void RegisterLeader(UFF_CrowdSimulationSubsystem& Crowd, int32 Index)
	{ Crowd.PartyLeaderByID.Add(Crowd.UnitDatas[Index].PartyID, Index); }
	static bool Arrive(UFF_CrowdSimulationSubsystem& Crowd, int32 Index) { return Crowd.AdvancePartyQueueOnArrival(Index); }
	static void Step(UFF_CrowdSimulationSubsystem& Crowd) { Crowd.StepSimulation(1.f / 30.f); }
	static void Paths(UFF_CrowdSimulationSubsystem& Crowd) { Crowd.UpdatePathFollowing(); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_HoldsPositionAndResumes,
	"FestaFiesta.Crowd.Queue.HoldsPositionAndResumes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_HoldsPositionAndResumes::RunTest(const FString& Parameters)
{
	(void)Parameters;
	for (const bool bForceDriven : { false, true })
	{
		FUnitData Unit;
		Unit.bIsActive = true;
		Unit.bHasQueueSlot = true;
		Unit.Position = FVector(100, 100, 0);
		Unit.TargetLocation = Unit.Position + FVector(0, 10, 0);
		Unit.QueueSlotLocation = Unit.TargetLocation;
		Unit.ArrivalRadiusUU = 45.f;
		Unit.Speed = 200.f;
		Unit.MaxSpeed = 250.f;
		Unit.LaneYaw = 90.f;
		Unit.Velocity = FVector(200, 0, 0);
		Unit.bHasSteerTarget = true;
		Unit.SteerTargetLocation = FVector(500, 0, 0);
		CrowdSteering::FStepParams Params;
		Params.bDrivingForceInForce = bForceDriven;
		TestTrue(TEXT("A settled slot has no desired walking velocity"),
			CrowdSteering::ComputeDesiredVelocity(Unit, Params).IsNearlyZero());
		const FVector SettledPosition = Unit.Position;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			Unit.Force = FVector(3000, -2000, 0);
			CrowdSteering::StepAgent(Unit, Params, 1.f, 1.f / 30.f);
		}
		TestEqual(TEXT("Residual velocity and avoidance do not push a waiting NPC out of line"), Unit.Position, SettledPosition);
		TestTrue(TEXT("Waiting NPC faces the queue head"), FMath::Abs(FMath::FindDeltaAngleDegrees(Unit.Rotation.Yaw, Unit.LaneYaw)) < 5.f);
		Unit.TargetLocation += FVector(0, 115, 0);
		Unit.QueueSlotLocation = Unit.TargetLocation;
		Unit.bHasSteerTarget = false;
		Unit.Force = FVector(0, 500, 0);
		CrowdSteering::StepAgent(Unit, Params, 1.f, 1.f / 30.f);
		TestTrue(TEXT("Moving the slot lets the waiting NPC walk forward again"), Unit.Position.Y > SettledPosition.Y);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_SlotRefreshResetsPathAndAllMemberRadii,
	"FestaFiesta.Crowd.Queue.SlotRefreshResetsPathAndAllMemberRadii", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_SlotRefreshResetsPathAndAllMemberRadii::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 1, 4, AFF_BaseCrowd::StaticClass());
	const int32 Leader = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	const int32 Member = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	if (Leader == INDEX_NONE || Member == INDEX_NONE) return false;
	using Access = FFF_CrowdQueueTestAccess;
	const int32 Party = Access::Unit(*Crowd, Leader).PartyID;
	Access::RegisterLeader(*Crowd, Leader);
	Access::Unit(*Crowd, Member).PartyID = Party;
	Access::Unit(*Crowd, Member).LeaderAgentIndex = Leader;
	Access::Unit(*Crowd, Member).PartySlotIndex = 1;
	TArray<FVector> Slots = { FVector(1000, 0, 0), FVector(1000, 90, 0) };
	TestTrue(TEXT("Both members receive independent slots"), Crowd->SetPartyQueueColumn(Party, Slots, 180.f));
	TestEqual(TEXT("Member target changes immediately, before avoidance calculates forces"), Crowd->GetAgentDestination(Member), Slots[1]);
	Access::State(*Crowd, Leader).QueuePhase = EFF_PartyQueuePhase::AtSlot;
	for (const int32 Index : { Leader, Member })
	{
		Access::Path(*Crowd, Index).bHasPath = true;
		Access::Path(*Crowd, Index).Points = { FVector(-1000, 0, 0) };
		Access::Unit(*Crowd, Index).bHasSteerTarget = true;
	}
	for (FVector& Slot : Slots) Slot.X -= 90.f; // Smaller than ordinary 100cm repath tolerance.
	TestTrue(TEXT("A one-slot move is accepted"), Crowd->SetPartyQueueColumn(Party, Slots, 180.f));
	TestEqual(TEXT("A shifted party must confirm its new arrival"), Access::State(*Crowd, Leader).QueuePhase, EFF_PartyQueuePhase::Joined);
	for (const int32 Index : { Leader, Member })
	{
		TestFalse(TEXT("Old path is discarded even for a 90cm slot move"), Access::Path(*Crowd, Index).bHasPath);
		TestFalse(TEXT("Old steering waypoint is discarded"), Access::Unit(*Crowd, Index).bHasSteerTarget);
	}
	Access::Paths(*Crowd);
	TestEqual(TEXT("Leader plans a path to its current slot"), Access::Path(*Crowd, Leader).PlannedForTarget, Slots[0]);
	TestEqual(TEXT("Companion plans its own slot path instead of walking through facilities"), Access::Path(*Crowd, Member).PlannedForTarget, Slots[1]);
	Crowd->bEnablePathFollowing = false;
	Crowd->bEnableAvoidance = false;
	Access::Step(*Crowd);
	TestEqual(TEXT("Leader uses the slot arrival radius"), Crowd->GetAgentArrivalRadius(Leader), Crowd->QueueSlotArrivalRadiusUU);
	TestEqual(TEXT("Party member also uses the slot arrival radius"), Crowd->GetAgentArrivalRadius(Member), Crowd->QueueSlotArrivalRadiusUU);
	TestFalse(TEXT("Missing member locations are rejected instead of stacking members in one slot"),
		Crowd->SetPartyQueueColumn(Party, { Slots[0] }, 180.f));
	TestEqual(TEXT("Rejected assignment preserves the previous member slot"), Access::Unit(*Crowd, Member).QueueSlotLocation, Slots[1]);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_ArrivalAndAdvanceLifecycle,
	"FestaFiesta.Crowd.Queue.ArrivalAndAdvanceLifecycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_ArrivalAndAdvanceLifecycle::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!Zone || !Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha")) || !Zones->RegisterZone(Zone)) return false;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_Taco");
	Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall.Taco");
	Spec.RevenueRuleKey = NAME_None;
	Spec.QueueRules.ServiceDurationSeconds = 1.f;
	Spec.QueueRules.AbandonAfterSeconds = 0.f;
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
	const FFFZSCommandResult Result = Zones->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("Queue test facility installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
	World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()->OpenEligibleFacilities(false);
	auto* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	auto* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(20000)), 1, 4, AFF_BaseCrowd::StaticClass());
	Crowd->QueueStallToleranceSeconds = 0.f;
	Crowd->bEnableQueuePatienceAbandon = false;
	using Access = FFF_CrowdQueueTestAccess;
	const int32 First = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	const int32 Member = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	const int32 Second = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	if (First == INDEX_NONE || Member == INDEX_NONE || Second == INDEX_NONE) return false;
	Access::RegisterLeader(*Crowd, First);
	Access::RegisterLeader(*Crowd, Second);
	Access::Unit(*Crowd, Member).PartyID = Access::Unit(*Crowd, First).PartyID;
	Access::Unit(*Crowd, Member).LeaderAgentIndex = First;
	Access::Unit(*Crowd, Member).PartySlotIndex = 1;
	for (const int32 Leader : { First, Second })
	{
		Access::State(*Crowd, Leader).CurrentDestinationId = FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(Install.FacilityInstanceId);
		TestTrue(TEXT("Crowd arrival joins the real facility queue"), Access::Arrive(*Crowd, Leader));
	}
	const int32 QueueId = Bridge->FindQueueFacilityId(Install.FacilityInstanceId);
	TestEqual(TEXT("Two parties joined"), Queue->GetQueueState(QueueId).WaitingPartyCount, 2);
	Access::Arrive(*Crowd, First);
	TestEqual(TEXT("A deferred slot assignment cannot confirm an approach-point arrival"), Crowd->GetQueueArrivalNotifyCount(), 0);
	Queue->FlushPendingEvents();
	Queue->AdvanceFixedSteps(90);
	TestEqual(TEXT("Walking toward the queue does not complete service"), Queue->GetQueueState(QueueId).CompletedPartyCount, 0);
	Access::Unit(*Crowd, First).Position = Access::Unit(*Crowd, First).QueueSlotLocation;
	Access::Arrive(*Crowd, First);
	TestEqual(TEXT("Leader arrival waits for its companion"), Crowd->GetQueueArrivalNotifyCount(), 0);
	Access::Unit(*Crowd, Member).Position = Access::Unit(*Crowd, Member).QueueSlotLocation;
	Access::Arrive(*Crowd, First);
	Access::Unit(*Crowd, Second).Position = Access::Unit(*Crowd, Second).QueueSlotLocation;
	Access::Arrive(*Crowd, Second);
	const FVector PreviousSecondSlot = Access::Unit(*Crowd, Second).QueueSlotLocation;
	Queue->AdvanceFixedSteps(31);
	TestEqual(TEXT("First party completes only after both members arrived"), Queue->GetQueueState(QueueId).CompletedPartyCount, 1);
	TestFalse(TEXT("Next party receives a forward slot"), Access::Unit(*Crowd, Second).QueueSlotLocation.Equals(PreviousSecondSlot));
	TestEqual(TEXT("Forward movement requests a fresh arrival"), Access::State(*Crowd, Second).QueuePhase, EFF_PartyQueuePhase::Joined);
	Access::Arrive(*Crowd, Second);
	Queue->AdvanceFixedSteps(60);
	TestEqual(TEXT("Second party is not served from its previous waiting position"), Queue->GetQueueState(QueueId).CompletedPartyCount, 1);
	Access::Unit(*Crowd, Second).Position = Access::Unit(*Crowd, Second).QueueSlotLocation;
	Access::Arrive(*Crowd, Second);
	Queue->AdvanceFixedSteps(31);
	TestEqual(TEXT("Second party completes after reaching the newly assigned slot"), Queue->GetQueueState(QueueId).CompletedPartyCount, 2);
	TestEqual(TEXT("Queue drains without getting stuck after a slot shift"), Queue->GetQueueState(QueueId).WaitingPartyCount, 0);
	return true;
}

#endif
