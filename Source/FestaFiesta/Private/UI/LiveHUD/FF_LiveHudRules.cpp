// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudTypes.h"

bool FFF_LiveHudRules::IsLiveStage(EFF_FestivalStage Stage)
{
	// 시계가 실제로 흐르고 배속이 걸리는 단계. 배속 판정에 쓴다.
	return Stage == EFF_FestivalStage::Live;
}

bool FFF_LiveHudRules::IsResultStage(EFF_FestivalStage Stage)
{
	// 하루가 끝나고 결과를 읽는 시간. `리포트`·`피날레` 탭이 여기서만 열린다.
	return Stage == EFF_FestivalStage::Closing || Stage == EFF_FestivalStage::Report;
}

bool FFF_LiveHudRules::IsLiveHudStage(EFF_FestivalStage Stage)
{
	switch (Stage)
	{
	// 셋업에서 확정한 순간부터 축제는 시작된 것이다. 여기서 준비 단계 바가 다시 나오면
	// "확정했는데 왜 또 준비 화면인가" 가 된다 (2026-08-27 백종태 결정).
	case EFF_FestivalStage::PromotionPreview:

	// 라이브로 넘어가는 중이라 같은 HUD 를 유지한다.
	case EFF_FestivalStage::SunsetTransition:

	case EFF_FestivalStage::Live:

	// 🔴 2026-09-02 통합 — 마감·리포트·피날레도 **이 바 하나**를 쓴다.
	//
	// 전에는 결산용 바를 따로 만들었는데, 같은 구조를 두 벌 두니 채워진 정보량이 달라
	// 화면이 단계마다 다르게 보였다. 이제 바는 셋업용과 이것 둘뿐이고, 단계 구분은
	// **가운데 초록 칩**이 한다.
	case EFF_FestivalStage::Closing:
	case EFF_FestivalStage::Report:
	case EFF_FestivalStage::Finale:
		return true;

	default:
		// 셋업·개장 준비는 준비 단계 바가 맡는다.
		return false;
	}
}

bool FFF_LiveHudRules::IsTabVisible(EFF_LiveTab Tab, EFF_FestivalStage Stage, bool bHasPurchasedFinale)
{
	if (!IsLiveHudStage(Stage))
	{
		return false;
	}

	// 🔴 피날레 연출 구간에는 탭을 **하나도** 안 띄운다. 불꽃을 보는 시간이고
	//    결과는 그 뒤 마감에서 본다.
	if (IsFinaleShowStage(Stage))
	{
		return false;
	}

	const bool bIsResultTab = (Tab == EFF_LiveTab::Report || Tab == EFF_LiveTab::Finale);
	const bool bResultStage = IsResultStage(Stage);

	// === 운영 탭과 결산 탭은 서로 배타다 ===
	// 라이브 중에 `리포트` 를 열면 하루가 안 끝나 빈 표만 보이고, 마감에 `대기열` 을
	// 남겨 두면 이미 끝난 줄을 보게 된다. 볼 수 있는 시점에만 띄운다.
	if (bIsResultTab != bResultStage)
	{
		return false;
	}

	// 🔴 `피날레` 탭은 결산 화면에 두지 않는다 (2026-10-06 백종태 결정).
	//    구매 등급의 보너스는 마감 결산의 평가 만족도와 최종 결산에 이미 들어가 있어,
	//    탭을 하나 더 열어 볼 내용이 없다. 구매 여부 인자는 호출부 호환을 위해 남긴다.
	if (Tab == EFF_LiveTab::Finale)
	{
		return false;
	}

	return true;
}

FFF_LiveTabSelection FFF_LiveHudRules::ClampSelectionToVisible(
	const FFF_LiveTabSelection& Current,
	EFF_FestivalStage Stage,
	bool bHasPurchasedFinale)
{
	if (!Current.bHasSelection)
	{
		return FFF_LiveTabSelection::None();
	}

	return IsTabVisible(Current.Tab, Stage, bHasPurchasedFinale)
		? Current
		: FFF_LiveTabSelection::None();
}

FFF_LiveTabSelection FFF_LiveHudRules::ToggleTab(const FFF_LiveTabSelection& Current, EFF_LiveTab Clicked)
{
	return Current.IsSelected(Clicked)
		? FFF_LiveTabSelection::None()
		: FFF_LiveTabSelection::Of(Clicked);
}

EFF_LiveSpeedState FFF_LiveHudRules::ResolveSpeedState(
	EFF_FestivalStage Stage,
	bool bIsMainProgramLocked)
{
	if (!IsLiveStage(Stage))
	{
		return EFF_LiveSpeedState::Hidden;
	}

	// 🔴 잠기는 곳은 **메인 공연 구간 하나뿐**이다.
	//
	// 웹 프로토타입에 `마지막 날은 배속을 쓸 수 없습니다` 가 있어서 옮겼었는데,
	// 소유자 코드에는 그런 잠금이 없다 — `Tick` 의 Live 분기는 공연 잠금만 본다.
	// UI 혼자 막고 있던 셈이라 뺐다 (2026-09-02 백종태 결정).
	if (bIsMainProgramLocked)
	{
		return EFF_LiveSpeedState::LockedMainProgram;
	}

	return EFF_LiveSpeedState::Ready;
}

EFF_LiveCtaState FFF_LiveHudRules::ResolveCtaState(
	EFF_FestivalStage Stage, bool bIsFinalDay, bool bCanSkipStage)
{
	// ① 라이브 중에는 넘기는 버튼이 없다. 시간이 알아서 흐른다.
	if (!IsResultStage(Stage) && !IsFinaleShowStage(Stage))
	{
		return EFF_LiveCtaState::Hidden;
	}

	// ② 피날레는 **다른 행동**이다. 날짜를 넘기는 것이 아니라 연출 단계를 건너뛴다.
	//    기다리게 하지 않는다 — 불꽃을 다 봤는지는 플레이어가 정한다.
	//
	//    못 넘기면 버튼째 숨긴다. `bSkippable` 이 꺼져 있으면 Stage 시스템이 무시하고
	//    경고만 남기므로, 버튼을 두면 눌러도 안 되는 버튼이 된다.
	if (IsFinaleShowStage(Stage))
	{
		return bCanSkipStage
			? EFF_LiveCtaState::ReadyFinaleSkip
			: EFF_LiveCtaState::Hidden;
	}

	// ③ 🔴 **마지막 날은 마감이 마지막 화면이다.**
	//
	//    3일차에는 리포트 단계를 두지 않는다 — 피날레를 이미 봤고 마감에서 최종 결산을
	//    확인하면 끝이다. 이 분기가 ④ 보다 뒤에 있으면 3일차 마감에서 `아직 넘길 수 없는
	//    단계입니다` 만 뜨고 **게임이 안 끝난다.**
	if (bIsFinalDay)
	{
		return EFF_LiveCtaState::ReadyFinishFestival;
	}

	// ④ 리포트 단계면 바로 넘긴다.
	if (Stage == EFF_FestivalStage::Report)
	{
		return EFF_LiveCtaState::ReadyNextDay;
	}

	// ⑤ 🔴 마감에서도 **기다리게 하지 않는다** (2026-09-03 백종태 결정).
	//
	//    전에는 마감이면 무조건 `BlockedNotReport` 로 막고 `남은 마감 시간이 지나면
	//    리포트로 넘어갑니다` 를 띄웠다. 그런데 마감에서 할 일은 결산을 읽는 것뿐이고,
	//    다 읽었는지는 플레이어가 안다. 남은 시간을 세는 것은 기다림일 뿐이다.
	//
	//    `AdvanceToNextDay()` 가 리포트 밖에서 거절되는 것은 그대로다 — 그래서 화면이
	//    마감을 먼저 건너뛴 뒤 넘긴다 (`UFF_LiveHudScreen::HandleCtaClicked`).
	//    건너뛸 수 없는 마감이면 그 두 걸음의 **첫 걸음부터** 막히므로 버튼을 열지 않는다.
	return bCanSkipStage
		? EFF_LiveCtaState::ReadyNextDay
		: EFF_LiveCtaState::BlockedNotReport;
}
