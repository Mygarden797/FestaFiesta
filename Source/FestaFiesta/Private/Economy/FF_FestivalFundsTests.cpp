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

// 초기 자금이 그대로 잔액이 되고, 가용액도 같아야 한다(S1 에는 홀드가 없다).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalFunds_InitializeSetsBalance, "FestaFiesta.Economy.FestivalFunds.InitializeSetsBalance", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalFunds_InitializeSetsBalance::RunTest(const FString& Parameters)
{
	const FFF_FestivalFunds Funds = MakeFundsWithGold(10000);

	TestEqual(TEXT("초기 자금이 잔액이 된다"), Funds.GetGold(), 10000);
	TestEqual(TEXT("S1 에서는 가용액이 잔액과 같다"), Funds.GetAvailableGold(), 10000);

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

#endif // WITH_DEV_AUTOMATION_TESTS
