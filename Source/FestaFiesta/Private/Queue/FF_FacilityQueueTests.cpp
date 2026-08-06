// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Queue/FF_FacilityQueue.h"

// 설계문서 12.1 P0 체크리스트와 8·9장의 "여기서 틀리면 통합 때 터진다" 지점을 테스트로 옮긴다.

namespace FFF_FacilityQueueTestHelpers
{
	constexpr float TestStepSeconds = 1.f / 30.f;

	FFF_QueueConfig MakeConfig(int32 MaxCapacityMembers, float ServiceSecondsPerMember, int32 ConcurrentSlots = 1)
	{
		FFF_QueueConfig Config;
		Config.MaxCapacityMembers = MaxCapacityMembers;
		Config.ServiceSecondsPerMember = ServiceSecondsPerMember;
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
		Lane.SlotSpacingCm = 115.f;   // 설계문서 8.4
		Lane.SlotYawDegrees = 180.f;
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

// 1. Party 일괄 가입 · 전원 거절 (설계문서 8.2① / 12.1)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_PartyJoinIsAllOrNothing, "FestaFiesta.Queue.FacilityQueue.PartyJoinIsAllOrNothing", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PartyJoinIsAllOrNothing::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(/*MaxCapacityMembers=*/6, /*ServiceSecondsPerMember=*/100.f), MakeLane());

	TestEqual(TEXT("4인 Party는 빈 줄(6칸)에 들어간다"),
		Join(Queue, MakeRequest(10, 4), 0.f), EFF_QueueJoinResult::Accepted);

	// 남은 칸은 2. 4인 가족은 절반만 넣지 않고 전원 거절해야 한다.
	TestEqual(TEXT("남은 칸보다 큰 Party는 전원 거절된다"),
		Join(Queue, MakeRequest(20, 4), 0.f), EFF_QueueJoinResult::RejectedFull);

	TestEqual(TEXT("거절된 Party는 줄에 흔적을 남기지 않는다"), Queue.GetOccupiedMemberCount(), 4);
	TestFalse(TEXT("거절된 Party는 대기열에 없다"), Queue.ContainsParty(20));

	TestEqual(TEXT("남은 칸에 딱 맞는 Party는 수락된다"),
		Join(Queue, MakeRequest(30, 2), 0.f), EFF_QueueJoinResult::Accepted);
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

// 3. 동시 서비스 1명 — 4인 가족은 4번 순차 처리 (설계문서 9.3)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_PartyIsServedMemberByMember, "FestaFiesta.Queue.FacilityQueue.PartyIsServedMemberByMember", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_PartyIsServedMemberByMember::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerMember=*/10.f, /*ConcurrentSlots=*/1), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 4), SimTime);

	StepFor(Queue, 25.f, SimTime);
	TestEqual(TEXT("2명만 처리된 시점에도 Party는 줄에 남아 있다"), Queue.GetWaitingPartyCount(), 1);
	// 서비스를 마친 멤버도 슬롯을 비우지 않는다 — 전원이 끝나야 Recovering으로 넘어간다.
	TestEqual(TEXT("처리 중에도 슬롯 점유는 4칸 그대로"), Queue.GetOccupiedMemberCount(), 4);
	TestEqual(TEXT("남은 처리 인원은 줄어든다"), Queue.GetRemainingServiceMemberCount(), 2);

	StepFor(Queue, 20.f, SimTime);
	TestEqual(TEXT("4인 × 10초 = 40초가 지나면 Party 전체가 빠진다"), Queue.GetWaitingPartyCount(), 0);

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("Served 로그가 1건 남는다"), CountExits(Logs, EFF_QueueExit::Served), 1);
	TestEqual(TEXT("서비스 소요시간이 1인당 시간 × 인원에 근접한다"), Logs.Last().ServiceSeconds, 40.f, 1.f);
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

// 5. MaxWaitSeconds 기반 이탈 — 폴링 없이 Queue가 타이머를 든다 (설계문서 9.2)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_AbandonsAfterMaxWait, "FestaFiesta.Queue.FacilityQueue.AbandonsAfterMaxWait", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_AbandonsAfterMaxWait::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerMember=*/300.f), MakeLane());

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
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerMember=*/60.f), MakeLane());

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

// 7. 시설 긴급 폐쇄 → 대기 중 전원 Cancelled (설계문서 9.3)
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

// 8. 슬롯 목표 위치 — 연속 배정 + 115cm 간격 (설계문서 8.2② / 8.4)
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

	TestEqual(TEXT("총 인원만큼 슬롯이 나온다"), Assignments.Num(), 5);
	if (Assignments.Num() == 5)
	{
		// 4인 가족은 슬롯 4칸을 연속으로 잡아야 한다 — 흩어지면 포메이션이 깨진다.
		TestEqual(TEXT("앞 Party가 슬롯 0·1을 연속으로 잡는다"), Assignments[1].SlotIndex, 1);
		TestEqual(TEXT("뒤 Party는 슬롯 2부터 시작한다"), Assignments[2].SlotIndex, 2);
		TestEqual(TEXT("같은 Party 멤버는 같은 PartyId를 갖는다"), Assignments[4].PartyId, 20);
		TestEqual(TEXT("슬롯 간격이 115cm다"),
			Assignments[1].TargetLocation.X - Assignments[0].TargetLocation.X, 115, 0.01);
		TestEqual(TEXT("AgentId가 그대로 전달된다"), Assignments[0].AgentId, 1000);
	}

	// 앞 Party가 빠지면 뒷사람이 앞으로 당겨진다 — Queue는 목표만 바꾸고 Crowd가 걸어서 이동한다.
	Queue.LeaveQueue(10, SimTime);
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("앞 Party가 빠지면 슬롯이 다시 0부터 채워진다"), Assignments.Num(), 3);
	if (Assignments.Num() == 3)
	{
		TestEqual(TEXT("뒷 Party가 슬롯 0으로 당겨진다"), Assignments[0].SlotIndex, 0);
	}
	TestTrue(TEXT("줄 구성이 바뀌면 슬롯 갱신 플래그가 선다"), Queue.ConsumeSlotTargetsDirty());
	TestFalse(TEXT("플래그는 한 번 소비하면 내려간다"), Queue.ConsumeSlotTargetsDirty());
	return true;
}

// 9. ExpectedWait — 단위(초)와 단조성 (설계문서 9.1)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_ExpectedWaitIsSecondsAndMonotonic, "FestaFiesta.Queue.FacilityQueue.ExpectedWaitIsSecondsAndMonotonic", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_ExpectedWaitIsSecondsAndMonotonic::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	// 초당 처리 인원 = 1 / 20 = 0.05
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerMember=*/20.f, /*ConcurrentSlots=*/1), MakeLane());

	TestEqual(TEXT("빈 줄에 1명이 오면 20초"), Queue.GetExpectedWaitSeconds(1), 20.f, 0.01f);
	// 분 단위로 그대로 꽂으면 wait_ref_minutes=30 기준으로 60배 과대평가된다 — 어댑터 필수.
	TestEqual(TEXT("분 어댑터는 60으로 나눈 값을 준다"), Queue.GetExpectedWaitMinutes(1), 20.f / 60.f, 0.001f);

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 3), SimTime);
	const float WaitWithThreeAhead = Queue.GetExpectedWaitSeconds(1);
	TestTrue(TEXT("앞에 사람이 늘면 예상 대기가 늘어난다"), WaitWithThreeAhead > 20.f);
	TestEqual(TEXT("(앞 3명 + 나 1명) / 0.05 = 80초"), WaitWithThreeAhead, 80.f, 0.01f);

	// 처리된 인원은 분자에서 빠져야 한다.
	StepFor(Queue, 25.f, SimTime);
	TestTrue(TEXT("앞사람이 처리될수록 예상 대기가 줄어든다"),
		Queue.GetExpectedWaitSeconds(1) < WaitWithThreeAhead);
	return true;
}

// 10. 예상 대기 vs 실제 대기 — Utility AI 튜닝의 직접 지표 (설계문서 10.2)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityQueue_LogsEstimatedAndActualWait, "FestaFiesta.Queue.FacilityQueue.LogsEstimatedAndActualWait", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FacilityQueue_LogsEstimatedAndActualWait::RunTest(const FString& Parameters)
{
	FFF_FacilityQueue Queue;
	Queue.Configure(1, MakeConfig(20, /*ServiceSecondsPerMember=*/10.f), MakeLane());

	float SimTime = 0.f;
	Join(Queue, MakeRequest(10, 2), SimTime);   // 앞 Party: 2명 × 10초 = 20초
	Join(Queue, MakeRequest(20, 1, -1.f), SimTime);

	StepFor(Queue, 40.f, SimTime);

	TArray<FFF_QueueLog> Logs;
	Queue.ConsumePendingLogs(Logs);
	TestEqual(TEXT("두 Party 모두 로그를 남긴다"), Logs.Num(), 2);
	if (Logs.Num() == 2)
	{
		const FFF_QueueLog& Second = Logs[1];
		TestEqual(TEXT("가입 시점 예측이 기록된다"), Second.EstimatedWaitAtJoinSeconds, 30.f, 0.01f);
		// 앞 Party 2명 × 10초 = 20초 후에 서비스가 시작된다. 예측(30초)과 실제(20초)의 괴리가
		// 바로 튜닝 지표다 — 예측 분자에 자기 일행을 포함하기 때문에 구조적으로 과대추정된다.
		TestEqual(TEXT("실제 대기는 서비스 시작 시점 기준으로 기록된다"), Second.ActualWaitSeconds, 20.f, 1.f);
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

// 12. 도착 확인 게이트 — Crowd 통지 전에는 서비스가 시작되지 않는다 (설계문서 8.2①)
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

#endif // WITH_DEV_AUTOMATION_TESTS
