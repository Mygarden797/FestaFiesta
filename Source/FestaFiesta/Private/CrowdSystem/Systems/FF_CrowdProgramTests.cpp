// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSAudienceLibrary.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "NPC/Party/FF_ProgramReservationReturnTrip.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/UnrealType.h"

// 실제 상태 전이 함수를 호출한다. 테스트용 로직 복제는 하지 않는다.
struct FFF_CrowdProgramTestAccess
{
	static FPartyDecisionState& State(UFF_CrowdSimulationSubsystem& C, int32 I) { return C.PartyDecisionStates[I]; }
	static FUnitData& Unit(UFF_CrowdSimulationSubsystem& C, int32 I) { return C.UnitDatas[I]; }
	static float& Stay(UFF_CrowdSimulationSubsystem& C, int32 I) { return C.PartyRemainingStayMinutes[I]; }
	static bool Return(UFF_CrowdSimulationSubsystem& C, int32 I, const FFF_ProgramReservation& R)
	{ return C.BeginProgramReturn(I, R, FVector(1000, 0, 0)); }
	static bool Arrive(UFF_CrowdSimulationSubsystem& C, int32 I) { return C.HandleProgramArrival(I); }
	static void Update(UFF_CrowdSimulationSubsystem& C) { C.UpdateProgramReservationStates(); }
	static void Decide(UFF_CrowdSimulationSubsystem& C, int32 I)
	{
		C.RequestDecision(I, EFF_DecisionReason::Arrived);
		C.ProcessDecisions();
	}
	static void Needs(UFF_CrowdSimulationSubsystem& C)
	{ C.SimTimeSeconds += 1.f; C.AdvanceNeeds(false); }
	static void LeaveAll(UFF_CrowdSimulationSubsystem& C) { C.AllAgentLeavingTrigger(); }
	static void Process(UFF_CrowdSimulationSubsystem& C) { C.ProcessDecisions(); }
	static void ClearSpawnRequests(UFF_CrowdSimulationSubsystem& C) { C.DecisionQueue.Reset(); }
	static void RegisterLeader(UFF_CrowdSimulationSubsystem& C, int32 I)
	{ C.PartyLeaderByID.Add(C.UnitDatas[I].PartyID, I); }
	static bool Deadline(UFF_CrowdSimulationSubsystem& C, int32 I, const FFF_ProgramReservation& R, float Now)
	{ return C.TryBeginProgramReturnIfDue(I, R, FVector(1500, 0, 0), Now); }
	static FVector Wander(UFF_CrowdSimulationSubsystem& C, int32 I) { return C.PickWanderDestination(I); }
	static bool InBounds(const UFF_CrowdSimulationSubsystem& C, const FVector& P)
	{ return C.IsInsideFestivalBounds(P); }
	static bool Audience(UFF_CrowdSimulationSubsystem& C, int32 I, const FGuid& Facility,
		const FFFZSAudienceSnapshot& Snapshot) { return C.BeginAudienceVisit(I, Facility, Snapshot); }
	static bool AudienceArrive(UFF_CrowdSimulationSubsystem& C, int32 I)
	{ return C.HandleAudienceArrival(I); }
	static void StartMainAudience(UFF_CrowdSimulationSubsystem& C)
	{ C.HandleMainProgramStarted(TEXT("TestShow")); }
	static void EndMainAudience(UFF_CrowdSimulationSubsystem& C)
	{ C.HandleMainProgramEnded(TEXT("TestShow")); }
	static void ReleaseAudience(UFF_CrowdSimulationSubsystem& C, int32 I)
	{ C.ReleaseAudienceParty(I, false); }
	static void TickShowState(UFF_CrowdSimulationSubsystem& C) { C.UpdateAudienceShowState(); }
	static void Step(UFF_CrowdSimulationSubsystem& C, float Dt) { C.StepSimulation(Dt); }
	static void ClearQueue(UFF_CrowdSimulationSubsystem& C, int32 I, bool bLeave)
	{ C.ClearPartyQueueState(I, bLeave); }
	static int32 Reclaim(UFF_CrowdSimulationSubsystem& C, float ShowEndSeconds)
	{ C.MainProgramEndLiveSeconds = ShowEndSeconds; return C.ReclaimUnreachableAudienceSeats(); }
};

struct FFF_CrowdProgramDecisionTestAccess
{
	static void EnterLive(UFF_CrowdDecisionSubsystem& D) { D.HandleStageChanged(EFF_FestivalStage::Setup, EFF_FestivalStage::Live); }
	static void StartMain(UFF_CrowdDecisionSubsystem& D) { D.HandleMainProgramStarted(TEXT("Test main stage")); }
	static void EndMain(UFF_CrowdDecisionSubsystem& D) { D.HandleMainProgramEnded(TEXT("Test main stage")); }
	static void SetReadModel(UFF_CrowdDecisionSubsystem& D, const FFFZSReadModel& Model)
	{ D.CachedReadModel = Model; D.CachedReadModelFrame = GFrameCounter; }
	static bool Decide(UFF_CrowdDecisionSubsystem& D, const FFF_PartyDecisionContext& C, FFF_PartyDecisionResult& R)
	{ return D.HandlePartyNeedsDecision(C, R); }
	static EFF_PartyFallbackAction Fallback(UFF_CrowdDecisionSubsystem& D, const FFF_PartyDecisionContext& C)
	{ return D.ResolveFallbackAction(C); }
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdProgramLifecycle,
	"FestaFiesta.Crowd.Reservation.Lifecycle", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdProgramLifecycle::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	UFF_FestivalStageSubsystem* Stage = World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	Stage->CurrentStage = EFF_FestivalStage::Live;
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector, FBox::BuildAABB(FVector::ZeroVector, FVector(10000.f, 10000.f, 10000.f)), 1, 4, AFF_BaseCrowd::StaticClass());
	const int32 Leader = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestTrue(TEXT("Leader spawned"), Leader != INDEX_NONE)) { return false; }
	using Access = FFF_CrowdProgramTestAccess;
	Access::RegisterLeader(*Crowd, Leader);
	Crowd->DecisionCooldownSeconds = 0.f;
	UFF_ProgramReservationSubsystem* Reservations = World->GetGameInstance()->GetSubsystem<UFF_ProgramReservationSubsystem>();
	const FGuid FacilityId = FGuid::NewGuid();
	const FGuid Id = Reservations->RegisterProgram(FacilityId, 30.f, 60.f, 1);
	TestTrue(TEXT("Reservation assigned"), Reservations->ConfirmReservedParty(Id, Access::Unit(*Crowd, Leader).PartyID, 1));
	FFF_ProgramReservation Reservation;
	Reservations->FindReservation(Id, Reservation);
	Access::Stay(*Crowd, Leader) = 0.f;
	Crowd->OnPartyNeedsDecision.BindLambda([](const FFF_PartyDecisionContext&, FFF_PartyDecisionResult& Out)
	{ Out.FallbackAction = EFF_PartyFallbackAction::Leave; return false; });
	Access::Decide(*Crowd, Leader);
	TestFalse(TEXT("Leave is blocked before start even with empty stay budget"), Access::State(*Crowd, Leader).bIsLeaving);
	TestEqual(TEXT("Free activity is not program waiting"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::None);
	Crowd->OnPartyNeedsDecision.BindLambda([this, Crowd](const FFF_PartyDecisionContext& Context, FFF_PartyDecisionResult& Out)
	{
		TestEqual(TEXT("Decision receives Simulation return buffer"), Context.ProgramReturnBufferMinutes, Crowd->ProgramReturnBufferMinutes);
		Out.FallbackAction = EFF_PartyFallbackAction::Wander;
		Out.bCountAsNoChoice = true;
		return false;
	});
	for (int32 Round = 0; Round < 4; ++Round)
	{
		Access::Decide(*Crowd, Leader);
		TestEqual(TEXT("Reserved wandering never accumulates no_choice"), Access::State(*Crowd, Leader).ConsecutiveNoChoiceCount, 0);
		TestFalse(TEXT("Repeated wandering does not leave"), Access::State(*Crowd, Leader).bIsLeaving);
		TestEqual(TEXT("Repeated wandering does not wait at program"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::None);
	}
	TestTrue(TEXT("Wandering retries advance choice round"), Access::State(*Crowd, Leader).ChoiceRoundIndex >= 4);
	Access::Unit(*Crowd, Leader).Position = FVector::ZeroVector;
	Access::Unit(*Crowd, Leader).Speed = 150.f;
	TestEqual(TEXT("Default return buffer is ten festival minutes"), Crowd->ProgramReturnBufferMinutes, 10.f);
	TestFalse(TEXT("Before buffered deadline keep wandering"), Access::Deadline(*Crowd, Leader, Reservation, 9.f));
	TestTrue(TEXT("Buffered deadline interrupts wandering ten minutes earlier"), Access::Deadline(*Crowd, Leader, Reservation, 10.f));
	TestEqual(TEXT("Returning"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::Returning);
	// 후보가 있는 일반 방문 중이어도 같은 마감 함수가 전환한다.
	Access::State(*Crowd, Leader).ProgramPhase = EFF_PartyProgramPhase::None;
	Access::State(*Crowd, Leader).CurrentDestinationId = TEXT("OrdinaryFacility");
	Access::Unit(*Crowd, Leader).TargetLocation = FVector(-500, 0, 0);
	Crowd->ProgramReturnBufferMinutes = 0.f;
	TestFalse(TEXT("Zero buffer restores original deadline"), Access::Deadline(*Crowd, Leader, Reservation, 19.f));
	Crowd->ProgramReturnBufferMinutes = -5.f;
	TestTrue(TEXT("Negative buffer is clamped to zero"), Access::Deadline(*Crowd, Leader, Reservation, 20.f));
	Crowd->ProgramReturnBufferMinutes = 10.f;
	Stage->ElapsedLiveSeconds = 10.f;
	TestTrue(TEXT("Arrival handled by reservation"), Access::Arrive(*Crowd, Leader));
	TestEqual(TEXT("Early arrival waits"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::Waiting);
	Access::Decide(*Crowd, Leader);
	TestFalse(TEXT("Waiting does not requeue decision"), Access::State(*Crowd, Leader).bNeedsDecision);
	TestEqual(TEXT("Reservation never joins ordinary Queue"), Crowd->GetQueueJoinAttemptCount(), 0);
	Stage->ElapsedLiveSeconds = 30.f;
	Access::Update(*Crowd);
	TestEqual(TEXT("Stage clock starts participation, independent of simulation clock"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::Participating);
	Access::Stay(*Crowd, Leader) = 5.f;
	Access::Unit(*Crowd, Leader).Needs.Hunger = 0.f;
	Access::Needs(*Crowd);
	TestEqual(TEXT("Participation freezes stay consumption"), Access::Stay(*Crowd, Leader), 5.f);
	TestTrue(TEXT("Needs still rise"), Access::Unit(*Crowd, Leader).Needs.Hunger > 0.f);
	Stage->ElapsedLiveSeconds = 60.f;
	Access::Update(*Crowd);
	TestEqual(TEXT("End releases party"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::None);
	TestEqual(TEXT("End grants grace"), Access::Stay(*Crowd, Leader), Crowd->ProgramExitGraceMinutes);
	Access::Stay(*Crowd, Leader) = 2.f;
	Access::Update(*Crowd);
	TestEqual(TEXT("Grace is applied only once"), Access::Stay(*Crowd, Leader), 2.f);
	TestTrue(TEXT("Facility stays reservation-only after end"), Reservations->IsProgramFacility(FacilityId));
	Crowd->OnPartyNeedsDecision.BindLambda([](const FFF_PartyDecisionContext&, FFF_PartyDecisionResult& Out)
	{ Out.FallbackAction = EFF_PartyFallbackAction::Leave; return false; });
	Access::Decide(*Crowd, Leader);
	TestTrue(TEXT("Normal leave works after end"), Access::State(*Crowd, Leader).bIsLeaving);
	Access::State(*Crowd, Leader).bIsLeaving = false;
	Access::Return(*Crowd, Leader, Reservation);
	Access::LeaveAll(*Crowd);
	TestEqual(TEXT("Festival closing clears reservation state"), Access::State(*Crowd, Leader).ProgramPhase, EFF_PartyProgramPhase::None);
	TestTrue(TEXT("Festival closing overrides reservation"), Access::State(*Crowd, Leader).bIsLeaving);
	Reservations->ClearAssignments();
	TestTrue(TEXT("Assignment reset does not reopen scheduled facility"), Reservations->IsProgramFacility(FacilityId));
	Reservations->RemoveReservation(Id);
	TestFalse(TEXT("Removing schedule releases exclusivity"), Reservations->IsProgramFacility(FacilityId));
	Crowd->OnPartyNeedsDecision.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdProgramTravelClock,
	"FestaFiesta.Crowd.Reservation.TravelClock", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdProgramTravelClock::RunTest(const FString& Parameters)
{
	FFF_ProgramReservation Reservation;
	Reservation.FacilityInstanceId = FGuid::NewGuid();
	Reservation.StartMinutes = 30.f;
	Reservation.EndMinutes = 60.f;
	Reservation.Capacity = 1;
	FFF_ReturnTripInput Input;
	Input.DistanceToCandidateMeters = 15.f;
	Input.CandidateToProgramMeters = 15.f;
	Input.MinutesPerTravelSecond = UFF_FestivalStageSubsystem::StageSecondsToFestivalMinutes(1.f);
	const FFF_ReturnTripVerdict Early = FFF_ProgramReservationReturnTrip::Evaluate(Reservation, 0.f, Input);
	TestTrue(TEXT("Early trip is possible"), Early.bWasEvaluated && Early.bCanReturnInTime);
	TestEqual(TEXT("30m at 1.5m/s requires 20 festival minutes"), Early.RequiredMinutes, 20.f);
	TestFalse(TEXT("Same trip becomes unavailable near start"), FFF_ProgramReservationReturnTrip::Evaluate(Reservation, 20.f, Input).bCanReturnInTime);
	Input.ReturnBufferMinutes = 10.f;
	TestTrue(TEXT("Candidate can arrive exactly ten minutes early"), FFF_ProgramReservationReturnTrip::Evaluate(Reservation, 0.f, Input).bCanReturnInTime);
	TestFalse(TEXT("Candidate must respect the same early arrival buffer"), FFF_ProgramReservationReturnTrip::Evaluate(Reservation, 1.f, Input).bCanReturnInTime);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdProgramNoCandidates,
	"FestaFiesta.Crowd.Reservation.NoCandidatesWanders", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdProgramNoCandidates::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	UFF_CrowdDecisionSubsystem* Decision = World->GetSubsystem<UFF_CrowdDecisionSubsystem>();
	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Decision subsystem"), Decision) || !TestNotNull(TEXT("Zone"), Zone)) return false;
	if (!TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(FFProjectZoneTest::ZoneId()))
		|| !TestTrue(TEXT("Zone registration"), Zones->RegisterZone(Zone))) return false;
	FVector2D Center = FVector2D::ZeroVector;
	const TArray<FVector2D> Polygon = Zone->GetBoundaryPolygonLocalCm();
	for (const FVector2D& Point : Polygon) Center += Point;
	Center /= Polygon.Num();
	FFF_PartyDecisionContext Context;
	Context.PartyWorldLocation = Zone->GetActorTransform().TransformPosition(FVector(Center, 0.f));
	if (!TestEqual(TEXT("Leader resolves to registered zone"), Zones->ResolveZoneAtWorldLocation(Context.PartyWorldLocation), Zone->ZoneId)) return false;
	Context.bHasActiveReservation = true;
	Context.ActiveReservation.FacilityInstanceId = FGuid::NewGuid();
	Context.ActiveReservation.StartMinutes = 30.f;
	Context.ActiveReservation.EndMinutes = 60.f;
	Context.ActiveReservation.Capacity = 1;
	Context.NowLiveMinutes = 0.f;
	Context.RemainingStayMinutes = 0.f;
	Context.ConsecutiveNoChoiceCount = 10;
	Context.WalkSpeedMetersPerSecond = 1.5f;
	UFF_ProgramReservationSubsystem* Reservations = World->GetGameInstance()->GetSubsystem<UFF_ProgramReservationSubsystem>();
	Reservations->RegisterProgram(Context.ActiveReservation.FacilityInstanceId, 30.f, 60.f, 1);
	FFFZSReadModel Model = Zones->BuildReadModel();
	FFFZSFacilityReadModel Facility;
	Facility.FacilityInstanceId = Context.ActiveReservation.FacilityInstanceId;
	Facility.FacilityDefinitionId = TEXT("Booth_CraftExperience_Test");
	Facility.ZoneId = Zone->ZoneId;
	Facility.State = EFFZSFacilityState::Active;
	Facility.Category = EFFZSFacilityCategory::Booth;
	Facility.bHasApproachWorldTransform = true;
	Facility.ApproachWorldTransform = FTransform(Context.PartyWorldLocation + FVector(1500, 0, 0));
	Model.Facilities.Add(Facility);
	Decision->DecisionMode = EFF_DecisionMode::Full;
	Decision->bEnableWalkDistances = false;
	using Access = FFF_CrowdProgramDecisionTestAccess;
	for (int32 Round = 0; Round < 3; ++Round)
	{
		Access::SetReadModel(*Decision, Model);
		FFF_PartyDecisionResult Result;
		TestFalse(TEXT("Only reserved facility is not a free activity destination"), Access::Decide(*Decision, Context, Result));
		TestEqual(TEXT("Empty candidates request wandering, not return/leave"), Result.FallbackAction, EFF_PartyFallbackAction::Wander);
		TestFalse(TEXT("No patience penalty during reservation"), Result.bCountAsNoChoice);
		TestTrue(TEXT("No forced program destination"), Result.DestinationId.IsNone());
	}
	Context.bHasActiveReservation = false;
	TestEqual(TEXT("Ordinary party still leaves on expired stay"), Access::Fallback(*Decision, Context), EFF_PartyFallbackAction::Leave);
	Context.RemainingStayMinutes = 100.f;
	TestEqual(TEXT("Ordinary party still leaves on no_choice patience"), Access::Fallback(*Decision, Context), EFF_PartyFallbackAction::Leave);

	// 실제 Decision 연결도 전역 후보를 전달하며, 그 뒤의 예약 전용 필터가 유지되는지 확인한다.
	Reservations->ResetAll();
	Context.RemainingStayMinutes = 100.f;
	Context.ConsecutiveNoChoiceCount = 0;
	Context.Needs.Stimulation = 1.f;
	FFFZSZoneReadModel RemoteZone;
	RemoteZone.ZoneId = TEXT("RemoteStageZone");
	Model.Zones.Add(RemoteZone);
	Model.Facilities[0].ZoneId = RemoteZone.ZoneId;
	Model.Facilities[0].Category = EFFZSFacilityCategory::Performance;
	Model.Facilities[0].FacilityDefinitionId = TEXT("Performance_MainStage_SunsetMainStage");
	Model.Facilities[0].WorldLocation = Context.PartyWorldLocation;
	// 전역 후보는 시작 알림만으로 열리지 않는다. 실제 Live 시간표의 집결 창을 만든다.
	auto* Stage = World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	UDataTable* Schedule = NewObject<UDataTable>(World->GetGameInstance());
	Schedule->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Show;
	Show.Day = 1;
	Show.TriggerSeconds = 180.f;
	Show.bIsMainProgramLockStart = true;
	Schedule->AddRow(TEXT("Main"), Show);
	Show.TriggerSeconds = 210.f;
	Show.bIsMainProgramLockStart = false;
	Schedule->AddRow(TEXT("MainEnd"), Show);
	FObjectPropertyBase* ScheduleProperty = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("Schedule property"), ScheduleProperty)) return false;
	ScheduleProperty->SetObjectPropertyValue_InContainer(Stage, Schedule);
	Stage->CurrentStage = EFF_FestivalStage::Live;
	Stage->CurrentDay = 1;
	Stage->ElapsedLiveSeconds = 177.f;
	Context.NowLiveMinutes = 177.f;
	Access::EnterLive(*Decision);
	Access::StartMain(*Decision);
	TestTrue(TEXT("Global candidate fixture is inside the real gathering window"),
		Decision->IsGlobalMainStageDiscoveryActive());
	Decision->ResetStats();
	int32 ChosenCount = 0;
	for (int32 Round = 0; Round < 16; ++Round)
	{
		Context.ChoiceRoundIndex = Round;
		Access::SetReadModel(*Decision, Model);
		FFF_PartyDecisionResult Result;
		if (Access::Decide(*Decision, Context, Result))
		{
			++ChosenCount;
			TestEqual(TEXT("Actual selector can choose distant main stage"), Result.Destination,
				Model.Facilities[0].ApproachWorldTransform.GetLocation());
		}
	}
	TestEqual(TEXT("Global candidates reach production selector"), Decision->GetStats().NoCandidate, 0);
	TestTrue(TEXT("Main stage selected in deterministic sample"), ChosenCount > 0);
	Reservations->RegisterProgram(Model.Facilities[0].FacilityInstanceId, 30.f, 60.f, 1);
	Access::SetReadModel(*Decision, Model);
	FFF_PartyDecisionResult ReservedResult;
	TestFalse(TEXT("Global discovery cannot bypass reservation-only filter"), Access::Decide(*Decision, Context, ReservedResult));
	TestEqual(TEXT("Reserved global stage removed before scoring"), Decision->GetStats().NoCandidate, 1);
	Stage->ElapsedLiveSeconds = 210.f;
	Access::EndMain(*Decision);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_MainStageBroadcast,
	"FestaFiesta.Crowd.Reservation.MainStageBroadcastReachesBeyond64",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_MainStageBroadcast::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	auto* Stage = World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	Stage->CurrentStage = EFF_FestivalStage::Live;
	Stage->ElapsedLiveSeconds = 30.f;
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	UDataTable* Schedule = NewObject<UDataTable>(World->GetGameInstance());
	Schedule->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Show;
	Show.Day = 1;
	Show.TriggerSeconds = 180.f;
	Show.bIsMainProgramLockStart = true;
	Schedule->AddRow(TEXT("Main"), Show);
	Show.TriggerSeconds = 210.f;
	Show.bIsMainProgramLockStart = false;
	Schedule->AddRow(TEXT("MainEnd"), Show);
	FObjectPropertyBase* ScheduleProperty = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("Schedule property"), ScheduleProperty)) return false;
	ScheduleProperty->SetObjectPropertyValue_InContainer(Stage, Schedule);
	Stage->CurrentDay = 1;
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	auto* Decision = World->GetSubsystem<UFF_CrowdDecisionSubsystem>();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector, FBox::BuildAABB(FVector::ZeroVector, FVector(10000.f, 10000.f, 10000.f)), 1, 78, AFF_BaseCrowd::StaticClass());
	using Access = FFF_CrowdProgramTestAccess;
	using Decisions = FFF_CrowdProgramDecisionTestAccess;
	TArray<int32> Agents;
	for (int32 I = 0; I < 78; ++I)
	{
		const int32 Agent = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
		if (!TestTrue(TEXT("Agent allocated"), Agent != INDEX_NONE)) return false;
		Agents.Add(Agent);
		Access::Unit(*Crowd, Agent).LeaderAgentIndex = Agent;
		Access::Unit(*Crowd, Agent).PartyID = I;
		Access::State(*Crowd, Agent) = FPartyDecisionState();
		Access::State(*Crowd, Agent).CurrentDestinationId = TEXT("ExistingDestination");
		Access::RegisterLeader(*Crowd, Agent);
	}
	Access::ClearSpawnRequests(*Crowd);
	Access::State(*Crowd, Agents[0]).QueuePhase = EFF_PartyQueuePhase::AtSlot;
	Access::State(*Crowd, Agents[1]).bIsLeaving = true;
	Access::State(*Crowd, Agents[2]).ProgramPhase = EFF_PartyProgramPhase::Returning;
	Access::State(*Crowd, Agents[3]).ProgramPhase = EFF_PartyProgramPhase::Waiting;
	Access::State(*Crowd, Agents[4]).ProgramPhase = EFF_PartyProgramPhase::Participating;
	Access::Unit(*Crowd, Agents[5]).bIsActive = false;
	Access::Unit(*Crowd, Agents[6]).LeaderAgentIndex = Agents[8];
	auto* Reservations = World->GetGameInstance()->GetSubsystem<UFF_ProgramReservationSubsystem>();
	const FGuid ProgramId = Reservations->RegisterProgram(FGuid::NewGuid(), 170.f, 210.f, 1);
	Reservations->ConfirmReservedParty(ProgramId, 7, 1);
	int32 Calls = 0;
	Crowd->OnPartyNeedsDecision.BindLambda([&Calls](const FFF_PartyDecisionContext&, FFF_PartyDecisionResult& Result)
	{
		++Calls;
		Result.Destination = FVector(1000, 0, 0);
		Result.DestinationId = TEXT("ChosenByTestSelector");
		return true;
	});
	TestFalse(TEXT("Early Live is not global main stage discovery"), Decision->IsGlobalMainStageDiscoveryActive());
	Decisions::EnterLive(*Decision);
	TestFalse(TEXT("Main stage is closed before the three-minute gathering window"), Decision->IsMainStageOpen());
	Stage->ElapsedLiveSeconds = 120.f;
	Decisions::StartMain(*Decision);
	TestFalse(TEXT("The 19:00 speed-lock signal does not open global discovery"), Decision->IsGlobalMainStageDiscoveryActive());
	TestEqual(TEXT("The speed-lock signal does not interrupt ordinary decisions"), Crowd->GetPendingDecisionCount(), 0);
	Stage->ElapsedLiveSeconds = 176.99f;
	TestFalse(TEXT("Main stage remains closed immediately before 19:57"), Decision->IsMainStageOpen());
	Stage->ElapsedLiveSeconds = 177.f;
	TestTrue(TEXT("Main stage opens at 19:57 for the 20:00 show"), Decision->IsMainStageOpen());
	TestTrue(TEXT("Global discovery follows the shared time boundary"), Decision->IsGlobalMainStageDiscoveryActive());
	Decisions::StartMain(*Decision);
	TestEqual(TEXT("All seventy eligible parties notified, including beyond old cap"), Crowd->GetPendingDecisionCount(), 70);
	for (int32 I = 0; I < 8; ++I)
	{
		TestFalse(TEXT("Protected party/member is not queued"), Access::State(*Crowd, Agents[I]).bNeedsDecision);
		TestEqual(TEXT("Protected destination is preserved"), Access::State(*Crowd, Agents[I]).CurrentDestinationId, FName(TEXT("ExistingDestination")));
	}
	TestEqual(TEXT("Existing queue membership is preserved"), Access::State(*Crowd, Agents[0]).QueuePhase, EFF_PartyQueuePhase::AtSlot);
	Access::Process(*Crowd);
	TestEqual(TEXT("Only frame budget is processed"), Calls, 64);
	TestEqual(TEXT("Remaining parties are retained for next frame"), Crowd->GetPendingDecisionCount(), 6);
	Access::Process(*Crowd);
	TestEqual(TEXT("All eligible parties eventually evaluated"), Calls, 70);
	TestEqual(TEXT("Broadcast drained"), Crowd->GetPendingDecisionCount(), 0);
	Stage->ElapsedLiveSeconds = 210.f;
	Decisions::EndMain(*Decision);
	TestFalse(TEXT("Main end removes global discovery"), Decision->IsGlobalMainStageDiscoveryActive());
	TestFalse(TEXT("Main stage closes after the show"), Decision->IsMainStageOpen());
	Crowd->OnPartyNeedsDecision.Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAudienceLifecycle,
	"FestaFiesta.Crowd.Audience.UsesIndividualSlotsWithoutFIFO",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAudienceLifecycle::RunTest(const FString& Parameters)
{
	// 설치 검증을 결정적으로 만든다. TGuardValue 라 조기 반환에서도 복원된다.
	UFFZSDeveloperSettings* ZoneSettings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundChecks(ZoneSettings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionChecks(ZoneSettings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavigationChecks(ZoneSettings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> ProductionPolicies(ZoneSettings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> InitialPhase(ZoneSettings->InitialPhase, EFFZSChangePhase::InitialSetup);

	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	auto* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Zones"), Zones) || !TestNotNull(TEXT("Zone"), Zone)) return false;
	if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;

	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Performance);
	Spec.FacilityDefinitionId = TEXT("Performance_MainStage_SunsetMainStage");
	Spec.InstallCostKey = NAME_None;
	Spec.Audience.bEnabled = true;
	Spec.Audience.Columns = 2;
	Spec.Audience.Rows = 1;
	// 관람 영역은 건물 바닥과 겹치지 않게 둔다. 메인스테이지의 옛 FIFO는
	// 설치 정규화에서 제거되며 관객석의 자리와 입퇴장 경로는 그대로 유지된다.
	Spec.Audience.FirstSlotLocalCm = FVector(-60, -900, 0);
	Spec.Audience.FocusLocalCm = FVector(0, 100, 200);
	Spec.Audience.CenterAisleWidthCm = 0.f;
	// 입퇴장 좌표는 저작 API 가 계산한다. 손으로 적으면 가장자리 통로 폭 규칙을 놓친다.
	if (!TestTrue(TEXT("Audience portals are authored by the layout API"),
		Spec.Audience.TryCenterAndArrangePortals())) return false;

	// 설치는 권위 명령으로만 한다. InitializeFacility 는 ZoneSystem 내부 API 다.
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
		Spec, Zone->ZoneId, Placement, Zones->GetWorldRevision());
	const FFFZSCommandResult InstallResult = Zones->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("Audience facility installs: %s"),
		*InstallResult.Message.ToString()), InstallResult.bSucceeded)) return false;
	const FGuid FacilityId = Install.FacilityInstanceId;
	TestFalse(TEXT("Main-stage installation retires the legacy FIFO without disabling its audience"),
		Zones->FindFacility(FacilityId)->GetFacilitySpec().UsesServiceQueue());

	// 설치 직후는 Validated 다. 관람 스냅샷은 Active 시설에만 열린다.
	auto* StageBridge = World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>();
	if (!TestNotNull(TEXT("Stage bridge"), StageBridge)) return false;
	StageBridge->OpenEligibleFacilities(false);
	if (!TestEqual(TEXT("Installed stage is Active"), Zones->FindFacility(FacilityId)->GetFacilityState(),
		EFFZSFacilityState::Active)) return false;

	FFFZSAudienceSnapshot Snapshot;
	FText Error;
	if (!TestTrue(*FString::Printf(TEXT("Audience snapshot: %s"), *Error.ToString()),
		UFFZSAudienceLibrary::GetFacilityAudienceSnapshot(World, FacilityId, Snapshot, Error))) return false;
	TestEqual(TEXT("Audience capacity comes from the layout, not the FIFO queue"), Snapshot.Capacity, 2);
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 1, 3, AFF_BaseCrowd::StaticClass());
	const int32 Leader = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	const int32 Member = Crowd->SpawnAgent(*World, FVector::ZeroVector, FRotator::ZeroRotator);
	if (Leader == INDEX_NONE || Member == INDEX_NONE) return false;
	using Access = FFF_CrowdProgramTestAccess;
	const int32 PartyId = Access::Unit(*Crowd, Leader).PartyID;
	Access::Unit(*Crowd, Leader).LeaderAgentIndex = Leader;
	Access::Unit(*Crowd, Member).PartyID = PartyId;
	Access::Unit(*Crowd, Member).LeaderAgentIndex = Leader;
	Access::RegisterLeader(*Crowd, Leader);

	// 화면 시계 20:00 공연은 19:57부터 집결한다. 19:00 배속 잠금 신호는 시작점이 아니다.
	UFF_FestivalStageSubsystem* Stage = World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
	UDataTable* Schedule = NewObject<UDataTable>(World->GetGameInstance());
	Schedule->RowStruct = FFF_LiveTriggerEvent::StaticStruct();
	FFF_LiveTriggerEvent Show;
	Show.Day = 1;
	Show.TriggerSeconds = 180.f;
	Show.EventLabel = TEXT("TestShow");
	Show.bIsMainProgramLockStart = true;
	Schedule->AddRow(TEXT("Main"), Show);
	Show.TriggerSeconds = 210.f;
	Show.bIsMainProgramLockStart = false;
	Schedule->AddRow(TEXT("MainEnd"), Show);
	FObjectPropertyBase* ScheduleProperty = FindFProperty<FObjectPropertyBase>(
		UFF_FestivalStageSubsystem::StaticClass(), TEXT("ProgramScheduleTable"));
	if (!TestNotNull(TEXT("Schedule property"), ScheduleProperty)) return false;
	ScheduleProperty->SetObjectPropertyValue_InContainer(Stage, Schedule);
	Stage->CurrentDay = 1;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	// 집결은 대기열 중단(3축제분 = 3초)보다 일찍 시작해야 먼 손님이 공연 전에 앉는다.
	// 집결 창은 AudienceGatherLeadSeconds 로 따로 정한다.
	Crowd->AudienceGatherLeadSeconds = 10.f;
	Stage->ElapsedLiveSeconds = 150.f;
	Access::StartMainAudience(*Crowd);
	TestFalse(TEXT("Speed lock does not start gathering"), Crowd->IsMainProgramAudienceActive());
	TestFalse(TEXT("An early main stage arrival cannot bypass the gathering window"),
		Access::Audience(*Crowd, Leader, FacilityId, Snapshot));
	TestEqual(TEXT("The early arrival holds no seats"), Crowd->GetOccupiedAudienceSlotCount(FacilityId), 0);
	Stage->ElapsedLiveSeconds = 169.99f;
	Access::TickShowState(*Crowd);
	TestFalse(TEXT("Gathering is still closed just before the gather window"), Crowd->IsMainProgramAudienceActive());
	Stage->ElapsedLiveSeconds = 170.f;
	// 참석 비율 0 이면 창이 열려도 아무도 관람하러 가지 않는다.
	Crowd->AudienceAttendanceRatio = 0.f;
	Access::TickShowState(*Crowd);
	TestTrue(TEXT("Gathering starts the gather lead before the show"), Crowd->IsMainProgramAudienceActive());
	TestFalse(TEXT("The queue break is still closed while gathering"), Stage->IsMainProgramQueueBreakActive());
	TestEqual(TEXT("Nobody attends at ratio 0"), Crowd->GetOccupiedAudienceSlotCount(FacilityId), 0);
	// 비율 1 이면 전원이 간다(예전 동작). 같은 공연을 처음부터 다시 판정한다.
	Crowd->AudienceAttendanceRatio = 1.f;
	Access::EndMainAudience(*Crowd);
	Access::TickShowState(*Crowd);
	TestEqual(TEXT("The boundary assigns both party members"), Crowd->GetOccupiedAudienceSlotCount(FacilityId), 2);

	// 좌석은 배정 시점에 바로 나눠 준다. 입구 좌표 하나를 수백 명이 공유하면 그 점에서 막힌다.
	TestEqual(TEXT("Party heads straight to its seats, not to the shared entry"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::GoingToSlot);
	TestTrue(TEXT("Member owns an individual slot"), Access::Unit(*Crowd, Member).bHasQueueSlot);
	TestFalse(TEXT("Member and leader slots differ"), Access::Unit(*Crowd, Member).QueueSlotLocation.Equals(
		Access::Unit(*Crowd, Leader).TargetLocation));
	TestFalse(TEXT("Leader does not target the shared entry"),
		Access::Unit(*Crowd, Leader).TargetLocation.Equals(Snapshot.EntryWorldCm));
	TestTrue(TEXT("Arrival callbacks keep the party locked while still travelling"), Access::AudienceArrive(*Crowd, Leader));
	TestEqual(TEXT("Calling arrival away from seats cannot start waiting"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::GoingToSlot);
	TestTrue(TEXT("Leader also owns a precise individual seat"),
		Access::Unit(*Crowd, Leader).bHasQueueSlot && Access::Unit(*Crowd, Leader).bUsePreciseSlotPosition);
	Access::Unit(*Crowd, Leader).Position = Access::Unit(*Crowd, Leader).QueueSlotLocation;
	Access::AudienceArrive(*Crowd, Leader);
	TestEqual(TEXT("Leader arrival alone cannot seat the whole party"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::GoingToSlot);
	Access::Unit(*Crowd, Member).Position = Access::Unit(*Crowd, Member).QueueSlotLocation + FVector(10, 0, 0);
	Access::AudienceArrive(*Crowd, Leader);
	TestEqual(TEXT("A member ten centimetres away has not reached the assigned seat"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::GoingToSlot);
	Access::Unit(*Crowd, Member).Position = Access::Unit(*Crowd, Member).QueueSlotLocation;
	TestTrue(TEXT("Arrival is accepted when all party members stand at their seats"), Access::AudienceArrive(*Crowd, Leader));
	TestEqual(TEXT("Audience waits instead of entering FIFO"), Access::State(*Crowd, Leader).AudiencePhase,
		EFF_PartyAudiencePhase::Waiting);

	// 집결 후 좌석에 도착해도 실제 공연 시각까지는 대기한다.
	Stage->ElapsedLiveSeconds = 179.99f;
	Access::TickShowState(*Crowd);
	TestEqual(TEXT("Gathering window keeps the party waiting"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::Waiting);
	TestFalse(TEXT("Show has not started during the lead window"), Crowd->IsMainProgramShowStarted());
	Stage->ElapsedLiveSeconds = 180.f;
	Access::TickShowState(*Crowd);
	TestTrue(TEXT("Show starts on the schedule time, not on the lock entry"),
		Crowd->IsMainProgramShowStarted());
	TestEqual(TEXT("Waiting party becomes an audience at the show time"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::Spectating);
	TestEqual(TEXT("Audience never attempted ordinary queue"), Crowd->GetQueueJoinAttemptCount(), 0);
	TestEqual(TEXT("Both seats are held while the party is inside"),
		Crowd->GetOccupiedAudienceSlotCount(FacilityId), 2);
	TestEqual(TEXT("One party is seated"), Crowd->GetAudiencePartyCount(), 1);

	// 종료 시각을 프레임이 넘어가도 별도 잠금 이벤트 없이 좌석을 반납한다.
	Stage->ElapsedLiveSeconds = 210.1f;
	Access::TickShowState(*Crowd);
	TestEqual(TEXT("Seats are released when the show ends"),
		Crowd->GetOccupiedAudienceSlotCount(FacilityId), 0);
	TestEqual(TEXT("Released party walks to the audience exit"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::Exiting);
	for (const int32 Agent : {Leader, Member})
	{
		TestFalse(TEXT("Show end clears each precise seat flag"), Access::Unit(*Crowd, Agent).bUsePreciseSlotPosition);
		TestTrue(TEXT("Show end clears each seat coordinate"), Access::Unit(*Crowd, Agent).QueueSlotLocation.IsNearlyZero());
	}
	TestTrue(TEXT("Exit arrival returns the party to ordinary decisions"),
		Access::AudienceArrive(*Crowd, Leader));
	TestEqual(TEXT("Audience state is cleared after exiting"),
		Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::None);

	// 줄에 서 있던 파티를 무대로 끌어낼 때 일행도 같이 풀려야 한다. 안 풀면
	// UpdatePartyMemberTarget 이 옛 슬롯 좌표로 계속 보내서 리더만 무대로 간다.
	Access::Unit(*Crowd, Member).bHasQueueSlot = true;
	Access::Unit(*Crowd, Member).QueueSlotLocation = FVector(9000, 9000, 0);
	Access::State(*Crowd, Leader).QueuePhase = EFF_PartyQueuePhase::AtSlot;
	Access::ClearQueue(*Crowd, Leader, false);
	TestFalse(TEXT("Leaving a queue releases the member slots too"),
		Access::Unit(*Crowd, Member).bHasQueueSlot);
	TestTrue(TEXT("Released member slot coordinate is cleared"),
		Access::Unit(*Crowd, Member).QueueSlotLocation.IsNearlyZero());

	// 집결 경계를 건너뛴 프레임에서도 현재 공연 창으로 바로 복구한다.
	Stage->ElapsedLiveSeconds = 180.5f;
	Access::TickShowState(*Crowd);
	TestEqual(TEXT("Overshooting the gathering boundary still seats the whole party"),
		Crowd->GetOccupiedAudienceSlotCount(FacilityId), 2);
	TestTrue(TEXT("Joining after 20:00 recognizes that the show has started"), Crowd->IsMainProgramShowStarted());
	TestTrue(TEXT("Member is handed its own seat, not left at the old queue slot"),
		Access::Unit(*Crowd, Member).bHasQueueSlot
		&& !Access::Unit(*Crowd, Member).QueueSlotLocation.Equals(FVector(9000, 9000, 0)));

	// 걸음 재촉은 매 스텝 곱하면 안 된다. 리더의 Speed 는 다시 세워지지 않아서
	// 누적되면 1초 안에 감속 구간이 사라지고 자리를 영원히 지나친다.
	Crowd->ScheduledHurrySpeedMultiplier = 2.f;
	Access::Unit(*Crowd, Leader).Position = FVector(-50000, 0, 0);
	Access::Unit(*Crowd, Member).Position = FVector(-50000, 0, 0);
	const float CruiseSpeed = Access::Unit(*Crowd, Leader).CruiseSpeed;
	for (int32 StepIndex = 0; StepIndex < 5; ++StepIndex)
	{
		Access::Step(*Crowd, 1.f / 30.f);
	}
	TestEqual(TEXT("Hurry speed is absolute, not compounded per step"),
		Access::Unit(*Crowd, Leader).Speed, CruiseSpeed * 2.f, CruiseSpeed * 0.01f);
	TestTrue(TEXT("Hurry raises MaxSpeed too, otherwise the clamp eats it"),
		Access::Unit(*Crowd, Leader).MaxSpeed
			> Access::Unit(*Crowd, Leader).CruiseMaxSpeed * 1.5f);

	// 못 올 파티가 자리를 쥐고 있으면 그 자리는 공연 내내 비고, 닿을 수 있는 손님이 정원에서 밀린다.
	{
		UFF_FestivalStageSubsystem* ReclaimStage =
			World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>();
		ReclaimStage->CurrentStage = EFF_FestivalStage::Live;
		ReclaimStage->ElapsedLiveSeconds = 200.f;
		TestEqual(TEXT("Party is still walking to its seat"),
			Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::GoingToSlot);

		TestEqual(TEXT("A party that can still make it keeps its seat"),
			Access::Reclaim(*Crowd, 200.f + 100000.f), 0);
		TestTrue(TEXT("Seat is still held"), Crowd->GetOccupiedAudienceSlotCount(FacilityId) > 0);

		TestEqual(TEXT("A party that cannot arrive before the end gives its seat back"),
			Access::Reclaim(*Crowd, 201.f), 1);
		TestEqual(TEXT("Reclaimed seats return to the pool"),
			Crowd->GetOccupiedAudienceSlotCount(FacilityId), 0);
		TestEqual(TEXT("Reclaimed party leaves the audience state machine"),
			Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::None);
		TestFalse(TEXT("The same show cannot immediately reassign a reclaimed party"),
			Access::Audience(*Crowd, Leader, FacilityId, Snapshot));
		Access::EndMainAudience(*Crowd);
		Show.TriggerSeconds = 240.f;
		Show.bIsMainProgramLockStart = true;
		Schedule->AddRow(TEXT("NextMain"), Show);
		Show.TriggerSeconds = 270.f;
		Show.bIsMainProgramLockStart = false;
		Schedule->AddRow(TEXT("NextMainEnd"), Show);
		ReclaimStage->ElapsedLiveSeconds = 237.f;
		TestTrue(TEXT("A reclaimed party may join a later show"),
			Access::Audience(*Crowd, Leader, FacilityId, Snapshot));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdAudiencePreciseSeatMovement,
	"FestaFiesta.Crowd.Audience.MultiRowPreciseSeatMovement",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdAudiencePreciseSeatMovement::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (!TestNotNull(TEXT("Crowd subsystem"), Crowd)) return false;
	using Access = FFF_CrowdProgramTestAccess;

	// 12개 파티가 8열 6행을 채운다. 기본 사회력·신체 충돌을 켠 실제 고정 스텝으로 검증한다.
	// 이동 검증은 공연 시계와 독립적이며, 시계 경계는 AudienceLifecycle에서 따로 확인한다.
	constexpr int32 PartySize = 4;
	constexpr int32 AgentCount = 48;
	FFFZSAudienceLayout Layout;
	Layout.bEnabled = true;
	Layout.Columns = 8;
	Layout.Rows = 6;
	Layout.CenterAisleWidthCm = 0.f;
	Layout.FocusLocalCm = FVector(0, -1000, 200);
	if (!TestTrue(TEXT("Valid multi-row audience layout"), Layout.TryCenterAndArrangePortals())) return false;
	FFFZSAudienceSnapshot Snapshot;
	FText Error;
	if (!TestTrue(TEXT("Audience snapshot builds"), UFFZSAudienceLibrary::BuildSnapshot(
		Layout, FTransform(FRotator(0, 35, 0), FVector(1000, 1000, 0)), Snapshot, Error))) return false;
	const FGuid FacilityId = FGuid::NewGuid();
	Snapshot.FacilityInstanceId = FacilityId;
	Crowd->bEnablePartyGrouping = false;
	Crowd->OnPartyNeedsDecision.Unbind();
	Crowd->InitializeSimulationWithBounds(FVector::ZeroVector,
		FBox::BuildAABB(FVector::ZeroVector, FVector(10000)), 1, AgentCount, AFF_BaseCrowd::StaticClass());
	TestTrue(TEXT("Default crowd avoidance remains enabled"), Crowd->bEnableAvoidance);
	TestTrue(TEXT("Default social-force velocity integration remains enabled"), Crowd->bAvoidanceDrivesVelocity);

	TArray<int32> Agents;
	TArray<int32> Leaders;
	for (int32 Slot = 0; Slot < AgentCount; ++Slot)
	{
		FTransform Seat;
		if (!UFFZSAudienceLibrary::GetAudienceSlotTransform(Snapshot, Slot, Seat)) return false;
		const FVector Start = Seat.GetLocation() + FVector(250, 250, 0);
		const int32 Agent = Crowd->SpawnAgent(*World, Start, FRotator(0, 120, 0));
		if (!TestTrue(TEXT("Audience member spawned"), Agent != INDEX_NONE)) return false;
		Agents.Add(Agent);
		if (Slot % PartySize == 0) Leaders.Add(Agent);
		FUnitData& Unit = Access::Unit(*Crowd, Agent);
		Unit.PartyID = 1000 + Slot / PartySize;
		Unit.LeaderAgentIndex = Leaders.Last();
		Unit.PartySlotIndex = Slot % PartySize;
		Unit.CruiseSpeed = Unit.CruiseMaxSpeed = 150.f;
		// 이전 체류/경로 상태를 물려받아도 새 좌석으로 즉시 이동할 수 있어야 한다.
		Unit.Speed = Unit.MaxSpeed = 0.f;
		Unit.bHasSteerTarget = true;
		Unit.SteerTargetLocation = Start + FVector(5000, 5000, 0);
		Unit.StuckSteps = Crowd->DecisionStuckSteps;
	}
	Access::ClearSpawnRequests(*Crowd);
	for (const int32 Leader : Leaders)
	{
		Access::RegisterLeader(*Crowd, Leader);
		if (!TestTrue(TEXT("A whole party receives its distinct seats"),
			Access::Audience(*Crowd, Leader, FacilityId, Snapshot))) return false;
	}
	TestEqual(TEXT("All forty-eight individual seats are reserved"),
		Crowd->GetOccupiedAudienceSlotCount(FacilityId), AgentCount);
	TArray<FVector> Seats;
	for (const int32 Agent : Agents)
	{
		const FUnitData& Unit = Access::Unit(*Crowd, Agent);
		Seats.Add(Unit.QueueSlotLocation);
		TestTrue(TEXT("Each immediate movement target equals its assigned seat"), Unit.TargetLocation.Equals(Unit.QueueSlotLocation));
		TestTrue(TEXT("Leaders and members both use precise seats"), Unit.bHasQueueSlot && Unit.bUsePreciseSlotPosition);
		TestFalse(TEXT("Old steering cannot divert seat movement"), Unit.bHasSteerTarget);
		TestTrue(TEXT("Assignment restores walking speed after prior waiting"), Unit.Speed > 0.f && Unit.MaxSpeed > 0.f);
	}

	constexpr float Dt = 1.f / 30.f;
	for (int32 Step = 0; Step < 900; ++Step)
	{
		TArray<FVector> Before;
		for (const int32 Agent : Agents) Before.Add(Access::Unit(*Crowd, Agent).Position);
		Access::Step(*Crowd, Dt);
		for (int32 Index = 0; Index < Agents.Num(); ++Index)
		{
			const FUnitData& Unit = Access::Unit(*Crowd, Agents[Index]);
			if (!TestTrue(TEXT("Every movement step respects walking speed; no seat teleport"),
				FVector::Dist2D(Before[Index], Unit.Position) <= Unit.MaxSpeed * Dt + 0.01f)) return false;
		}
	}
	float MaxSeatError = 0.f;
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		const FUnitData& Unit = Access::Unit(*Crowd, Agents[Index]);
		MaxSeatError = FMath::Max(MaxSeatError, static_cast<float>(FVector::Dist2D(Unit.Position, Seats[Index])));
		TestTrue(TEXT("Reserved seat stays stable throughout actual movement"), Unit.QueueSlotLocation.Equals(Seats[Index]));
		TestTrue(TEXT("Seated visitors face their authored stage focus"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Unit.Rotation.Yaw, Unit.LaneYaw)) <= 5.f);
	}
	TestTrue(*FString::Printf(TEXT("All forty-eight visitors reach their own seat within 5cm (max %.2fcm)"), MaxSeatError),
		MaxSeatError <= Crowd->GetAssignedSlotArrivalRadius());
	for (const int32 Leader : Leaders)
	{
		TestEqual(TEXT("Only fully seated parties transition to waiting"),
			Access::State(*Crowd, Leader).AudiencePhase, EFF_PartyAudiencePhase::Waiting);
	}
	for (int32 Step = 0; Step < 120; ++Step) Access::Step(*Crowd, Dt);
	for (int32 Index = 0; Index < Agents.Num(); ++Index)
	{
		TestTrue(TEXT("Default avoidance does not push seated visitors out of their seats"),
			FVector::Dist2D(Access::Unit(*Crowd, Agents[Index]).Position, Seats[Index]) <= Crowd->GetAssignedSlotArrivalRadius());
	}
	for (const int32 Leader : Leaders) Access::ReleaseAudience(*Crowd, Leader);
	for (const int32 Agent : Agents)
	{
		const FUnitData& Unit = Access::Unit(*Crowd, Agent);
		TestFalse(TEXT("Release removes the precise seat movement contract"), Unit.bHasQueueSlot || Unit.bUsePreciseSlotPosition);
		TestTrue(TEXT("Release removes obsolete seat coordinates"), Unit.QueueSlotLocation.IsNearlyZero());
	}
	return true;
}

// 배회 봉쇄. 원판을 Zone보다 크게 둬서 Zone 검사만 일하도록 만들어 놓고 본다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdWanderZoneConfinement,
	"FestaFiesta.Crowd.Wander.ZoneConfinement", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_CrowdWanderZoneConfinement::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	if (!Fixture.BeginPlayInTestWorld()) { Fixture.ForwardErrorMessages(this); return false; }

	UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Zone subsystem"), Zones) || !TestNotNull(TEXT("Zone"), Zone)) { return false; }
	if (!TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(FFProjectZoneTest::ZoneId()))
		|| !TestTrue(TEXT("Zone registration"), Zones->RegisterZone(Zone))) { return false; }

	const TArray<FVector2D> Polygon = Zone->GetBoundaryPolygonLocalCm();
	if (!TestTrue(TEXT("Zone has a polygon"), Polygon.Num() >= 3)) { return false; }

	FVector2D CenterLocal = FVector2D::ZeroVector;
	for (const FVector2D& Point : Polygon) { CenterLocal += Point; }
	CenterLocal /= Polygon.Num();

	float MaxRadiusCm = 0.f;
	for (const FVector2D& Point : Polygon)
	{
		MaxRadiusCm = FMath::Max(MaxRadiusCm, static_cast<float>(FVector2D::Distance(Point, CenterLocal)));
	}

	const FVector ZoneCenter = Zone->GetActorTransform().TransformPosition(FVector(CenterLocal, 0.f));
	if (!TestEqual(TEXT("중심은 Zone 안이다"), Zones->ResolveZoneAtWorldLocation(ZoneCenter), Zone->ZoneId))
	{
		return false;
	}

	UFF_CrowdSimulationSubsystem* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	if (!TestNotNull(TEXT("Crowd subsystem"), Crowd)) { return false; }

	Crowd->bEnablePartyGrouping = false;
	Crowd->MaxWalkableHeightAboveOriginUU = 0.f;
	Crowd->OnPartyNeedsDecision.Unbind();
	Crowd->InitializeSimulationWithBounds(ZoneCenter, FBox::BuildAABB(ZoneCenter, FVector(100000.f, 100000.f, 100000.f)), 2, 4, AFF_BaseCrowd::StaticClass());

	// 원판이 Zone을 넘도록 키운다. 작으면 후보가 전부 안쪽에 떨어져 봉쇄가 없어도 통과한다
	Crowd->WanderRadiusUU = MaxRadiusCm * 3.f;

	// 두 경로를 프로젝트 기본값에 맡기지 않고 여기서 못 박는다. 이 블록은 원판 봉쇄를 보는
	// 것이라, 기본값이 바뀌면 조용히 다른 경로를 재게 된다
	Crowd->bEnableDiscWander = true;
	Crowd->bEnableRoadPatrolWander = false;

	const int32 Leader = Crowd->SpawnAgent(*World, ZoneCenter, FRotator::ZeroRotator);
	if (!TestTrue(TEXT("Leader spawned"), Leader != INDEX_NONE)) { return false; }

	using Access = FFF_CrowdProgramTestAccess;

	int32 OutsideCount = 0;
	for (int32 Round = 0; Round < 64; ++Round)
	{
		Access::State(*Crowd, Leader).ChoiceRoundIndex = Round;
		const FVector Destination = Access::Wander(*Crowd, Leader);
		if (!Access::InBounds(*Crowd, Destination))
		{
			++OutsideCount;
		}
	}
	TestEqual(TEXT("배회 목적지는 한 번도 Zone 밖으로 나가지 않는다"), OutsideCount, 0);
	TestTrue(TEXT("봉쇄가 실제로 후보를 기각했다"), Crowd->GetWanderZoneRejects() > 0);

	// 밖에 선 개체의 복귀. 원판을 좁혀야 그 원판이 Zone을 다시 덮지 않아 복귀 경로가 실제로 불린다
	Crowd->WanderRadiusUU = MaxRadiusCm * 0.5f;
	const FVector Outside = ZoneCenter + FVector(MaxRadiusCm * 4.f, 0.f, 0.f);
	if (!TestFalse(TEXT("시험 지점은 Zone 밖이다"), Access::InBounds(*Crowd, Outside)))
	{
		return false;
	}

	Access::Unit(*Crowd, Leader).Position = Outside;
	Access::State(*Crowd, Leader).ChoiceRoundIndex = 0;
	const FVector Recovered = Access::Wander(*Crowd, Leader);

	TestTrue(TEXT("밖에 있던 개체는 제자리를 받지 않는다"), !Recovered.Equals(Outside, 1.f));
	TestTrue(TEXT("밖에 있던 개체는 축제장 쪽으로 당겨진다"),
		FVector::Dist2D(Recovered, ZoneCenter) < FVector::Dist2D(Outside, ZoneCenter));
	TestTrue(TEXT("복귀 계수기가 올랐다"), Crowd->GetWanderContainmentNudges() > 0);

	// 원판을 끄면 레인이 없는 이 월드에서는 목적지가 안 나온다. 그래도 밖에 선 개체의 복귀만은
	// 살아 있어야 한다 — 그게 죽으면 나간 개체를 되돌릴 수단이 하나도 안 남는다
	Crowd->bEnableDiscWander = false;
	const int32 DiscBefore = Crowd->GetDiscWanderCount();
	const int32 StallsBefore = Crowd->GetWanderStalls();

	Access::Unit(*Crowd, Leader).Position = ZoneCenter;
	const FVector Stalled = Access::Wander(*Crowd, Leader);
	TestTrue(TEXT("원판을 끄면 안쪽 개체는 제자리를 받는다"), Stalled.Equals(ZoneCenter, 1.f));
	TestEqual(TEXT("원판을 끄면 원판 계수기가 안 오른다"), Crowd->GetDiscWanderCount(), DiscBefore);
	TestEqual(TEXT("제자리는 stall로 집계된다"), Crowd->GetWanderStalls(), StallsBefore + 1);

	Access::Unit(*Crowd, Leader).Position = Outside;
	const FVector StillRecovers = Access::Wander(*Crowd, Leader);
	TestTrue(TEXT("원판을 꺼도 밖에 선 개체는 되돌아온다"),
		FVector::Dist2D(StillRecovers, ZoneCenter) < FVector::Dist2D(Outside, ZoneCenter));

	return true;
}
#endif
