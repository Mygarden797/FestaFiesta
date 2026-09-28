// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSDeveloperSettings.h"
#include "Queue/FF_FacilityQueue.h"
#include "Queue/FF_QueueSubsystem.h"

// 통합 시 조용히 틀리는 지점들을 테스트로 고정한다.

namespace FFF_FacilityQueueTestHelpers
{
	constexpr float TestStepSeconds = 1.f / 30.f;

	FFF_QueueConfig MakeConfig(int32 MaxCapacityParties, float ServiceSecondsPerParty, int32 ConcurrentSlots = 1)
	{
		FFF_QueueConfig Config;
		Config.MaxCapacityParties = MaxCapacityParties;
		Config.ServiceSecondsPerParty = ServiceSecondsPerParty;
		Config.ConcurrentServiceSlots = ConcurrentSlots;
		Config.ExitLocation = FVector(-500.f, 0.f, 0.f);
		Config.bRequireArrivalConfirmation = false;
		return Config;
	}

	FFF_QueueLaneGeometry MakeLane()
	{
		FFF_QueueLaneGeometry Lane;
		Lane.HeadLocation = FVector::ZeroVector;
		Lane.TailDirection = FVector(1.f, 0.f, 0.f);
		Lane.SlotSpacingCm = 115.f;   // Lane.SlotYawDegrees = 180.f;
		return Lane;
	}

	FFF_JoinRequest MakeRequest(int32 PartyId, int32 MemberCount, float MaxWaitSeconds = -1.f)
	{
		FFF_JoinRequest Request;
		Request.PartyId = PartyId;
		Request.FacilityId = 1;
		Request.MemberCount = MemberCount;
		Request.MaxWaitSeconds = MaxWaitSeconds;
		Request.CauseId = FName(*FString::Printf(TEXT("cause_%d"), PartyId));
		for (int32 Member = 0; Member < MemberCount; ++Member)
		{
			Request.MemberAgentIds.Add(PartyId * 100 + Member);
		}
		return Request;
	}

	EFF_QueueJoinResult Join(FFF_FacilityQueue& Queue, const FFF_JoinRequest& Request, float SimTime)
	{
		int32 SlotIndex = INDEX_NONE;
		float EstimatedWait = 0.f;
		return Queue.TryJoin(Request, SimTime, SlotIndex, EstimatedWait);
	}

	/** SimTime을 고정 스텝으로 전진시킨다. 실제 런타임(UFF_QueueSubsystem::Tick)과 같은 방식이다. */
	void StepFor(FFF_FacilityQueue& Queue, float Seconds, float& InOutSimTime)
	{
		const int32 StepCount = FMath::Max(0, FMath::RoundToInt(Seconds / TestStepSeconds));
		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			InOutSimTime += TestStepSeconds;
			Queue.Advance(TestStepSeconds, InOutSimTime);
		}
	}

	int32 CountExits(const TArray<FFF_QueueLog>& Logs, EFF_QueueExit ExitType)
	{
		int32 Count = 0;
		for (const FFF_QueueLog& Log : Logs)
		{
			if (Log.ExitType == ExitType)
			{
				++Count;
			}
		}
		return Count;
	}
}

using namespace FFF_FacilityQueueTestHelpers;

// 1. Party 일괄 가입 · 전원 거절
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_PartyJoinIsAllOrNothing, "FestaFiesta.Queue.FacilityQueue.PartyJoinIsAllOrNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PartyJoinIsAllOrNothing::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(2, 100.f), MakeLane());
	TestEqual(TEXT("4인 팀도 한 자리에 들어간다"), Join(Queue, MakeRequest(10, 4), 0.f), EFF_QueueJoinResult::Accepted);
	TestEqual(TEXT("다른 크기의 팀도 남은 한 자리에 들어간다"), Join(Queue, MakeRequest(30, 2), 0.f), EFF_QueueJoinResult::Accepted);
	TestEqual(TEXT("팀 용량 2칸에 총 6명"), Queue.GetOccupiedMemberCount(), 6);
	TestEqual(TEXT("만석이면 1인 팀도 거절한다"), Join(Queue, MakeRequest(20, 1), 0.f), EFF_QueueJoinResult::RejectedFull);
	TestFalse(TEXT("거절된 팀은 흔적이 없다"), Queue.ContainsParty(20));
	TestFalse(TEXT("만석이면 더 이상 받지 않는다"), Queue.IsAcceptingNew());
	return true;
}

// 2. 중복 가입·잘못된 Party 방어
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_RejectsDuplicateAndInvalidJoin, "FestaFiesta.Queue.FacilityQueue.RejectsDuplicateAndInvalidJoin", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_RejectsDuplicateAndInvalidJoin::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 100.f), MakeLane());

	TestEqual(TEXT("첫 가입은 수락"), Join(Queue, MakeRequest(10, 2), 0.f), EFF_QueueJoinResult::Accepted);
	TestEqual(TEXT("같은 Party의 재가입은 거절"), Join(Queue, MakeRequest(10, 2), 0.f), EFF_QueueJoinResult::RejectedAlreadyQueued);
	TestEqual(TEXT("0인 Party는 거절"), Join(Queue, MakeRequest(11, 0), 0.f), EFF_QueueJoinResult::RejectedInvalidParty);

	Queue.CloseFacility(EFF_QueueCloseReason::Emergency, 0.f);
	TestEqual(TEXT("폐쇄된 시설은 거절"), Join(Queue, MakeRequest(12, 1), 0.f), EFF_QueueJoinResult::RejectedClosed);
	return true;
}

// 3. 일행 인원수와 관계없이 팀당 같은 이용 시간
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_PartyDurationIgnoresMemberCount, "FestaFiesta.Queue.FacilityQueue.PartyDurationIgnoresMemberCount", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PartyDurationIgnoresMemberCount::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 10.f), MakeLane());
	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 4), SimTime);
	Join(Queue, MakeRequest(20, 1), SimTime);
	StepFor(Queue, 5.f, SimTime);
	TestEqual(TEXT("이용 중에도 실제 인원은 전원 유지"), Queue.GetRemainingServiceMemberCount(), 5);
	TestEqual(TEXT("이용 중인 팀 하나"), Queue.GetState().ActiveServicePartyCount, 1);
	StepFor(Queue, 16.f, SimTime);
	TestEqual(TEXT("1인 팀과 4인 팀이 총 20초에 완료"), Queue.GetWaitingPartyCount(), 0);
	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("각 팀에 완료 기록 하나"), CountExits(Logs, EFF_QueueExit::Served), 2);
	for (const FFF_QueueLog& Log : Logs)
	{
		TestEqual(TEXT("팀 크기와 관계없이 10초"), Log.ServiceSeconds, 10.f, 0.01f);
	}
	return true;
}

// 4. FIFO 순서 보존
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_PreservesFifoOrder, "FestaFiesta.Queue.FacilityQueue.PreservesFifoOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PreservesFifoOrder::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 10.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 1), SimTime);
	Join(Queue, MakeRequest(20, 1), SimTime);
	Join(Queue, MakeRequest(30, 1), SimTime);

	TArray<FFF_QueueLog> Logs;
	StepFor(Queue, 35.f, SimTime);
	Queue.ConsumePendingLogs(Logs);

	TestEqual(TEXT("3 Party 전부 처리된다"), Logs.Num(), 3);
	if (Logs.Num() == 3)
	{
		TestEqual(TEXT("먼저 온 순서대로 나간다 (1번째)"), Logs[0].PartyId, 10);
		TestEqual(TEXT("먼저 온 순서대로 나간다 (2번째)"), Logs[1].PartyId, 20);
		TestEqual(TEXT("먼저 온 순서대로 나간다 (3번째)"), Logs[2].PartyId, 30);
	}
	return true;
}

// 5. MaxWaitSeconds 기반 이탈 — 폴링 없이 Queue가 타이머를 든다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_AbandonsAfterMaxWait, "FestaFiesta.Queue.FacilityQueue.AbandonsAfterMaxWait", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_AbandonsAfterMaxWait::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerParty=*/300.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 1, /*MaxWaitSeconds=*/-1.f), SimTime);   // 앞사람: 무제한 대기 + 긴 서비스
	Join(Queue, MakeRequest(20, 2, /*MaxWaitSeconds=*/30.f), SimTime);   // 참을성 없는 Party
	Join(Queue, MakeRequest(30, 1, /*MaxWaitSeconds=*/120.f), SimTime);  // 참을성 있는 Party

	StepFor(Queue, 40.f, SimTime);

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("인내심을 넘긴 Party만 이탈한다"), CountExits(Logs, EFF_QueueExit::Abandoned), 1);
	if (Logs.Num() > 0)
	{
		TestEqual(TEXT("이탈한 건 MaxWait 30초짜리 Party다"), Logs[0].PartyId, 20);
		TestEqual(TEXT("실제 대기시간이 인내심 근처로 기록된다"), Logs[0].ActualWaitSeconds, 30.f, 1.f);
	}
	TestTrue(TEXT("아직 참는 Party는 줄에 남아 있다"), Queue.ContainsParty(30));
	TestEqual(TEXT("이탈로 비운 슬롯이 반영된다"), Queue.GetOccupiedMemberCount(), 2);
	return true;
}

// 6. 서비스가 시작된 Party는 이탈하지 않는다
//    창구에 선 사람이 갑자기 사라지면 관측이 이상해지고 서비스·이탈 타이머가 경쟁한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_ServingPartyNeverAbandons, "FestaFiesta.Queue.FacilityQueue.ServingPartyNeverAbandons", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ServingPartyNeverAbandons::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerParty=*/60.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 1, /*MaxWaitSeconds=*/1.f), SimTime);   // 인내심 1초지만 바로 선두라 서비스가 시작된다

	StepFor(Queue, 30.f, SimTime);
	TestTrue(TEXT("서비스 중인 Party는 인내심을 넘겨도 이탈하지 않는다"), Queue.ContainsParty(10));

	StepFor(Queue, 35.f, SimTime);
	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("정상적으로 서비스 완료로 끝난다"), CountExits(Logs, EFF_QueueExit::Served), 1);
	return true;
}

// 7. 시설 긴급 폐쇄 → 대기 중 전원 Cancelled
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_EmergencyCloseCancelsEveryone, "FestaFiesta.Queue.FacilityQueue.EmergencyCloseCancelsEveryone", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_EmergencyCloseCancelsEveryone::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 300.f), MakeLane());

	float SimTime = 0.f;
	for (int32 PartyId = 1; PartyId <= 5; ++PartyId)
	{
		Join(Queue, MakeRequest(PartyId, 2), SimTime);
	}
	StepFor(Queue, 10.f, SimTime);
	Queue.CloseFacility(EFF_QueueCloseReason::Emergency, SimTime);

	TestEqual(TEXT("대기열이 비워진다"), Queue.GetWaitingPartyCount(), 0);
	TestFalse(TEXT("폐쇄 후에는 신규 가입을 받지 않는다"), Queue.IsAcceptingNew());

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("전원이 Cancelled로 기록된다 — 조용히 사라지면 통계에 구멍이 난다"),
		CountExits(Logs, EFF_QueueExit::Cancelled), 5);

	TArray<FFF_QueueEvent> Events;
	Queue.ConsumePendingEvents(Events);
	int32 ExitEvents = 0;
	for (const FFF_QueueEvent& Event : Events)
	{
		if (Event.Type == EFF_QueueEventType::PartyExited)
		{
			++ExitEvents;
		}
	}
	TestEqual(TEXT("Decision이 재평가할 수 있도록 Party마다 이벤트가 나간다"), ExitEvents, 5);
	return true;
}

// 8. 슬롯 목표 위치 — 연속 배정 + 115cm 간격
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_SlotTargetsAreContiguous, "FestaFiesta.Queue.FacilityQueue.SlotTargetsAreContiguous", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_SlotTargetsAreContiguous::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 300.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 2), SimTime);
	Join(Queue, MakeRequest(20, 3), SimTime);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);

	TestEqual(TEXT("인원 수만큼 배정이 나온다"), Assignments.Num(), 5);
	if (Assignments.Num() == 5)
	{
		TestEqual(TEXT("첫 사람이 슬롯 0을 잡는다"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("배정에 본인 AgentId가 실린다"), Assignments[0].AgentId, 1000);
		TestEqual(TEXT("같은 일행의 둘째 멤버도 배정된다"), Assignments[1].AgentId, 1001);
		TestEqual(TEXT("2인 일행 다음이 두 번째 일행이다"), Assignments[2].PartyId, 20);
		TestEqual(TEXT("두 번째 일행은 팀 슬롯 1이다"), Assignments[2].SlotIndex, 1);
		TestEqual(TEXT("같은 일행은 같은 팀 슬롯이다"), Assignments[1].SlotIndex, 0);
		TestEqual(TEXT("같은 팀 멤버는 대기열을 뒤로 늘리지 않는다"),
			Assignments[1].TargetLocation.X - Assignments[0].TargetLocation.X, 0, 0.01);
	}

	Queue.LeaveQueue(10, SimTime);
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("2인 일행이 빠지면 배정이 둘 줄어든다"), Assignments.Num(), 3);
	if (Assignments.Num() == 3)
	{
		TestEqual(TEXT("뒷 일행이 슬롯 0으로 당겨진다"), Assignments[0].SlotIndex, 0);
	}

	TestTrue(TEXT("줄 구성이 바뀌면 슬롯 갱신 플래그가 선다"), Queue.ConsumeSlotTargetsDirty());
	TestFalse(TEXT("플래그는 한 번 소비하면 내려간다"), Queue.ConsumeSlotTargetsDirty());
	return true;
}

// 9. ExpectedWait — 단위(초)와 단조성
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityQueue_ExplicitSlotTargetsOverrideLegacyLane,
	"FestaFiesta.Queue.FacilityQueue.ExplicitSlotTargetsOverrideLegacyLane",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ExplicitSlotTargetsOverrideLegacyLane::RunTest(const FString& Parameters)
{
	FFF_QueueLaneGeometry Lane = MakeLane();
	Lane.HeadLocation = FVector(9000.f, 9000.f, 0.f);
	Lane.ExplicitSlotTransforms = {
		FTransform(FRotator(0.f, 15.f, 0.f), FVector(10.f, 20.f, 0.f)),
		FTransform(FRotator(0.f, 45.f, 0.f), FVector(110.f, 20.f, 0.f)),
		FTransform(FRotator(0.f, 90.f, 0.f), FVector(110.f, 120.f, 0.f))
	};

	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(3, 300.f), Lane);

	Join(Queue, MakeRequest(10, 1), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	Join(Queue, MakeRequest(30, 1), 0.f);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("Explicit slots produce one target per member"), Assignments.Num(), 3);
	if (Assignments.Num() == 3)
	{
		TestEqual(TEXT("Explicit slot keeps its authored corner location"), Assignments[2].TargetLocation, FVector(110.f, 120.f, 0.f));
		TestEqual(TEXT("Explicit slot keeps its per-slot yaw"), Assignments[2].TargetYaw, 90.f, 0.01f);
		TestFalse(TEXT("Explicit geometry wins over the legacy head"),
			Assignments[0].TargetLocation.Equals(Lane.HeadLocation, UE_KINDA_SMALL_NUMBER));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityQueue_PolylineTargetsFollowEverySegment,
	"FestaFiesta.Queue.FacilityQueue.PolylineTargetsFollowEverySegment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PolylineTargetsFollowEverySegment::RunTest(const FString& Parameters)
{
	FFF_QueueLaneGeometry Lane = MakeLane();
	Lane.SlotSpacingCm = 100.f;
	Lane.PolylinePoints = {
		FVector(0.f, 0.f, 0.f),
		FVector(100.f, 0.f, 0.f),
		FVector(100.f, 200.f, 0.f)
	};

	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(3, 300.f), Lane);

	Join(Queue, MakeRequest(10, 1), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	Join(Queue, MakeRequest(30, 1), 0.f);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("Polyline produces one target per member"), Assignments.Num(), 3);
	if (Assignments.Num() == 3)
	{
		TestEqual(TEXT("Slot zero begins at the polyline head"), Assignments[0].TargetLocation, FVector::ZeroVector);
		TestEqual(TEXT("Slot one reaches the bend"), Assignments[1].TargetLocation, FVector(100.f, 0.f, 0.f));
		TestTrue(TEXT("The incoming segment owns facing at the exact bend"),
			FMath::Abs(FMath::FindDeltaAngleDegrees(Assignments[1].TargetYaw, 180.f)) <= 0.01f);
		TestEqual(TEXT("Slot two follows the second segment"), Assignments[2].TargetLocation, FVector(100.f, 100.f, 0.f));
		TestEqual(TEXT("Second segment supplies its own facing"), Assignments[2].TargetYaw, -90.f, 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_ExpectedWaitIsSecondsAndMonotonic, "FestaFiesta.Queue.FacilityQueue.ExpectedWaitIsSecondsAndMonotonic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ExpectedWaitIsSecondsAndMonotonic::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 20.f), MakeLane());
	TestEqual(TEXT("빈 줄은 바로 이용 시작"), Queue.GetExpectedWaitSeconds(1), 0.f, 0.01f);
	Join(Queue, MakeRequest(10, 3), 0.f);
	TestEqual(TEXT("앞에 3인 팀 하나면 20초"), Queue.GetExpectedWaitSeconds(1), 20.f, 0.01f);
	TestEqual(TEXT("들어오는 팀이 4인이어도 같은 대기"), Queue.GetExpectedWaitSeconds(4), 20.f, 0.01f);
	TestEqual(TEXT("분 변환은 60으로 나눈다"), Queue.GetExpectedWaitMinutes(4), 20.f / 60.f, 0.001f);
	Queue.Advance(5.f, 5.f);
	TestEqual(TEXT("현재 팀이 이용한 5초를 빼면 15초"), Queue.GetExpectedWaitSeconds(4), 15.f, 0.01f);
	return true;
}

// 10. 예상 대기 vs 실제 대기 — Utility AI 튜닝의 직접 지표
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_LogsEstimatedAndActualWait, "FestaFiesta.Queue.FacilityQueue.LogsEstimatedAndActualWait", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_LogsEstimatedAndActualWait::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerParty=*/10.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 2), SimTime);   // 앞 팀: 인원수와 관계없이 10초
	Join(Queue, MakeRequest(20, 1, -1.f), SimTime);

	StepFor(Queue, 40.f, SimTime);

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("두 Party 모두 로그를 남긴다"), Logs.Num(), 2);
	if (Logs.Num() == 2)
	{
		const FFF_QueueLog& Second = Logs[1];
		TestEqual(TEXT("가입 시점 예측이 기록된다"), Second.EstimatedWaitAtJoinSeconds, 10.f, 0.01f);
		TestEqual(TEXT("실제 대기는 서비스 시작 시점 기준으로 기록된다"), Second.ActualWaitSeconds, 10.f, 0.01f);
		TestFalse(TEXT("CauseId가 DecisionLog 조인 키로 보존된다"), Second.CauseId.IsNone());
		TestEqual(TEXT("CauseId가 가입 요청 값 그대로다"), Second.CauseId, FName(TEXT("cause_20")));
	}
	return true;
}

// 11. 결정론 — 같은 입력 시퀀스는 같은 로그를 낸다 (AT-001 Seed 재현성)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_SameInputGivesSameLog, "FestaFiesta.Queue.FacilityQueue.SameInputGivesSameLog", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_SameInputGivesSameLog::RunTest(const FString& Parameters)
{
	auto RunScenario = [](TArray<FFF_QueueLog>& OutLogs)
	{
		FFF_FacilityQueue Queue;
		Queue.Configure(1, MakeConfig(10, 8.f), MakeLane());

		float SimTime = 0.f;
		Join(Queue, MakeRequest(10, 2, 100.f), SimTime);
		Join(Queue, MakeRequest(20, 3, 12.f), SimTime);
		StepFor(Queue, 15.f, SimTime);
		Join(Queue, MakeRequest(30, 1, 100.f), SimTime);
		StepFor(Queue, 45.f, SimTime);
		Queue.ConsumePendingLogs(OutLogs);
	};

	TArray<FFF_QueueLog> First;
	TArray<FFF_QueueLog> Second;
	RunScenario(First);
	RunScenario(Second);

	TestEqual(TEXT("같은 시퀀스는 같은 개수의 로그를 낸다"), First.Num(), Second.Num());
	if (First.Num() == Second.Num())
	{
		bool bIdentical = true;
		for (int32 Index = 0; Index < First.Num(); ++Index)
		{
			bIdentical &= (First[Index].PartyId == Second[Index].PartyId);
			bIdentical &= (First[Index].ExitType == Second[Index].ExitType);
			bIdentical &= FMath::IsNearlyEqual(First[Index].ActualWaitSeconds, Second[Index].ActualWaitSeconds, 0.0001f);
			bIdentical &= FMath::IsNearlyEqual(First[Index].ExitSimTime, Second[Index].ExitSimTime, 0.0001f);
		}
		TestTrue(TEXT("퇴장 순서·유형·시각이 완전히 동일하다"), bIdentical);
	}
	return true;
}

// 12. 도착 확인 게이트 — Crowd 통지 전에는 서비스가 시작되지 않는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_WaitsForArrivalWhenRequired, "FestaFiesta.Queue.FacilityQueue.WaitsForArrivalWhenRequired", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_WaitsForArrivalWhenRequired::RunTest(const FString& Parameters)
{
	FFF_QueueConfig Config = MakeConfig(20, 10.f);
	Config.bRequireArrivalConfirmation = true;

	FFF_FacilityQueue Queue;
	Queue.Configure(1, Config, MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 1, -1.f), SimTime);

	StepFor(Queue, 30.f, SimTime);
	TestTrue(TEXT("도착 통지가 없으면 서비스 타이머가 돌지 않는다"), Queue.ContainsParty(10));

	Queue.NotifyPartyArrived(10);
	StepFor(Queue, 15.f, SimTime);
	TestFalse(TEXT("도착 통지 후에는 정상적으로 처리된다"), Queue.ContainsParty(10));

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	if (Logs.Num() == 1)
	{
		// 걸어오는 데 걸린 30초도 대기시간이다 — 이게 빠지면 실제 대기가 과소 기록된다.
		TestTrue(TEXT("걸어오는 동안의 시간도 실제 대기에 포함된다"), Logs[0].ActualWaitSeconds >= 29.f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityQueue_RejectsUnsafePartyShapesAndClampsConfig,
	"FestaFiesta.Queue.FacilityQueue.RejectsUnsafePartyShapesAndClampsConfig",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityQueue_RejectsUnsafePartyShapesAndClampsConfig::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FFF_QueueConfig UnsafeConfig = MakeConfig(
		MAX_int32,
		100.f,
		MAX_int32);
	FFF_FacilityQueue Queue;
	Queue.Configure(1, UnsafeConfig, MakeLane());
	TestEqual(TEXT("Party capacity clamps to the canonical facility safety limit"),
		Queue.GetConfig().MaxCapacityParties,
		UFFZSDeveloperSettings::MaxQueueWaitingAgents);
	TestEqual(TEXT("Concurrent service clamps to the canonical facility safety limit"),
		Queue.GetConfig().ConcurrentServiceSlots,
		UFFZSDeveloperSettings::MaxFacilityServiceCapacity);

	FFF_JoinRequest TooManyMembers = MakeRequest(10, 1);
	TooManyMembers.MemberCount = MAX_int32;
	TooManyMembers.MemberAgentIds.Reset();
	TestEqual(TEXT("An untrusted MAX_int32 MemberCount is rejected before arithmetic"),
		Join(Queue, TooManyMembers, 0.f),
		EFF_QueueJoinResult::RejectedInvalidParty);

	FFF_JoinRequest MissingMemberId = MakeRequest(20, 2);
	MissingMemberId.MemberAgentIds.SetNum(1);
	TestEqual(TEXT("A non-empty agent-id list must exactly match MemberCount"),
		Join(Queue, MissingMemberId, 0.f),
		EFF_QueueJoinResult::RejectedInvalidParty);

	FFF_JoinRequest OversizedMemberIds = MakeRequest(30, 1);
	OversizedMemberIds.MemberAgentIds.SetNum(
		UFFZSDeveloperSettings::MaxQueueWaitingAgents + 1);
	TestEqual(TEXT("An oversized member-id array is rejected without being copied"),
		Join(Queue, OversizedMemberIds, 0.f),
		EFF_QueueJoinResult::RejectedInvalidParty);
	TestEqual(TEXT("Rejected unsafe party shapes leave the queue empty"),
		Queue.GetOccupiedMemberCount(), 0);

	FFF_JoinRequest AtLimit = MakeRequest(40, 1);
	AtLimit.MemberCount = UFFZSDeveloperSettings::MaxQueueWaitingAgents;
	AtLimit.MemberAgentIds.Reset();
	TestEqual(TEXT("The documented maximum party size remains inclusive"),
		Join(Queue, AtLimit, 0.f),
		EFF_QueueJoinResult::Accepted);
	TestEqual(TEXT("An accepted maximum-size party occupies exactly the bounded capacity"),
		Queue.GetOccupiedMemberCount(),
		UFFZSDeveloperSettings::MaxQueueWaitingAgents);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_QueueSubsystem_RegisterFacilityIsNotBlueprintCallable,
	"FestaFiesta.Queue.Subsystem.RegisterFacilityIsNotBlueprintCallable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_QueueSubsystem_RegisterFacilityIsNotBlueprintCallable::RunTest(
	const FString& Parameters)
{
	(void)Parameters;
	const UClass* QueueSubsystemClass = UFF_QueueSubsystem::StaticClass();
	TestNotNull(TEXT("The queue subsystem class is reflected"), QueueSubsystemClass);
	TestTrue(TEXT("RegisterFacility cannot become a second Blueprint-authored facility data source"),
		QueueSubsystemClass->FindFunctionByName(TEXT("RegisterFacility")) == nullptr);
	TestTrue(TEXT("UnregisterFacility remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("UnregisterFacility")) == nullptr);
	TestTrue(TEXT("TryJoin remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("TryJoin")) == nullptr);
	TestTrue(TEXT("LeaveQueue remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("LeaveQueue")) == nullptr);
	TestTrue(TEXT("NotifyPartyArrived remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("NotifyPartyArrived")) == nullptr);
	TestTrue(TEXT("CloseFacility remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("CloseFacility")) == nullptr);
	TestTrue(TEXT("ReopenFacility remains bridge-only"),
		QueueSubsystemClass->FindFunctionByName(TEXT("ReopenFacility")) == nullptr);
	TestNotNull(TEXT("A read-only Blueprint query remains available"),
		QueueSubsystemClass->FindFunctionByName(TEXT("GetQueueState")));
	return true;
}

// ── Crowd 결선을 위한 발행 검증 ─────────────────────────────────────────────
//
// 슬롯 목표는 Queue 가 발행하고 Crowd 가 구독해서 NPC 를 움직인다. 지금 구독자가 없어서
// 통합 때 "줄이 안 선다" 가 나오면 발행 문제인지 수신 문제인지 가려야 하는데,
// 양쪽 다 미검증이면 그 판별에만 시간이 든다.
//
// 아래는 발행 쪽 계약을 못박는다. 이게 통과하면 문제는 수신 쪽이다.

// 17. 줄이 바뀔 때마다 슬롯 갱신 신호가 정확히 서고, 소비하면 내려간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_SlotDirtyFlagTracksEveryChange, "FestaFiesta.Queue.FacilityQueue.SlotDirtyFlagTracksEveryChange", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_SlotDirtyFlagTracksEveryChange::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 10.f), MakeLane());
	Queue.ConsumeSlotTargetsDirty();   // Configure 가 세운 초기 신호를 비운다

	float SimTime = 0.f;

	Join(Queue, MakeRequest(10, 2), SimTime);
	TestTrue(TEXT("가입하면 슬롯 갱신이 필요하다"), Queue.ConsumeSlotTargetsDirty());
	TestFalse(TEXT("소비하면 내려간다"), Queue.ConsumeSlotTargetsDirty());

	Join(Queue, MakeRequest(20, 1), SimTime);
	TestTrue(TEXT("두 번째 가입도 마찬가지"), Queue.ConsumeSlotTargetsDirty());
	Queue.ConsumeSlotTargetsDirty();

	// 서비스가 진행되는 동안에는 줄 구성이 안 바뀌므로 신호가 서면 안 된다.
	// 매 틱 신호가 서면 Crowd 가 매 틱 목표를 다시 받아 경로를 재계산한다.
	StepFor(Queue, 5.f, SimTime);
	TestFalse(TEXT("서비스 중에는 불필요하게 서지 않는다"), Queue.ConsumeSlotTargetsDirty());

	// 앞 Party 가 빠지면 뒷사람이 당겨지므로 반드시 서야 한다.
	StepFor(Queue, 20.f, SimTime);
	TestTrue(TEXT("앞 Party 가 빠지면 신호가 선다"), Queue.ConsumeSlotTargetsDirty());
	return true;
}

// 18. 발행되는 슬롯 배열이 Crowd 가 그대로 쓸 수 있는 형태인지.
//     AgentId 가 비거나 슬롯이 건너뛰면 Crowd 는 어느 NPC 를 어디로 보낼지 알 수 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_SlotAssignmentsAreConsumableByCrowd, "FestaFiesta.Queue.FacilityQueue.SlotAssignmentsAreConsumableByCrowd", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_SlotAssignmentsAreConsumableByCrowd::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 300.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 2), SimTime);
	Join(Queue, MakeRequest(20, 3), SimTime);
	Join(Queue, MakeRequest(30, 1), SimTime);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);

	TestEqual(TEXT("인원 수만큼 배정이 나온다"), Assignments.Num(), 6);

	bool bAllHaveAgent = true;
	bool bAllFinite = true;
	bool bSlotsAscend = true;
	int32 PreviousSlot = -1;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		bAllHaveAgent &= (Assignment.AgentId != INDEX_NONE);
		bAllFinite &= !Assignment.TargetLocation.ContainsNaN()
			&& FMath::IsFinite(Assignment.TargetYaw);
		bSlotsAscend &= (Assignment.SlotIndex >= PreviousSlot);
		PreviousSlot = Assignment.SlotIndex;
	}
	TestTrue(TEXT("모든 배정에 AgentId 가 있다"), bAllHaveAgent);
	TestTrue(TEXT("모든 좌표가 유한하다"), bAllFinite);
	TestTrue(TEXT("팀 슬롯 인덱스는 감소하지 않는다"), bSlotsAscend);

	TMap<int32, int32> CountByParty;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		CountByParty.FindOrAdd(Assignment.PartyId)++;
	}
	TestEqual(TEXT("2인 일행은 배정 둘"), CountByParty.FindRef(10), 2);
	TestEqual(TEXT("3인 일행은 배정 셋"), CountByParty.FindRef(20), 3);
	TestEqual(TEXT("1인 일행은 배정 하나"), CountByParty.FindRef(30), 1);

	if (Assignments.Num() == 6)
	{
		TestEqual(TEXT("첫 일행은 슬롯 0 부터"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("2인 일행 뒤는 팀 슬롯 1"), Assignments[2].SlotIndex, 1);
		TestEqual(TEXT("3인 일행 뒤는 팀 슬롯 2"), Assignments[5].SlotIndex, 2);
	}
	return true;
}

// 19. Party 하나가 겪는 이벤트 순서.
//     Decision 이 상태를 전이시키는 근거라, 순서가 뒤집히거나 빠지면 상태 머신이 어긋난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_EventSequenceIsOrderedAndComplete, "FestaFiesta.Queue.FacilityQueue.EventSequenceIsOrderedAndComplete", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_EventSequenceIsOrderedAndComplete::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, 10.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 1, -1.f), SimTime);
	StepFor(Queue, 15.f, SimTime);

	TArray<FFF_QueueEvent> Events;
	Queue.ConsumePendingEvents(Events);

	// 가입 → 서비스 시작 → 퇴장. 이 셋이 이 순서로 정확히 한 번씩 나와야 한다.
	TArray<EFF_QueueEventType> Sequence;
	for (const FFF_QueueEvent& Event : Events)
	{
		if (Event.PartyId == 10)
		{
			Sequence.Add(Event.Type);
		}
	}

	TestEqual(TEXT("이벤트가 정확히 3건"), Sequence.Num(), 3);
	if (Sequence.Num() == 3)
	{
		TestEqual(TEXT("1. 가입"), Sequence[0], EFF_QueueEventType::PartyJoined);
		TestEqual(TEXT("2. 서비스 시작"), Sequence[1], EFF_QueueEventType::ServiceStarted);
		TestEqual(TEXT("3. 퇴장"), Sequence[2], EFF_QueueEventType::PartyExited);
	}

	// 소비하면 비어야 한다. 안 그러면 구독자가 같은 이벤트를 두 번 받는다.
	TArray<FFF_QueueEvent> Again;
	Queue.ConsumePendingEvents(Again);
	TestEqual(TEXT("소비한 이벤트는 다시 나오지 않는다"), Again.Num(), 0);
	return true;
}

// 20. 거절은 이벤트만 나가고 줄에는 아무 흔적도 남기지 않아야 한다.
//     만석 시설을 후보에 남기기로 하면서 도착 후 거절이 늘어난다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_RejectionLeavesNoResidue, "FestaFiesta.Queue.FacilityQueue.RejectionLeavesNoResidue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_RejectionLeavesNoResidue::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(1, 300.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 4), SimTime);
	Queue.ConsumeSlotTargetsDirty();

	TArray<FFF_QueueEvent> Ignored;
	Queue.ConsumePendingEvents(Ignored);

	// 만석 상태에서 거절.
	TestEqual(TEXT("만석이면 거절된다"),
		Join(Queue, MakeRequest(20, 2), SimTime), EFF_QueueJoinResult::RejectedFull);

	TestEqual(TEXT("줄 길이가 그대로다"), Queue.GetWaitingPartyCount(), 1);
	TestEqual(TEXT("점유 인원이 그대로다"), Queue.GetOccupiedMemberCount(), 4);
	TestFalse(TEXT("거절은 슬롯 갱신을 유발하지 않는다"), Queue.ConsumeSlotTargetsDirty());

	TArray<FFF_QueueEvent> Events;
	Queue.ConsumePendingEvents(Events);
	TestEqual(TEXT("거절 이벤트가 1건 나간다"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("유형이 거절이다"), Events[0].Type, EFF_QueueEventType::PartyRejected);
		TestEqual(TEXT("사유가 만석이다"), Events[0].RejectReason, EFF_QueueJoinResult::RejectedFull);
	}

	// 배정 배열에도 거절된 Party 가 없어야 한다.
	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	bool bRejectedPresent = false;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		bRejectedPresent |= (Assignment.PartyId == 20);
	}
	TestFalse(TEXT("거절된 Party 는 슬롯 배정에 없다"), bRejectedPresent);
	return true;
}

// 21. Crowd 결선의 전제 — 한 일행의 배정이 배열 안에서 붙어 있고,
//     PartyMemberIndex 가 일행마다 0부터 다시 세어야 한다.
//     결선 글루는 이 둘로 일행별 열(column)을 조립한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_AssignmentsAreGroupedByParty, "FestaFiesta.Queue.FacilityQueue.AssignmentsAreGroupedByParty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_AssignmentsAreGroupedByParty::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(30, 300.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 3), SimTime);
	Join(Queue, MakeRequest(20, 1), SimTime);
	Join(Queue, MakeRequest(30, 4), SimTime);
	Join(Queue, MakeRequest(40, 2), SimTime);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("인원 수만큼 배정된다"), Assignments.Num(), 10);

	TMap<int32, int32> CountByParty;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		CountByParty.FindOrAdd(Assignment.PartyId)++;
	}
	TestEqual(TEXT("Party 10 은 셋"), CountByParty.FindRef(10), 3);
	TestEqual(TEXT("Party 20 은 하나"), CountByParty.FindRef(20), 1);
	TestEqual(TEXT("Party 30 은 넷"), CountByParty.FindRef(30), 4);
	TestEqual(TEXT("Party 40 은 둘"), CountByParty.FindRef(40), 2);

	bool bPartiesAreContiguous = true;
	bool bMemberIndexRestarts = true;
	TSet<int32> ClosedParties;
	int32 CurrentParty = INDEX_NONE;
	int32 ExpectedMemberIndex = 0;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		if (Assignment.PartyId != CurrentParty)
		{
			bPartiesAreContiguous &= !ClosedParties.Contains(Assignment.PartyId);
			ClosedParties.Add(CurrentParty);
			CurrentParty = Assignment.PartyId;
			ExpectedMemberIndex = 0;
		}
		bMemberIndexRestarts &= (Assignment.PartyMemberIndex == ExpectedMemberIndex);
		++ExpectedMemberIndex;
	}
	TestTrue(TEXT("한 일행의 배정이 배열 안에서 붙어 있다"), bPartiesAreContiguous);
	TestTrue(TEXT("PartyMemberIndex 가 일행마다 0부터 다시 센다"), bMemberIndexRestarts);

	if (Assignments.Num() == 10)
	{
		TestEqual(TEXT("3인 일행은 슬롯 0 부터"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("1인 일행은 팀 슬롯 1"), Assignments[3].SlotIndex, 1);
		TestEqual(TEXT("4인 일행은 팀 슬롯 2"), Assignments[4].SlotIndex, 2);
		TestEqual(TEXT("2인 일행은 팀 슬롯 3"), Assignments[8].SlotIndex, 3);
	}

	Queue.LeaveQueue(20, SimTime);
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("1인 일행이 빠지면 배정이 하나 줄어든다"), Assignments.Num(), 9);
	if (Assignments.Num() == 9)
	{
		TestEqual(TEXT("맨 앞은 그대로 슬롯 0"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("빈 팀 자리만큼 당겨진다"), Assignments[3].SlotIndex, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_ConcurrentTeamsKeepFullDuration,
	"FestaFiesta.Queue.FacilityQueue.ConcurrentTeamsKeepFullDuration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ConcurrentTeamsKeepFullDuration::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(3, 10.f, 2), MakeLane());
	Join(Queue, MakeRequest(10, 1), 0.f);
	Join(Queue, MakeRequest(20, 4), 0.f);
	TestEqual(TEXT("두 자리가 사용 중이면 다음 팀은 10초 대기"), Queue.GetExpectedWaitSeconds(4), 10.f, 0.001f);
	Join(Queue, MakeRequest(30, 2), 0.f);
	Queue.Advance(9.f, 9.f);
	TestEqual(TEXT("9초에는 두 팀 모두 이용 중"), Queue.GetState().ActiveServicePartyCount, 2);
	TestEqual(TEXT("시간을 동시 팀 수로 나누어 조기 완료하지 않는다"), Queue.GetWaitingPartyCount(), 3);
	TestEqual(TEXT("남은 1초 뒤 빈 두 번째 자리에서 다음 외부 팀이 시작 가능"), Queue.GetExpectedWaitSeconds(1), 1.f, 0.001f);
	Queue.Advance(1.f, 10.f);
	TestEqual(TEXT("서로 다른 인원의 두 팀이 10초에 같이 완료"), Queue.GetWaitingPartyCount(), 1);
	Queue.Advance(10.f, 20.f);
	TestEqual(TEXT("세 번째 팀도 10초를 온전히 이용"), Queue.GetWaitingPartyCount(), 0);
	TestEqual(TEXT("완료 팀 수 집계"), Queue.GetState().CompletedPartyCount, 3);
	TestEqual(TEXT("팀 평균 실제 대기: 0초, 0초, 10초"), Queue.GetState().AverageCompletedWaitSeconds, 10.f / 3.f, 0.001f);
	TArray<FFF_QueueEvent> Events;
	Queue.ConsumePendingEvents(Events);
	TMap<int32, int32> ServedCounts;
	int32 CompletedEvents = 0;
	for (const FFF_QueueEvent& Event : Events)
	{
		if (Event.Type == EFF_QueueEventType::PartyExited && Event.ExitType == EFF_QueueExit::Served)
		{
			++CompletedEvents;
			ServedCounts.Add(Event.PartyId, Event.ServedMemberCount);
		}
	}
	TestEqual(TEXT("각 팀에 정확히 한 번 완료 이벤트"), CompletedEvents, 3);
	TestEqual(TEXT("1인 팀 정산 인원"), ServedCounts.FindRef(10), 1);
	TestEqual(TEXT("4인 팀 정산 인원"), ServedCounts.FindRef(20), 4);
	TestEqual(TEXT("2인 팀 정산 인원"), ServedCounts.FindRef(30), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_OneAuthoredAnchorPerTeam,
	"FestaFiesta.Queue.FacilityQueue.OneAuthoredAnchorPerTeam",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_OneAuthoredAnchorPerTeam::RunTest(const FString& Parameters)
{
	FFF_QueueLaneGeometry Lane = MakeLane();
	Lane.ExplicitSlotTransforms = {
		FTransform(FRotator(0.f, 90.f, 0.f), FVector(10.f, 20.f, 0.f)),
		FTransform(FRotator(0.f, 180.f, 0.f), FVector(100.f, 200.f, 0.f))
	};
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(2, 10.f), Lane);
	Join(Queue, MakeRequest(10, 4), 0.f);
	Join(Queue, MakeRequest(20, 2), 0.f);
	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("Crowd에 실제 멤버 목표 여섯 개 전달"), Assignments.Num(), 6);
	if (Assignments.Num() != 6)
	{
		return false;
	}
	TestEqual(TEXT("첫 리더는 저작된 첫 팀 자리"), Assignments[0].TargetLocation, Lane.ExplicitSlotTransforms[0].GetLocation());
	TestEqual(TEXT("둘째 리더는 저작된 둘째 팀 자리"), Assignments[4].TargetLocation, Lane.ExplicitSlotTransforms[1].GetLocation());
	for (int32 Index = 0; Index < Assignments.Num(); ++Index)
	{
		TestEqual(TEXT("멤버가 추가 팀 자리를 소비하지 않는다"), Assignments[Index].SlotIndex, Index < 4 ? 0 : 1);
	}
	TestFalse(TEXT("리더와 멤버가 동일 지점에 겹치지 않는다"), Assignments[0].TargetLocation.Equals(Assignments[1].TargetLocation));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_ShiftedTeamNeedsFreshArrival,
	"FestaFiesta.Queue.FacilityQueue.ShiftedTeamNeedsFreshArrival",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ShiftedTeamNeedsFreshArrival::RunTest(const FString& Parameters)
{
	FFF_QueueConfig Config = MakeConfig(3, 10.f, 2);
	Config.bRequireArrivalConfirmation = true;
	FFF_FacilityQueue Queue;
	Queue.Configure(1, Config, MakeLane());
	Join(Queue, MakeRequest(10, 4), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	Join(Queue, MakeRequest(30, 2), 0.f);
	Queue.NotifyPartyArrived(20);
	Queue.NotifyPartyArrived(30); // 대기 자리 도착은 이용 자리 도착이 아니다.
	Queue.Advance(1.f, 1.f);
	TestEqual(TEXT("도착하지 않은 앞팀을 추월하지 않는다"), Queue.GetState().ActiveServicePartyCount, 0);
	Queue.NotifyPartyArrived(10);
	Queue.Advance(10.f, 11.f);
	TestEqual(TEXT("도착한 앞 두 팀이 함께 완료"), Queue.GetWaitingPartyCount(), 1);
	Queue.Advance(10.f, 21.f);
	TestEqual(TEXT("자리 이동 뒤에는 이전 도착 통지로 이용하지 않는다"), Queue.GetState().ActiveServicePartyCount, 0);
	Queue.NotifyPartyArrived(30);
	Queue.Advance(10.f, 31.f);
	TestEqual(TEXT("새 이용 자리 도착 후 온전한 시간으로 완료"), Queue.GetWaitingPartyCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_TeamCapacityDoesNotTruncateMembers,
	"FestaFiesta.Queue.FacilityQueue.TeamCapacityDoesNotTruncateMembers",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_TeamCapacityDoesNotTruncateMembers::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(2, 10.f, 4), MakeLane());
	Join(Queue, MakeRequest(10, UFFZSDeveloperSettings::MaxQueueWaitingAgents), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	TestEqual(TEXT("실제 인원 통계는 팀 자리 상한으로 잘리지 않는다"),
		Queue.GetOccupiedMemberCount(), UFFZSDeveloperSettings::MaxQueueWaitingAgents + 1);
	TestEqual(TEXT("처리율은 실제 수용 가능한 두 팀 기준"), Queue.GetServiceRate(), 0.2f, 0.001f);
	Queue.Configure(1, MakeConfig(0, 10.f, 4), MakeLane());
	TestEqual(TEXT("팀 자리가 없으면 처리율도 0"), Queue.GetServiceRate(), 0.f, 0.001f);
	TestEqual(TEXT("팀 자리가 없으면 도달 불가 대기"), Queue.GetExpectedWaitSeconds(1), FFF_QueueTimings::UnreachableWaitSeconds, 0.001f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
