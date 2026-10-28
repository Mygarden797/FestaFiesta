// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "Festival/FF_RequiredFacilityTypes.h"

int32 FFF_PrepHudRules::CountPendingRequiredTasks(const FFF_RequiredFacilitiesStatus& Status)
{
	int32 Count = 0;
	for (const FFF_RequiredFacilityRow& Row : Status.Facilities)
	{
		if (!Row.IsSatisfied()) { ++Count; }
	}
	return Count;
}

bool FFF_PrepHudRules::NeedsRequiredAttention(EFF_PrepHudTab Tab, EFF_FestivalStage Stage,
	const FFF_RequiredFacilitiesStatus& Status)
{
	if (!IsPrepStage(Stage) || Status.bCanProceed) { return false; }
	if (Tab == EFF_PrepHudTab::Check) { return true; }
	if (Tab != EFF_PrepHudTab::Staff) { return false; }
	// 아직 시설을 설치할 차례라면 점검의 배치 바로가기를 먼저 안내한다.
	return Status.Facilities.ContainsByPredicate([](const FFF_RequiredFacilityRow& Row)
	{
		return Row.bDefinitionValid && Row.InstalledCount > 0 && Row.MissingCount == 0 && Row.MissingStaffCount > 0;
	});
}

bool FFF_PrepHudRules::IsPrepStage(EFF_FestivalStage Stage)
{
	switch (Stage)
	{
	case EFF_FestivalStage::Setup:
	case EFF_FestivalStage::PromotionPreview:
	case EFF_FestivalStage::MorningCheck:
		return true;

	default:
		// 노을 전환·라이브·마감·결산·피날레는 준비 단계가 아니다.
		return false;
	}
}

bool FFF_PrepHudRules::IsTabVisible(EFF_PrepHudTab Tab, EFF_FestivalStage Stage, bool bIsFinalDay)
{
	if (!IsPrepStage(Stage))
	{
		return false;
	}

	switch (Tab)
	{
	case EFF_PrepHudTab::Promotion:
		return Stage == EFF_FestivalStage::Setup;

	case EFF_PrepHudTab::Finale:
		// 마지막 날 개장 준비에서만. 구매는 축제당 1회이고 되돌릴 수 없으므로
		// 살 수 없는 시점에 여는 쪽이 안 여는 쪽보다 나쁘다.
		return Stage == EFF_FestivalStage::MorningCheck && bIsFinalDay;

	default:
		return true;
	}
}

FFF_PrepTabSelection FFF_PrepHudRules::ClampSelectionToVisible(
	const FFF_PrepTabSelection& Current,
	EFF_FestivalStage Stage,
	bool bIsFinalDay)
{
	if (!Current.bHasSelection)
	{
		return FFF_PrepTabSelection::None();
	}

	return IsTabVisible(Current.Tab, Stage, bIsFinalDay)
		? Current
		: FFF_PrepTabSelection::None();
}

FFF_PrepTabSelection FFF_PrepHudRules::ToggleTab(const FFF_PrepTabSelection& Current, EFF_PrepHudTab Clicked)
{
	return Current.IsSelected(Clicked)
		? FFF_PrepTabSelection::None()
		: FFF_PrepTabSelection::Of(Clicked);
}

EFF_PrepCtaState FFF_PrepHudRules::ResolveCtaState(
	EFF_FestivalStage Stage,
	bool bHasCtaLabel,
	bool bStageAdvanceAvailable,
	int32 FacilityCount,
	bool bRequireFacility)
{
	if (!IsPrepStage(Stage) || !bHasCtaLabel)
	{
		return EFF_PrepCtaState::Hidden;
	}

	// 시설 판정을 단계 판정보다 먼저 한다. 둘 다 막혀 있을 때 플레이어가 할 수 있는 일은
	// 시설을 놓는 것뿐이고, "기다리세요"를 먼저 보여주면 그 행동을 안내하지 못한다.
	if (bRequireFacility && FacilityCount != INDEX_NONE && FacilityCount <= 0)
	{
		return EFF_PrepCtaState::BlockedNoFacility;
	}

	if (!bStageAdvanceAvailable)
	{
		return EFF_PrepCtaState::BlockedStageLocked;
	}

	return EFF_PrepCtaState::Ready;
}
