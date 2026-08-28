// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudTypes.h"

bool FFF_LiveHudRules::IsLiveStage(EFF_FestivalStage Stage)
{
	// 마감·결산은 풀스크린 시트(U7)이고, 노을 전환은 HUD 없이 지나간다.
	return Stage == EFF_FestivalStage::Live;
}

bool FFF_LiveHudRules::IsTabVisible(EFF_LiveTab Tab, EFF_FestivalStage Stage)
{
	// 라이브면 7종 전부 보인다. 탭별 조건은 아직 없다.
	return IsLiveStage(Stage);
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
