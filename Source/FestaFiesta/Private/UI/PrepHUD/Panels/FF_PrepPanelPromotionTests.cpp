// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Panels/FF_PrepPanelPromotion.h"

/**
 * 홍보 패널 구매 견적 고정 테스트 (U5).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 홍보 탭은 **셋업에서 한 번만** 열린다. 축제를 새로 시작하지 않으면 다시 못 보는 화면이라
 * "이미 샀는데 셋업이 지났다" 같은 조합은 손으로 밟기가 사실상 불가능하다.
 * 세 가지를 못으로 박는다 —
 *   ① 막힌 이유가 이미 구매·단계·잔액으로 구분되는 것 (AT-011)
 *   ② `이미 구매` 가 `셋업 아님` 보다 먼저인 순서 (산 사람에게 못 산 것처럼 말하지 않기)
 *   ③ `안 함` 의 0 Gold 와 `모름` 의 `INDEX_NONE` 을 섞지 않는 것
 */
namespace FFF_PrepPromotionRulesTestHelpers
{
	/** 첫 축제 기준값. 규칙 자체는 이 숫자를 모른다 — 전부 인자로 들어간다. */
	FFF_PromotionOptionView MakeOption(EFF_PromotionChoice Choice, int32 CostGold, float Multiplier)
	{
		FFF_PromotionOptionView Option;
		Option.Choice = Choice;
		Option.CostGold = CostGold;
		Option.VisitorMultiplier = Multiplier;
		return Option;
	}
}

// 막힌 이유가 갈라져야 문구를 다르게 띄울 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionRules_BlockReason, "FestaFiesta.UI.PrepPromotion.BlockReason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepPromotionRules_BlockReason::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("셋업에서 등급을 고르고 돈이 있으면 살 수 있다"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, EFF_PromotionChoice::None,
			/*bCanPurchaseNow*/ true, /*Cost*/ 500, /*Gold*/ 7600),
		EFF_PrepPromotionBlockReason::None);

	TestEqual(TEXT("이미 샀으면 그 이유"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Focused, EFF_PromotionChoice::Basic, true, 1200, 7600),
		EFF_PrepPromotionBlockReason::AlreadyPurchased);

	TestEqual(TEXT("셋업이 아니면 그 이유"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, EFF_PromotionChoice::None,
			/*bCanPurchaseNow*/ false, 500, 7600),
		EFF_PrepPromotionBlockReason::NotSetupStage);

	// `안 함` 은 사는 것이 아니라 안 사는 것이다. 0 Gold 라고 통과시키면 안 된다.
	TestEqual(TEXT("`안 함` 은 구매가 아니다"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::None, EFF_PromotionChoice::None, true, /*Cost*/ 0, 7600),
		EFF_PrepPromotionBlockReason::NoChoiceSelected);

	TestEqual(TEXT("잔액이 모자라면 그 이유"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Focused, EFF_PromotionChoice::None, true, /*Cost*/ 1200, /*Gold*/ 400),
		EFF_PrepPromotionBlockReason::NotEnoughGold);

	TestEqual(TEXT("비용을 못 읽었으면 모름 — 살 수 있다고 하지 않는다"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, EFF_PromotionChoice::None, true, /*Cost*/ INDEX_NONE, 7600),
		EFF_PrepPromotionBlockReason::Unknown);

	// 잔액을 모르는 것과 모자란 것은 다르다. 모르면 막지 않고 `TrySpend` 에 맡긴다.
	TestEqual(TEXT("잔액을 모르면 막지 않는다"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, EFF_PromotionChoice::None, true, 500, /*Gold*/ INDEX_NONE),
		EFF_PrepPromotionBlockReason::None);

	// 딱 맞는 잔액은 살 수 있어야 한다. `<=` 로 쓰면 여기서 걸린다.
	TestEqual(TEXT("잔액이 비용과 같으면 살 수 있다"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, EFF_PromotionChoice::None, true, /*Cost*/ 500, /*Gold*/ 500),
		EFF_PrepPromotionBlockReason::None);

	return true;
}

// ★ 순서가 뒤집히면 산 사람에게 못 산 것처럼 말하게 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionRules_ReasonOrder, "FestaFiesta.UI.PrepPromotion.ReasonOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepPromotionRules_ReasonOrder::RunTest(const FString& Parameters)
{
	// 셋업이 지난 뒤 홍보 패널을 보면 대개 이 상태다 — 이미 샀고 단계도 지났다.
	TestEqual(TEXT("이미 구매가 셋업 아님보다 먼저"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Basic, /*Purchased*/ EFF_PromotionChoice::Basic,
			/*bCanPurchaseNow*/ false, 500, 7600),
		EFF_PrepPromotionBlockReason::AlreadyPurchased);

	// 돈이 없어도 이미 샀으면 `잔액 부족` 은 틀린 안내다.
	TestEqual(TEXT("이미 구매가 잔액 부족보다 먼저"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::Focused, EFF_PromotionChoice::Basic, true, /*Cost*/ 1200, /*Gold*/ 0),
		EFF_PrepPromotionBlockReason::AlreadyPurchased);

	// 단계가 지났으면 등급을 안 골랐다는 안내는 의미가 없다.
	TestEqual(TEXT("셋업 아님이 등급 미선택보다 먼저"),
		FFF_PrepPromotionRules::ResolveBlockReason(
			EFF_PromotionChoice::None, EFF_PromotionChoice::None, /*bCanPurchaseNow*/ false, 0, 7600),
		EFF_PrepPromotionBlockReason::NotSetupStage);

	return true;
}

// 견적 숫자가 화면에 그대로 나간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionRules_Quote, "FestaFiesta.UI.PrepPromotion.Quote", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepPromotionRules_Quote::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepPromotionRulesTestHelpers;

	{
		const FFF_PromotionOptionView Focused = MakeOption(EFF_PromotionChoice::Focused, 1200, 1.18f);
		const FFF_PrepPromotionQuote Quote = FFF_PrepPromotionRules::BuildQuote(
			&Focused, EFF_PromotionChoice::None, /*bCanPurchaseNow*/ true, /*Gold*/ 7600);

		TestEqual(TEXT("비용은 데이터 그대로"), Quote.CostGold, 1200);
		TestEqual(TEXT("구매 후 잔액"), Quote.GoldAfterPurchase, 6400);
		TestTrue(TEXT("배수는 데이터 그대로"), FMath::IsNearlyEqual(Quote.VisitorMultiplier, 1.18f));
		TestTrue(TEXT("막힌 이유가 없으면 살 수 있다"), Quote.CanPurchase());
	}

	{
		// 🔴 `안 함` 의 0 Gold 는 **확정값**이다. `모름`(INDEX_NONE)과 섞으면
		//    화면이 `아직 알 수 없습니다` 를 띄우게 된다.
		const FFF_PromotionOptionView NoneOption = MakeOption(EFF_PromotionChoice::None, 0, 1.f);
		const FFF_PrepPromotionQuote Quote = FFF_PrepPromotionRules::BuildQuote(
			&NoneOption, EFF_PromotionChoice::None, true, 7600);

		TestEqual(TEXT("`안 함` 의 비용은 0 이고 모름이 아니다"), Quote.CostGold, 0);
		TestEqual(TEXT("`안 함` 은 잔액이 그대로다"), Quote.GoldAfterPurchase, 7600);
		TestFalse(TEXT("`안 함` 은 살 수 없다"), Quote.CanPurchase());
	}

	{
		// 아무것도 안 고른 상태. `안 함` 을 고른 것과 달리 비용을 모른다.
		const FFF_PrepPromotionQuote Quote = FFF_PrepPromotionRules::BuildQuote(
			nullptr, EFF_PromotionChoice::None, true, 7600);

		TestEqual(TEXT("안 골랐으면 비용은 모름"), Quote.CostGold, INDEX_NONE);
		TestEqual(TEXT("안 골랐으면 구매 후 잔액도 모름"), Quote.GoldAfterPurchase, INDEX_NONE);
		TestFalse(TEXT("안 골랐으면 살 수 없다"), Quote.CanPurchase());
	}

	{
		// 모자라는 금액을 음수로 남긴다. 0 으로 자르면 얼마나 부족한지 못 보여준다.
		const FFF_PromotionOptionView Focused = MakeOption(EFF_PromotionChoice::Focused, 1200, 1.18f);
		const FFF_PrepPromotionQuote Quote = FFF_PrepPromotionRules::BuildQuote(
			&Focused, EFF_PromotionChoice::None, true, /*Gold*/ 400);

		TestEqual(TEXT("잔액이 모자라면 음수 그대로"), Quote.GoldAfterPurchase, -800);
		TestEqual(TEXT("사유는 잔액 부족"), Quote.BlockReason, EFF_PrepPromotionBlockReason::NotEnoughGold);
		TestTrue(TEXT("음수여도 `모름` 이 아니다"), Quote.bHasGoldAfterPurchase);
	}

	{
		// 🔴 정확히 1 Gold 모자란 경우. 결과가 -1 이라 `INDEX_NONE` 과 값이 같다 —
		//    표시부가 값으로 판정하면 **이 한 지점에서만 줄이 사라진다.**
		const FFF_PromotionOptionView Basic = MakeOption(EFF_PromotionChoice::Basic, 500, 1.08f);
		const FFF_PrepPromotionQuote Quote = FFF_PrepPromotionRules::BuildQuote(
			&Basic, EFF_PromotionChoice::None, true, /*Gold*/ 499);

		TestEqual(TEXT("1 Gold 모자라면 -1"), Quote.GoldAfterPurchase, -1);
		TestTrue(TEXT("-1 이어도 `모름` 이 아니다"), Quote.bHasGoldAfterPurchase);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
