#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Staff/FF_StaffDispatchSubsystem.h"
#include "Staff/FF_StaffSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchJourneyTest,
	"FestaFiesta.Incident.StaffDispatch.HomeRouteAndCancellation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchJourneyTest::RunTest(const FString& Parameters)
{
	UFF_StaffSubsystem::FStaffRuntime Journey;
	const FVector Home(1000.f, 2500.f, 20.f);
	const FVector Corner = Home + FVector(300.f, 0.f, 0.f);
	const FVector Destination = Corner + FVector(0.f, 300.f, 0.f);
	Journey.Position = Home;
	Journey.Travelled.Add(Home);
	Journey.Path = {Home, Corner, Destination};

	TestFalse(TEXT("A partial trip remains outbound"),
		UFF_StaffSubsystem::AdvanceAlongPath(Journey, 450.f));
	TestEqual(TEXT("Movement follows the path corner instead of crossing the facility"),
		Journey.Position, Corner + FVector(0.f, 150.f, 0.f));
	UFF_StaffSubsystem::ReverseTravelledPath(Journey);
	TestFalse(TEXT("Returning retraces the reached corner before home"),
		UFF_StaffSubsystem::AdvanceAlongPath(Journey, 150.f));
	TestEqual(TEXT("Return reaches the same corner"), Journey.Position, Corner);
	TestTrue(TEXT("The remainder of the trip reaches the assigned home"),
		UFF_StaffSubsystem::AdvanceAlongPath(Journey, 300.f));
	TestEqual(TEXT("Return ends at the actual facility departure point"), Journey.Position, Home);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchIncidentTest,
	"FestaFiesta.Incident.StaffDispatch.ArrivalAndCancellationLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchIncidentTest::RunTest(const FString& Parameters)
{
	UFF_IncidentSubsystem* Incident = NewObject<UFF_IncidentSubsystem>();
	UFF_StaffDispatchSubsystem* Dispatch = NewObject<UFF_StaffDispatchSubsystem>();
	UFF_StaffSubsystem* Bodies = NewObject<UFF_StaffSubsystem>();
	UFF_IncidentDefinition* Definition = NewObject<UFF_IncidentDefinition>();
	Definition->IncidentTag = FFIncidentTags::Crowd::MemberMissing;
	Incident->CachedDef.Add(Definition->IncidentTag, Definition);
	Dispatch->Incident = Incident;
	Dispatch->StaffBodies = Bodies;
	Incident->OnIncidentClosed.AddUniqueDynamic(Dispatch, &UFF_StaffDispatchSubsystem::HandleIncidentClosed);
	auto Step = [Bodies, Dispatch](float DeltaTime)
	{
		Bodies->Tick(DeltaTime);
		Dispatch->Tick(DeltaTime);
	};

	const FVector Home(2000.f, 1000.f, 0.f);
	const FVector Target = Home + FVector(600.f, 0.f, 0.f);
	FFF_IncidentHandle Handle;
	Handle.Index = Incident->IncidentSlots.AddDefaulted();
	UFF_IncidentSubsystem::FIncidentSlot& Slot = Incident->IncidentSlots[Handle.Index];
	Slot.bInUse = true;
	Slot.IncidentTag = Definition->IncidentTag;
	Slot.DedupeKey = TEXT("StaffDispatchTest");
	Slot.Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
	Slot.Anchor.WorldPoint = Target;
	Incident->ActiveByKey.Add(Slot.DedupeKey, Handle);

	constexpr int32 StaffId = 42;
	UFF_StaffSubsystem::FStaffRuntime Journey;
	Journey.State = EFF_StaffState::Moving;
	Journey.Target = Slot.Anchor;
	Journey.PathGoal = Target;
	Journey.Position = Home;
	Journey.HomeLocation = Home;
	Journey.Path = {Home, Target};
	Journey.Travelled.Add(Home);

	UFF_StaffDispatchSubsystem::FDispatch Response;
	Response.Handle = Handle;
	Response.Anchor = Slot.Anchor;
	Response.StaffIds.Add(StaffId);
	Bodies->Staff.Add(StaffId, Journey);
	Dispatch->Dispatches.Add(Response);

	TestFalse(TEXT("A missing staffing owner cannot dispatch an additional team"),
		Dispatch->TryDispatch(Handle, INDEX_NONE, 1, Slot.Anchor));
	TestTrue(TEXT("Missing team does not close the incident"), Incident->IsHandleValid(Handle));
	Step(1.f);
	TestTrue(TEXT("Incident remains active while staff travels"), Incident->IsHandleValid(Handle));
	TestTrue(TEXT("The response is still outbound"), Dispatch->IsResponding(Handle));
	Step(1.f);
	TestFalse(TEXT("Incident resolves only after arrival"), Incident->IsHandleValid(Handle));
	TestEqual(TEXT("The visual remains during the return trip"), Dispatch->Dispatches.Num(), 1);
	TestTrue(TEXT("Resolved incident starts return"), Dispatch->Dispatches[0].bReturning);
	TestEqual(TEXT("The staff body is walking home"), Bodies->GetStaffState(StaffId), EFF_StaffState::Returning);
	TestFalse(TEXT("A stale handle cannot resolve a reused incident"), Incident->CompleteStaffResponse(Handle, INDEX_NONE));
	Step(2.f);
	TestTrue(TEXT("Return releases the journey"), Dispatch->Dispatches.IsEmpty());
	TestTrue(TEXT("Return removes the staff body"), Bodies->IsIdle(StaffId));

	// An externally withdrawn incident cancels an in-flight response instead of resolving it later.
	Handle.Serial = Incident->IncidentSlots[Handle.Index].Serial;
	UFF_IncidentSubsystem::FIncidentSlot& ReusedSlot = Incident->IncidentSlots[Handle.Index];
	ReusedSlot.bInUse = true;
	ReusedSlot.DedupeKey = TEXT("CancelledStaffDispatchTest");
	Incident->ActiveByKey.Add(ReusedSlot.DedupeKey, Handle);
	Response.Handle = Handle;
	Bodies->Staff.Add(StaffId, Journey);
	Dispatch->Dispatches.Add(Response);
	Step(1.f);
	Incident->WithdrawIncident(ReusedSlot.DedupeKey);
	TestTrue(TEXT("External closure turns staff back"), Dispatch->Dispatches[0].bReturning);
	Step(1.f);
	TestTrue(TEXT("Cancelled response returns only the distance already travelled"), Dispatch->Dispatches.IsEmpty());
	TestEqual(TEXT("Cancellation preserves the expired state"),
		Incident->IncidentSlots[Handle.Index].State, EFF_IncidentState::Expired);
	Incident->OnIncidentClosed.RemoveDynamic(Dispatch, &UFF_StaffDispatchSubsystem::HandleIncidentClosed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchWarpTest,
	"FestaFiesta.Incident.StaffDispatch.WorkerFollowsAndWarpsWhenBlocked",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchWarpTest::RunTest(const FString& Parameters)
{
	UFF_IncidentSubsystem* Incident = NewObject<UFF_IncidentSubsystem>();
	UFF_StaffDispatchSubsystem* Dispatch = NewObject<UFF_StaffDispatchSubsystem>();
	UFF_StaffSubsystem* Bodies = NewObject<UFF_StaffSubsystem>();
	UFF_IncidentDefinition* Definition = NewObject<UFF_IncidentDefinition>();
	Definition->IncidentTag = FFIncidentTags::Crowd::MemberMissing;
	Incident->CachedDef.Add(Definition->IncidentTag, Definition);
	Dispatch->Incident = Incident;
	Dispatch->StaffBodies = Bodies;
	auto Step = [Bodies, Dispatch](float DeltaTime)
	{
		Bodies->Tick(DeltaTime);
		Dispatch->Tick(DeltaTime);
	};

	const FVector Home(2000.f, 1000.f, 0.f);
	const FVector Target = Home + FVector(600.f, 0.f, 0.f);
	FFF_IncidentHandle Handle;
	Handle.Index = Incident->IncidentSlots.AddDefaulted();
	UFF_IncidentSubsystem::FIncidentSlot& Slot = Incident->IncidentSlots[Handle.Index];
	Slot.bInUse = true;
	Slot.IncidentTag = Definition->IncidentTag;
	Slot.DedupeKey = TEXT("StaffGiveUpTest");
	Slot.Anchor.Kind = EFF_IncidentAnchorKind::WorldPoint;
	Slot.Anchor.WorldPoint = Target;
	Incident->ActiveByKey.Add(Slot.DedupeKey, Handle);

	constexpr int32 StaffId = 7;
	UFF_StaffSubsystem::FStaffRuntime Journey;
	Journey.State = EFF_StaffState::Moving;
	Journey.Target = Slot.Anchor;
	Journey.PathGoal = Target;
	Journey.Position = Home;
	Journey.HomeLocation = Home;
	Journey.Path = {Home, Target};
	Journey.Travelled.Add(Home);
	Bodies->Staff.Add(StaffId, Journey);

	UFF_StaffDispatchSubsystem::FDispatch Response;
	Response.Handle = Handle;
	Response.Anchor = Slot.Anchor;
	Response.StaffIds.Add(StaffId);
	Response.RemainingWorkMinutes = 10.f;
	Dispatch->Dispatches.Add(Response);

	Step(1.f);
	Step(1.f);
	TestEqual(TEXT("Reaching the target puts the member to work"), Bodies->GetStaffState(StaffId), EFF_StaffState::Working);
	float Remaining = 0.f;
	TestTrue(TEXT("Everyone at work starts the incident work"), Dispatch->GetRemainingWorkMinutes(Handle, Remaining));

	const FVector MovedTarget = Target + FVector(500.f, 0.f, 0.f);
	Bodies->Staff[StaffId].Target.WorldPoint = MovedTarget;
	Step(0.1f);
	TestEqual(TEXT("A target that walks away pulls the worker back to moving"), Bodies->GetStaffState(StaffId), EFF_StaffState::Moving);
	TestFalse(TEXT("Work pauses while a member follows the target"), Dispatch->GetRemainingWorkMinutes(Handle, Remaining));
	TestTrue(TEXT("Following is still a response"), Dispatch->IsResponding(Handle));

	Step(0.1f);
	TestEqual(TEXT("One failed path search keeps the member trying"), Bodies->GetStaffState(StaffId), EFF_StaffState::Moving);
	TestTrue(TEXT("A failed path search does not cancel the response"), Dispatch->IsResponding(Handle));

	Step(0.5f);
	TestEqual(TEXT("A second failed path search warps the member to work"), Bodies->GetStaffState(StaffId), EFF_StaffState::Working);
	TestEqual(TEXT("The warp lands on the moved target"), Bodies->Staff[StaffId].Position, MovedTarget);
	TestTrue(TEXT("Warping keeps the response alive"), Dispatch->IsResponding(Handle));
	Step(0.1f);
	TestTrue(TEXT("Work resumes after the warp"), Dispatch->GetRemainingWorkMinutes(Handle, Remaining));
	TestTrue(TEXT("Warping does not resolve the incident by itself"), Incident->IsHandleValid(Handle));

	Bodies->RecallStaff(StaffId);
	Step(0.1f);
	TestTrue(TEXT("A warped member has no route to retrace and leaves on the spot"), Bodies->IsIdle(StaffId));
	Step(0.1f);
	TestTrue(TEXT("The recalled response is cleared once everyone is home"), Dispatch->Dispatches.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchStallTest,
	"FestaFiesta.Incident.StaffDispatch.StalledWalkWarpsAfterTwoTimeouts",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchStallTest::RunTest(const FString& Parameters)
{
	UFF_StaffSubsystem::FStaffRuntime Journey;
	const FVector Home(0.f, 0.f, 0.f);
	const FVector Target(1000.f, 0.f, 0.f);
	Journey.Position = Home;
	Journey.Path = {Home, FVector(0.f, 400.f, 0.f), FVector(1000.f, 400.f, 0.f), Target};
	UFF_StaffSubsystem::ResetProgress(Journey, Target);
	TestEqual(TEXT("Progress is measured along the path, not in a straight line"), Journey.BestDistance, 1800.f);

	UFF_StaffSubsystem* Bodies = NewObject<UFF_StaffSubsystem>();
	UFF_StaffSubsystem::AdvanceAlongPath(Journey, 300.f);
	TestFalse(TEXT("Walking a detour away from the target still counts as progress"),
		Bodies->CheckProgress(Journey, Target, 1.f));
	TestEqual(TEXT("Progress clears the stall clock"), Journey.StallElapsed, 0.f);

	TestFalse(TEXT("Standing still under the timeout is tolerated"), Bodies->CheckProgress(Journey, Target, 4.9f));
	TestFalse(TEXT("The first timeout only counts one failure"), Bodies->CheckProgress(Journey, Target, 0.2f));
	TestEqual(TEXT("One failure is recorded"), Journey.FailCount, 1);
	TestTrue(TEXT("The second timeout asks for a warp"), Bodies->CheckProgress(Journey, Target, 5.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffPathProjectionTest,
	"FestaFiesta.Incident.StaffDispatch.ExitInsideNavHoleLeavesOnSameFloor",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffPathProjectionTest::RunTest(const FString& Parameters)
{
	const float Near = UFF_StaffSubsystem::NearProjectionGapCm;
	const float Wide = FVector(UFF_StaffSubsystem::WideProjectionHorizontalCm,
		UFF_StaffSubsystem::WideProjectionHorizontalCm, 0.f).Size2D();

	const FVector Exit(82685.f, 34747.f, 585.16f);
	TestFalse(TEXT("A floor 139cm below the facility exit is never accepted"),
		UFF_StaffSubsystem::IsProjectionAcceptable(Exit, FVector(82685.f, 34747.f, 446.29f), Wide));
	const FVector OutsideFootprint(82685.f + 320.f, 34747.f, 560.f);
	TestFalse(TEXT("The near search keeps its 100cm horizontal limit"),
		UFF_StaffSubsystem::IsProjectionAcceptable(Exit, OutsideFootprint, Near));
	TestTrue(TEXT("The wide search reaches the same-height floor outside the facility"),
		UFF_StaffSubsystem::IsProjectionAcceptable(Exit, OutsideFootprint, Wide));
	TestTrue(TEXT("An incident point on the floor projects with the near search"),
		UFF_StaffSubsystem::IsProjectionAcceptable(FVector(82702.47f, 32089.11f, 556.3f),
			FVector(82702.47f, 32089.11f, 564.43f), Near));
	TestTrue(TEXT("Exactly 100cm of height difference is still accepted"),
		UFF_StaffSubsystem::IsProjectionAcceptable(FVector::ZeroVector, FVector(0.f, 0.f, -100.f), Near));
	TestFalse(TEXT("More than 100cm of height difference is rejected"),
		UFF_StaffSubsystem::IsProjectionAcceptable(FVector::ZeroVector, FVector(0.f, 0.f, -100.5f), Wide));

	const FVector Goal(82702.f, 32089.f, 556.f);
	const TArray<FVector> NavPoints = {OutsideFootprint, FVector(82700.f, 33000.f, 560.f), FVector(82702.f, 32089.f, 564.f)};
	TArray<FVector> Path;
	UFF_StaffSubsystem::AssemblePath(Exit, Goal, NavPoints, false, Path);
	TestEqual(TEXT("A complete route is exit, navigation points, then the incident point"), Path.Num(), 5);
	TestEqual(TEXT("The walk starts at the facility exit itself"), Path[0], Exit);
	TestEqual(TEXT("The walk ends on the incident point, inside the arrival radius"), Path.Last(), Goal);

	UFF_StaffSubsystem::AssemblePath(Exit, Goal, NavPoints, true, Path);
	TestEqual(TEXT("A partial route stops where navigation ends"), Path.Last(), NavPoints.Last());
	TestEqual(TEXT("A partial route does not cut straight to an unreachable target"), Path.Num(), 4);

	UFF_StaffSubsystem::AssemblePath(Exit, Goal, {Exit, Goal}, false, Path);
	TestEqual(TEXT("Endpoints already on navigation are not duplicated"), Path.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffTimeScaleTest,
	"FestaFiesta.Incident.StaffDispatch.WalkFollowsFestivalTimeScale",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffTimeScaleTest::RunTest(const FString& Parameters)
{
	UFF_StaffSubsystem* Bodies = NewObject<UFF_StaffSubsystem>();
	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UFF_FestivalStageSubsystem* Stage = NewObject<UFF_FestivalStageSubsystem>(GameInstance);
	Bodies->Stage = Stage;

	constexpr int32 StaffId = 3;
	const FVector Home(0.f, 0.f, 0.f);
	const FVector Target(10000.f, 0.f, 0.f);
	UFF_StaffSubsystem::FStaffRuntime Journey;
	Journey.State = EFF_StaffState::Moving;
	Journey.Target.Kind = EFF_IncidentAnchorKind::WorldPoint;
	Journey.Target.WorldPoint = Target;
	Journey.PathGoal = Target;
	Journey.Position = Home;
	Journey.Path = {Home, Target};
	Journey.Travelled.Add(Home);
	UFF_StaffSubsystem::ResetProgress(Journey, Target);
	Bodies->Staff.Add(StaffId, Journey);
	const double Speed = Bodies->TravelSpeed;

	Stage->SetTimeScale(1.f);
	Bodies->Tick(1.f);
	TestEqual(TEXT("At x1 one real second walks one second of travel"), Bodies->Staff[StaffId].Position.X, Speed);

	Stage->SetTimeScale(4.f);
	Bodies->Tick(1.f);
	TestEqual(TEXT("At x4 one real second walks four seconds of travel"), Bodies->Staff[StaffId].Position.X, Speed * 5.f);

	Stage->SetTimeScale(0.f);
	Bodies->Tick(1.f);
	TestEqual(TEXT("A paused festival holds the staff in place"), Bodies->Staff[StaffId].Position.X, Speed * 5.f);

	Stage->SetMinigameInProgress(true);
	Stage->SetTimeScale(4.f);
	Bodies->Tick(1.f);
	TestEqual(TEXT("The effective scale wins over the raw speed button"), Bodies->Staff[StaffId].Position.X, Speed * 6.f);
	return true;
}

#endif
