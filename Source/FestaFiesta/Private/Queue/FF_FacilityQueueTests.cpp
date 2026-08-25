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

// 3. 동시 서비스 1명 — 4인 가족은 4번 순차 처리
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

// 5. MaxWaitSeconds 기반 이탈 — 폴링 없이 Queue가 타이머를 든다
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

	// 일행당 하나씩 나온다. 인원수만큼이 아니다.
	// 4인 가족이 한 줄로 늘어서지 않으므로 자리 하나를 주고 멤버 배치는 포메이션이 맡는다.
	TestEqual(TEXT("일행 수만큼 배정이 나온다"), Assignments.Num(), 2);
	if (Assignments.Num() == 2)
	{
		TestEqual(TEXT("첫 일행이 슬롯 0을 잡는다"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("배정에 리더 AgentId가 실린다"), Assignments[0].AgentId, 1000);
		TestEqual(TEXT("두 번째 일행의 PartyId"), Assignments[1].PartyId, 20);

		// 2인은 나란히 서므로 한 칸이면 충분하다. 그래서 다음 일행이 슬롯 1에 온다.
		TestEqual(TEXT("2인 일행 뒤는 한 칸만 띄운다"), Assignments[1].SlotIndex, 1);
		TestEqual(TEXT("슬롯 간격이 115cm다"),
			Assignments[1].TargetLocation.X - Assignments[0].TargetLocation.X, 115, 0.01);
	}

	// 앞 일행이 빠지면 뒷사람이 앞으로 당겨진다.
	Queue.LeaveQueue(10, SimTime);
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("한 일행이 빠지면 하나 줄어든다"), Assignments.Num(), 1);
	if (Assignments.Num() == 1)
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

	// 슬롯 기하를 보는 테스트다. 1인 일행 셋을 세워 슬롯 0·1·2 를 그대로 얻는다.
	// 한 일행에 3명을 넣으면 배정이 하나만 나와 슬롯 1·2 를 확인할 수 없다.
	Join(Queue, MakeRequest(10, 1), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	Join(Queue, MakeRequest(30, 1), 0.f);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("Explicit slots produce one target per party"), Assignments.Num(), 3);
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

	// 폴리라인 구간별 좌표를 보는 테스트다. 1인 일행 셋으로 슬롯 0·1·2 를 확보한다.
	Join(Queue, MakeRequest(10, 1), 0.f);
	Join(Queue, MakeRequest(20, 1), 0.f);
	Join(Queue, MakeRequest(30, 1), 0.f);

	TArray<FFF_SlotAssignment> Assignments;
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("Polyline produces one target per party"), Assignments.Num(), 3);
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

// 10. 예상 대기 vs 실제 대기 — Utility AI 튜닝의 직접 지표
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
		Queue.GetConfig().MaxCapacityMembers,
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

	// 일행당 하나. Crowd 는 리더에게만 목적지를 주고 멤버는 포메이션으로 따라붙는다.
	TestEqual(TEXT("일행 수만큼 배정이 나온다"), Assignments.Num(), 3);

	bool bAllHaveAgent = true;
	bool bAllFinite = true;
	bool bSlotsAscend = true;
	int32 PreviousSlot = -1;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		bAllHaveAgent &= (Assignment.AgentId != INDEX_NONE);
		bAllFinite &= !Assignment.TargetLocation.ContainsNaN()
			&& FMath::IsFinite(Assignment.TargetYaw);
		bSlotsAscend &= (Assignment.SlotIndex > PreviousSlot);
		PreviousSlot = Assignment.SlotIndex;
	}
	TestTrue(TEXT("모든 배정에 리더 AgentId 가 있다"), bAllHaveAgent);
	TestTrue(TEXT("모든 좌표가 유한하다"), bAllFinite);
	TestTrue(TEXT("슬롯 인덱스가 뒤로 갈수록 커진다"), bSlotsAscend);

	// 같은 일행이 두 번 나오면 리더 목적지가 덮어써진다.
	TSet<int32> SeenParties;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		SeenParties.Add(Assignment.PartyId);
	}
	TestEqual(TEXT("일행이 중복되지 않는다"), SeenParties.Num(), Assignments.Num());

	// 앞 일행이 차지한 만큼 뒤 일행이 물러난다. 2인은 나란히 서므로 한 칸이면 된다.
	TestEqual(TEXT("첫 일행은 슬롯 0"), Assignments[0].SlotIndex, 0);
	TestEqual(TEXT("2인 일행 뒤는 한 칸 뒤"), Assignments[1].SlotIndex, 1);
	TestEqual(TEXT("3인 일행 뒤는 두 칸 뒤"), Assignments[2].SlotIndex, 3);
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
	Queue.Configure(1, MakeConfig(4, 300.f), MakeLane());

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

// 21. Crowd 결선의 전제 — 슬롯 배정이 Party 별로 연속이어야 한다.
//
//     결선 글루는 배정 배열을 Party 경계로 잘라서 GetPartyMembers 와 짝지운다.
//     Party 멤버가 배열 안에서 흩어져 있으면 그 방식이 통째로 깨진다.
//     Queue 쪽 구현이 바뀌어도 이 성질만은 유지돼야 한다.
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
	// 일행 넷이므로 배정도 넷이다.
	TestEqual(TEXT("일행 수만큼 배정된다"), Assignments.Num(), 4);

	// 일행마다 정확히 하나씩. 예전에는 인원수만큼 나와서 Crowd 가 멤버 목적지를
	// 리더로 되돌리는 구조와 충돌했고, 그 결과 리더가 줄 맨 뒤에 혼자 섰다.
	TMap<int32, int32> CountByParty;
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		CountByParty.FindOrAdd(Assignment.PartyId)++;
	}
	TestEqual(TEXT("Party 10 은 하나"), CountByParty.FindRef(10), 1);
	TestEqual(TEXT("Party 20 은 하나"), CountByParty.FindRef(20), 1);
	TestEqual(TEXT("Party 30 은 하나"), CountByParty.FindRef(30), 1);
	TestEqual(TEXT("Party 40 은 하나"), CountByParty.FindRef(40), 1);

	// 인원이 많은 일행일수록 뒤를 더 밀어낸다.
	// 3인(2칸) → 1인(1칸) → 4인(2칸) 순이므로 슬롯은 0, 2, 3, 5 가 된다.
	TestEqual(TEXT("3인 일행은 슬롯 0"), Assignments[0].SlotIndex, 0);
	TestEqual(TEXT("1인 일행은 두 칸 뒤"), Assignments[1].SlotIndex, 2);
	TestEqual(TEXT("4인 일행은 한 칸 뒤"), Assignments[2].SlotIndex, 3);
	TestEqual(TEXT("2인 일행은 두 칸 뒤"), Assignments[3].SlotIndex, 5);

	// 앞 일행이 빠지면 나머지가 당겨진다.
	Queue.LeaveQueue(20, SimTime);
	Queue.BuildSlotAssignments(Assignments);
	TestEqual(TEXT("한 일행이 빠지면 하나 줄어든다"), Assignments.Num(), 3);
	if (Assignments.Num() == 3)
	{
		TestEqual(TEXT("맨 앞은 그대로 슬롯 0"), Assignments[0].SlotIndex, 0);
		TestEqual(TEXT("빈 칸만큼 당겨진다"), Assignments[1].SlotIndex, 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
