// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_FixedCostArrears.h"

/**
 * FFF_FixedCostArrears 검증 (S6).
 *
 * 여기서 고정하는 것은 미납이 **이자 없이 누적되고, 상환은 예산과 잔여 미납 중 작은 쪽까지만**
 * 이뤄진다는 성질이다. 초과 상환하면 호출부가 있지도 않은 빚에 돈을 버린다.
 *
 * 오버플로 케이스가 특히 중요하다 — 미납이 음수로 뒤집히면 "빚이 사라진 것"과 구분되지 않고,
 * 안 갚았는데 갚은 상태가 된다. economy_spec.md §3.7.
 */
namespace FFF_FixedCostArrearsTestHelpers
{
	// 임대료 2,400/일 (economy_spec.md §1.2 · E-23).
	constexpr int32 DailyRent = 2400;
}

// 미납은 이자 없이 그대로 누적된다. 0·음수는 조용히 무시한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FixedCostArrears_AccumulatesWithoutInterest, "FestaFiesta.Economy.FixedCostArrears.AccumulatesWithoutInterest", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FixedCostArrears_AccumulatesWithoutInterest::RunTest(const FString& Parameters)
{
	FFF_FixedCostArrears Arrears;

	TestTrue(TEXT("처음에는 미납이 없다"), Arrears.HasNoArrears());
	TestEqual(TEXT("초기 미납 총액은 0"), Arrears.GetTotalArrears(), 0);

	// 이틀 연속 전액 미납 — 이자 없이 산술 합이어야 한다.
	Arrears.AddArrears(FFF_FixedCostArrearsTestHelpers::DailyRent);
	TestFalse(TEXT("미납이 있으면 HasNoArrears 가 false"), Arrears.HasNoArrears());
	Arrears.AddArrears(FFF_FixedCostArrearsTestHelpers::DailyRent);
	TestEqual(TEXT("이자 없이 산술 합으로 쌓인다"), Arrears.GetTotalArrears(),
		FFF_FixedCostArrearsTestHelpers::DailyRent * 2);

	// 0 이하는 조용히 무시한다 — "전액 냈다"가 정상 경로다.
	Arrears.AddArrears(0);
	TestEqual(TEXT("0 은 미납을 바꾸지 않는다"), Arrears.GetTotalArrears(),
		FFF_FixedCostArrearsTestHelpers::DailyRent * 2);
	Arrears.AddArrears(-1000);
	TestEqual(TEXT("음수로 미납을 깎을 수 없다"), Arrears.GetTotalArrears(),
		FFF_FixedCostArrearsTestHelpers::DailyRent * 2);

	// 축제가 바뀌면 초기화된다. 하루 경계에서는 부르지 않는다.
	Arrears.ResetArrears();
	TestTrue(TEXT("Reset 이 미납을 지운다"), Arrears.HasNoArrears());

	return true;
}

// 상환은 예산과 잔여 미납 중 **작은 쪽**까지만이다. 양방향 모두 확인한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FixedCostArrears_RepaysLesserOfBudgetAndDebt, "FestaFiesta.Economy.FixedCostArrears.RepaysLesserOfBudgetAndDebt", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FixedCostArrears_RepaysLesserOfBudgetAndDebt::RunTest(const FString& Parameters)
{
	FFF_FixedCostArrears Arrears;
	Arrears.AddArrears(FFF_FixedCostArrearsTestHelpers::DailyRent);   // 2,400

	// 예산이 모자라면 예산만큼만.
	TestEqual(TEXT("예산까지만 상환된다"), Arrears.RepayArrears(1000), 1000);
	TestEqual(TEXT("남은 미납이 줄어든다"), Arrears.GetTotalArrears(), 1400);

	// 예산이 남아도 미납보다 많이 갚지 않는다 — 초과 상환은 없는 빚에 돈을 버리는 것이다.
	TestEqual(TEXT("미납액을 넘겨 갚지 않는다"), Arrears.RepayArrears(100'000), 1400);
	TestTrue(TEXT("전액 상환되면 비어 있다"), Arrears.HasNoArrears());

	// 미납이 없으면 상환액도 0. 호출부가 잔액에서 0 을 빼도록 해야 한다.
	TestEqual(TEXT("미납이 없으면 상환액 0"), Arrears.RepayArrears(5000), 0);

	// 예산 경계.
	Arrears.AddArrears(FFF_FixedCostArrearsTestHelpers::DailyRent);
	TestEqual(TEXT("예산 0 으로는 갚지 못한다"), Arrears.RepayArrears(0), 0);
	TestEqual(TEXT("예산이 음수여도 갚지 못한다"), Arrears.RepayArrears(-500), 0);
	TestEqual(TEXT("실패한 상환이 미납을 바꾸지 않는다"), Arrears.GetTotalArrears(),
		FFF_FixedCostArrearsTestHelpers::DailyRent);

	// 미납과 정확히 같은 예산이면 딱 비운다 (off-by-one 방지).
	TestEqual(TEXT("미납과 같은 예산은 전액 상환한다"),
		Arrears.RepayArrears(FFF_FixedCostArrearsTestHelpers::DailyRent),
		FFF_FixedCostArrearsTestHelpers::DailyRent);
	TestTrue(TEXT("정확히 비워진다"), Arrears.HasNoArrears());

	return true;
}

// 오버플로는 ensure 로 알리고 **상한에서 멈춘다.**
// 미납이 음수로 뒤집히면 "안 갚았는데 갚은 상태"가 되어 빚이 조용히 사라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FixedCostArrears_OverflowClampsInsteadOfWrapping, "FestaFiesta.Economy.FixedCostArrears.OverflowClampsInsteadOfWrapping", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FixedCostArrears_OverflowClampsInsteadOfWrapping::RunTest(const FString& Parameters)
{
	FFF_FixedCostArrears Arrears;

	// 상한 근처까지 쌓아둔다. 여기까지는 ensure 가 발동하지 않는다.
	Arrears.AddArrears(MAX_int32 - 10);
	TestEqual(TEXT("상한 근처까지는 정상 누적"), Arrears.GetTotalArrears(), MAX_int32 - 10);

	// ensureMsgf 는 배너·조건식·메시지·콜스택을 여러 줄로 나눠 찍고 실행마다 어느 줄이
	// Error 인지가 달라진다. 빈 문자열로 이 테스트의 모든 Error 를 통과시킨다.
	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);

	// 넘치는 금액을 더한다. 상한에서 멈춰야 한다.
	Arrears.AddArrears(100);

	const int32 Total = Arrears.GetTotalArrears();
	// 이 줄이 "감지하고도 그대로 더하는" 구현과 클램프 구현을 구분한다.
	// 감지 후 계속 더하면 MAX_int32 + 100 이 되어 음수가 된다.
	TestTrue(TEXT("오버플로가 미납을 음수로 만들지 않는다"), Total > 0);
	TestEqual(TEXT("오버플로는 상한에서 멈춘다"), Total, MAX_int32);
	TestFalse(TEXT("상한에 걸린 미납은 비어 있지 않다"), Arrears.HasNoArrears());

	// 상한에 걸린 뒤에도 상환은 정상 동작해야 한다.
	TestEqual(TEXT("상한 상태에서도 예산만큼 갚는다"), Arrears.RepayArrears(1000), 1000);
	TestEqual(TEXT("상환이 상한에서 정확히 빠진다"), Arrears.GetTotalArrears(), MAX_int32 - 1000);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
