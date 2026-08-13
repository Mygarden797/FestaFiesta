// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Party/FF_ProgramReservationReturnTrip.h"

/**
 * 예약 귀환 안전장치 검증 — **v0 범위 밖 기능이다.**
 *
 * 프로덕션에서 아무도 부르지 않는다(`EstimatedWaitMinutes` 가 0이라 켜면 지각이 늘어난다).
 * 그래도 테스트를 두는 이유: 켤 때 계산식과 fail-open 방향을 다시 유도하지 않기 위해서다.
 * 파킹 이유와 켜는 절차는 `FF_ProgramReservationReturnTrip.h` 상단 주석에 있다.
 */
namespace FFF_ReturnTripTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_ProgramReservation MakeReturnTripTestProgram(float StartMinutes, float EndMinutes)
	{
		FFF_ProgramReservation Reservation;
		Reservation.FacilityInstanceId = FGuid(1, 2, 3, 4);
		Reservation.Capacity = 10;
		Reservation.StartMinutes = StartMinutes;
		Reservation.EndMinutes = EndMinutes;
		return Reservation;
	}
}

using namespace FFF_ReturnTripTestHelpers;

// ─────────────────────────────────────────────────────────────────────────────
// 귀환 안전장치 1차 — EvaluateReturnTrip
// ─────────────────────────────────────────────────────────────────────────────

// 왕복 시간이 남은 시간보다 길면 탈락한다. 1.5 m/s = 90 m/분 으로 검산한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_RejectsCandidateTooFarToReturnFrom, "FestaFiesta.NPC.ReservationReturnTrip.RejectsCandidateTooFarToReturnFrom", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_RejectsCandidateTooFarToReturnFrom::RunTest(const FString& Parameters)
{
	// 시작 60분, 현재 30분 → 30분 쓸 수 있다
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);

	// 1.5 m/s = 90 m/분. 왕복 900 m + 900 m = 20분
	FFF_ReturnTripInput Near;
	Near.DistanceToCandidateMeters = 900.f;
	Near.CandidateToProgramMeters = 900.f;

	const FFF_ReturnTripVerdict NearVerdict =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Near);
	TestTrue(TEXT("판정이 수행됐다"), NearVerdict.bWasEvaluated);
	TestEqual(TEXT("왕복 1800 m 는 20분"), NearVerdict.RequiredMinutes, 20.f, 0.01f);
	TestEqual(TEXT("쓸 수 있는 시간은 30분"), NearVerdict.AvailableMinutes, 30.f, 0.01f);
	TestTrue(TEXT("20분 왕복은 30분 안에 통과한다"), NearVerdict.bCanReturnInTime);

	// 왕복 3600 m = 40분 → 30분 안에 못 돌아온다
	FFF_ReturnTripInput Far = Near;
	Far.DistanceToCandidateMeters = 1800.f;
	Far.CandidateToProgramMeters = 1800.f;

	const FFF_ReturnTripVerdict FarVerdict =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Far);
	TestEqual(TEXT("왕복 3600 m 는 40분"), FarVerdict.RequiredMinutes, 40.f, 0.01f);
	TestFalse(TEXT("40분 왕복은 탈락한다"), FarVerdict.bCanReturnInTime);
	return true;
}

// 대기시간과 이용시간이 왕복 시간에 들어간다.
// 대기가 0 이면 통과했던 후보가 실제 대기시간이 붙으면 탈락할 수 있다 — 그래서 어댑터 결선 전에
// 이 필터를 켜면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_WaitAndUseTimeCountTowardReturnTrip, "FestaFiesta.NPC.ReservationReturnTrip.WaitAndUseTimeCountTowardReturnTrip", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_WaitAndUseTimeCountTowardReturnTrip::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);

	// 왕복 1800 m = 20분. 남은 30분이라 이동만 보면 통과다
	FFF_ReturnTripInput Input;
	Input.DistanceToCandidateMeters = 900.f;
	Input.CandidateToProgramMeters = 900.f;

	TestTrue(TEXT("대기 0 이면 통과한다"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Input).bCanReturnInTime);

	// 대기 8분 + 이용 5분을 더하면 33분이 되어 탈락한다
	Input.CandidateWaitMinutes = 8.f;
	Input.CandidateUseMinutes = 5.f;
	const FFF_ReturnTripVerdict Verdict =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Input);
	TestEqual(TEXT("대기·이용이 필요 시간에 더해진다"), Verdict.RequiredMinutes, 33.f, 0.01f);
	TestFalse(TEXT("대기가 붙으면 같은 후보가 탈락한다"), Verdict.bCanReturnInTime);
	return true;
}

// 귀환 여유가 쓸 수 있는 시간을 깎는다. 기본 0 은 "여유 없음"이고 그래서 낙관적이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ReturnBufferShrinksAvailableTime, "FestaFiesta.NPC.ReservationReturnTrip.ReturnBufferShrinksAvailableTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ReturnBufferShrinksAvailableTime::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);

	FFF_ReturnTripInput Input;
	Input.DistanceToCandidateMeters = 1260.f;   // 14분
	Input.CandidateToProgramMeters = 1260.f;    // 14분 → 왕복 28분

	TestTrue(TEXT("여유 0 이면 28분 왕복이 30분 안에 통과"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Input).bCanReturnInTime);

	Input.ReturnBufferMinutes = 5.f;
	const FFF_ReturnTripVerdict Verdict =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Input);
	TestEqual(TEXT("여유가 가용 시간을 깎는다"), Verdict.AvailableMinutes, 25.f, 0.01f);
	TestFalse(TEXT("여유 5분이면 같은 후보가 탈락한다"), Verdict.bCanReturnInTime);

	// 음수 여유는 가용 시간을 늘리는 방향이라 0 으로 막는다
	Input.ReturnBufferMinutes = -10.f;
	TestEqual(TEXT("음수 여유는 0 으로 막는다"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Input).AvailableMinutes,
		30.f, 0.01f);
	return true;
}

// 판정할 수 없으면 fail-open 한다. 후보를 전부 닫으면 no_choice → Leave 로 정원이 비고
// 매출이 비결정적이 된다 — 몇 명 지각보다 그게 훨씬 나쁘다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_ReturnTripFailsOpenWhenUnjudgeable, "FestaFiesta.NPC.ReservationReturnTrip.ReturnTripFailsOpenWhenUnjudgeable", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_ReturnTripFailsOpenWhenUnjudgeable::RunTest(const FString& Parameters)
{
	FFF_ReturnTripInput Impossible;
	Impossible.DistanceToCandidateMeters = 100000.f;
	Impossible.CandidateToProgramMeters = 100000.f;

	// 예약이 없으면 귀환 제약 자체가 없다
	const FFF_ProgramReservation NoReservation;
	const FFF_ReturnTripVerdict NoResv =
		FFF_ProgramReservationReturnTrip::Evaluate(NoReservation, 30.f, Impossible);
	TestFalse(TEXT("예약이 없으면 판정하지 않는다"), NoResv.bWasEvaluated);
	TestTrue(TEXT("예약이 없으면 통과시킨다"), NoResv.bCanReturnInTime);

	// 속도가 0 이면 이동시간을 계산할 수 없다
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);
	FFF_ReturnTripInput NoSpeed = Impossible;
	NoSpeed.WalkSpeedMetersPerSecond = 0.f;
	const FFF_ReturnTripVerdict Unjudgeable =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, NoSpeed);
	TestFalse(TEXT("속도 0 이면 판정하지 않는다"), Unjudgeable.bWasEvaluated);
	TestTrue(TEXT("판정 못 하면 통과시킨다 (fail-open)"), Unjudgeable.bCanReturnInTime);

	// 속도가 정상이면 같은 입력이 탈락한다 — fail-open 이 판정을 대체한 것이 맞다
	TestFalse(TEXT("속도가 정상이면 같은 후보가 탈락한다"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Impossible).bCanReturnInTime);
	return true;
}

// 기본 보행 속도는 crowdAI 의 FUnitData::Speed(150 cm/s)에서 온 값이다.
// 이 테스트가 깨지면 누군가 근거 없는 숫자로 바꿨다는 뜻이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_DefaultWalkSpeedMatchesCrowdSystem, "FestaFiesta.NPC.ReservationReturnTrip.DefaultWalkSpeedMatchesCrowdSystem", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_DefaultWalkSpeedMatchesCrowdSystem::RunTest(const FString& Parameters)
{
	// FUnitData::Speed = 150.f (cm/s). UE 는 1uu = 1cm 이므로 1.5 m/s 다.
	TestEqual(TEXT("기본 보행 속도는 1.5 m/s = crowdAI 의 150 cm/s"),
		FFF_ReturnTripInput::DefaultWalkSpeedMetersPerSecond, 1.5f, 0.0001f);

	const FFF_ReturnTripInput Fresh;
	TestEqual(TEXT("입력 구조체 기본값도 같다"),
		Fresh.WalkSpeedMetersPerSecond, 1.5f, 0.0001f);

	// 1.5 m/s = 90 m/분 이라는 환산이 실제로 성립하는지 확인한다
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);
	FFF_ReturnTripInput NinetyMeters;
	NinetyMeters.DistanceToCandidateMeters = 90.f;
	TestEqual(TEXT("90 m 는 1분이다"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, NinetyMeters).RequiredMinutes,
		1.f, 0.01f);
	return true;
}

// 시작 시각에 가까워지면 가용 시간이 0 이 되어 아무 후보도 통과하지 못한다.
// 그 상태가 곧 하드 데드라인(2차 안전장치)이 필요한 시점이다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Reservation_NoCandidatePassesAtDeadline, "FestaFiesta.NPC.ReservationReturnTrip.NoCandidatePassesAtDeadline", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Reservation_NoCandidatePassesAtDeadline::RunTest(const FString& Parameters)
{
	const FFF_ProgramReservation Program = MakeReturnTripTestProgram(60.f, 120.f);

	FFF_ReturnTripInput Tiny;
	Tiny.DistanceToCandidateMeters = 9.f;   // 0.1분
	Tiny.CandidateToProgramMeters = 9.f;

	TestTrue(TEXT("여유가 있으면 가까운 후보는 통과"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 30.f, Tiny).bCanReturnInTime);

	// 시작 시각에 도달 → 가용 0
	const FFF_ReturnTripVerdict AtStart =
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 60.f, Tiny);
	TestEqual(TEXT("시작 시각에는 가용 시간이 0"), AtStart.AvailableMinutes, 0.f, 0.01f);
	TestFalse(TEXT("가용 0 이면 아무 후보도 통과하지 못한다"), AtStart.bCanReturnInTime);

	// 이동이 전혀 필요 없는 후보(거리 0·대기 0)는 가용 0 에서도 통과한다 — 경계값
	const FFF_ReturnTripInput Zero;
	TestTrue(TEXT("필요 시간 0 이면 가용 0 에서도 통과한다"),
		FFF_ProgramReservationReturnTrip::Evaluate(Program, 60.f, Zero).bCanReturnInTime);
	return true;
}


#endif // WITH_DEV_AUTOMATION_TESTS
