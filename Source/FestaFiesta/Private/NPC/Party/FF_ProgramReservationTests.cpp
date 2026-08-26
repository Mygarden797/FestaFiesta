// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Party/FF_ProgramReservation.h"

/**
 * FFF_ProgramReservationRules 검증 — **시간 구간 규칙만** 남아 있다.
 *
 *   ④ 예약 확정 뒤로는 프로그램이 끝날 때까지 퇴장하지 않는다
 *
 * 초과 금지는 `UFF_ProgramReservationSubsystem::ConfirmReservedParty` 가 지키고
 * 그쪽 테스트가 본다.
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
}

using namespace FFF_ReservationTestHelpers;

// 저작이 틀린 예약은 유효하지 않다. 잠금·퇴장 판정이 전부 IsValid 를 먼저 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_RejectsInvalidProgram, "FestaFiesta.NPC.Reservation.RejectsInvalidProgram", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_RejectsInvalidProgram::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("정원 0 은 유효하지 않다"),
		MakeReservationTestProgram(/*Capacity=*/0, 60.f, 120.f).IsValid());
	TestFalse(TEXT("종료가 시작보다 앞이면 유효하지 않다"),
		MakeReservationTestProgram(/*Capacity=*/10, 120.f, 60.f).IsValid());
	TestTrue(TEXT("정상 저작은 유효하다"),
		MakeReservationTestProgram(/*Capacity=*/10, 60.f, 120.f).IsValid());

	// 유효하지 않으면 아무 시각에도 잠기지 않고 퇴장도 막히지 않는다 —
	// 저작 실수가 NPC 를 영원히 묶어두는 경로가 없어야 한다.
	const FFF_ProgramReservation Backwards = MakeReservationTestProgram(/*Capacity=*/10, 120.f, 60.f);
	TestFalse(TEXT("무효한 예약은 잠그지 않는다"),
		FFF_ProgramReservationRules::IsLockedAt(Backwards, 100.f));
	TestFalse(TEXT("무효한 예약은 퇴장을 막지 않는다"),
		FFF_ProgramReservationRules::IsExitBlockedAt(Backwards, 100.f));
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
