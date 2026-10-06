// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LiveQueueRowWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

UFF_LiveQueueRowWidget::UFF_LiveQueueRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateLabels = {
		{EFF_LiveFacilityQueueState::Open, NSLOCTEXT("FestaFiestaLiveQueueRow", "Open", "접수 중")},
		{EFF_LiveFacilityQueueState::Full, NSLOCTEXT("FestaFiestaLiveQueueRow", "Full", "정원 참")},
		{EFF_LiveFacilityQueueState::Closed, NSLOCTEXT("FestaFiestaLiveQueueRow", "Closed", "미접수")},
		{EFF_LiveFacilityQueueState::Draining, NSLOCTEXT("FestaFiestaLiveQueueRow", "Draining", "마감 중")},
		{EFF_LiveFacilityQueueState::Blocked, NSLOCTEXT("FestaFiestaLiveQueueRow", "Blocked", "막힘")},
		{EFF_LiveFacilityQueueState::Unknown, NSLOCTEXT("FestaFiestaLiveQueueRow", "Unknown", "미확인")}};
	StateBackgroundColors = {
		{EFF_LiveFacilityQueueState::Open, FLinearColor::FromSRGBColor(FColor(229, 245, 233))},
		{EFF_LiveFacilityQueueState::Full, FLinearColor::FromSRGBColor(FColor(255, 245, 220))},
		{EFF_LiveFacilityQueueState::Closed, FLinearColor::FromSRGBColor(FColor(238, 242, 243))},
		{EFF_LiveFacilityQueueState::Draining, FLinearColor::FromSRGBColor(FColor(231, 244, 250))},
		{EFF_LiveFacilityQueueState::Blocked, FLinearColor::FromSRGBColor(FColor(255, 233, 225))},
		{EFF_LiveFacilityQueueState::Unknown, FLinearColor::FromSRGBColor(FColor(238, 242, 243))}};
}

void UFF_LiveQueueRowWidget::SetRowData(const FFF_LiveFacilityView& InRow)
{
	RowData = InRow;
	bHasRowData = true;
	RefreshRow();
}

void UFF_LiveQueueRowWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (bHasRowData)
	{
		RefreshRow();
	}
}

void UFF_LiveQueueRowWidget::RefreshRow()
{
	if (Text_Name)
	{
		Text_Name->SetText(RowData.DisplayName);
		Text_Name->SetToolTipText(RowData.DisplayName);
	}
	if (Text_Waiting)
	{
		Text_Waiting->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.WaitingCount)}}));
	}
	if (Text_InService)
	{
		Text_InService->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.ActiveServiceCount)}}));
	}
	if (Text_ExpectedWait)
	{
		Text_ExpectedWait->SetText(RowData.ExpectedWaitSeconds >= 0
			? FText::Format(ExpectedWaitTextFormat,
				FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(RowData.ExpectedWaitSeconds)}})
			: MissingValueText);
		Text_ExpectedWait->SetToolTipText(RowData.AverageWaitSeconds >= 0
			? FText::Format(AverageWaitTooltipFormat,
				FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(RowData.AverageWaitSeconds)}})
			: AverageWaitUnknownText);
	}
	if (Text_Status)
	{
		const FText* Label = StateLabels.Find(RowData.QueueState);
		Text_Status->SetText(Label ? *Label : NSLOCTEXT("FestaFiestaLiveQueueRow", "Unknown", "미확인"));
	}
	if (Border_Status)
	{
		const FLinearColor* Color = StateBackgroundColors.Find(RowData.QueueState);
		Border_Status->SetBrushColor(Color ? *Color : FLinearColor::White);
	}
	if (Image_Status)
	{
		const TObjectPtr<UTexture2D>* Icon = StateIcons.Find(RowData.QueueState);
		// 크기와 틴트는 WBP Designer 값을 유지하고 텍스처만 상태에 맞춰 바꾼다.
		Image_Status->SetBrushFromTexture(Icon ? Icon->Get() : nullptr, false);
		Image_Status->SetVisibility(Icon && Icon->Get()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
