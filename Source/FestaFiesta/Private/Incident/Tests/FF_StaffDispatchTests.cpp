#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Incident/FF_IncidentDefinition.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Incident/FF_IncidentTags.h"
#include "Incident/FF_StaffDispatchSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffDispatchJourneyTest,
	"FestaFiesta.Incident.StaffDispatch.HomeRouteAndCancellation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_StaffDispatchJourneyTest::RunTest(const FString& Parameters)
{
	UFF_StaffDispatchSubsystem::FDispatch Response;
	UFF_StaffDispatchSubsystem::FJourney& Journey = Response.Journeys.AddDefaulted_GetRef();
	const FVector Home(1000.f, 2500.f, 20.f);
	const FVector Corner = Home + FVector(300.f, 0.f, 0.f);
	const FVector Destination = Corner + FVector(0.f, 300.f, 0.f);
	Journey.Position = Home;
	Journey.Travelled.Add(Home);
	Journey.Path = {Home, Corner, Destination};

	TestFalse(TEXT("A partial trip remains outbound"),
		UFF_StaffDispatchSubsystem::AdvanceJourney(Journey, 450.f));
	TestEqual(TEXT("Movement follows the path corner instead of crossing the facility"),
		Journey.Position, Corner + FVector(0.f, 150.f, 0.f));
	UFF_StaffDispatchSubsystem::BeginReturn(Response);
	TestTrue(TEXT("Cancellation switches the journey to return"), Response.bReturning);
	TestFalse(TEXT("Returning retraces the reached corner before home"),
		UFF_StaffDispatchSubsystem::AdvanceJourney(Journey, 150.f));
	TestEqual(TEXT("Return reaches the same corner"), Journey.Position, Corner);
	TestTrue(TEXT("The remainder of the trip reaches the assigned home"),
		UFF_StaffDispatchSubsystem::AdvanceJourney(Journey, 300.f));
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
	UFF_IncidentDefinition* Definition = NewObject<UFF_IncidentDefinition>();
	Definition->IncidentTag = FFIncidentTags::Crowd::MemberMissing;
	Incident->CachedDef.Add(Definition->IncidentTag, Definition);
	Dispatch->Incident = Incident;
	Incident->OnIncidentClosed.AddUniqueDynamic(Dispatch, &UFF_StaffDispatchSubsystem::HandleIncidentClosed);

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

	UFF_StaffDispatchSubsystem::FDispatch Response;
	Response.Handle = Handle;
	Response.Anchor = Slot.Anchor;
	Response.Destination = Target;
	UFF_StaffDispatchSubsystem::FJourney& Journey = Response.Journeys.AddDefaulted_GetRef();
	Journey.StaffId = 42;
	Journey.Position = Home;
	Journey.Path = {Home, Target};
	Journey.Travelled.Add(Home);
	Dispatch->Dispatches.Add(Response);

	TestFalse(TEXT("A missing staffing owner cannot dispatch an additional team"),
		Dispatch->TryDispatch(Handle, INDEX_NONE, 1, Slot.Anchor));
	TestTrue(TEXT("Missing team does not close the incident"), Incident->IsHandleValid(Handle));
	Dispatch->Tick(1.f);
	TestTrue(TEXT("Incident remains active while staff travels"), Incident->IsHandleValid(Handle));
	TestTrue(TEXT("The response is still outbound"), Dispatch->IsResponding(Handle));
	Dispatch->Tick(1.f);
	TestFalse(TEXT("Incident resolves only after arrival"), Incident->IsHandleValid(Handle));
	TestEqual(TEXT("The visual remains during the return trip"), Dispatch->Dispatches.Num(), 1);
	TestTrue(TEXT("Resolved incident starts return"), Dispatch->Dispatches[0].bReturning);
	TestFalse(TEXT("A stale handle cannot resolve a reused incident"), Incident->CompleteStaffResponse(Handle, INDEX_NONE));
	Dispatch->Tick(2.f);
	TestTrue(TEXT("Return releases the journey"), Dispatch->Dispatches.IsEmpty());

	// An externally withdrawn incident cancels an in-flight response instead of resolving it later.
	Handle.Serial = Incident->IncidentSlots[Handle.Index].Serial;
	UFF_IncidentSubsystem::FIncidentSlot& ReusedSlot = Incident->IncidentSlots[Handle.Index];
	ReusedSlot.bInUse = true;
	ReusedSlot.DedupeKey = TEXT("CancelledStaffDispatchTest");
	Incident->ActiveByKey.Add(ReusedSlot.DedupeKey, Handle);
	Response.Handle = Handle;
	Dispatch->Dispatches.Add(Response);
	Dispatch->Tick(1.f);
	Incident->WithdrawIncident(ReusedSlot.DedupeKey);
	TestTrue(TEXT("External closure turns staff back"), Dispatch->Dispatches[0].bReturning);
	Dispatch->Tick(1.f);
	TestTrue(TEXT("Cancelled response returns only the distance already travelled"), Dispatch->Dispatches.IsEmpty());
	TestEqual(TEXT("Cancellation preserves the expired state"),
		Incident->IncidentSlots[Handle.Index].State, EFF_IncidentState::Expired);
	Incident->OnIncidentClosed.RemoveDynamic(Dispatch, &UFF_StaffDispatchSubsystem::HandleIncidentClosed);
	return true;
}

#endif
