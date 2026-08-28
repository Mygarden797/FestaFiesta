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

	// 🔴 노을 전환은 20초짜리 순수 연출이라 HUD 를 띄우지 않는다 (2026-08-27 결정).
	TestFalse(TEXT("노을 전환은 라이브가 아니다"),
		FFF_LiveHudRules::IsLiveStage(EFF_FestivalStage::SunsetTransition));

	// 마감·결산·피날레는 풀스크린 시트다 (U7).
	for (const EFF_FestivalStage Stage : {
		EFF_FestivalStage::Closing,
		EFF_FestivalStage::Report,
		EFF_FestivalStage::Finale })
	{
		TestFalse(FString::Printf(TEXT("시트 단계는 라이브가 아니다 (%s)"), *UEnum::GetValueAsString(Stage)),
			FFF_LiveHudRules::IsLiveStage(Stage));
	}

	// 탭 7종은 라이브면 전부 보이고, 아니면 전부 안 보인다.
	for (const EFF_LiveTab Tab : {
		EFF_LiveTab::Operations, EFF_LiveTab::Facilities, EFF_LiveTab::Guests,
		EFF_LiveTab::Queues, EFF_LiveTab::SNS, EFF_LiveTab::Report, EFF_LiveTab::Staff })
	{
		TestTrue(TEXT("라이브에서는 탭이 보인다"),
			FFF_LiveHudRules::IsTabVisible(Tab, EFF_FestivalStage::Live));
		TestFalse(TEXT("라이브가 아니면 탭이 안 보인다"),
			FFF_LiveHudRules::IsTabVisible(Tab, EFF_FestivalStage::Closing));
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
		FFF_LiveHudRules::ClampSelectionToVisible(Switched, EFF_FestivalStage::Closing).bHasSelection);

	TestTrue(TEXT("라이브 안에서는 선택이 유지된다"),
		FFF_LiveHudRules::ClampSelectionToVisible(Switched, EFF_FestivalStage::Live)
			.IsSelected(EFF_LiveTab::SNS));

	return true;
}

// ③ 배속 잠금 — 이유가 갈려야 문구를 다르게 띄울 수 있다 (AT-011).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_LiveHudRules_SpeedLock, "FestaFiesta.UI.LiveHud.SpeedLock", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_LiveHudRules_SpeedLock::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("평소에는 바꿀 수 있다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, false, false),
		EFF_LiveSpeedState::Ready);

	TestEqual(TEXT("메인 공연 중에는 잠긴다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, /*MainProgram*/ true, false),
		EFF_LiveSpeedState::LockedMainProgram);

	TestEqual(TEXT("마지막 날에도 잠긴다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, false, /*FinalDay*/ true),
		EFF_LiveSpeedState::LockedFinalDay);

	// ★ 순서가 뒤집히면 3일차 공연 중에 `마지막 날이라 못 바꿉니다` 가 떠서,
	//   공연이 끝나면 풀린다는 사실을 못 알리게 된다.
	TestEqual(TEXT("둘 다 걸리면 공연 잠금을 먼저 알린다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Live, /*MainProgram*/ true, /*FinalDay*/ true),
		EFF_LiveSpeedState::LockedMainProgram);

	// 라이브가 아니면 배속 줄째 숨긴다 — 준비 단계에는 배속이 없다.
	TestEqual(TEXT("라이브가 아니면 숨긴다"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::MorningCheck, false, false),
		EFF_LiveSpeedState::Hidden);

	// 잠금 인자가 켜져 있어도 단계가 아니면 숨김이 먼저다.
	TestEqual(TEXT("라이브가 아니면 잠금보다 숨김이 먼저"),
		FFF_LiveHudRules::ResolveSpeedState(EFF_FestivalStage::Closing, true, true),
		EFF_LiveSpeedState::Hidden);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
