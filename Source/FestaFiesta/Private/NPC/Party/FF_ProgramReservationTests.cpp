// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "NPC/Party/FF_ProgramReservation.h"

/**
 * FFF_ProgramReservationRules 검증. 확정 규칙이 코드에 실제로 들어갔는지 고정한다:
 *   ① 배정 단위는 그룹  ② 정원을 정확히 채운다  ③ 초과 없음
 *   ④ 예약 확정 뒤로는 프로그램이 끝날 때까지 퇴장하지 않는다
 *
 * 귀환 안전장치(v0 범위 밖)는 `FF_ProgramReservationReturnTripTests.cpp` 에 있다.
 */
namespace FFF_ReservationTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_ProgramReservation MakeReservationTestProgram(int32 Capacity, float StartMinutes, float EndMinutes)
	{
		FFF_ProgramReservation Reservation;
		Reservation.FacilityInstanceId = FGuid(1, 2, 3, 4);
		Reservation.Capacity = Capacity;
		Reservation.StartMinutes = StartMinutes;
		Reservation.EndMinutes = EndMinutes;
		return Reservation;
	}

	FFF_ReservationCandidateGroup MakeReservationTestGroup(int32 GroupId, int32 MemberCount, float Preference = 0.f)
	{
		FFF_ReservationCandidateGroup Group;
		Group.GroupId = GroupId;
		Group.MemberCount = MemberCount;
		Group.PreferenceScore = Preference;
		return Group;
	}

	// 배정 결과를 실제 그룹 인원으로 다시 더해본다 — AssignedHeadcount 가 자기 자신을
	// 검산하지 못하므로 독립 경로로 확인한다.
	int32 SumAssignedMembers(const FFF_ProgramReservation& Reservation,
		const TArray<FFF_ReservationCandidateGroup>& Candidates)
	{
		int32 Total = 0;
		for (const int32 AssignedId : Reservation.AssignedGroupIds)
		{
			for (const FFF_ReservationCandidateGroup& Group : Candidates)
			{
				if (Group.GroupId == AssignedId)
				{
					Total += Group.MemberCount;
					break;
				}
			}
		}
		return Total;
	}

	FFF_ProgramReservation AssignInto(const FFF_ProgramReservation& Program,
		const TArray<FFF_ReservationCandidateGroup>& Candidates)
	{
		FFF_ProgramReservation Result = Program;
		FFF_ProgramReservationRules::Assign(Result, Candidates);
		return Result;
	}
}

using namespace FFF_ReservationTestHelpers;

// 사용자 예시 그대로 — 정원 10 을 5+2+2+1 로 정확히 채운다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_FillsCapacityExactlyWithMixedGroupSizes, "FestaFiesta.NPC.Reservation.FillsCapacityExactlyWithMixedGroupSizes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_FillsCapacityExactlyWithMixedGroupSizes::RunTest(const FString& Parameters)
{
	const TArray<FFF_ReservationCandidateGroup> Candidates = {
		MakeReservationTestGroup(1, 5),
		MakeReservationTestGroup(2, 2),
		MakeReservationTestGroup(3, 2),
		MakeReservationTestGroup(4, 1),
		MakeReservationTestGroup(5, 4)
	};
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/10, 60.f, 120.f), Candidates);

	TestEqual(TEXT("정원을 정확히 채운다"), Assigned.AssignedHeadcount, 10);
	TestEqual(TEXT("남는 자리가 없다"), Assigned.UnfilledSeats, 0);
	TestEqual(TEXT("배정 인원 합이 실제 그룹 인원 합과 같다"), SumAssignedMembers(Assigned, Candidates), 10);
	TestTrue(TEXT("한 그룹을 두 번 배정하지 않는다"),
		TSet<int32>(Assigned.AssignedGroupIds).Num() == Assigned.AssignedGroupIds.Num());
	return true;
}

// 앞에서부터 채우기(greedy)가 실패하는 경우. 정원 8 에 5·4·4 면 greedy 는 5 를 담고 3 을 남기지만
// 정답은 4+4 다. 이 테스트가 깨지면 누군가 DP 를 greedy 로 되돌렸다는 뜻이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_BeatsGreedyWhenGreedyWouldStrand, "FestaFiesta.NPC.Reservation.BeatsGreedyWhenGreedyWouldStrand", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_BeatsGreedyWhenGreedyWouldStrand::RunTest(const FString& Parameters)
{
	// 취향을 내림차순으로 줘서 greedy 라면 반드시 5인 그룹을 먼저 담게 만든다
	const TArray<FFF_ReservationCandidateGroup> Candidates = {
		MakeReservationTestGroup(1, 5, /*Preference=*/0.9f),
		MakeReservationTestGroup(2, 4, /*Preference=*/0.5f),
		MakeReservationTestGroup(3, 4, /*Preference=*/0.4f)
	};
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/8, 60.f, 120.f), Candidates);

	TestEqual(TEXT("취향 1위를 포기하더라도 정원을 정확히 채운다"), Assigned.AssignedHeadcount, 8);
	TestEqual(TEXT("남는 자리가 없다"), Assigned.UnfilledSeats, 0);
	TestFalse(TEXT("5인 그룹은 배정되지 않는다"), Assigned.AssignedGroupIds.Contains(1));
	TestEqual(TEXT("4인 그룹 두 개가 배정된다"), Assigned.AssignedGroupIds.Num(), 2);
	return true;
}

// 같은 크기 안에서는 취향이 높은 그룹이 먼저 배정된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_PrefersHigherPreferenceWithinSameSize, "FestaFiesta.NPC.Reservation.PrefersHigherPreferenceWithinSameSize", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_PrefersHigherPreferenceWithinSameSize::RunTest(const FString& Parameters)
{
	const TArray<FFF_ReservationCandidateGroup> Candidates = {
		MakeReservationTestGroup(10, 2, /*Preference=*/0.1f),
		MakeReservationTestGroup(11, 2, /*Preference=*/0.9f),
		MakeReservationTestGroup(12, 2, /*Preference=*/0.5f)
	};
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/2, 60.f, 120.f), Candidates);

	TestEqual(TEXT("한 그룹만 들어간다"), Assigned.AssignedGroupIds.Num(), 1);
	TestEqual(TEXT("취향이 가장 높은 그룹이 배정된다"), Assigned.AssignedGroupIds[0], 11);
	return true;
}

// 정확히 채울 조합이 없으면 가장 많이 채우고 남는 자리를 보고한다.
// 개인을 그룹에서 떼어내 억지로 맞추지 않는다 — 그룹은 통째로 움직인다는 전제를 지킨다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ReportsUnfilledSeatsInsteadOfSplittingGroups, "FestaFiesta.NPC.Reservation.ReportsUnfilledSeatsInsteadOfSplittingGroups", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ReportsUnfilledSeatsInsteadOfSplittingGroups::RunTest(const FString& Parameters)
{
	// 정원 5, 후보는 2인 그룹 둘뿐 → 4 명까지만 채울 수 있다
	const TArray<FFF_ReservationCandidateGroup> Candidates = {
		MakeReservationTestGroup(1, 2),
		MakeReservationTestGroup(2, 2)
	};
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/5, 60.f, 120.f), Candidates);

	TestEqual(TEXT("가능한 만큼만 채운다"), Assigned.AssignedHeadcount, 4);
	TestEqual(TEXT("남는 자리를 보고한다"), Assigned.UnfilledSeats, 1);
	TestEqual(TEXT("정원을 넘기지 않는다"), Assigned.AssignedGroupIds.Num(), 2);
	return true;
}

// 초과 배정은 절대 없다. 시설 정원은 안전 제약이기도 하다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_NeverExceedsCapacity, "FestaFiesta.NPC.Reservation.NeverExceedsCapacity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_NeverExceedsCapacity::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReservationTestProgram(/*Capacity=*/3, 60.f, 120.f);

	// 정원 3 인데 후보가 전부 4·5·6 인 그룹 → 아무도 못 들어간다
	const FFF_ProgramReservation Empty = AssignInto(Program, {
		MakeReservationTestGroup(1, 4),
		MakeReservationTestGroup(2, 5),
		MakeReservationTestGroup(3, 6)
	});
	TestEqual(TEXT("정원보다 큰 그룹만 있으면 아무도 배정되지 않는다"), Empty.AssignedHeadcount, 0);
	TestEqual(TEXT("정원 전체가 남는 자리로 보고된다"), Empty.UnfilledSeats, 3);

	// 후보가 넘쳐도 정원을 넘기지 않는다
	TArray<FFF_ReservationCandidateGroup> Many;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		Many.Add(MakeReservationTestGroup(Index, /*MemberCount=*/1));
	}
	const FFF_ProgramReservation Filled = AssignInto(Program, Many);
	TestEqual(TEXT("후보가 넘쳐도 정원까지만 배정한다"), Filled.AssignedHeadcount, 3);
	TestEqual(TEXT("배정 그룹 수도 정원을 넘지 않는다"), Filled.AssignedGroupIds.Num(), 3);
	return true;
}

// 결정적이어야 한다 — 입력 순서가 바뀌어도 같아야 한다
// (crowdAI 가 후보를 어떤 순서로 넘길지 정해지지 않았다).
// 취향을 전부 같게 두므로 순서를 정하는 것은 GroupId 뿐이다 — v0 의 실제 상황이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_IsDeterministicRegardlessOfInputOrder, "FestaFiesta.NPC.Reservation.IsDeterministicRegardlessOfInputOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_IsDeterministicRegardlessOfInputOrder::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReservationTestProgram(/*Capacity=*/12, 60.f, 120.f);

	TArray<FFF_ReservationCandidateGroup> Forward;
	for (int32 Index = 1; Index <= 8; ++Index)
	{
		Forward.Add(MakeReservationTestGroup(Index, /*MemberCount=*/(Index % 6) + 1));
	}
	TArray<FFF_ReservationCandidateGroup> Reversed = Forward;
	Algo::Reverse(Reversed);

	const FFF_ProgramReservation A = AssignInto(Program, Forward);
	const FFF_ProgramReservation B = AssignInto(Program, Reversed);

	TestEqual(TEXT("인원 합이 같다"), A.AssignedHeadcount, B.AssignedHeadcount);
	TestTrue(TEXT("배정된 그룹 목록이 같다"), A.AssignedGroupIds == B.AssignedGroupIds);
	TestTrue(TEXT("같은 입력을 두 번 넣으면 같은 결과"),
		A.AssignedGroupIds == AssignInto(Program, Forward).AssignedGroupIds);
	return true;
}

// 잘못된 데이터가 조용히 배정되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_RejectsInvalidInput, "FestaFiesta.NPC.Reservation.RejectsInvalidInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_RejectsInvalidInput::RunTest(const FString& Parameters)
{
	const TArray<FFF_ReservationCandidateGroup> Candidates = { MakeReservationTestGroup(1, 2) };

	const FFF_ProgramReservation NoCapacity = MakeReservationTestProgram(/*Capacity=*/0, 60.f, 120.f);
	TestFalse(TEXT("정원 0 은 유효하지 않다"), NoCapacity.IsValid());
	TestEqual(TEXT("정원 0 이면 아무도 배정되지 않는다"),
		AssignInto(NoCapacity, Candidates).AssignedGroupIds.Num(), 0);

	const FFF_ProgramReservation Backwards = MakeReservationTestProgram(/*Capacity=*/10, 120.f, 60.f);
	TestFalse(TEXT("종료가 시작보다 앞이면 유효하지 않다"), Backwards.IsValid());

	// 인원 0·음수 그룹은 배정 대상이 아니다
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/4, 60.f, 120.f), {
			MakeReservationTestGroup(1, 0),
			MakeReservationTestGroup(2, -3),
			MakeReservationTestGroup(3, 4)
		});
	TestEqual(TEXT("정상 그룹만 배정된다"), Assigned.AssignedGroupIds.Num(), 1);
	TestEqual(TEXT("정상 그룹이 정원을 채운다"), Assigned.AssignedHeadcount, 4);
	return true;
}

// 배정 결과를 예약이 직접 들고 있어야 런타임이 "이 그룹 예약 있나"를 물을 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ReservationKnowsItsGroups, "FestaFiesta.NPC.Reservation.ReservationKnowsItsGroups", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ReservationKnowsItsGroups::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Assigned =
		AssignInto(MakeReservationTestProgram(/*Capacity=*/3, 60.f, 120.f), {
			MakeReservationTestGroup(7, 2),
			MakeReservationTestGroup(8, 1),
			MakeReservationTestGroup(9, 5)
		});

	TestTrue(TEXT("배정된 그룹을 안다"), Assigned.HasGroup(7));
	TestTrue(TEXT("배정된 그룹을 안다(2)"), Assigned.HasGroup(8));
	TestFalse(TEXT("정원보다 큰 그룹은 배정되지 않았다"), Assigned.HasGroup(9));
	TestFalse(TEXT("없는 그룹은 false"), Assigned.HasGroup(999));
	return true;
}

// 잠금 = 목적지 선택을 건너뛰는 구간. 진행 중에만이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_LockAppliesOnlyDuringProgram, "FestaFiesta.NPC.Reservation.LockAppliesOnlyDuringProgram", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_LockAppliesOnlyDuringProgram::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReservationTestProgram(/*Capacity=*/10, 60.f, 120.f);

	TestFalse(TEXT("시작 전에는 잠기지 않는다 — 자유 행동이다"),
		FFF_ProgramReservationRules::IsLockedAt(Program, 59.9f));
	TestTrue(TEXT("시작 시각에 잠긴다"),
		FFF_ProgramReservationRules::IsLockedAt(Program, 60.f));
	TestTrue(TEXT("진행 중에는 잠긴다"),
		FFF_ProgramReservationRules::IsLockedAt(Program, 100.f));
	TestFalse(TEXT("종료 시각부터는 풀린다"),
		FFF_ProgramReservationRules::IsLockedAt(Program, 120.f));
	return true;
}

// 퇴장 금지는 잠금보다 넓다 — 확정 뒤로는 시작 전이든 진행 중이든 금지다.
// 이 구분이 없으면 자유 행동 구간에서 체류 예산이 말라 예약자가 프로그램 전에 나간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ExitBlockedUntilProgramEnds, "FestaFiesta.NPC.Reservation.ExitBlockedUntilProgramEnds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ExitBlockedUntilProgramEnds::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReservationTestProgram(/*Capacity=*/10, 100.f, 160.f);

	TestFalse(TEXT("시작 전에는 잠기지 않는다(자유 행동)"),
		FFF_ProgramReservationRules::IsLockedAt(Program, 10.f));
	TestTrue(TEXT("그래도 퇴장은 금지된다 — 잠금과 구간이 다르다"),
		FFF_ProgramReservationRules::IsExitBlockedAt(Program, 10.f));
	TestTrue(TEXT("진행 중에도 퇴장 금지"),
		FFF_ProgramReservationRules::IsExitBlockedAt(Program, 130.f));
	TestFalse(TEXT("종료 시각부터 퇴장 가능"),
		FFF_ProgramReservationRules::IsExitBlockedAt(Program, 160.f));

	const FFF_ProgramReservation NoReservation;
	TestFalse(TEXT("예약이 없으면 퇴장 금지가 아니다"),
		FFF_ProgramReservationRules::IsExitBlockedAt(NoReservation, 10.f));

	// 잠금 구간은 퇴장 금지 구간에 완전히 포함돼야 한다 — 어긋나면 한쪽에 구멍이 생긴다
	for (float Now = 0.f; Now <= 200.f; Now += 5.f)
	{
		if (FFF_ProgramReservationRules::IsLockedAt(Program, Now))
		{
			TestTrue(FString::Printf(TEXT("잠긴 시각 %.0f 에는 퇴장도 금지여야 한다"), Now),
				FFF_ProgramReservationRules::IsExitBlockedAt(Program, Now));
		}
	}
	return true;
}

// 종료 시 이탈 유예 — 퇴장을 막는 동안 예산이 0 에 붙어 있을 수 있어서,
// 그대로 풀면 프로그램이 끝나는 순간 사라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ExitGraceRefillsButNeverShrinks, "FestaFiesta.NPC.Reservation.ExitGraceRefillsButNeverShrinks", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ExitGraceRefillsButNeverShrinks::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("예산이 말랐으면 유예값으로 채운다"),
		FFF_ProgramReservationRules::ApplyExitGrace(/*Current=*/0.f, /*Grace=*/10.f), 10.f, 0.001f);
	TestEqual(TEXT("이미 더 많으면 줄이지 않는다"),
		FFF_ProgramReservationRules::ApplyExitGrace(/*Current=*/50.f, /*Grace=*/10.f), 50.f, 0.001f);
	TestEqual(TEXT("유예 0 이면 아무것도 하지 않는다 — 종료 즉시 퇴장 가능"),
		FFF_ProgramReservationRules::ApplyExitGrace(/*Current=*/0.f, /*Grace=*/0.f), 0.f, 0.001f);
	TestEqual(TEXT("음수 유예는 0 으로 막는다"),
		FFF_ProgramReservationRules::ApplyExitGrace(/*Current=*/5.f, /*Grace=*/-30.f), 5.f, 0.001f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
