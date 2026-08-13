// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/FF_FestivalFunds.h"

/**
 * FFF_FestivalFunds 검증 (S1).
 *
 * 여기서 고정하는 것은 "Gold 는 음수가 되지 않는다"를 **클램프가 아니라 거부로** 구현했다는
 * 성질이다. 클램프면 잔액이 0 이 되면서 true 를 돌려주고, 그러면 "돈이 없는데 물건은 샀다"가
 * 조용히 생긴다. 실패 시 잔액이 **그대로**인 것까지 확인해야 두 구현이 구분된다.
 *
 * 홀드(TryHold/CommitHold/ReleaseHold)는 S2 라서 여기 없다.
 */
namespace FFF_FestivalFundsTestHelpers
{
	// Unity Build 동명 헬퍼 충돌을 피하려고 파일 전용 이름을 쓴다.
	FFF_FestivalFunds MakeFundsWithGold(int32 InitialGold)
	{
		FFF_FestivalFunds Funds;
		Funds.Initialize(InitialGold);
		return Funds;
	}
}

using namespace FFF_FestivalFundsTestHelpers;

// 초기 자금이 그대로 잔액이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_InitializeSetsBalance, "FestaFiesta.Economy.FestivalFunds.InitializeSetsBalance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_InitializeSetsBalance::RunTest(const FString& Parameters)
{
	const FFF_FestivalFunds Funds = MakeFundsWithGold(10000);

	TestEqual(TEXT("초기 자금이 잔액이 된다"), Funds.GetGold(), 10000);

	// 음수 초기 자금은 0 으로 막는다 — 불변식을 이 구조체 안에서 닫는다.
	const FFF_FestivalFunds Negative = MakeFundsWithGold(-500);
	TestEqual(TEXT("음수 초기 자금은 0 이 된다"), Negative.GetGold(), 0);

	return true;
}

// 잔액이 부족하면 **클램프하지 않고 거부**한다. 잔액이 그대로여야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_RejectsInsteadOfClamping, "FestaFiesta.Economy.FestivalFunds.RejectsInsteadOfClamping", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_RejectsInsteadOfClamping::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(400);

	TestFalse(TEXT("잔액보다 큰 지출은 거부된다"), Funds.TrySpend(401));
	// 이 줄이 클램프 구현과 거부 구현을 구분한다. 클램프면 0 이 되어 있다.
	TestEqual(TEXT("거부되면 잔액이 그대로다"), Funds.GetGold(), 400);

	TestFalse(TEXT("CanAfford 도 같은 판정이다"), Funds.CanAfford(401));

	// 잔액과 정확히 같은 금액은 살 수 있어야 한다 (off-by-one 방지).
	TestTrue(TEXT("잔액과 같은 금액은 지출된다"), Funds.TrySpend(400));
	TestEqual(TEXT("전액 지출 후 잔액은 0"), Funds.GetGold(), 0);

	return true;
}

// 음수 지출로 돈을 벌 수 없어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_NegativeSpendCannotEarn, "FestaFiesta.Economy.FestivalFunds.NegativeSpendCannotEarn", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_NegativeSpendCannotEarn::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(1000);

	TestFalse(TEXT("음수 지출은 거부된다"), Funds.TrySpend(-500));
	TestEqual(TEXT("음수 지출로 잔액이 늘지 않는다"), Funds.GetGold(), 1000);
	TestFalse(TEXT("CanAfford(음수)도 false 다"), Funds.CanAfford(-1));

	return true;
}

// 비용 0 은 성공이어야 한다 — 시설 철거처럼 비용 0 인 명령이 실제로 있다.
// 0 을 거부하면 호출부가 특수 분기를 넣어야 하고, 빠뜨리면 철거가 전부 실패한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_ZeroCostSucceeds, "FestaFiesta.Economy.FestivalFunds.ZeroCostSucceeds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_ZeroCostSucceeds::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(100);

	TestTrue(TEXT("비용 0 은 CanAfford 다"), Funds.CanAfford(0));
	TestTrue(TEXT("비용 0 지출은 성공한다"), Funds.TrySpend(0));
	TestEqual(TEXT("비용 0 지출은 잔액을 바꾸지 않는다"), Funds.GetGold(), 100);

	// 잔액 0 에서도 비용 0 은 통과해야 한다.
	FFF_FestivalFunds Empty = MakeFundsWithGold(0);
	TestTrue(TEXT("잔액 0 에서도 비용 0 은 성공한다"), Empty.TrySpend(0));

	return true;
}

// 연속 지출이 정확히 누적 차감돼야 한다. 3일 수지(economy_spec.md §2.5)가 이 산술 위에 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_SequentialSpendsAccumulate, "FestaFiesta.Economy.FestivalFunds.SequentialSpendsAccumulate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_SequentialSpendsAccumulate::RunTest(const FString& Parameters)
{
	// 기본 배치 프리셋의 일부 — economy_spec.md §2.3
	FFF_FestivalFunds Funds = MakeFundsWithGold(10000);

	TestTrue(TEXT("FoodStall 400"), Funds.TrySpend(400));
	TestTrue(TEXT("MarketGoods 500"), Funds.TrySpend(500));
	TestTrue(TEXT("Toilet 700"), Funds.TrySpend(700));

	TestEqual(TEXT("잔액 = 10000 - 1600"), Funds.GetGold(), 8400);

	return true;
}

// 매출은 Gold와 TotalRevenue·TodayRevenue를 함께 늘린다. 누적 매출은 줄지 않고 쌓인다(S4).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_AddRevenueIncreasesGoldAndRevenue, "FestaFiesta.Economy.FestivalFunds.AddRevenueIncreasesGoldAndRevenue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_AddRevenueIncreasesGoldAndRevenue::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(1000);

	Funds.AddRevenue(300);
	TestEqual(TEXT("매출이 Gold를 늘린다"), Funds.GetGold(), 1300);
	TestEqual(TEXT("누적 매출에 반영된다"), Funds.GetTotalRevenue(), 300);
	TestEqual(TEXT("오늘 매출에도 반영된다"), Funds.GetTodayRevenue(), 300);

	Funds.AddRevenue(200);
	TestEqual(TEXT("누적 매출은 줄지 않고 쌓인다"), Funds.GetTotalRevenue(), 500);

	return true;
}

// 지출과 매출은 완전히 독립이다 — 지출이 매출 원장을 건드리면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_SpendDoesNotAffectRevenue, "FestaFiesta.Economy.FestivalFunds.SpendDoesNotAffectRevenue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_SpendDoesNotAffectRevenue::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(1000);
	Funds.AddRevenue(500);

	TestTrue(TEXT("지출은 성공한다"), Funds.TrySpend(400));
	TestEqual(TEXT("지출은 누적 매출을 안 건드린다"), Funds.GetTotalRevenue(), 500);
	TestEqual(TEXT("지출은 오늘 매출도 안 건드린다"), Funds.GetTodayRevenue(), 500);

	return true;
}

// 음수 매출은 ensure로 막고 상태를 전혀 바꾸지 않는다. 0은 성공한다 — "오늘 매출 0"은 정상 상태다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_NegativeRevenueDoesNotMutate, "FestaFiesta.Economy.FestivalFunds.NegativeRevenueDoesNotMutate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_NegativeRevenueDoesNotMutate::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(1000);

	// ensureMsgf 는 배너·조건식 요약·커스텀 메시지·콜스택을 여러 줄로 나눠 찍고,
	// 실행마다 그중 어느 줄이 Error/Verbose 인지가 달라진다(관찰된 실측).
	// 텍스트 하나로 못 고정하니 빈 문자열로 이 테스트의 모든 Error 를 통과시킨다.
	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);
	Funds.AddRevenue(-500);
	TestEqual(TEXT("음수 매출은 Gold를 안 바꾼다"), Funds.GetGold(), 1000);
	TestEqual(TEXT("음수 매출은 누적 매출을 안 바꾼다"), Funds.GetTotalRevenue(), 0);

	Funds.AddRevenue(0);
	TestEqual(TEXT("0 매출은 상태를 안 바꾼다"), Funds.GetGold(), 1000);
	TestEqual(TEXT("0 매출도 성공이지만 값은 그대로다"), Funds.GetTotalRevenue(), 0);

	return true;
}

// 철거 환급이 들어오는 유일한 통로다. 음수로 잔액을 깎을 수 없어야 하고,
// 상한이 없는 int32 잔액이 오버플로로 음수가 되어서도 안 된다 — 음수 잔액은 "지출 거부"와 구분되지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_RefundAddsGold, "FestaFiesta.Economy.FestivalFunds.RefundAddsGold", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_RefundAddsGold::RunTest(const FString& Parameters)
{
	FFF_FestivalFunds Funds = MakeFundsWithGold(1000);

	// 설치비 500 짜리 시설을 80% 로 철거한 금액.
	Funds.Refund(400);
	TestEqual(TEXT("환급이 잔액에 더해진다"), Funds.GetGold(), 1400);

	Funds.Refund(0);
	TestEqual(TEXT("환급 0 은 잔액을 바꾸지 않는다"), Funds.GetGold(), 1400);

	Funds.Refund(-100);
	TestEqual(TEXT("음수 환급으로 잔액이 줄지 않는다"), Funds.GetGold(), 1400);

	FFF_FestivalFunds Rich = MakeFundsWithGold(MAX_int32 - 10);
	Rich.Refund(100);
	TestEqual(TEXT("오버플로 대신 상한에서 멈춘다"), Rich.GetGold(), MAX_int32);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
