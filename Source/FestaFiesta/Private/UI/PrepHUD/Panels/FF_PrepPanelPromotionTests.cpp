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

// 구매 완료 화면에서 지불한 홍보비를 다시 빼면 잔액이 실제 경제 값과 달라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionRules_SummaryBalance,
	"FestaFiesta.UI.PrepPromotion.SummaryBalance",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepPromotionRules_SummaryBalance::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepPromotionRulesTestHelpers;
	const FFF_PromotionOptionView Focused = MakeOption(EFF_PromotionChoice::Focused, 1400, 1.12f);
	const FFF_PrepPromotionQuote BeforePurchase = FFF_PrepPromotionRules::BuildQuote(
		&Focused, EFF_PromotionChoice::None, true, 9700);
	const TOptional<int32> BeforeBalance = FFF_PrepPromotionRules::ResolveSummaryBalance(
		BeforePurchase, EFF_PromotionChoice::None, 9700);
	TestTrue(TEXT("구매 전에는 예상 잔액이 있다"), BeforeBalance.IsSet());
	if (BeforeBalance.IsSet())
	{
		TestEqual(TEXT("구매 전 비용은 한 번 뺀다"), BeforeBalance.GetValue(), 8300);
	}

	// 재개한 패널에는 선택 카드가 없지만, 구매한 등급과 소유자의 현재 잔액은 남아 있다.
	const FFF_PrepPromotionQuote Reopened = FFF_PrepPromotionRules::BuildQuote(
		nullptr, EFF_PromotionChoice::Focused, true, 8300);
	const TOptional<int32> ReopenedBalance = FFF_PrepPromotionRules::ResolveSummaryBalance(
		Reopened, EFF_PromotionChoice::Focused, 8300);
	TestTrue(TEXT("다시 연 구매 완료 화면도 잔액을 표시한다"), ReopenedBalance.IsSet());
	if (ReopenedBalance.IsSet())
	{
		TestEqual(TEXT("구매 완료 뒤 1400을 다시 차감하지 않는다"), ReopenedBalance.GetValue(), 8300);
	}

	// 다른 지출 결과가 오면 현재 잔액을 따라야 한다. 이전 구매 견적을 재사용하지 않는다.
	const TOptional<int32> LaterBalance = FFF_PrepPromotionRules::ResolveSummaryBalance(
		BeforePurchase, EFF_PromotionChoice::Focused, 7200);
	TestTrue(TEXT("후속 결과의 잔액이 있다"), LaterBalance.IsSet());
	if (LaterBalance.IsSet())
	{
		TestEqual(TEXT("구매 후 다른 지출도 현재 잔액을 반영한다"), LaterBalance.GetValue(), 7200);
	}
	TestFalse(TEXT("소유자의 잔액을 모르면 과거 견적으로 대신하지 않는다"),
		FFF_PrepPromotionRules::ResolveSummaryBalance(
			BeforePurchase, EFF_PromotionChoice::Focused, INDEX_NONE).IsSet());

	const FFF_PrepPromotionQuote OneShort = FFF_PrepPromotionRules::BuildQuote(
		&Focused, EFF_PromotionChoice::None, true, 1399);
	const TOptional<int32> OneShortBalance = FFF_PrepPromotionRules::ResolveSummaryBalance(
		OneShort, EFF_PromotionChoice::None, 1399);
	TestTrue(TEXT("-1 예상 잔액은 미확인 값이 아니다"), OneShortBalance.IsSet());
	if (OneShortBalance.IsSet())
	{
		TestEqual(TEXT("1 Gold 부족 표시를 유지한다"), OneShortBalance.GetValue(), -1);
	}
	const FFF_PrepPromotionQuote Unselected = FFF_PrepPromotionRules::BuildQuote(
		nullptr, EFF_PromotionChoice::None, true, 9700);
	TestFalse(TEXT("미선택 비용이 없으면 구매 후 잔액도 단정하지 않는다"),
		FFF_PrepPromotionRules::ResolveSummaryBalance(
			Unselected, EFF_PromotionChoice::None, 9700).IsSet());
	return true;
}

// 백틱은 FText에서 중괄호를 이스케이프한다. 실제 확인창 제목에 치환자가 노출된 회귀를 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepPromotionRules_ConfirmTitle,
	"FestaFiesta.UI.PrepPromotion.ConfirmTitle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepPromotionRules_ConfirmTitle::RunTest(const FString& Parameters)
{
	const UFF_PrepPanelPromotion* Panel = GetDefault<UFF_PrepPanelPromotion>();
	const struct { EFF_PromotionChoice Choice; const TCHAR* Label; } Cases[] =
	{
		{ EFF_PromotionChoice::Basic, TEXT("지역 SNS") },
		{ EFF_PromotionChoice::Focused, TEXT("전국 홍보") },
		{ EFF_PromotionChoice::Influencer, TEXT("인플루언서") }
	};
	for (const auto& One : Cases)
	{
		const FString Title = Panel->GetPurchaseConfirmationTitle(One.Choice).ToString();
		TestTrue(FString::Printf(TEXT("%s 확인창에 실제 등급명을 표시한다"), One.Label),
			Title.Contains(One.Label));
		TestFalse(TEXT("확인창에 포맷 치환자가 남지 않는다"), Title.Contains(TEXT("{0}")));
		TestFalse(TEXT("확인창에 이스케이프 백틱이 노출되지 않는다"), Title.Contains(TEXT("`")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
