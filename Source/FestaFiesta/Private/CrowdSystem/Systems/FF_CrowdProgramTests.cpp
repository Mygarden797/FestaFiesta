// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdDecisionSubsystem.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "NPC/Party/FF_ProgramReservationReturnTrip.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"

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
	Crowd->InitializeSimulation(FVector::ZeroVector, 10000.f, 1, 4, AFF_BaseCrowd::StaticClass());
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
	if (!TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha")))
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
	Access::EnterLive(*Decision);
	Access::StartMain(*Decision);
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
	auto* Crowd = World->GetSubsystem<UFF_CrowdSimulationSubsystem>();
	auto* Decision = World->GetSubsystem<UFF_CrowdDecisionSubsystem>();
	Crowd->InitializeSimulation(FVector::ZeroVector, 10000.f, 1, 78, AFF_BaseCrowd::StaticClass());
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
	const FGuid ProgramId = Reservations->RegisterProgram(FGuid::NewGuid(), 10.f, 60.f, 1);
	Reservations->ConfirmReservedParty(ProgramId, 7, 1);
	int32 Calls = 0;
	Crowd->OnPartyNeedsDecision.BindLambda([&Calls](const FFF_PartyDecisionContext&, FFF_PartyDecisionResult& Result)
	{
		++Calls;
		Result.Destination = FVector(1000, 0, 0);
		Result.DestinationId = TEXT("ChosenByTestSelector");
		return true;
	});
	TestFalse(TEXT("Standalone open gate is not global discovery"), Decision->IsGlobalMainStageDiscoveryActive());
	Decisions::EnterLive(*Decision);
	TestFalse(TEXT("Before main event global discovery is off"), Decision->IsGlobalMainStageDiscoveryActive());
	Decisions::StartMain(*Decision);
	TestTrue(TEXT("Main event enables global discovery"), Decision->IsGlobalMainStageDiscoveryActive());
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
	Decisions::EndMain(*Decision);
	TestFalse(TEXT("Main end removes global discovery"), Decision->IsGlobalMainStageDiscoveryActive());
	Crowd->OnPartyNeedsDecision.Unbind();
	return true;
}

// 배회 봉쇄. 원판을 Zone보다 크게, SimRadius는 느슨하게 둬서 Zone 검사만 일하도록 만들어 놓고 본다
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
	if (!TestTrue(TEXT("Zone preset"), Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha")))
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
	Crowd->InitializeSimulation(ZoneCenter, 100000.f, 2, 4, AFF_BaseCrowd::StaticClass());

	// 원판이 Zone을 넘도록 키운다. 작으면 후보가 전부 안쪽에 떨어져 봉쇄가 없어도 통과한다
	Crowd->WanderRadiusUU = MaxRadiusCm * 3.f;

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

	return true;
}
#endif
