#include "UI/LiveHUD/Panels/FF_LiveDensityLegendRowWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

void UFF_LiveDensityLegendRowWidget::SetRowData(const FFF_DensityLegendRow& InRow)
{
	RowData = InRow;
	bHasRowData = true;
	RefreshRow();
}

void UFF_LiveDensityLegendRowWidget::SetCurrentPeak(const bool bInCurrentPeak)
{
	bCurrentPeak = bInCurrentPeak;
	RefreshPeak();
}

void UFF_LiveDensityLegendRowWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	if (bHasRowData)
	{
		RefreshRow();
	}
}

void UFF_LiveDensityLegendRowWidget::RefreshRow()
{
	if (Text_Grade)
	{
		Text_Grade->SetText(FText::Format(GradeTextFormat,
			FFormatNamedArguments{{TEXT("Grade"), RowData.GradeLetter}, {TEXT("Name"), RowData.GradeName}}));
	}
	if (Text_Range)
	{
		// BuildLegend가 원본 함수를 스캔한 미세 오차만 표시 시 반올림한다.
		FNumberFormattingOptions Format;
		Format.MinimumFractionalDigits = 0;
		Format.MaximumFractionalDigits = 2;
		const FFormatNamedArguments Arguments{
			{TEXT("Min"), FText::AsNumber(RowData.MinDensity, &Format)},
			{TEXT("Max"), FText::AsNumber(RowData.MaxDensity, &Format)}};
		Text_Range->SetText(FText::Format(RowData.HasUpperBound() ? BoundedRangeTextFormat : OpenRangeTextFormat, Arguments));
	}
	if (Border_GradeColor)
	{
		Border_GradeColor->SetBrushColor(FLinearColor::FromSRGBColor(FCrowdDensityGrid::LOSToColor(RowData.Grade)));
	}
	RefreshPeak();
}

void UFF_LiveDensityLegendRowWidget::RefreshPeak()
{
	if (Border_Row)
	{
		Border_Row->SetBrushColor(bCurrentPeak ? PeakBackgroundColor : RowBackgroundColor);
	}
	if (Text_CurrentPeak)
	{
		Text_CurrentPeak->SetText(CurrentPeakText);
		Text_CurrentPeak->SetVisibility(bCurrentPeak ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Border_CurrentPeak)
	{
		// 배지 전체를 숨기되 공간은 유지하여 등급 갱신 시 열 폭이 움직이지 않는다.
		Border_CurrentPeak->SetVisibility(bCurrentPeak ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
