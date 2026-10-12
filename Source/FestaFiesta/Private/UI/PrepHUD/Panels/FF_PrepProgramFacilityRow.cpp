#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

void UFF_PrepProgramFacilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Border_Selection) NormalBrush = Border_Selection->Background;
	if (Button_Select) Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
	if (Button_Locate)
	{
		Button_Locate->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLocateClicked);
		Button_Locate->SetToolTipText(LocateTooltip);
	}
}

void UFF_PrepProgramFacilityRow::ConfigureFacility(int32 InIndex, const FFF_PrepFacilityOption& Facility)
{
	FacilityIndex = InIndex;
	if (Text_FacilityName) Text_FacilityName->SetText(Facility.InstanceLabel);
	if (Text_Location) Text_Location->SetText(Facility.LocationText);
	if (Button_Select) Button_Select->SetToolTipText(FText::Format(
		NSLOCTEXT("FestaFiestaPrepProgram", "FacilityRowTooltip", "{0}\n{1}"), Facility.InstanceLabel, Facility.LocationText));
}

void UFF_PrepProgramFacilityRow::SetFacilityState(bool bSelected, bool bScheduled)
{
	if (Text_SelectionMark) Text_SelectionMark->SetText(FText::FromString(bSelected ? TEXT("●") : TEXT("○")));
	if (Border_Selection)
	{
		FSlateBrush Brush = NormalBrush;
		if (bSelected)
		{
			Brush.TintColor = SelectedBackgroundColor;
			Brush.OutlineSettings.Color = SelectedOutlineColor;
		}
		Border_Selection->SetBrush(Brush);
	}
	if (Text_Status)
	{
		Text_Status->SetText(ScheduledText);
		Text_Status->SetVisibility(bScheduled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_PrepProgramFacilityRow::HandleSelectClicked()
{
	OnFacilitySelected.ExecuteIfBound(FacilityIndex);
}

void UFF_PrepProgramFacilityRow::HandleLocateClicked()
{
	OnLocateRequested.ExecuteIfBound(FacilityIndex);
}
