#include "UI/PrepHUD/Panels/FF_PrepProgramFacilityRow.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

void UFF_PrepProgramFacilityRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Border_Selection) NormalBrush = Border_Selection->Background;
	if (Button_Select)
	{
		Button_Select->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSelectClicked);
		// 저장된 WBP도 행 전체 배경을 사용한다. 버튼별 호버 색이 선택색을 덮지 않게 한다.
		FButtonStyle Style = Button_Select->GetStyle();
		for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled })
			Brush->DrawAs = ESlateBrushDrawType::NoDrawType;
		Button_Select->SetStyle(Style);
		Button_Select->SetToolTipText(FText::GetEmpty());
	}
	if (Button_Locate)
	{
		Button_Locate->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLocateClicked);
		FButtonStyle Style = Button_Locate->GetStyle();
		for (FSlateBrush* Brush : { &Style.Normal, &Style.Hovered, &Style.Pressed, &Style.Disabled })
			Brush->TintColor = FLinearColor::Transparent;
		Button_Locate->SetStyle(Style); // 테두리는 남기고 내부는 행의 공통 배경을 비춘다.
		Button_Locate->SetToolTipText(LocateTooltip);
	}
}

void UFF_PrepProgramFacilityRow::NativeDestruct()
{
	bPointerOverRow = false;
	RefreshRowBackground();
	Super::NativeDestruct();
}

void UFF_PrepProgramFacilityRow::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bPointerOverRow = true;
	RefreshRowBackground();
}

void UFF_PrepProgramFacilityRow::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bPointerOverRow = false;
	RefreshRowBackground();
}

void UFF_PrepProgramFacilityRow::ConfigureFacility(int32 InIndex, const FFF_PrepFacilityOption& Facility)
{
	FacilityIndex = InIndex;
	if (Text_FacilityName) Text_FacilityName->SetText(Facility.InstanceLabel);
	if (Text_Location) Text_Location->SetText(Facility.LocationText);
}

void UFF_PrepProgramFacilityRow::SetFacilityState(bool bSelected, bool bScheduled)
{
	bFacilitySelected = bSelected;
	if (Text_SelectionMark) Text_SelectionMark->SetText(FText::FromString(bSelected ? TEXT("●") : TEXT("○")));
	RefreshRowBackground();
	if (Text_Status)
	{
		Text_Status->SetText(ScheduledText);
		Text_Status->SetVisibility(bScheduled ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_PrepProgramFacilityRow::RefreshRowBackground()
{
	if (!Border_Selection) return;
	FSlateBrush Brush = NormalBrush;
	if (bFacilitySelected)
	{
		Brush.TintColor = bPointerOverRow ? SelectedHoveredBackgroundColor : SelectedBackgroundColor;
		Brush.OutlineSettings.Color = SelectedOutlineColor;
	}
	else if (bPointerOverRow)
	{
		Brush.TintColor = HoveredBackgroundColor;
	}
	Border_Selection->SetBrush(Brush);
}

void UFF_PrepProgramFacilityRow::HandleSelectClicked()
{
	OnFacilitySelected.ExecuteIfBound(FacilityIndex);
}

void UFF_PrepProgramFacilityRow::HandleLocateClicked()
{
	OnLocateRequested.ExecuteIfBound(FacilityIndex);
}
