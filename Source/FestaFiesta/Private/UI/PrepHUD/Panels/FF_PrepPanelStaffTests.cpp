// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Panels/FF_PrepPanelStaff.h"

/**
 * 인력 패널 고용 견적 고정 테스트 (U5).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * "상한은 남았는데 돈이 없다" 같은 조합은 **에디터에서 그 잔액을 실제로 만들어야만** 재현된다.
 * 잔액을 7,600 에서 특정 값까지 깎아야 하는데, 그러려면 시설을 사고 프로그램을 사야 한다.
 * 네 가지를 못으로 박는다 —
 *   ① 막힌 이유가 `상한`·`잔액`·`모름` 으로 구분되는 것 (AT-011 — 흐려짐만으로는 알 수 없다)
 *   ② 상한을 잔액보다 먼저 보는 순서 (돈을 모아도 안 되는 행동을 안내하지 않기)
 *   ③ 모르는 값을 0 으로 바꾸지 않는 것 (`0 Gold` 는 "공짜"라는 틀린 사실)
 *   ④ 원하는 인원이 상한에 잘리는 것
 */
namespace FFF_PrepStaffRulesTestHelpers
{
	/** 첫 축제 기준값. 규칙 자체는 이 숫자를 모른다 — 전부 인자로 들어간다. */
	constexpr int32 HireCostPerStaff = 450;
	constexpr int32 MaxHiredStaff = 6;
}

// 막힌 이유가 셋으로 구분돼야 문구를 다르게 띄울 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepStaffRules_BlockReason, "FestaFiesta.UI.PrepStaff.BlockReason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepStaffRules_BlockReason::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("여유가 있으면 막지 않는다"),
		FFF_PrepStaffRules::ResolveBlockReason(/*Affordable*/ 3, /*RemainingCap*/ 6),
		EFF_PrepHireBlockReason::None);

	TestEqual(TEXT("상한을 다 쓰면 상한 이유"),
		FFF_PrepStaffRules::ResolveBlockReason(0, 0),
		EFF_PrepHireBlockReason::CapReached);

	TestEqual(TEXT("상한은 남았지만 돈이 없으면 잔액 이유"),
		FFF_PrepStaffRules::ResolveBlockReason(0, 4),
		EFF_PrepHireBlockReason::NotEnoughGold);

	// ★ 순서가 뒤집히면 "돈을 모으세요"를 띄우는데 돈을 모아도 안 되는 상태가 된다.
	TestEqual(TEXT("상한 도달이 잔액 부족보다 먼저다"),
		FFF_PrepStaffRules::ResolveBlockReason(/*Affordable*/ 0, /*RemainingCap*/ 0),
		EFF_PrepHireBlockReason::CapReached);

	TestEqual(TEXT("값을 못 읽었으면 모름 — 잔액"),
		FFF_PrepStaffRules::ResolveBlockReason(INDEX_NONE, 6),
		EFF_PrepHireBlockReason::Unknown);

	TestEqual(TEXT("값을 못 읽었으면 모름 — 상한"),
		FFF_PrepStaffRules::ResolveBlockReason(3, INDEX_NONE),
		EFF_PrepHireBlockReason::Unknown);
	return true;
}

// 원하는 인원은 고용 가능 인원에 잘린다. 화면 숫자와 실제 요청이 갈라지면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepStaffRules_QuoteClamps, "FestaFiesta.UI.PrepStaff.QuoteClamps", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepStaffRules_QuoteClamps::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepStaffRulesTestHelpers;

	// 잔액 7,600 · 상한 6 → 고용비 450 이면 돈으로는 16명, 상한 때문에 6명.
	// `Affordable` 은 소유자가 이미 min 을 취해 주므로 6 이 들어온다.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			/*Desired*/ 3, /*Affordable*/ 6, /*RemainingCap*/ 6, HireCostPerStaff, /*Gold*/ 7600);

		TestEqual(TEXT("최대 6명"), Quote.MaxHireable, 6);
		TestEqual(TEXT("고른 3명 그대로"), Quote.SelectedCount, 3);
		TestEqual(TEXT("고용비 1,350"), Quote.HireCostTotal, 1350);
		TestEqual(TEXT("고용 후 잔액 6,250"), Quote.GoldAfterHire, 6250);
		TestTrue(TEXT("고용할 수 있다"), Quote.CanHire());
	}

	// 상한을 넘겨 요청하면 잘린다.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			/*Desired*/ 99, /*Affordable*/ 2, /*RemainingCap*/ 6, HireCostPerStaff, 7600);

		TestEqual(TEXT("고용 가능 인원까지만"), Quote.SelectedCount, 2);
		TestEqual(TEXT("고용비도 잘린 인원 기준"), Quote.HireCostTotal, 900);
	}

	// 음수 요청은 0 이 된다.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			/*Desired*/ -5, /*Affordable*/ 6, /*RemainingCap*/ 6, HireCostPerStaff, 7600);

		TestEqual(TEXT("음수는 0"), Quote.SelectedCount, 0);
		TestEqual(TEXT("0명이면 고용비 0"), Quote.HireCostTotal, 0);
		TestFalse(TEXT("0명은 고용 버튼이 꺼진다"), Quote.CanHire());
	}

	// 상한을 다 쓴 상태 — 인원 0, 이유는 상한.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			/*Desired*/ 3, /*Affordable*/ 0, /*RemainingCap*/ 0, HireCostPerStaff, 7600);

		TestEqual(TEXT("고를 수 있는 인원이 없다"), Quote.MaxHireable, 0);
		TestEqual(TEXT("상한 이유"), Quote.BlockReason, EFF_PrepHireBlockReason::CapReached);
		TestFalse(TEXT("고용 불가"), Quote.CanHire());
	}
	return true;
}

// 모르는 값을 0 으로 바꾸면 `공짜`·`고용 가능`이라는 틀린 사실이 화면에 나간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepStaffRules_UnknownStaysUnknown, "FestaFiesta.UI.PrepStaff.UnknownStaysUnknown", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepStaffRules_UnknownStaysUnknown::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepStaffRulesTestHelpers;

	// 축제 시작 전 — 전부 모름.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			3, INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE);

		TestEqual(TEXT("모름"), Quote.BlockReason, EFF_PrepHireBlockReason::Unknown);
		TestEqual(TEXT("인원을 만들지 않는다"), Quote.SelectedCount, 0);
		TestEqual(TEXT("고용비 칸은 비어 있다"), Quote.HireCostTotal, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("잔액 칸도 비어 있다"), Quote.GoldAfterHire, static_cast<int32>(INDEX_NONE));
		TestFalse(TEXT("고용 불가"), Quote.CanHire());
	}

	// 인원은 알지만 단가를 모르는 경우 — 인원은 유효하고 금액만 비운다.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			2, /*Affordable*/ 4, /*RemainingCap*/ 6, /*HireCost*/ INDEX_NONE, /*Gold*/ 7600);

		TestEqual(TEXT("인원은 그대로"), Quote.SelectedCount, 2);
		TestEqual(TEXT("고용비는 비운다"), Quote.HireCostTotal, static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("고용 후 잔액도 비운다"), Quote.GoldAfterHire, static_cast<int32>(INDEX_NONE));
	}

	// 단가는 알지만 잔액을 모르는 경우 — 고용비는 계산하고 잔액만 비운다.
	{
		const FFF_PrepHireQuote Quote = FFF_PrepStaffRules::BuildQuote(
			2, 4, MaxHiredStaff, HireCostPerStaff, /*Gold*/ INDEX_NONE);

		TestEqual(TEXT("고용비는 계산된다"), Quote.HireCostTotal, 900);
		TestEqual(TEXT("고용 후 잔액은 비운다"), Quote.GoldAfterHire, static_cast<int32>(INDEX_NONE));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
