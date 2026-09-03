// Fill out your copyright notice in the Description page of Project Settings.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/LiveHUD/FF_LiveHudTypes.h"

/**
 * 라이브 HUD 판단 규칙 고정 테스트 (U6).
 *
 * === 왜 이 테스트가 있어야 하나 ===
 * 라이브 화면은 **셋업부터 시작해 노을을 지나야** 도달하고, 배속 잠금은 그 위에
 * `메인 공연 구간` 이나 `3일차` 라는 조건이 더 붙는다. 손으로 밟으면 한 번에 몇 분이고
 * 3일차는 Report 를 두 번 거쳐야 한다. 셋을 못으로 박는다 —
 *   ① 라이브가 아닌 단계에서 HUD·탭이 안 뜨는 것
 *   ② 단계가 넘어가면 열려 있던 패널의 선택이 풀리는 것 (탭은 사라졌는데 패널만 남지 않게)
 *   ③ 배속 잠금 이유가 `공연` 과 `마지막 날` 로 구분되고, 그 순서가 지켜지는 것 (AT-011)
 */

// 라이브에서만 HUD 를 띄운다. 준비 단계·마감·결산은 다른 화면이 맡는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveHudRules_LiveStageOnly, "FestaFiesta.UI.LiveHud.LiveStageOnly", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveHudRules_LiveStageOnly::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("라이브는 라이브 단계다"),
		FFF_LiveHudRules::IsLiveStage(EFF_FestivalStage::Live));

	// 준비 단계 3종은 `UFF_PrepHudBar` 가 맡는다.
	for (const EFF_FestivalStage Stage : {
		EFF_FestivalStage::Setup,
		EFF_FestivalStage::PromotionPreview,
		EFF_FestivalStage::MorningCheck })
	{
		TestFalse(FString::Printf(TEXT("준비 단계는 라이브가 아니다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveStage(Stage));
	}

	// 🔴 시계가 흐르는 단계와 HUD 를 띄우는 단계는 다르다 (2026-08-27 결정).
	//    홍보 프리뷰·노을 전환은 시계가 안 흐르지만 HUD 는 그대로 떠 있어야 한다 —
	//    셋업에서 확정한 뒤 준비 단계 바가 다시 나오면 안 된다.
	for (const EFF_FestivalStage Stage : {
		EFF_FestivalStage::PromotionPreview,
		EFF_FestivalStage::SunsetTransition })
	{
		TestFalse(FString::Printf(TEXT("시계는 안 흐른다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveStage(Stage));
		TestTrue(FString::Printf(TEXT("그래도 HUD 는 뜬다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveHudStage(Stage));
	}

	TestTrue(TEXT("라이브에서도 HUD 가 뜬다"),
		FFF_LiveHudRules::IsLiveHudStage(EFF_FestivalStage::Live));

	// 준비 단계 바가 맡는 두 단계에서는 라이브 HUD 가 뜨지 않는다.
	// 바는 셋업용과 이것 둘뿐이다.
	TestFalse(TEXT("셋업에서는 이 바가 안 뜬다"),
		FFF_LiveHudRules::IsLiveHudStage(EFF_FestivalStage::Setup));
	TestFalse(TEXT("개장 준비에서는 라이브 HUD 가 안 뜬다"),
		FFF_LiveHudRules::IsLiveHudStage(EFF_FestivalStage::MorningCheck));

	// 🔴 마감·리포트·피날레도 **같은 바**를 쓴다 (2026-09-02 통합). 다만 시계가 흐르는
	//    `IsLiveStage` 는 아니다 — 배속이 걸리는 곳은 라이브뿐이다.
	for (const EFF_FestivalStage Stage : {
		EFF_FestivalStage::Closing,
		EFF_FestivalStage::Report,
		EFF_FestivalStage::Finale })
	{
		TestFalse(FString::Printf(TEXT("결산 단계는 라이브가 아니다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveStage(Stage));
		TestTrue(FString::Printf(TEXT("그래도 같은 바를 쓴다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveHudStage(Stage));
	}

	// === 운영 탭과 결산 탭은 서로 배타다 (2026-09-02 통합) ===
	//
	// 🔴 라이브 중에 `리포트` 를 열면 하루가 안 끝나 빈 표만 보이고, 마감에 `대기열` 을
	// 남겨 두면 이미 끝난 줄을 보게 된다. 볼 수 있는 시점에만 띄운다.
	for (const EFF_LiveTab Tab : {
		EFF_LiveTab::Operations, EFF_LiveTab::Facilities, EFF_LiveTab::Guests,
		EFF_LiveTab::Queues, EFF_LiveTab::SNS, EFF_LiveTab::Staff })
	{
		TestTrue(TEXT("라이브에서는 운영 탭이 보인다"),
			FFF_LiveHudRules::IsTabVisible(Tab, EFF_FestivalStage::Live, true));
		TestFalse(TEXT("마감에서는 운영 탭이 안 보인다"),
			FFF_LiveHudRules::IsTabVisible(Tab, EFF_FestivalStage::Closing, true));
	}

	TestFalse(TEXT("라이브에서는 리포트 탭이 안 보인다"),
		FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::Report, EFF_FestivalStage::Live, true));
	TestTrue(TEXT("마감에서는 리포트 탭이 보인다"),
		FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::Report, EFF_FestivalStage::Closing, true));
	TestTrue(TEXT("리포트 단계에서도 보인다"),
		FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::Report, EFF_FestivalStage::Report, true));

	// 🔴 피날레를 안 샀으면 그 탭은 숨긴다. 빈 패널은 "고장났나" 로 읽히고,
	//    살 수 있는 곳(셋업)은 이미 지나간 뒤다.
	TestFalse(TEXT("안 샀으면 피날레 탭이 숨는다"),
		FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::Finale, EFF_FestivalStage::Closing, /*Purchased*/ false));
	TestTrue(TEXT("샀으면 보인다"),
		FFF_LiveHudRules::IsTabVisible(EFF_LiveTab::Finale, EFF_FestivalStage::Closing, /*Purchased*/ true));

	// ★ 피날레 **연출 구간**에는 탭을 하나도 안 띄운다. 불꽃을 보는 시간이다.
	for (const EFF_LiveTab Tab : {
		EFF_LiveTab::Operations, EFF_LiveTab::Report, EFF_LiveTab::Finale })
	{
		TestFalse(TEXT("피날레 연출 구간에는 탭이 없다"),
			FFF_LiveHudRules::IsTabVisible(Tab, EFF_FestivalStage::Finale, true));
	}
	return true;
}

// 탭 배타 규칙. 준비 단계와 같은 모양이라 여기서도 같은 답이 나와야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveHudRules_TabExclusivity, "FestaFiesta.UI.LiveHud.TabExclusivity", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveHudRules_TabExclusivity::RunTest(const FString& Parameters)
{
	const FFF_LiveTabSelection None = FFF_LiveTabSelection::None();

	const FFF_LiveTabSelection Opened = FFF_LiveHudRules::ToggleTab(None, EFF_LiveTab::Queues);
	TestTrue(TEXT("닫힌 상태에서 누르면 열린다"), Opened.IsSelected(EFF_LiveTab::Queues));

	TestFalse(TEXT("같은 탭을 다시 누르면 닫힌다"),
		FFF_LiveHudRules::ToggleTab(Opened, EFF_LiveTab::Queues).bHasSelection);

	const FFF_LiveTabSelection Switched = FFF_LiveHudRules::ToggleTab(Opened, EFF_LiveTab::SNS);
	TestTrue(TEXT("다른 탭을 누르면 그쪽으로 바뀐다"), Switched.IsSelected(EFF_LiveTab::SNS));
	TestFalse(TEXT("앞의 패널은 닫힌다"), Switched.IsSelected(EFF_LiveTab::Queues));

	// 선택이 없으면 `Tab` 은 쓰레기값이다. 비교에 넣으면 안 된다.
	TestTrue(TEXT("선택 없음끼리는 같다"),
		FFF_LiveTabSelection::None() == FFF_LiveTabSelection::None());

	// ② 라이브가 끝나면 선택이 풀려야 한다. 안 그러면 탭은 사라졌는데 패널만 남는다.
	TestFalse(TEXT("마감으로 넘어가면 선택이 풀린다"),
		FFF_LiveHudRules::ClampSelectionToVisible(Switched, EFF_FestivalStage::Closing, true).bHasSelection);

	TestTrue(TEXT("라이브 안에서는 선택이 유지된다"),
		FFF_LiveHudRules::ClampSelectionToVisible(Switched, EFF_FestivalStage::Live, true)
			.IsSelected(EFF_LiveTab::SNS));

	return true;
}

// ③ 배속 잠금 — 이유가 갈려야 문구를 다르게 띄울 수 있다 (AT-011).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveHudRules_SpeedLock, "FestaFiesta.UI.LiveHud.SpeedLock", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveHudRules_SpeedLock::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("평소에는 바꿀 수 있다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, false),
		EFF_LiveSpeedState::Ready);

	TestEqual(TEXT("메인 공연 중에는 잠긴다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, /*MainProgram*/ true),
		EFF_LiveSpeedState::LockedMainProgram);

	// ★ **마지막 날에도 바꿀 수 있다** (2026-09-02 백종태 결정).
	//
	// 웹 프로토타입에 `마지막 날은 배속을 쓸 수 없습니다` 가 있어서 그대로 옮겼었는데,
	// 소유자 코드에는 그런 잠금이 없다 — `Tick` 의 Live 분기는 공연 잠금만 본다.
	// UI 혼자 막고 있었다. 3일차는 가장 긴 날이라 오히려 배속이 필요하다.
	//
	// 이 줄이 없으면 프로토타입을 다시 보고 잠금을 되살릴 수 있다.
	TestEqual(TEXT("마지막 날에도 바꿀 수 있다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, /*MainProgram*/ false),
		EFF_LiveSpeedState::Ready);

	// 라이브가 아니면 배속 줄째 숨긴다.
	//
	// ⚠️ 마감·준비 단계도 시계는 흐르지만 **배속이 안 먹는다** — `Tick` 의 비-Live 분기가
	// `TimeScale` 을 안 쓰고 항상 1배속으로 더한다. 눌러도 안 되는 버튼을 만들지 않으려고
	// 줄째 숨긴다. 소유자가 그 분기에도 배속을 적용하면 그때 연다 (`UI_API.md` 요청).
	TestEqual(TEXT("개장 준비에서는 숨긴다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::MorningCheck, false),
		EFF_LiveSpeedState::Hidden);

	// 잠금 인자가 켜져 있어도 단계가 아니면 숨김이 먼저다.
	TestEqual(TEXT("라이브가 아니면 잠금보다 숨김이 먼저"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Closing, true),
		EFF_LiveSpeedState::Hidden);

	return true;
}

// =============================================================================
// 넘기는 버튼 — 단계·날짜·건너뛰기 가능 여부가 셋 다 걸린다
// =============================================================================
//
// 🔴 손으로 밟기 가장 비싼 자리다. 3일차 마감을 보려면 **축제를 끝까지 세 번 플레이**해야
// 하고, 거기서 틀리면 `게임이 안 끝난다` 는 형태로 터진다. 못으로 박는다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveHudRules_Cta, "FestaFiesta.UI.LiveHud.Cta", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveHudRules_Cta::RunTest(const FString& Parameters)
{
	// --- 라이브 중에는 버튼이 없다. 시간이 알아서 흐른다 ---
	TestEqual(TEXT("라이브에는 버튼이 없다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Live, false, true),
		EFF_LiveCtaState::Hidden);

	// --- 마감에서도 바로 넘긴다 (2026-09-03) ---
	//
	// 전에는 무조건 `BlockedNotReport` 로 막고 남은 마감 시간을 기다리게 했다.
	// 그 5초·수십 초 동안 플레이어가 할 수 있는 것이 아무것도 없었다.
	TestEqual(TEXT("마감에서 바로 넘긴다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Closing, false, /*Skippable*/ true),
		EFF_LiveCtaState::ReadyNextDay);

	// ★ 다만 **건너뛸 수 없는 마감**이면 열지 않는다. 화면은 마감을 먼저 건너뛴 뒤
	//   날짜를 넘기므로, 첫 걸음이 막히면 버튼이 눌러도 안 되는 버튼이 된다.
	TestEqual(TEXT("건너뛸 수 없는 마감이면 막는다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Closing, false, /*Skippable*/ false),
		EFF_LiveCtaState::BlockedNotReport);

	TestEqual(TEXT("리포트는 건너뛰기와 무관하게 넘긴다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Report, false, /*Skippable*/ false),
		EFF_LiveCtaState::ReadyNextDay);

	// --- 마지막 날은 `축제 마치기` 다 ---
	//
	// 🔴 이 판정이 건너뛰기 검사보다 **먼저** 와야 한다. 3일차에는 리포트 단계를 두지
	// 않으므로, 순서가 뒤바뀌면 3일차 마감에서 버튼이 막히고 **게임이 안 끝난다.**
	TestEqual(TEXT("마지막 날 마감은 축제 마치기"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Closing, /*FinalDay*/ true, /*Skippable*/ false),
		EFF_LiveCtaState::ReadyFinishFestival);

	// --- 피날레 연출은 **다른 행동**이다 ---
	TestEqual(TEXT("피날레는 건너뛰기"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Finale, true, /*Skippable*/ true),
		EFF_LiveCtaState::ReadyFinaleSkip);

	// 못 넘기면 버튼째 숨긴다. `bSkippable` 이 꺼져 있으면 Stage 시스템이 무시하고
	// 경고만 남기므로, 버튼을 두면 눌러도 안 되는 버튼이 된다.
	TestEqual(TEXT("건너뛸 수 없는 피날레는 숨긴다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Finale, true, /*Skippable*/ false),
		EFF_LiveCtaState::Hidden);

	// ★ 피날레는 마지막 날에만 나오지만, `bIsFinalDay` 로 축제 종료가 되면 안 된다 —
	//   불꽃을 보다가 나라 지도로 튕긴다.
	TestEqual(TEXT("피날레가 축제 종료로 새지 않는다"),
		FFF_LiveHudRules::ResolveCtaState(EFF_FestivalStage::Finale, /*FinalDay*/ true, true),
		EFF_LiveCtaState::ReadyFinaleSkip);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
