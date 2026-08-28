// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudTypes.h"

bool FFF_LiveHudRules::IsLiveStage(EFF_FestivalStage Stage)
{
	// 시계가 실제로 흐르는 단계. 배속 판정에 쓴다.
	return Stage == EFF_FestivalStage::Live;
}

bool FFF_LiveHudRules::IsLiveHudStage(EFF_FestivalStage Stage)
{
	switch (Stage)
	{
	// 셋업에서 확정한 순간부터 축제는 시작된 것이다. 여기서 준비 단계 바가 다시 나오면
	// "확정했는데 왜 또 준비 화면인가" 가 된다 (2026-08-27 백종태 결정).
	case EFF_FestivalStage::PromotionPreview:

	// 라이브로 넘어가는 중이라 같은 HUD 를 유지한다. 20초 동안 화면이 비지 않는다.
	case EFF_FestivalStage::SunsetTransition:

	case EFF_FestivalStage::Live:
		return true;

	default:
		// 셋업·개장 준비는 준비 단계 바가, 마감·결산·피날레는 풀스크린 시트가 맡는다.
		return false;
	}
}

bool FFF_LiveHudRules::IsTabVisible(EFF_LiveTab Tab, EFF_FestivalStage Stage)
{
	// HUD 가 뜨는 단계면 7종 전부 보인다. 전부 읽기 전용 정보라 미리 열어도 잘못될 것이 없다.
	return IsLiveHudStage(Stage);
}

FFF_LiveTabSelection FFF_LiveHudRules::ClampSelectionToVisible(
	const FFF_LiveTabSelection& Current,
	EFF_FestivalStage Stage)
{
	if (!Current.bHasSelection)
	{
		return FFF_LiveTabSelection::None();
	}

	return IsTabVisible(Current.Tab, Stage)
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
	bool bIsMainProgramLocked,
	bool bIsFinalDay)
{
	if (!IsLiveStage(Stage))
	{
		return EFF_LiveSpeedState::Hidden;
	}

	// ★ 공연 잠금을 먼저 본다. 3일차 공연 중에 `마지막 날이라 못 바꿉니다` 라고 하면
	//   공연이 끝나면 풀린다는 사실을 못 알린다 — 더 빨리 풀리는 이유를 먼저 말한다.
	if (bIsMainProgramLocked)
	{
		return EFF_LiveSpeedState::LockedMainProgram;
	}

	if (bIsFinalDay)
	{
		return EFF_LiveSpeedState::LockedFinalDay;
	}

	return EFF_LiveSpeedState::Ready;
}
