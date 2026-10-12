// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LiveFacilityRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/FF_UIDisplayTextLibrary.h"

void UFF_LiveFacilityRowWidget::SetRowData(const FFF_LiveFacilityRow& InRow)
{
	RowData = InRow;
	bHasRowData = true;
	RefreshRow();
}

void UFF_LiveFacilityRowWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (bHasRowData)
	{
		RefreshRow();
	}
}

void UFF_LiveFacilityRowWidget::RefreshRow()
{
	if (Text_Name)
	{
		// 식별자를 임의로 번역하지 않고 원본 표시 이름을 그대로 쓴다.
		Text_Name->SetText(RowData.View.DisplayName);
		Text_Name->SetToolTipText(RowData.View.DisplayName);
	}

	const FText* StatusText = &UnknownText;
	const FLinearColor* StatusColor = &UnknownBackgroundColor;
	switch (RowData.Alert)
	{
	case EFF_LiveFacilityAlert::Ok:
		StatusText = &OkText;
		StatusColor = &OkBackgroundColor;
		break;
	case EFF_LiveFacilityAlert::Watch:
		StatusText = &WatchText;
		StatusColor = &WatchBackgroundColor;
		break;
	case EFF_LiveFacilityAlert::Blocked:
		StatusText = &BlockedText;
		StatusColor = &BlockedBackgroundColor;
		break;
	default:
		break;
	}
	if (Text_Status)
	{
		Text_Status->SetText(*StatusText);
	}
	if (Border_Status)
	{
		Border_Status->SetBrushColor(*StatusColor);
	}
	if (Text_Reason)
	{
		const FText Reason = GetReasonText();
		Text_Reason->SetText(Reason);
		Text_Reason->SetVisibility(Reason.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if (Text_Waiting)
	{
		Text_Waiting->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.View.WaitingCount)}}));
	}
	if (Text_InService)
	{
		Text_InService->SetText(FText::Format(TeamCountTextFormat,
			FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.View.ActiveServiceCount)}}));
	}
	if (Text_ExpectedWait)
	{
		// 예상 대기는 현재 값, 툴팁 평균은 완료한 고객의 과거 값이다. 모르는 시간을 0초로 바꾸지 않는다.
		Text_ExpectedWait->SetText(RowData.View.ExpectedWaitSeconds >= 0
			? FText::Format(ExpectedWaitTextFormat,
				FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(RowData.View.ExpectedWaitSeconds)}})
			: MissingValueText);
		Text_ExpectedWait->SetToolTipText(RowData.View.AverageWaitSeconds >= 0
			? FText::Format(AverageWaitTooltipFormat,
				FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(RowData.View.AverageWaitSeconds)}})
			: AverageWaitUnknownText);
	}
}

FText UFF_LiveFacilityRowWidget::GetReasonText() const
{
	// 기존 ResolveAlert/DescribeAlert와 같은 우선순위이며, 문구만 WBP 기본값으로 교체한다.
	switch (RowData.Alert)
	{
	case EFF_LiveFacilityAlert::Blocked:
		if (RowData.View.bNeedsRelocation)
		{
			return NeedsRelocationText;
		}
		if (RowData.View.QueueState == EFF_LiveFacilityQueueState::Blocked)
		{
			return QueueBlockedText;
		}
		return FText::Format(NotActiveTextFormat,
			FFormatNamedArguments{{TEXT("State"), UFF_UIDisplayTextLibrary::GetFacilityStateText(RowData.View.State)}});
	case EFF_LiveFacilityAlert::Watch:
		return RowData.View.QueueState == EFF_LiveFacilityQueueState::Full
			? QueueFullText
			: FText::Format(LongQueueTextFormat,
				FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(RowData.View.WaitingCount)}});
	case EFF_LiveFacilityAlert::Unknown:
		return UnknownReasonText;
	default:
		return FText::GetEmpty();
	}
}
