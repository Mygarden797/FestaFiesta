// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LiveQueueRowWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/FF_UIDisplayTextLibrary.h"

UFF_LiveQueueRowWidget::UFF_LiveQueueRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	StateLabels = {
		{EFF_LiveFacilityQueueState::Open, NSLOCTEXT("FestaFiestaLiveQueueRow", "Open", "접수 중")},
		{EFF_LiveFacilityQueueState::Full, NSLOCTEXT("FestaFiestaLiveQueueRow", "Full", "정원 참")},
		{EFF_LiveFacilityQueueState::Closed, NSLOCTEXT("FestaFiestaLiveQueueRow", "Closed", "미접수")},
		{EFF_LiveFacilityQueueState::Draining, NSLOCTEXT("FestaFiestaLiveQueueRow", "Draining", "마감 중")},
		{EFF_LiveFacilityQueueState::Blocked, NSLOCTEXT("FestaFiestaLiveQueueRow", "Blocked", "막힘")},
		{EFF_LiveFacilityQueueState::Unknown, NSLOCTEXT("FestaFiestaLiveQueueRow", "Unknown", "미확인")},
		{EFF_LiveFacilityQueueState::Paused, NSLOCTEXT("FestaFiestaLiveQueueRow", "Paused", "공연 중 중단")}};
	StateBackgroundColors = {
		{EFF_LiveFacilityQueueState::Open, FLinearColor::FromSRGBColor(FColor(229, 245, 233))},
		{EFF_LiveFacilityQueueState::Full, FLinearColor::FromSRGBColor(FColor(255, 245, 220))},
		{EFF_LiveFacilityQueueState::Closed, FLinearColor::FromSRGBColor(FColor(238, 242, 243))},
		{EFF_LiveFacilityQueueState::Draining, FLinearColor::FromSRGBColor(FColor(231, 244, 250))},
		{EFF_LiveFacilityQueueState::Blocked, FLinearColor::FromSRGBColor(FColor(255, 233, 225))},
		{EFF_LiveFacilityQueueState::Unknown, FLinearColor::FromSRGBColor(FColor(238, 242, 243))},
		{EFF_LiveFacilityQueueState::Paused, FLinearColor::FromSRGBColor(FColor(231, 244, 250))}};
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
	if (RowData.bSeatedVenue)
	{
		RefreshSeatedRow();
		return;
	}
	// 행은 순서별로 재사용된다. 관람석이던 행의 안내 툴팁을 남기지 않는다.
	if (Text_Waiting)
	{
		Text_Waiting->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.WaitingCount)}}));
		Text_Waiting->SetToolTipText(FText::GetEmpty());
	}
	if (Text_InService)
	{
		Text_InService->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.ActiveServiceCount)}}));
		Text_InService->SetToolTipText(FText::GetEmpty());
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
	// 저장된 WBP의 상태 표에 나중에 생긴 상태(공연 중 중단)가 없을 수 있다. 공용 문구로 채운다.
	const FText* Label = StateLabels.Find(RowData.QueueState);
	const FLinearColor* Color = StateBackgroundColors.Find(RowData.QueueState);
	const TObjectPtr<UTexture2D>* Icon = StateIcons.Find(RowData.QueueState);
	if (!Icon && RowData.QueueState == EFF_LiveFacilityQueueState::Paused)
	{
		// 잠시 안 받는 것은 닫힘과 같은 대기 아이콘을 쓴다. 구분은 문구가 한다.
		Icon = StateIcons.Find(EFF_LiveFacilityQueueState::Closed);
	}
	ApplyStatus(Label ? *Label : UFF_UIDisplayTextLibrary::GetFacilityQueueStateText(RowData.QueueState),
		Color ? *Color : FLinearColor::White, Icon ? Icon->Get() : nullptr);
}

void UFF_LiveQueueRowWidget::RefreshSeatedRow()
{
	for (UTextBlock* Value : { Text_Waiting.Get(), Text_InService.Get(), Text_ExpectedWait.Get() })
	{
		if (Value)
		{
			Value->SetText(MissingValueText);
			Value->SetToolTipText(SeatedTooltipText);
		}
	}
	ApplyStatus(SeatedStatusText, SeatedBackgroundColor, SeatedIcon);
	if (Text_Status) { Text_Status->SetToolTipText(SeatedTooltipText); }
}

void UFF_LiveQueueRowWidget::ApplyStatus(const FText& Label, const FLinearColor& Background, UTexture2D* Icon)
{
	if (Text_Status)
	{
		Text_Status->SetText(Label);
		Text_Status->SetToolTipText(FText::GetEmpty());
	}
	if (Border_Status) { Border_Status->SetBrushColor(Background); }
	if (Image_Status)
	{
		// 크기와 틴트는 WBP Designer 값을 유지하고 텍스처만 상태에 맞춰 바꾼다.
		Image_Status->SetBrushFromTexture(Icon, false);
		Image_Status->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}
