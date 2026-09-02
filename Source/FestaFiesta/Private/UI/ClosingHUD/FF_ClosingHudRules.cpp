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

	// 🔴 피날레도 이 바를 쓴다 (2026-09-02 변경). 다만 **탭은 전부 숨기고 CTA 만** 띄운다 —
	//    불꽃이 터지는 연출 구간이라 결산 표로 덮으면 연출을 가린다. 화면 아래에 얇은 바만
	//    남겨 두고, 다 봤으면 오른쪽 버튼으로 넘긴다.
	case EFF_FestivalStage::Finale:
		return true;

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

	// 🔴 연출 구간에는 **탭을 하나도 안 띄운다.** 결과는 그 뒤 마감에서 본다.
	if (IsFinaleShowStage(Stage))
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
	EFF_FestivalStage Stage, bool bIsReportStage, bool bIsFinalDay, bool bCanSkipStage)
{
	// ① HUD 가 안 뜨는 단계면 CTA 도 없다.
	if (!IsClosingHudStage(Stage))
	{
		return EFF_ClosingCtaState::Hidden;
	}

	// ★ 피날레는 **다른 행동**이다. 날짜를 넘기는 것이 아니라 연출 단계를 건너뛴다.
	//   기다리게 하지 않는다 — 불꽃을 다 봤는지는 플레이어가 정한다 (2026-09-02).
	//
	//   못 넘기는 것은 `DT_DaySequence` 의 `bSkippable` 이 꺼져 있을 때뿐이고,
	//   그 판정은 Stage 시스템이 소유한다. UI 는 그대로 비춘다.
	if (IsFinaleShowStage(Stage))
	{
		return bCanSkipStage
			? EFF_ClosingCtaState::ReadyFinaleSkip
			: EFF_ClosingCtaState::Hidden;
	}

	// ② 🔴 **마지막 날은 마감이 마지막 화면이다** (2026-09-02 결정).
	//
	// 3일차에는 리포트 단계를 두지 않는다 — 피날레를 이미 봤고 마감에서 최종 결산을
	// 확인하면 끝이다. 그래서 리포트 단계를 기다리지 않고 여기서 끝낼 수 있어야 한다.
	// 이 분기가 없으면 3일차에 `아직 넘길 수 없는 단계입니다` 만 뜨고 **게임이 안 끝난다.**
	if (bIsFinalDay)
	{
		return EFF_ClosingCtaState::ReadyFinishFestival;
	}

	// ③ 그 외에는 리포트 단계에서만 넘긴다. `AdvanceToNextDay()` 가 리포트 밖에서
	//    무시되기 때문이다 — UI 가 판정을 다시 쓰는 것이 아니라 소유자 조건을 비춘다.
	if (!bIsReportStage)
	{
		return EFF_ClosingCtaState::BlockedNotReport;
	}

	return EFF_ClosingCtaState::Ready;
}
