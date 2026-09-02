// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/ClosingHUD/FF_ClosingHudTypes.h"

/**
 * 결산 HUD 판단 규칙 고정 테스트 (U7).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 여기 오려면 **하루를 통째로 돌려야 한다.** 3일차 조합(피날레를 산 경우와 안 산 경우,
 * 마지막 날 CTA)은 축제를 세 번 밟아야 나온다. 셋을 못으로 박는다 —
 *   ① 마감과 리포트가 같은 화면이고, 피날레 연출 구간은 아닌 것
 *   ② 피날레를 안 샀으면 탭이 숨는 것 — 빈 패널을 보여 주지 않는다
 *   ③ CTA 판정 순서 — `마지막 날` 이 `리포트 단계 아님` 을 가리지 않는 것
 */

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ClosingHudRules_Stage, "FestaFiesta.UI.ClosingHud.Stage", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ClosingHudRules_Stage::RunTest(const FString& Parameters)
{
	// ① 마감과 리포트는 같은 화면이다.
	TestTrue(TEXT("마감에서 뜬다"),
		FFF_ClosingHudRules::IsClosingHudStage(EFF_FestivalStage::Closing));
	TestTrue(TEXT("리포트에서도 뜬다"),
		FFF_ClosingHudRules::IsClosingHudStage(EFF_FestivalStage::Report));

	// ★ 피날레도 이 바를 쓴다 (2026-09-02 변경). 다만 **탭은 하나도 안 띄운다** —
	//   불꽃이 터지는 연출 구간이라 결산 표로 덮으면 연출을 가린다.
	TestTrue(TEXT("피날레에서도 바는 뜬다"),
		FFF_ClosingHudRules::IsClosingHudStage(EFF_FestivalStage::Finale));

	for (const EFF_ClosingTab Tab : { EFF_ClosingTab::Report, EFF_ClosingTab::Finale })
	{
		TestFalse(TEXT("피날레 연출 구간에는 탭이 하나도 없다"),
			FFF_ClosingHudRules::IsTabVisible(Tab, EFF_FestivalStage::Finale, /*Purchased*/ true));
	}

	for (const EFF_FestivalStage Stage : {
		EFF_FestivalStage::Setup,
		EFF_FestivalStage::PromotionPreview,
		EFF_FestivalStage::SunsetTransition,
		EFF_FestivalStage::MorningCheck,
		EFF_FestivalStage::Live })
	{
		TestFalse(FString::Printf(TEXT("낮 단계에서는 안 뜬다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_ClosingHudRules::IsClosingHudStage(Stage));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ClosingHudRules_Tabs, "FestaFiesta.UI.ClosingHud.Tabs", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ClosingHudRules_Tabs::RunTest(const FString& Parameters)
{
	const EFF_FestivalStage Closing = EFF_FestivalStage::Closing;

	// 리포트는 언제나 보인다.
	TestTrue(TEXT("리포트 탭은 항상 보인다"),
		FFF_ClosingHudRules::IsTabVisible(EFF_ClosingTab::Report, Closing, /*Purchased*/ false));

	// ② 🔴 안 샀으면 피날레 탭을 숨긴다. 빈 패널은 "고장났나" 로 읽히고,
	//    살 수 있는 곳(셋업)은 이미 지나간 뒤다.
	TestFalse(TEXT("안 샀으면 피날레 탭이 숨는다"),
		FFF_ClosingHudRules::IsTabVisible(EFF_ClosingTab::Finale, Closing, /*Purchased*/ false));
	TestTrue(TEXT("샀으면 보인다"),
		FFF_ClosingHudRules::IsTabVisible(EFF_ClosingTab::Finale, Closing, /*Purchased*/ true));

	// 단계가 아니면 산 것과 무관하게 안 보인다.
	TestFalse(TEXT("라이브에서는 샀어도 안 보인다"),
		FFF_ClosingHudRules::IsTabVisible(EFF_ClosingTab::Finale, EFF_FestivalStage::Live, true));

	// --- 토글 ---
	const FFF_ClosingTabSelection None = FFF_ClosingTabSelection::None();
	const FFF_ClosingTabSelection Opened = FFF_ClosingHudRules::ToggleTab(None, EFF_ClosingTab::Report);

	TestTrue(TEXT("닫힌 상태에서 누르면 열린다"), Opened.IsSelected(EFF_ClosingTab::Report));
	TestFalse(TEXT("같은 탭을 다시 누르면 닫힌다"),
		FFF_ClosingHudRules::ToggleTab(Opened, EFF_ClosingTab::Report).bHasSelection);

	const FFF_ClosingTabSelection Switched = FFF_ClosingHudRules::ToggleTab(Opened, EFF_ClosingTab::Finale);
	TestTrue(TEXT("다른 탭을 누르면 그쪽으로"), Switched.IsSelected(EFF_ClosingTab::Finale));

	// 선택이 없으면 `Tab` 은 쓰레기값이다. 비교에 넣으면 안 된다.
	TestTrue(TEXT("선택 없음끼리는 같다"),
		FFF_ClosingTabSelection::None() == FFF_ClosingTabSelection::None());

	// ★ 피날레 탭을 열어 둔 채 "안 산 상태" 가 되면 선택이 풀려야 한다.
	//   안 그러면 탭은 사라졌는데 패널만 남는다.
	TestFalse(TEXT("안 보이게 되면 선택이 풀린다"),
		FFF_ClosingHudRules::ClampSelectionToVisible(Switched, Closing, /*Purchased*/ false).bHasSelection);

	TestTrue(TEXT("보이는 동안은 유지된다"),
		FFF_ClosingHudRules::ClampSelectionToVisible(Switched, Closing, true).IsSelected(EFF_ClosingTab::Finale));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ClosingHudRules_Cta, "FestaFiesta.UI.ClosingHud.Cta", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ClosingHudRules_Cta::RunTest(const FString& Parameters)
{
	using EState = EFF_ClosingCtaState;

	TestEqual(TEXT("HUD 단계가 아니면 숨긴다"),
		FFF_ClosingHudRules::ResolveCtaState(EFF_FestivalStage::Live, false, false, false), EState::Hidden);

	TestEqual(TEXT("마감에서는 아직 못 넘긴다"),
		FFF_ClosingHudRules::ResolveCtaState(EFF_FestivalStage::Closing, /*Report*/ false, false, false),
		EState::BlockedNotReport);

	TestEqual(TEXT("리포트에서는 넘길 수 있다"),
		FFF_ClosingHudRules::ResolveCtaState(EFF_FestivalStage::Report, /*Report*/ true, false, false),
		EState::Ready);

	TestEqual(TEXT("마지막 날이면 넘길 날이 없다"),
		FFF_ClosingHudRules::ResolveCtaState(EFF_FestivalStage::Report, true, /*FinalDay*/ true, false),
		EState::BlockedFinalDay);

	// ③ ★ 순서. `마지막 날` 을 앞에 두면 **마감 단계에서** `마지막 날이라 넘길 수 없습니다`
	//   가 떠서, 리포트로 넘어가면 축제를 끝낼 수 있다는 사실을 못 알린다.
	TestEqual(TEXT("마감 + 마지막 날이면 `리포트 단계 아님` 이 먼저"),
		FFF_ClosingHudRules::ResolveCtaState(EFF_FestivalStage::Closing, /*Report*/ false, /*FinalDay*/ true, false),
		EState::BlockedNotReport);

	// --- 피날레 (2026-09-02) ---
	//
	// ★ 리포트의 `다음 날로` 와 **다른 행동**이다. 피날레는 연출 단계를 건너뛰고
	//   (`SkipCurrentStage`), 리포트는 날짜를 넘긴다 (`AdvanceToNextDay`).
	//   하나로 묶으면 `AdvanceToNextDay` 가 리포트 밖에서 무시되어 조용히 안 먹는다.
	TestEqual(TEXT("피날레에서 넘길 수 있으면 넘기기 상태"),
		FFF_ClosingHudRules::ResolveCtaState(
			EFF_FestivalStage::Finale, false, /*FinalDay*/ true, /*CanSkip*/ true),
		EState::ReadyFinaleSkip);

	// 🔴 `bSkippable` 이 꺼져 있으면 버튼째 숨긴다. 눌러도 안 되는 버튼을 두지 않는다 —
	//    Stage 시스템이 무시하고 경고만 남기므로 화면에서는 고장으로 보인다.
	TestEqual(TEXT("못 넘기면 버튼을 숨긴다"),
		FFF_ClosingHudRules::ResolveCtaState(
			EFF_FestivalStage::Finale, false, true, /*CanSkip*/ false),
		EState::Hidden);

	// ★ 마지막 날 판정이 피날레를 가리면 안 된다. 피날레는 3일차에만 오므로
	//   `bIsFinalDay` 가 항상 참인데, 그것으로 막으면 영영 못 넘긴다.
	TestEqual(TEXT("마지막 날이어도 피날레는 넘길 수 있다"),
		FFF_ClosingHudRules::ResolveCtaState(
			EFF_FestivalStage::Finale, /*Report*/ false, /*FinalDay*/ true, /*CanSkip*/ true),
		EState::ReadyFinaleSkip);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
