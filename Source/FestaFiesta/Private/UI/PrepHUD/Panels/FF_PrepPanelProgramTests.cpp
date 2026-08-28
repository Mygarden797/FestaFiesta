// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Panels/FF_PrepPanelProgram.h"

/**
 * 프로그램 패널 편성 견적 고정 테스트 (U5).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 프로그램 탭은 **셋업과 개장 준비에서만** 열리고, "인원은 되는데 열 시설이 없다" 같은 조합은
 * 에디터에서 부스를 실제로 지웠다 세워야 재현된다. 그 왕복이 한 번에 몇 분이라 손으로는
 * 다 못 밟는다. 네 가지를 못으로 박는다 —
 *   ① 막힌 이유가 단계·시설·인원·잔액으로 구분되는 것 (AT-011 — 흐려짐만으로는 알 수 없다)
 *   ② 이유를 보는 순서 (더 근본적인 막힘을 먼저 안내한다)
 *   ③ 모르는 값을 0 으로 바꾸지 않는 것 (`0 Gold` 는 "공짜"라는 틀린 사실)
 *   ④ 편성 후 잔액·인원이 음수로 남는 것 (얼마나 모자란지가 화면에 필요하다)
 */
namespace FFF_PrepProgramRulesTestHelpers
{
	/** 첫 축제의 `선셋 요가` 기준값. 규칙 자체는 이 숫자를 모른다 — 전부 인자로 들어간다. */
	FFF_ProgramCard MakeYogaCard()
	{
		FFF_ProgramCard Card;
		Card.ProgramId = TEXT("Program_SunsetYoga");
		Card.CostGold = 250;
		Card.RequiredStaff = 2;
		Card.Capacity = 16;
		Card.FeePerPerson = 25;
		Card.StartMinutes = 90.f;
		Card.EndMinutes = 120.f;
		Card.AllowedFacilityTypes = { TEXT("Performance_ExperienceStage") };
		return Card;
	}
}

// 막힌 이유가 갈라져야 문구를 다르게 띄울 수 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepProgramRules_BlockReason, "FestaFiesta.UI.PrepProgram.BlockReason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepProgramRules_BlockReason::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepProgramRulesTestHelpers;

	TestEqual(TEXT("전부 통과하면 막지 않는다"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::Success, /*bCanScheduleNow*/ true, /*bHasCard*/ true, /*Facilities*/ 1),
		EFF_PrepProgramBlockReason::None);

	TestEqual(TEXT("축제 전에는 모름"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::FestivalNotStarted, true, true, 1),
		EFF_PrepProgramBlockReason::Unknown);

	TestEqual(TEXT("편성 단계가 아니면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::Success, /*bCanScheduleNow*/ false, true, 1),
		EFF_PrepProgramBlockReason::NotSchedulingStage);

	TestEqual(TEXT("카드를 안 골랐으면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::Success, true, /*bHasCard*/ false, 1),
		EFF_PrepProgramBlockReason::NoCardSelected);

	TestEqual(TEXT("오늘 이미 편성했으면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::AlreadyScheduled, true, true, 1),
		EFF_PrepProgramBlockReason::AlreadyScheduled);

	TestEqual(TEXT("열 시설이 없으면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::Success, true, true, /*Facilities*/ 0),
		EFF_PrepProgramBlockReason::NoEligibleFacility);

	TestEqual(TEXT("인원이 모자라면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::NotEnoughStaff, true, true, 1),
		EFF_PrepProgramBlockReason::NotEnoughStaff);

	TestEqual(TEXT("잔액이 모자라면 그 이유"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::InsufficientGold, true, true, 1),
		EFF_PrepProgramBlockReason::NotEnoughGold);

	TestEqual(TEXT("카탈로그에 없는 카드는 데이터 문제로 구분한다"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::UnknownProgram, true, true, 1),
		EFF_PrepProgramBlockReason::UnknownProgram);

	// 시설 수를 아직 못 읽은 것과 "시설이 0개" 는 다르다. 0 으로 취급하면
	// 부스가 있는데도 `부스를 먼저 지으세요` 를 띄우게 된다.
	TestEqual(TEXT("시설 수를 못 읽었으면 모름 — 0 으로 치지 않는다"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::Success, true, true, /*Facilities*/ INDEX_NONE),
		EFF_PrepProgramBlockReason::Unknown);

	return true;
}

// ★ 순서가 뒤집히면 할 수 없는 행동을 안내하게 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepProgramRules_ReasonOrder, "FestaFiesta.UI.PrepProgram.ReasonOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepProgramRules_ReasonOrder::RunTest(const FString& Parameters)
{
	// 단계가 아니면 카드별 사유를 띄우지 않는다. 홍보 프리뷰에서 `잔액 부족` 을 띄우면
	// 돈을 모으면 편성될 것처럼 읽힌다.
	TestEqual(TEXT("단계 아님이 잔액 부족보다 먼저"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::InsufficientGold, /*bCanScheduleNow*/ false, true, 1),
		EFF_PrepProgramBlockReason::NotSchedulingStage);

	// 이미 편성한 카드에 `부스를 지으세요` 는 틀린 안내다.
	TestEqual(TEXT("이미 편성이 시설 없음보다 먼저"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::AlreadyScheduled, true, true, /*Facilities*/ 0),
		EFF_PrepProgramBlockReason::AlreadyScheduled);

	// 열 장소가 없으면 알바를 고용해도 이 카드를 못 연다.
	TestEqual(TEXT("시설 없음이 인원 부족보다 먼저"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::NotEnoughStaff, true, true, /*Facilities*/ 0),
		EFF_PrepProgramBlockReason::NoEligibleFacility);

	// 소유자의 순서를 그대로 따른다 — 알바 고용이 돈을 모으는 것보다 가까운 행동이다.
	TestEqual(TEXT("인원 부족이 잔액 부족보다 먼저 (소유자 판정을 그대로 옮긴다)"),
		FFF_PrepProgramRules::ResolveBlockReason(
			EFF_ProgramPurchaseResult::NotEnoughStaff, true, true, 1),
		EFF_PrepProgramBlockReason::NotEnoughStaff);

	return true;
}

// 견적 숫자가 화면에 그대로 나간다. 모르는 값을 0 으로 바꾸면 거짓말이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepProgramRules_Quote, "FestaFiesta.UI.PrepProgram.Quote", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepProgramRules_Quote::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepProgramRulesTestHelpers;
	const FFF_ProgramCard Yoga = MakeYogaCard();

	{
		const FFF_PrepProgramQuote Quote = FFF_PrepProgramRules::BuildQuote(
			&Yoga, EFF_ProgramPurchaseResult::Success,
			/*bCanScheduleNow*/ true, /*Facilities*/ 1,
			/*Gold*/ 7600, /*AvailableStaff*/ 6);

		TestEqual(TEXT("비용은 카드 값 그대로"), Quote.CostGold, 250);
		TestEqual(TEXT("필요 인원은 카드 값 그대로"), Quote.RequiredStaff, 2);
		// 정원이 보장되므로 확정 매출이다. 16 × 25 = 400
		TestEqual(TEXT("확정 매출은 정원 × 체험비"), Quote.MaxRevenue, 400);
		TestEqual(TEXT("편성 후 잔액"), Quote.GoldAfterSchedule, 7350);
		TestEqual(TEXT("편성 후 가용 인원"), Quote.AvailableStaffAfter, 4);
		TestTrue(TEXT("막힌 이유가 없으면 누를 수 있다"), Quote.CanSchedule());
	}

	{
		// 카드를 안 골랐을 때 금액 칸이 0 이 되면 "공짜 프로그램"이 화면에 뜬다.
		const FFF_PrepProgramQuote Quote = FFF_PrepProgramRules::BuildQuote(
			nullptr, EFF_ProgramPurchaseResult::Success, true, 1, 7600, 6);

		TestEqual(TEXT("카드가 없으면 비용은 모름"), Quote.CostGold, INDEX_NONE);
		TestEqual(TEXT("카드가 없으면 인원도 모름"), Quote.RequiredStaff, INDEX_NONE);
		TestEqual(TEXT("카드가 없으면 매출도 모름"), Quote.MaxRevenue, INDEX_NONE);
		TestEqual(TEXT("카드가 없으면 편성 후 잔액도 모름"), Quote.GoldAfterSchedule, INDEX_NONE);
		TestFalse(TEXT("카드가 없으면 누를 수 없다"), Quote.CanSchedule());
	}

	{
		// 잔액·인원을 못 읽었으면 계산하지 않는다. 0 을 넣으면 "잔액 0" 이 사실이 된다.
		const FFF_PrepProgramQuote Quote = FFF_PrepProgramRules::BuildQuote(
			&Yoga, EFF_ProgramPurchaseResult::Success, true, 1,
			/*Gold*/ INDEX_NONE, /*AvailableStaff*/ INDEX_NONE);

		TestEqual(TEXT("잔액을 모르면 편성 후 잔액도 모름"), Quote.GoldAfterSchedule, INDEX_NONE);
		TestEqual(TEXT("인원을 모르면 편성 후 인원도 모름"), Quote.AvailableStaffAfter, INDEX_NONE);
		// 카드에서 오는 값은 잔액과 무관하므로 그대로 채워져야 한다.
		TestEqual(TEXT("카드 값은 잔액과 무관하게 채워진다"), Quote.CostGold, 250);
	}

	{
		// ★ 음수를 0 으로 자르지 않는다. "얼마나 모자란지" 가 화면에 필요한 정보다.
		const FFF_PrepProgramQuote Quote = FFF_PrepProgramRules::BuildQuote(
			&Yoga, EFF_ProgramPurchaseResult::InsufficientGold, true, 1,
			/*Gold*/ 100, /*AvailableStaff*/ 1);

		TestEqual(TEXT("잔액이 모자라면 음수 그대로"), Quote.GoldAfterSchedule, -150);
		TestEqual(TEXT("인원이 모자라면 음수 그대로"), Quote.AvailableStaffAfter, -1);
		TestFalse(TEXT("막혀 있으면 누를 수 없다"), Quote.CanSchedule());

		// 🔴 `-1` 은 `INDEX_NONE` 과 값이 같다. 표시부가 그걸 `모름` 으로 오인하면
		//    **모자란 바로 그 순간에만 줄이 사라진다.** 플래그로 구분되는지 못박는다.
		TestTrue(TEXT("인원이 -1 이어도 `모름` 이 아니다"), Quote.bHasAvailableStaffAfter);
		TestTrue(TEXT("잔액을 계산했으면 `모름` 이 아니다"), Quote.bHasGoldAfterSchedule);
	}

	{
		// 값을 못 읽은 경우에만 플래그가 꺼진다.
		const FFF_PrepProgramQuote Quote = FFF_PrepProgramRules::BuildQuote(
			&Yoga, EFF_ProgramPurchaseResult::Success, true, 1,
			/*Gold*/ INDEX_NONE, /*AvailableStaff*/ INDEX_NONE);

		TestFalse(TEXT("잔액을 모르면 플래그가 꺼진다"), Quote.bHasGoldAfterSchedule);
		TestFalse(TEXT("인원을 모르면 플래그가 꺼진다"), Quote.bHasAvailableStaffAfter);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
