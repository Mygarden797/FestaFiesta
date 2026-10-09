#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdForces.h"
#include "CrowdSystem/Systems/FF_CrowdSpatialGrid.h"
#include "CrowdSystem/Systems/FF_CrowdSteering.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
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
	static int32 WallCount(const UFF_CrowdSimulationSubsystem& Crowd) { return Crowd.AvoidanceWalls.Num(); }
	static void SetWaitThreshold(UFF_CrowdSimulationSubsystem& Crowd, int32 Index, float Minutes)
	{ Crowd.AgentPersonalThresholds[Index].WaitThresholdMinutes = Minutes; }
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
	TestEqual(TEXT("Leader uses the precise assigned-slot arrival radius"), Crowd->GetAgentArrivalRadius(Leader), Crowd->GetAssignedSlotArrivalRadius());
	TestEqual(TEXT("Party member also uses the precise arrival radius"), Crowd->GetAgentArrivalRadius(Member), Crowd->GetAssignedSlotArrivalRadius());
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
	Access::Unit(*Crowd, First).Position = Access::Unit(*Crowd, First).QueueSlotLocation + FVector(20.f, 0.f, 0.f);
	Access::Arrive(*Crowd, First);
	TestEqual(TEXT("Leader arrival waits for its companion"), Crowd->GetQueueArrivalNotifyCount(), 0);
	Access::Unit(*Crowd, Member).Position = Access::Unit(*Crowd, Member).QueueSlotLocation;
	Access::Arrive(*Crowd, First);
	TestEqual(TEXT("The former 45cm tolerance cannot falsely confirm a leader still 20cm off its slot"), Crowd->GetQueueArrivalNotifyCount(), 0);
	Access::Unit(*Crowd, First).Position = Access::Unit(*Crowd, First).QueueSlotLocation;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_DefaultAvoidanceWalksToExactSlots,
	"FestaFiesta.Crowd.Queue.DefaultAvoidanceWalksToExactSlots", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_DefaultAvoidanceWalksToExactSlots::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->bEnableHeightField = false; // This isolated movement fixture has no ground mesh.
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 0, 4, AFF_BaseCrowd::StaticClass());
	Crowd->bEnablePathFollowing = false; // Open floor: test real steering and avoidance, not NavMesh generation.
	Crowd->QueueStallToleranceSeconds = 0.f;
	Crowd->bEnableQueuePatienceAbandon = false;
	TestTrue(TEXT("Production avoidance is enabled"), Crowd->bEnableAvoidance && Crowd->bAvoidanceDrivesVelocity);
	TestEqual(TEXT("The test keeps the production 4.5m braking setting"), Crowd->BrakingDistanceUU, 450.f);
	using Access = FFF_CrowdQueueTestAccess;
	TArray<int32> Agents;
	const TArray<FVector> Slots = { FVector(0, 0, 0), FVector(0, 90, 0), FVector(90, 0, 0), FVector(90, 90, 0) };
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const int32 Agent = Crowd->SpawnAgent(*World, Slots[Index] - FVector(350, 0, 0), FRotator::ZeroRotator);
		if (Agent == INDEX_NONE) return false;
		Agents.Add(Agent);
	}
	for (int32 Pair = 0; Pair < 2; ++Pair)
	{
		const int32 Leader = Agents[Pair * 2];
		const int32 Member = Agents[Pair * 2 + 1];
		const int32 Party = Access::Unit(*Crowd, Leader).PartyID;
		Access::RegisterLeader(*Crowd, Leader);
		Access::Unit(*Crowd, Member).PartyID = Party;
		Access::Unit(*Crowd, Member).LeaderAgentIndex = Leader;
		Access::Unit(*Crowd, Member).PartySlotIndex = 1;
		Access::State(*Crowd, Leader).QueuePhase = EFF_PartyQueuePhase::Joined;
		if (!TestTrue(TEXT("Each team receives its two distinct 90cm-spaced targets"),
			Crowd->SetPartyQueueColumn(Party, {Slots[Pair * 2], Slots[Pair * 2 + 1]}, 0.f))) return false;
	}
	bool bMovementWasBounded = true;
	for (int32 Step = 0; Step < 900; ++Step)
	{
		TArray<FVector> Before;
		for (const int32 Agent : Agents) Before.Add(Access::Unit(*Crowd, Agent).Position);
		Access::Step(*Crowd);
		for (int32 Index = 0; Index < Agents.Num(); ++Index)
		{
			const FUnitData& Unit = Access::Unit(*Crowd, Agents[Index]);
			bMovementWasBounded &= FVector::Dist2D(Before[Index], Unit.Position) <= Unit.MaxSpeed / 30.f + .01f;
		}
	}
	TestTrue(TEXT("Every approach uses bounded walking steps, never teleportation"), bMovementWasBounded);
	TArray<FVector> Settled;
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const FUnitData& Unit = Access::Unit(*Crowd, Agents[Index]);
		TestTrue(*FString::Printf(TEXT("Agent %d reaches its actual target, error %.2fcm"), Index,
			FVector::Dist2D(Unit.Position, Slots[Index])),
			FVector::Dist2D(Unit.Position, Slots[Index]) <= Crowd->GetAssignedSlotArrivalRadius());
		TestTrue(TEXT("A settled agent stops walking"), Unit.Velocity.IsNearlyZero());
		Settled.Add(Unit.Position);
	}
	for (int32 Step = 0; Step < 120; ++Step) Access::Step(*Crowd);
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		TestTrue(TEXT("Nearby queue occupants do not push each other off their assigned slots"),
			Access::Unit(*Crowd, Agents[Index]).Position.Equals(Settled[Index], .01f));
	}
	// Move both teams one authored queue interval. This must restart walking, not retain
	// the old stop radius or stale target when the change is less than repath tolerance.
	for (int32 Pair = 0; Pair < 2; ++Pair)
	{
		Crowd->SetPartyQueueColumn(Access::Unit(*Crowd, Agents[Pair * 2]).PartyID,
			{Slots[Pair * 2] + FVector(90, 0, 0), Slots[Pair * 2 + 1] + FVector(90, 0, 0)}, 0.f);
	}
	for (int32 Step = 0; Step < 600; ++Step) Access::Step(*Crowd);
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		TestTrue(TEXT("A one-slot advance reaches the new center with default avoidance still on"),
			FVector::Dist2D(Access::Unit(*Crowd, Agents[Index]).Position, Slots[Index] + FVector(90, 0, 0))
				<= Crowd->GetAssignedSlotArrivalRadius());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_ShortBlockedApproachStillQueriesPath,
	"FestaFiesta.Crowd.Queue.ShortBlockedApproachStillQueriesPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_ShortBlockedApproachStillQueriesPath::RunTest(const FString& Parameters)
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
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->bEnableHeightField = false;
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 0, 1, AFF_BaseCrowd::StaticClass());
	Crowd->QueueStallToleranceSeconds = 0.f;
	Crowd->bEnableQueuePatienceAbandon = false;
	using Access = FFF_CrowdQueueTestAccess;
	TestEqual(TEXT("The initial empty layout has no facility walls"), Access::WallCount(*Crowd), 0);

	// Install after crowd initialization so the very first path update must refresh
	// the wall cache. A hand-injected wall would disappear during that rebuild.
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!Zone || !Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha")) || !Zones->RegisterZone(Zone)) return false;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_Taco");
	Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall.Taco");
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
	const FFFZSCommandResult Result = Zones->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("Blocking facility installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
	const AFFZSFacilityActor* Facility = Zones->FindFacility(Install.FacilityInstanceId);
	if (!TestNotNull(TEXT("The installed wall has a real facility actor"), Facility)) return false;
	const FVector BlockedTarget = Facility->GetActorLocation();
	const FTransform FacilityTransform = Facility->GetActorTransform();
	const FIntPoint Footprint = Facility->GetFacilitySpec().FootprintSizeCells;
	const FVector2D HalfFootprint(Footprint.X * .5f, Footprint.Y * .5f);
	const FVector Start = FacilityTransform.TransformPosition(FVector(0.f, -HalfFootprint.Y - 150.f, 0.f));
	const int32 Leader = Crowd->SpawnAgent(*World, Start, FRotator::ZeroRotator);
	if (Leader == INDEX_NONE) return false;
	Access::RegisterLeader(*Crowd, Leader);
	Access::State(*Crowd, Leader).QueuePhase = EFF_PartyQueuePhase::Joined;
	Crowd->SetPartyQueueColumn(Access::Unit(*Crowd, Leader).PartyID, {BlockedTarget}, 0.f);
	const int32 QueriesBefore = Crowd->GetPathQueriesIssued();
	Access::Paths(*Crowd);
	TestEqual(TEXT("The first path update sees all four newly installed walls"), Access::WallCount(*Crowd), 4);
	TestEqual(TEXT("A blocked slot 3m away still requests an actual navigation path"),
		Crowd->GetPathQueriesIssued(), QueriesBefore + 1);
	// Deliberately place the target inside the solid footprint: even a malformed
	// assignment must not bypass wall/contact forces or report successful service.
	const auto SignedWallDistance = [&FacilityTransform, HalfFootprint](const FVector& Position)
	{
		const FVector Local = FacilityTransform.InverseTransformPosition(Position);
		const FVector2D Outside(FMath::Abs(Local.X) - HalfFootprint.X, FMath::Abs(Local.Y) - HalfFootprint.Y);
		return FVector2D(FMath::Max(Outside.X, 0.0), FMath::Max(Outside.Y, 0.0)).Size()
			+ FMath::Min(FMath::Max(Outside.X, Outside.Y), 0.0);
	};
	const float BodyRadius = Access::Unit(*Crowd, Leader).Radius;
	double MinimumBodyClearance = SignedWallDistance(Start) - BodyRadius;
	TestTrue(TEXT("The test starts outside the installed footprint"), MinimumBodyClearance > 0.0);
	for (int32 Step = 0; Step < 600; ++Step)
	{
		Access::Step(*Crowd);
		MinimumBodyClearance = FMath::Min(MinimumBodyClearance,
			SignedWallDistance(Access::Unit(*Crowd, Leader).Position) - BodyRadius);
	}
	TestEqual(TEXT("Real facility walls survive force-system layout refreshes"), Access::WallCount(*Crowd), 4);
	// Unstuck steering may walk around a corner, so remaining south of the first
	// wall is not the contract. Check the complete footprint on every step instead.
	// The contact spring permits tiny compression; a centimetre is well below the
	// 34cm radius and cannot let the NPC's centre enter the building.
	TestTrue(*FString::Printf(TEXT("An unreachable precise target cannot pull the agent through any wall (minimum body clearance %.3fcm, end %s)"),
		MinimumBodyClearance, *FacilityTransform.InverseTransformPosition(Access::Unit(*Crowd, Leader).Position).ToString()),
		MinimumBodyClearance >= -1.0);
	TestEqual(TEXT("A blocked approach does not report a queue arrival"), Crowd->GetQueueArrivalNotifyCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_PreciseSlotsRetainContactAndRejectOverlap,
	"FestaFiesta.Crowd.Queue.PreciseSlotsRetainContactAndRejectOverlap", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_PreciseSlotsRetainContactAndRejectOverlap::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto Solve = [](float PositionSpacingCm, float TargetSpacingCm, bool bOtherHasSlot)
	{
		TArray<FUnitData> Units;
		Units.SetNum(2);
		for (int32 Index = 0; Index < Units.Num(); ++Index)
		{
			FUnitData& Unit = Units[Index];
			Unit.bIsActive = true;
			Unit.bHasQueueSlot = Index == 0 || bOtherHasSlot;
			Unit.bUsePreciseSlotPosition = Unit.bHasQueueSlot;
			Unit.Position = FVector(Index * PositionSpacingCm, 0, 0);
			Unit.QueueSlotLocation = FVector(Index * TargetSpacingCm, 0, 0);
			Unit.TargetLocation = Unit.QueueSlotLocation;
			Unit.ArrivalRadiusUU = 5.f;
		}
		TArray<FVector2f> Positions = {FVector2f::ZeroVector, FVector2f(PositionSpacingCm * .01f, 0.f)};
		TArray<FVector2f> Velocities = {FVector2f::ZeroVector, FVector2f::ZeroVector};
		TArray<float> Radii = {.34f, .34f};
		TArray<uint8> Active = {1, 1};
		TArray<int32> ActiveIds = {0, 1};
		FFF_CrowdAvoidanceConfig Config;
		FFF_CrowdSpatialGrid Grid;
		Grid.Build(Positions, Active, ActiveIds, Config.BroadphaseCellSizeMeters);
		FFF_CrowdForceInput Input;
		Input.PosMeters = &Positions;
		Input.VelMeters = &Velocities;
		Input.RadiusMeters = &Radii;
		Input.ActiveIds = &ActiveIds;
		FFF_CrowdForceSolver Solver;
		TArray<FVector2f> Acceleration;
		Solver.ComputeAll(Units, Input, Grid, {}, Config, Acceleration);
		return Acceleration[0];
	};
	TestTrue(TEXT("Distinct 90cm slots allow their intended standing distance"), Solve(90.f, 90.f, true).IsNearlyZero());
	TestTrue(TEXT("Overlapping bodies still receive contact separation despite compatible targets"), Solve(40.f, 90.f, true).X < -1.f);
	TestTrue(TEXT("Conflicting assigned targets do not disable personal-space collision avoidance"), Solve(90.f, 15.f, true).X < -1.f);
	TestTrue(TEXT("A passer-by without an assigned slot still participates in normal avoidance"), Solve(90.f, 90.f, false).X < -1.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_ValidSlotsBesideFacilityWall,
	"FestaFiesta.Crowd.Queue.ValidSlotsBesideFacilityWall", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_ValidSlotsBesideFacilityWall::RunTest(const FString& Parameters)
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
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
	if (!TestTrue(TEXT("Real facility for wall-adjacent slots installs"), Zones->ExecuteCommand(Install).bSucceeded)) return false;
	const AFFZSFacilityActor* Facility = Zones->FindFacility(Install.FacilityInstanceId);
	if (!TestNotNull(TEXT("Wall fixture facility"), Facility)) return false;
	const FTransform Transform = Facility->GetActorTransform();
	const float HalfY = Facility->GetFacilitySpec().FootprintSizeCells.Y * .5f;
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->bEnableHeightField = false;
	Crowd->QueueStallToleranceSeconds = 0.f;
	Crowd->bEnableQueuePatienceAbandon = false;
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 0, 3, AFF_BaseCrowd::StaticClass());
	TestTrue(TEXT("Default wall and crowd forces remain enabled"), Crowd->bEnableAvoidance && Crowd->bAvoidanceDrivesVelocity);
	using Access = FFF_CrowdQueueTestAccess;
	const TArray<float> WallDistances = {40.f, 55.f, 75.f};
	TArray<int32> Agents;
	TArray<FVector> Targets;
	for (int32 Index = 0; Index < WallDistances.Num(); ++Index)
	{
		const FVector LocalTarget((Index - 1) * 180.f, -HalfY - WallDistances[Index], 0);
		const FVector Target = Transform.TransformPosition(LocalTarget);
		const int32 Agent = Crowd->SpawnAgent(*World,
			Transform.TransformPosition(LocalTarget - FVector(0, 300, 0)), FRotator::ZeroRotator);
		if (Agent == INDEX_NONE) return false;
		Agents.Add(Agent);
		Targets.Add(Target);
		Access::RegisterLeader(*Crowd, Agent);
		Access::State(*Crowd, Agent).QueuePhase = EFF_PartyQueuePhase::Joined;
		if (!Crowd->SetPartyQueueColumn(Access::Unit(*Crowd, Agent).PartyID, {Target}, 90.f)) return false;
	}
	double MinimumBodyClearance = TNumericLimits<double>::Max();
	bool bBoundedMovement = true;
	for (int32 Step = 0; Step < 900; ++Step)
	{
		TArray<FVector> Before;
		for (const int32 Agent : Agents) Before.Add(Access::Unit(*Crowd, Agent).Position);
		Access::Step(*Crowd);
		for (int32 Index = 0; Index < Agents.Num(); ++Index)
		{
			const FUnitData& Unit = Access::Unit(*Crowd, Agents[Index]);
			bBoundedMovement &= FVector::Dist2D(Before[Index], Unit.Position) <= Unit.MaxSpeed / 30.f + .01f;
			const FVector Local = Transform.InverseTransformPosition(Unit.Position);
			MinimumBodyClearance = FMath::Min(MinimumBodyClearance, -HalfY - Local.Y - Unit.Radius);
		}
	}
	TestTrue(TEXT("Approaching a wall-adjacent seat uses bounded walking steps"), bBoundedMovement);
	TestTrue(*FString::Printf(TEXT("Valid wall-adjacent approaches retain body clearance (minimum %.3fcm)"), MinimumBodyClearance),
		MinimumBodyClearance >= 0.0);
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const double Error = FVector::Dist2D(Access::Unit(*Crowd, Agents[Index]).Position, Targets[Index]);
		TestTrue(*FString::Printf(TEXT("A slot %.0fcm outside the wall reaches its assigned centre (error %.3fcm)"), WallDistances[Index], Error),
			Error <= Crowd->GetAssignedSlotArrivalRadius());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_PreciseWallClearanceGuards,
	"FestaFiesta.Crowd.Queue.PreciseWallClearanceGuards", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_PreciseWallClearanceGuards::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const auto WallAcceleration = [](float PositionY, float TargetY, bool bPrecise)
	{
		TArray<FUnitData> Units;
		FUnitData& Unit = Units.AddDefaulted_GetRef();
		Unit.bIsActive = true;
		Unit.bHasQueueSlot = Unit.bUsePreciseSlotPosition = bPrecise;
		Unit.Position = FVector(0, PositionY, 0);
		Unit.TargetLocation = Unit.QueueSlotLocation = FVector(0, TargetY, 0);
		Unit.ArrivalRadiusUU = 5.f;
		TArray<FVector2f> Positions = {FVector2f(0, PositionY * .01f)};
		TArray<FVector2f> Velocities = {FVector2f::ZeroVector};
		TArray<float> Radii = {.34f};
		TArray<uint8> Active = {1};
		TArray<int32> ActiveIds = {0};
		FFF_CrowdAvoidanceConfig Config;
		FFF_CrowdSpatialGrid Grid;
		Grid.Build(Positions, Active, ActiveIds, Config.BroadphaseCellSizeMeters);
		FFF_CrowdForceInput Input;
		Input.PosMeters = &Positions;
		Input.VelMeters = &Velocities;
		Input.RadiusMeters = &Radii;
		Input.ActiveIds = &ActiveIds;
		TArray<FFF_CrowdWallSegment> Walls = {{FVector2f(-5, 0), FVector2f(5, 0)}};
		FFF_CrowdForceSolver Solver;
		TArray<FVector2f> Acceleration;
		Solver.ComputeAll(Units, Input, Grid, Walls, Config, Acceleration);
		return Acceleration[0].Y;
	};
	TestEqual(TEXT("A body-clear precise approach may stand forty centimetres outside a wall"), WallAcceleration(45.f, 40.f, true), 0.f);
	TestTrue(TEXT("A target across the wall retains repulsion"), WallAcceleration(45.f, -40.f, true) > 0.f);
	TestTrue(TEXT("Body overlap retains wall contact force"), WallAcceleration(20.f, 40.f, true) > 6.f);
	TestTrue(TEXT("Ordinary walkers retain wall personal space"), WallAcceleration(45.f, 40.f, false) > 0.f);
	TestTrue(TEXT("A precise target inside body clearance does not disable wall forces"), WallAcceleration(45.f, 30.f, true) > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdQueue_StartedServiceSurvivesWaitingImpatience,
	"FestaFiesta.Crowd.Queue.StartedServiceSurvivesWaitingImpatience",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdQueue_StartedServiceSurvivesWaitingImpatience::RunTest(const FString& Parameters)
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
	auto* Bridge = World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>();
	auto* Queue = World->GetSubsystem<UFF_QueueSubsystem>();
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	auto* Progression = World->GetGameInstance()->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestTrue(TEXT("The real crowd, queue, and settlement subsystems exist"),
		Zones && Bridge && Queue && Crowd && Progression)) return false;
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(World->GetGameInstance());
	UDataTable* Costs = NewObject<UDataTable>(Definition);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost;
	Cost.InstallCost = 100;
	const FName CostKey(TEXT("Cost.Booth.FoodStall.PatienceTest"));
	Costs->AddRow(CostKey, Cost);
	Definition->FacilityCostTable = Costs;
	Definition->FacilityRevenueTable.Reset();
	Definition->FestivalId = TEXT("ServicePatienceAutomation");
	Definition->InitialGold = 10000;
	Definition->BoothCommissionRate = .25f;
	Progression->StartFestival(Definition);
	Queue->SetPaused(true);
	Queue->SimulationStepSeconds = .05f;
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!Zone || !Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha")) || !Zones->RegisterZone(Zone)) return false;
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_PatienceTest");
	Spec.InstallCostKey = CostKey;
	Spec.bOverrideRevenuePerUseGold = true;
	Spec.RevenuePerUseGold = 100;
	Spec.QueueRules.ServiceDurationSeconds = 5.f;
	Spec.QueueRules.AbandonAfterSeconds = 0.f;
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
		Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
	const FFFZSCommandResult Result = Zones->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("The paid test booth installs: %s"), *Result.Message.ToString()), Result.bSucceeded)) return false;
	World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()->OpenEligibleFacilities(false);
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(20000)), 1, 4, AFF_BaseCrowd::StaticClass());
	Crowd->QueueStallToleranceSeconds = 0.f;
	Crowd->bEnableQueuePatienceAbandon = true;
	Crowd->Temp_FestivalMinutesPerRealSecond = 1.f;
	using Access = FFF_CrowdQueueTestAccess;
	const int32 UsingLeader = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	const int32 WaitingLeader = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	if (UsingLeader == INDEX_NONE || WaitingLeader == INDEX_NONE) return false;
	for (const int32 Leader : {UsingLeader, WaitingLeader})
	{
		Access::RegisterLeader(*Crowd, Leader);
		Access::SetWaitThreshold(*Crowd, Leader, 1.f);
		Access::State(*Crowd, Leader).CurrentDestinationId =
			FFF_ZoneSystemBridge::FacilityInstanceIdToDestinationId(Install.FacilityInstanceId);
		if (!TestTrue(TEXT("Crowd admits the test team through the real queue"), Access::Arrive(*Crowd, Leader))) return false;
	}
	Queue->FlushPendingEvents();
	for (const int32 Leader : {UsingLeader, WaitingLeader})
	{
		// This test controls arrival, not movement: both teams stand at their assigned slots.
		Access::Unit(*Crowd, Leader).Position = Access::Unit(*Crowd, Leader).QueueSlotLocation;
		Access::Arrive(*Crowd, Leader);
	}
	const int32 QueueId = Bridge->FindQueueFacilityId(Install.FacilityInstanceId);
	const int32 UsingParty = Access::Unit(*Crowd, UsingLeader).PartyID;
	const int32 WaitingParty = Access::Unit(*Crowd, WaitingLeader).PartyID;
	Queue->AdvanceFixedSteps(1);
	if (!TestTrue(TEXT("The front team's actual service has started"), Queue->HasPartyStartedService(QueueId, UsingParty))) return false;
	TestFalse(TEXT("Arrival at a waiting slot is not a service start"), Queue->HasPartyStartedService(QueueId, WaitingParty));
	if (!TestEqual(TEXT("The using team reaches the crowd's patience-check phase"),
		Access::State(*Crowd, UsingLeader).QueuePhase, EFF_PartyQueuePhase::AtSlot)
		|| !TestEqual(TEXT("The waiting team reaches the same patience-check phase"),
			Access::State(*Crowd, WaitingLeader).QueuePhase, EFF_PartyQueuePhase::AtSlot)) return false;
	const int32 GoldBefore = Progression->GetGold();
	const int32 RevenueBefore = Progression->GetTodayRevenue();
	for (const int32 Leader : {UsingLeader, WaitingLeader})
	{
		// Their recorded queue wait now exceeds personal patience while the front team is using the booth.
		Access::State(*Crowd, Leader).QueueAtSlotSimTime = Crowd->GetSimTimeSeconds() - 2.f;
	}
	Access::Arrive(*Crowd, UsingLeader);
	TestEqual(TEXT("Waiting impatience never cancels an already-started use"), Crowd->GetQueuePatienceAbandons(), 0);
	TestTrue(TEXT("The using team remains in the authoritative service"), Queue->HasPartyStartedService(QueueId, UsingParty));
	Access::Arrive(*Crowd, WaitingLeader);
	TestEqual(TEXT("A team that has not started service still abandons when patience runs out"), Crowd->GetQueuePatienceAbandons(), 1);
	Queue->FlushPendingEvents();
	Queue->AdvanceFixedSteps(100);
	TestEqual(TEXT("The retained service completes normally"), Queue->GetQueueState(QueueId).CompletedPartyCount, 1);
	TestFalse(TEXT("A completed team no longer reports active service"), Queue->HasPartyStartedService(QueueId, UsingParty));
	TestFalse(TEXT("An unknown facility never reports active service"), Queue->HasPartyStartedService(INDEX_NONE, UsingParty));
	TestEqual(TEXT("The completed use adds spendable income; the waiting cancellation adds none"), Progression->GetGold(), GoldBefore + 25);
	TestEqual(TEXT("The retained service reaches today's actual revenue"), Progression->GetTodayRevenue(), RevenueBefore + 25);
	FFF_FacilityInstanceRevenue Ledger;
	if (!TestTrue(TEXT("The facility records the completed use"), Progression->GetFacilityInstanceRevenue(Install.FacilityInstanceId, Ledger))) return false;
	TestEqual(TEXT("Only the actual service creates a facility payment"), Ledger.TodayPaymentCount, 1);
	TestEqual(TEXT("The facility's today amount contains the real payment"), Ledger.TodayGold, 25);
	return true;
}

#endif
