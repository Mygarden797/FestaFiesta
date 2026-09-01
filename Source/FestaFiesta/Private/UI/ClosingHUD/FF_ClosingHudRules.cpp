// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ClosingHUD/FF_ClosingHudTypes.h"

bool FFF_ClosingHudRules::IsClosingHudStage(EFF_FestivalStage Stage)
{
	switch (Stage)
	{
	// 마감과 리포트는 **같은 화면**이다. 하루가 끝났을 때 결과를 보는 일 하나이고,
	// 리포트에서 달라지는 것은 `다음 날로` 를 누를 수 있다는 것뿐이다.
	// 화면을 두 장으로 나누면 같은 표를 두 번 그리게 된다 (2026-09-02 백종태 결정).
	case EFF_FestivalStage::Closing:
	case EFF_FestivalStage::Report:
		return true;

	// 🔴 `Finale` 단계는 **불꽃이 터지는 연출 구간**이다 (3일차 라이브 직후).
	//    거기서 결산 표를 덮으면 연출을 가린다 — 결과는 그 뒤 마감에서 본다.
	default:
		return false;
	}
}

bool FFF_ClosingHudRules::IsTabVisible(
	EFF_ClosingTab Tab, EFF_FestivalStage Stage, bool bHasPurchasedFinale)
{
	if (!IsClosingHudStage(Stage))
	{
		return false;
	}

	// 🔴 사지 않았으면 피날레 탭을 숨긴다. 빈 패널을 보여 주면 "고장났나" 가 되고,
	//    살 수 있는 곳(셋업)은 이미 지나간 뒤라 화면에서 할 수 있는 것이 없다.
	if (Tab == EFF_ClosingTab::Finale)
	{
		return bHasPurchasedFinale;
	}

	return true;
}

FFF_ClosingTabSelection FFF_ClosingHudRules::ClampSelectionToVisible(
	const FFF_ClosingTabSelection& Current, EFF_FestivalStage Stage, bool bHasPurchasedFinale)
{
	if (!Current.bHasSelection)
	{
		return FFF_ClosingTabSelection::None();
	}

	return IsTabVisible(Current.Tab, Stage, bHasPurchasedFinale)
		? Current
		: FFF_ClosingTabSelection::None();
}

FFF_ClosingTabSelection FFF_ClosingHudRules::ToggleTab(
	const FFF_ClosingTabSelection& Current, EFF_ClosingTab Clicked)
{
	return Current.IsSelected(Clicked)
		? FFF_ClosingTabSelection::None()
		: FFF_ClosingTabSelection::Of(Clicked);
}

EFF_ClosingCtaState FFF_ClosingHudRules::ResolveCtaState(
	EFF_FestivalStage Stage, bool bIsReportStage, bool bIsFinalDay)
{
	// ① HUD 가 안 뜨는 단계면 CTA 도 없다.
	if (!IsClosingHudStage(Stage))
	{
		return EFF_ClosingCtaState::Hidden;
	}

	// ② 마감에서는 아직 못 넘긴다. `AdvanceToNextDay()` 가 리포트 단계 밖에서는 무시된다 —
	//    UI 가 여기서 판정을 다시 쓰는 것이 아니라 **소유자의 조건을 그대로 비춘다.**
	if (!bIsReportStage)
	{
		return EFF_ClosingCtaState::BlockedNotReport;
	}

	// ★ ③ 을 ② 앞에 두면 마감 단계에서 `마지막 날이라 넘길 수 없습니다` 가 떠서,
	//   리포트로 넘어가면 축제를 끝낼 수 있다는 사실을 못 알린다.
	if (bIsFinalDay)
	{
		return EFF_ClosingCtaState::BlockedFinalDay;
	}

	return EFF_ClosingCtaState::Ready;
}
