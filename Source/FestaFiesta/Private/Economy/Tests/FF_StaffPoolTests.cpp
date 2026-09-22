// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_StaffPool.h"

/**
 * FFF_StaffPool 검증 (S6).
 *
 * 여기서 고정하는 것은 **기본 팀과 당일 고용 인원이 서로 다른 규칙을 받는다**는 성질이다:
 *   기본 팀      = 무페이. 자금난으로 줄지 않는다 (자금난의 바닥)
 *   당일 고용     = 선불. 낼 수 있는 만큼만이고 그날 하루뿐이다
 *
 * 기본 팀이 자금으로 줄어들게 구현하면 잔액 0 에서 인원이 0 이 되고, "아무것도 못 하는 상태"가
 * 생겨 회복이 불가능해진다(economy_spec.md §2.5.1). 그래서 잔액을 0 으로 밀어도 기본 팀이
 * 살아 있는지를 여러 테스트에서 함께 확인한다.
 */
namespace FFF_StaffPoolTestHelpers
{
	// 기획서 §8.2 값. 기본 6명 무페이, 1명 고용비 450(1회), 축제 총원 상한 +6명.
	constexpr int32 BaseTeamSize = 6;
	constexpr int32 HireCost = 450;
	constexpr int32 MaxHires = 6;

	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_StaffPool MakeStaffPoolWithBaseTeam(int32 BaseCount)
	{
		FFF_StaffPool Pool;
		Pool.Initialize(BaseCount);
		return Pool;
	}
}

// 기본 팀이 초기 인원이 되고, 고용 인원은 0 에서 시작한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffPool_InitializeSetsBaseTeam, "FestaFiesta.Economy.StaffPool.InitializeSetsBaseTeam", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffPool_InitializeSetsBaseTeam::RunTest(const FString& Parameters)
{
	const FFF_StaffPool Pool = FFF_StaffPoolTestHelpers::MakeStaffPoolWithBaseTeam(
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	TestEqual(TEXT("기본 팀이 초기 인원이 된다"), Pool.GetBaseStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize);
	// 아직 아침이 오지 않았다. 하루 경계가 고용 인원을 정한다.
	TestEqual(TEXT("고용 인원은 0 에서 시작한다"), Pool.GetHiredStaffCount(), 0);
	TestEqual(TEXT("전체 인원은 기본 팀뿐이다"), Pool.GetTotalStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	// 음수 인원은 0 으로 막는다 — 불변식을 이 구조체 안에서 닫는다.
	const FFF_StaffPool Negative = FFF_StaffPoolTestHelpers::MakeStaffPoolWithBaseTeam(-3);
	TestEqual(TEXT("음수 기본 팀은 0 이 된다"), Negative.GetBaseStaffCount(), 0);
	TestEqual(TEXT("기본 팀 0 이면 전체도 0"), Negative.GetTotalStaffCount(), 0);

	return true;
}

// 잔액이 모자라면 **낼 수 있는 인원만** 고용된다. 이것이 고용비가 빚이 되지 않는 이유다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffPool_AffordableHiresShrinkWithBudget, "FestaFiesta.Economy.StaffPool.AffordableHiresShrinkWithBudget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffPool_AffordableHiresShrinkWithBudget::RunTest(const FString& Parameters)
{
	const FFF_StaffPool Pool = FFF_StaffPoolTestHelpers::MakeStaffPoolWithBaseTeam(
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	// 6명분(2,700)이 있으면 상한까지 고용된다.
	TestEqual(TEXT("전액 있으면 상한까지 고용"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 2700,
			FFF_StaffPoolTestHelpers::MaxHires), FFF_StaffPoolTestHelpers::MaxHires);
	TestEqual(TEXT("돈이 남아도 상한을 넘지 않는다"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 100'000,
			FFF_StaffPoolTestHelpers::MaxHires), FFF_StaffPoolTestHelpers::MaxHires);

	// 1,000 이면 2명분(900)만. 반명분 비용을 내는 상태를 만들지 않는다 — 정수 나눗셈이 의도다.
	TestEqual(TEXT("1,000 으로는 2명만 고용된다"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 1000,
			FFF_StaffPoolTestHelpers::MaxHires), 2);
	TestEqual(TEXT("고용비보다 1 적으면 아무도 고용되지 않는다"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost,
			FFF_StaffPoolTestHelpers::HireCost - 1, FFF_StaffPoolTestHelpers::MaxHires), 0);
	TestEqual(TEXT("고용비와 정확히 같으면 1명 (off-by-one 방지)"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost,
			FFF_StaffPoolTestHelpers::HireCost, FFF_StaffPoolTestHelpers::MaxHires), 1);

	TestEqual(TEXT("잔액 0 이면 고용 불가"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 0,
			FFF_StaffPoolTestHelpers::MaxHires), 0);
	TestEqual(TEXT("잔액이 음수여도 0 이다"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, -500,
			FFF_StaffPoolTestHelpers::MaxHires), 0);

	// 상한 0 이면 돈이 아무리 많아도 고용되지 않는다.
	TestEqual(TEXT("상한 0 이면 고용 불가"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 100'000, 0), 0);
	TestEqual(TEXT("상한이 음수여도 0 이다"),
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 100'000, -6), 0);

	// 고용비가 0 이면 상한까지 고용된다 — 공짜인데 인원을 깎으면 잔액과 무관한 페널티가 된다.
	TestEqual(TEXT("고용비 0 이면 잔액 0 에서도 상한까지 고용"),
		Pool.CalcAffordableHireCount(0, 0, FFF_StaffPoolTestHelpers::MaxHires),
		FFF_StaffPoolTestHelpers::MaxHires);

	// 조회는 상태를 바꾸지 않는다.
	TestEqual(TEXT("조회가 고용 인원을 바꾸지 않는다"), Pool.GetHiredStaffCount(), 0);

	return true;
}

// 고용은 그날 하루뿐이고, 기본 팀은 자금난으로 줄지 않는다.
// 기본 팀이 줄어들면 잔액 0 에서 인원이 0 이 되어 회복이 불가능해진다(§2.5.1).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffPool_HiresAccumulateAndBaseTeamSurvives, "FestaFiesta.Economy.StaffPool.HiresAccumulateAndBaseTeamSurvives", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffPool_HiresAccumulateAndBaseTeamSurvives::RunTest(const FString& Parameters)
{
	FFF_StaffPool Pool = FFF_StaffPoolTestHelpers::MakeStaffPoolWithBaseTeam(
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	Pool.SetHiredStaffCount(4);
	TestEqual(TEXT("고용 인원이 설정된다"), Pool.GetHiredStaffCount(), 4);
	TestEqual(TEXT("전체 인원은 기본 + 고용이다"), Pool.GetTotalStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize + 4);
	TestEqual(TEXT("기본 팀은 고용과 무관하다"), Pool.GetBaseStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	// 이 자료구조는 "설정" 만 한다 — 누적은 소유자가 (기존 + 요청) 을 넘겨 만든다.
	// 하루 경계에서 0 으로 되돌리는 경로가 없다는 것이 영구 고용의 전부다.
	Pool.SetHiredStaffCount(4 + 2);
	TestEqual(TEXT("고용은 축제 내내 쌓인다"), Pool.GetHiredStaffCount(), 6);
	TestEqual(TEXT("전체 인원도 같이 는다"), Pool.GetTotalStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize + 6);

	// 잔액 0 이면 아무도 못 뽑는다. 그래도 기본 팀은 살아 있어야 한다.
	Pool.SetHiredStaffCount(
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 0,
			FFF_StaffPoolTestHelpers::MaxHires));
	TestEqual(TEXT("잔액 0 이면 아무도 고용되지 않는다"), Pool.GetHiredStaffCount(), 0);
	// 이 줄이 자금난의 바닥을 고정한다. 기본 팀이 줄면 회복 불가능한 스파이럴이 된다.
	TestEqual(TEXT("잔액 0 이어도 기본 팀은 일한다"), Pool.GetTotalStaffCount(),
		FFF_StaffPoolTestHelpers::BaseTeamSize);

	// 자금이 있으면 상한까지 고용할 수 있다. 450 x 6 = 2,700.
	Pool.SetHiredStaffCount(
		Pool.CalcAffordableHireCount(FFF_StaffPoolTestHelpers::HireCost, 2700,
			FFF_StaffPoolTestHelpers::MaxHires));
	TestEqual(TEXT("자금이 있으면 상한까지 뽑는다"),
		Pool.GetTotalStaffCount(), FFF_StaffPoolTestHelpers::BaseTeamSize
			+ FFF_StaffPoolTestHelpers::MaxHires);

	// 음수는 막는다.
	Pool.SetHiredStaffCount(-5);
	TestEqual(TEXT("고용 인원은 음수가 되지 않는다"), Pool.GetHiredStaffCount(), 0);

	return true;
}

// 고용비는 1인당 단가의 곱이고, 오버플로 때 음수가 되지 않는다.
// 음수 비용은 "지출로 돈을 버는 경로"가 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_StaffPool_HireCostIsPerHeadAndClampsOnOverflow, "FestaFiesta.Economy.StaffPool.HireCostIsPerHeadAndClampsOnOverflow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_StaffPool_HireCostIsPerHeadAndClampsOnOverflow::RunTest(const FString& Parameters)
{
	// 6명 × 450 = 2,700. 축제 전체 고용 최대 지출이 이 산술 위에 있다.
	TestEqual(TEXT("6명 고용비는 2,700"),
		FFF_StaffPool::CalcHireCost(FFF_StaffPoolTestHelpers::MaxHires,
			FFF_StaffPoolTestHelpers::HireCost), 2700);
	TestEqual(TEXT("1명 고용비는 450"),
		FFF_StaffPool::CalcHireCost(1, FFF_StaffPoolTestHelpers::HireCost),
		FFF_StaffPoolTestHelpers::HireCost);

	// 0 은 정상이다 — 아무도 고용하지 않으면 비용이 0 이고 그것도 성공이다.
	TestEqual(TEXT("0명 고용비는 0"),
		FFF_StaffPool::CalcHireCost(0, FFF_StaffPoolTestHelpers::HireCost), 0);
	TestEqual(TEXT("단가 0 이면 총액 0"),
		FFF_StaffPool::CalcHireCost(FFF_StaffPoolTestHelpers::MaxHires, 0), 0);

	// 음수로 돈을 벌 수 없어야 한다.
	TestEqual(TEXT("음수 인원은 0"),
		FFF_StaffPool::CalcHireCost(-6, FFF_StaffPoolTestHelpers::HireCost), 0);
	TestEqual(TEXT("음수 단가는 0"),
		FFF_StaffPool::CalcHireCost(FFF_StaffPoolTestHelpers::MaxHires, -450), 0);

	// 오버플로는 ensure 로 알리고 **상한에서 멈춘다.** 캐스팅만 하면 음수가 된다.
	// ensureMsgf 는 배너·조건식·메시지·콜스택을 여러 줄로 나눠 찍고 실행마다 어느 줄이
	// Error 인지가 달라진다. 빈 문자열로 이 테스트의 모든 Error 를 통과시킨다.
	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 Overflowed = FFF_StaffPool::CalcHireCost(MAX_int32, 2);
	TestEqual(TEXT("오버플로는 상한에서 멈춘다"), Overflowed, MAX_int32);
	TestTrue(TEXT("오버플로가 음수를 만들지 않는다"), Overflowed > 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
