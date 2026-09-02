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

	// 🔴 피날레를 사지 않았으면 그 탭은 숨긴다. 빈 패널은 "고장났나" 로 읽히고,
	//    살 수 있는 곳(셋업)은 이미 지나간 뒤라 화면에서 할 수 있는 것이 없다.
	if (Tab == EFF_LiveTab::Finale)
	{
		return bHasPurchasedFinale;
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

	// ④ 그 외에는 리포트 단계에서만 넘긴다. `AdvanceToNextDay()` 가 리포트 밖에서
	//    무시되기 때문이다 — UI 가 판정을 다시 쓰는 것이 아니라 소유자 조건을 비춘다.
	if (Stage != EFF_FestivalStage::Report)
	{
		return EFF_LiveCtaState::BlockedNotReport;
	}

	return EFF_LiveCtaState::ReadyNextDay;
}
