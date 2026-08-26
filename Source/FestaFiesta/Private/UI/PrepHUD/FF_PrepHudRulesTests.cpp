// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/FF_PrepHudTypes.h"

/**
 * 준비 단계 HUD 판단 규칙 고정 테스트 (U4).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 여기 규칙이 틀리면 **에디터에서 그 단계·그 날짜에 들어가 봐야만** 보인다.
 * "Day 3 개장 준비에서 피날레 탭이 안 뜬다" 같은 것은 한 판을 끝까지 돌려야 재현된다.
 * 세 가지를 못으로 박는다 —
 *   ① 탭 표시 조건 (홍보=셋업 전용 · 피날레=마지막 날 개장 준비 전용)
 *   ② 단계가 바뀌었을 때 열린 패널이 남지 않는 것
 *   ③ CTA 가 막힐 때 이유가 구분되는 것 (AT-011 — 흐려짐만으로 알 수 없다)
 */
namespace FFF_PrepHudRulesTestHelpers
{
	/**
	 * `UFF_FestivalViewModelSubsystem::IsFinalDay()` 가 돌려주는 값. 규칙은 총 일수를 모른다.
	 * 이름을 `true`/`false` 대신 상수로 둔 것은 호출부에서 무엇을 뜻하는지 보이게 하기 위함이다.
	 */
	constexpr bool bFinalDay = true;
	constexpr bool bNotFinalDay = false;

	inline const TCHAR* DescribeStage(EFF_FestivalStage Stage)
	{
		switch (Stage)
		{
		case EFF_FestivalStage::Setup:				return TEXT("셋업");
		case EFF_FestivalStage::PromotionPreview:	return TEXT("홍보");
		case EFF_FestivalStage::SunsetTransition:	return TEXT("노을 전환");
		case EFF_FestivalStage::MorningCheck:		return TEXT("개장 준비");
		case EFF_FestivalStage::Live:				return TEXT("라이브");
		case EFF_FestivalStage::Closing:			return TEXT("마감");
		case EFF_FestivalStage::Report:				return TEXT("결산");
		case EFF_FestivalStage::Finale:				return TEXT("피날레");
		default:									return TEXT("알 수 없음");
		}
	}
}

// 준비 단계는 셋 뿐이다. 노을 전환이 섞이면 조작할 것 없는 화면에 바가 뜬다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_PrepStages, "FestaFiesta.UI.PrepHud.PrepStages", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_PrepStages::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("셋업은 준비 단계"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::Setup));
	TestTrue(TEXT("홍보는 준비 단계"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::PromotionPreview));
	TestTrue(TEXT("개장 준비는 준비 단계"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::MorningCheck));

	TestFalse(TEXT("노을 전환은 준비 단계가 아니다"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::SunsetTransition));
	TestFalse(TEXT("라이브는 준비 단계가 아니다"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::Live));
	TestFalse(TEXT("마감은 준비 단계가 아니다"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::Closing));
	TestFalse(TEXT("결산은 준비 단계가 아니다"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::Report));
	TestFalse(TEXT("피날레는 준비 단계가 아니다"), FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage::Finale));
	return true;
}

// 홍보 탭은 셋업에서만. 개장 준비에서 또 보이면 이미 산 홍보를 다시 파는 화면이 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_PromotionTabSetupOnly, "FestaFiesta.UI.PrepHud.PromotionTabSetupOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_PromotionTabSetupOnly::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepHudRulesTestHelpers;

	TestTrue(TEXT("셋업에서 홍보 탭이 보인다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Promotion, EFF_FestivalStage::Setup, bNotFinalDay));

	TestFalse(TEXT("홍보 단계에서는 홍보 탭이 없다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Promotion, EFF_FestivalStage::PromotionPreview, bNotFinalDay));

	TestFalse(TEXT("개장 준비에서는 홍보 탭이 없다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Promotion, EFF_FestivalStage::MorningCheck, bNotFinalDay));

	// 배치·프로그램·인력·점검은 준비 단계 세 곳 모두에서 보인다.
	const EFF_PrepHudTab AlwaysTabs[] = {
		EFF_PrepHudTab::Build, EFF_PrepHudTab::Program, EFF_PrepHudTab::Staff, EFF_PrepHudTab::Check };
	const EFF_FestivalStage PrepStages[] = {
		EFF_FestivalStage::Setup, EFF_FestivalStage::PromotionPreview, EFF_FestivalStage::MorningCheck };

	for (const EFF_PrepHudTab Tab : AlwaysTabs)
	{
		for (const EFF_FestivalStage Stage : PrepStages)
		{
			TestTrue(*FString::Printf(TEXT("%s 에서 상시 탭이 보인다"), DescribeStage(Stage)),
				FFF_PrepHudRules::IsTabVisible(Tab, Stage, bNotFinalDay));
		}
	}
	return true;
}

// 피날레 탭은 마지막 날 개장 준비에서만. 1회 구매·환불 불가라 그 전에 열면 오해를 만든다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_FinaleTabFinalDayOnly, "FestaFiesta.UI.PrepHud.FinaleTabFinalDayOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_FinaleTabFinalDayOnly::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepHudRulesTestHelpers;

	TestTrue(TEXT("마지막 날 개장 준비에서 피날레 탭이 보인다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Finale, EFF_FestivalStage::MorningCheck, bFinalDay));

	TestFalse(TEXT("마지막 날이 아닌 개장 준비에는 없다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Finale, EFF_FestivalStage::MorningCheck, bNotFinalDay));

	TestFalse(TEXT("마지막 날이어도 셋업에는 없다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Finale, EFF_FestivalStage::Setup, bFinalDay));

	// 총 일수를 UI 가 박지 않는다. 2일짜리 축제면 Day2 가 마지막이고 그때도 이 판정만 본다.
	TestFalse(TEXT("라이브에서는 마지막 날이어도 없다"),
		FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab::Finale, EFF_FestivalStage::Live, bFinalDay));
	return true;
}

// 같은 탭 재클릭은 닫기, 다른 탭 클릭은 교체. 패널이 두 개 열리는 경로가 없어야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_TabExclusivity, "FestaFiesta.UI.PrepHud.TabExclusivity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_TabExclusivity::RunTest(const FString& Parameters)
{
	const FFF_PrepTabSelection Nothing = FFF_PrepTabSelection::None();

	const FFF_PrepTabSelection AfterBuild = FFF_PrepHudRules::ToggleTab(Nothing, EFF_PrepHudTab::Build);
	TestTrue(TEXT("닫힌 상태에서 배치를 누르면 배치가 열린다"), AfterBuild.IsSelected(EFF_PrepHudTab::Build));

	const FFF_PrepTabSelection AfterProgram = FFF_PrepHudRules::ToggleTab(AfterBuild, EFF_PrepHudTab::Program);
	TestTrue(TEXT("프로그램을 누르면 프로그램이 열린다"), AfterProgram.IsSelected(EFF_PrepHudTab::Program));
	TestFalse(TEXT("배치는 함께 닫힌다"), AfterProgram.IsSelected(EFF_PrepHudTab::Build));

	const FFF_PrepTabSelection AfterSameAgain = FFF_PrepHudRules::ToggleTab(AfterProgram, EFF_PrepHudTab::Program);
	TestFalse(TEXT("같은 탭을 다시 누르면 닫힌다"), AfterSameAgain.bHasSelection);

	// 선택이 없을 때는 Tab 값이 비교에 끼어들면 안 된다.
	FFF_PrepTabSelection StaleTab;
	StaleTab.bHasSelection = false;
	StaleTab.Tab = EFF_PrepHudTab::Check;
	TestTrue(TEXT("선택 없음끼리는 Tab 값과 무관하게 같다"), StaleTab == Nothing);
	return true;
}

// 단계가 바뀌어 탭이 사라지면 패널도 닫힌다. 안 그러면 닫을 수단 없는 패널이 남는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_SelectionFollowsStage, "FestaFiesta.UI.PrepHud.SelectionFollowsStage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_SelectionFollowsStage::RunTest(const FString& Parameters)
{
	using namespace FFF_PrepHudRulesTestHelpers;

	const FFF_PrepTabSelection PromotionOpen = FFF_PrepTabSelection::Of(EFF_PrepHudTab::Promotion);

	TestTrue(TEXT("셋업에서는 홍보 패널이 유지된다"),
		FFF_PrepHudRules::ClampSelectionToVisible(PromotionOpen, EFF_FestivalStage::Setup, bNotFinalDay)
			.IsSelected(EFF_PrepHudTab::Promotion));

	TestFalse(TEXT("홍보 단계로 넘어가면 홍보 패널이 닫힌다"),
		FFF_PrepHudRules::ClampSelectionToVisible(PromotionOpen, EFF_FestivalStage::PromotionPreview, bNotFinalDay)
			.bHasSelection);

	const FFF_PrepTabSelection FinaleOpen = FFF_PrepTabSelection::Of(EFF_PrepHudTab::Finale);
	TestFalse(TEXT("마지막 날 개장 준비를 벗어나면 피날레 패널이 닫힌다"),
		FFF_PrepHudRules::ClampSelectionToVisible(FinaleOpen, EFF_FestivalStage::Live, bFinalDay)
			.bHasSelection);

	const FFF_PrepTabSelection CheckOpen = FFF_PrepTabSelection::Of(EFF_PrepHudTab::Check);
	TestTrue(TEXT("상시 탭은 준비 단계 사이에서 유지된다"),
		FFF_PrepHudRules::ClampSelectionToVisible(CheckOpen, EFF_FestivalStage::MorningCheck, bNotFinalDay)
			.IsSelected(EFF_PrepHudTab::Check));
	return true;
}

// 막히는 이유가 구분돼야 문구를 다르게 띄울 수 있다 (AT-011).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PrepHudRules_CtaState, "FestaFiesta.UI.PrepHud.CtaState", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PrepHudRules_CtaState::RunTest(const FString& Parameters)
{
	// 문구가 배선된 준비 단계 + 시설 있음 + 단계 넘길 수 있음 = 누를 수 있다.
	TestEqual(TEXT("정상 상태는 Ready"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::Setup, true, true, 4, true),
		EFF_PrepCtaState::Ready);

	TestEqual(TEXT("문구가 없으면 버튼째 숨긴다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::PromotionPreview, false, true, 4, true),
		EFF_PrepCtaState::Hidden);

	TestEqual(TEXT("준비 단계가 아니면 숨긴다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::Live, true, true, 4, true),
		EFF_PrepCtaState::Hidden);

	TestEqual(TEXT("시설이 0개면 그 이유로 막는다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::MorningCheck, true, true, 0, true),
		EFF_PrepCtaState::BlockedNoFacility);

	TestEqual(TEXT("시설 0개라도 요구하지 않으면 막지 않는다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::MorningCheck, true, true, 0, false),
		EFF_PrepCtaState::Ready);

	// 아직 못 읽은 것을 0개로 취급하면 시설을 놓고도 시작할 수 없는 화면이 된다.
	TestEqual(TEXT("시설 수를 모르면 막지 않는다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::MorningCheck, true, true, INDEX_NONE, true),
		EFF_PrepCtaState::Ready);

	TestEqual(TEXT("단계를 넘길 수 없으면 그 이유로 막는다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::Setup, true, false, 4, true),
		EFF_PrepCtaState::BlockedStageLocked);

	// 둘 다 막혔을 때는 플레이어가 할 수 있는 일(시설 배치)을 먼저 안내한다.
	TestEqual(TEXT("시설 없음이 단계 잠김보다 먼저다"),
		FFF_PrepHudRules::ResolveCtaState(EFF_FestivalStage::Setup, true, false, 0, true),
		EFF_PrepCtaState::BlockedNoFacility);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
