#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Staff/FF_StaffDispatchSubsystem.h"
#include "Staff/FF_StaffSubsystem.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchGiveUpTest,
	"FestaFiesta.Incident.StaffDispatch.WorkerFollowsAndGivesUp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchGiveUpTest::RunTest(const FString& Parameters)
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

	Bodies->Staff[StaffId].Target.WorldPoint = Target + FVector(500.f, 0.f, 0.f);
	Step(0.1f);
	TestEqual(TEXT("A target that walks away pulls the worker back to moving"), Bodies->GetStaffState(StaffId), EFF_StaffState::Moving);
	TestFalse(TEXT("Work pauses while a member follows the target"), Dispatch->GetRemainingWorkMinutes(Handle, Remaining));
	TestTrue(TEXT("Following is still a response"), Dispatch->IsResponding(Handle));

	Step(0.1f);
	TestEqual(TEXT("A member that cannot find a new path gives up and returns"), Bodies->GetStaffState(StaffId), EFF_StaffState::Returning);
	TestFalse(TEXT("A member giving up cancels the whole response"), Dispatch->IsResponding(Handle));
	TestTrue(TEXT("Cancellation does not resolve the incident"), Incident->IsHandleValid(Handle));

	Step(2.f);
	TestTrue(TEXT("The member walks home and its body is removed"), Bodies->IsIdle(StaffId));
	TestTrue(TEXT("The cancelled response is cleared once everyone is home"), Dispatch->Dispatches.IsEmpty());
	return true;
}

#endif
