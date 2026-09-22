// FF_FacilityPaidProgramCard.cpp
// 역할: 유료 프로그램 하나의 이름·가격·편성 가능 여부를 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityPaidProgramCard.h"

#include "CommonTextBlock.h"
#include "Components/Button.h"
#include "UI/Data/FF_UIPalette.h"

#define LOCTEXT_NAMESPACE "FFFacilityPaidProgramCard"

void UFF_FacilityPaidProgramCard::NativePreConstruct()
{
	Super::NativePreConstruct();
	// The palette seeds Class Defaults; designer edits must survive preview and play.
}

void UFF_FacilityPaidProgramCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Select)
	{
		Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
	}
}

void UFF_FacilityPaidProgramCard::ApplyViewData(
	const FFF_FacilityPaidProgramCardViewData& ViewData)
{
	CurrentViewData = ViewData;
	if (Text_ProgramName)
	{
		Text_ProgramName->SetText(ViewData.DisplayName);
	}
	if (Text_ProgramMeta)
	{
		Text_ProgramMeta->SetText(FText::Format(
			LOCTEXT("CardMeta", "{0} · {1}"), ViewData.TimeText, ViewData.CostText));
	}
	if (Text_ProgramAvailability)
	{
		Text_ProgramAvailability->SetText(ViewData.AvailabilityText);
		Text_ProgramAvailability->SetColorAndOpacity(FSlateColor(
			ViewData.bCanSchedule ? AvailableColor : UnavailableColor));
	}
	SetSelected(false);
}

void UFF_FacilityPaidProgramCard::SetSelected(const bool bSelected)
{
	if (Button_Select)
	{
		Button_Select->SetBackgroundColor(
			bSelected ? SelectedColor : UnselectedColor);
	}
}

void UFF_FacilityPaidProgramCard::HandleSelectClicked()
{
	if (!CurrentViewData.ProgramId.IsNone())
	{
		OnSelected.Broadcast(CurrentViewData.ProgramId);
	}
}

#undef LOCTEXT_NAMESPACE
