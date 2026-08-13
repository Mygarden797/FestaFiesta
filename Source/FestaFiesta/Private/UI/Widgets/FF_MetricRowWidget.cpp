// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_MetricRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIMetricBandSettings.h"

void UFF_MetricRowWidget::SetMetric(const FText& InCaption, int32 InValue)
{
	if (Text_Caption)
	{
		Text_Caption->SetText(InCaption);
	}

	SetValue(InValue);
}

void UFF_MetricRowWidget::SetValue(int32 InValue)
{
	CurrentValue = InValue;

	if (Text_Value)
	{
		// 숫자 서식은 C++에서 끝낸다. 위젯 그래프에서 문자열을 만들면 리뷰가 안 된다.
		Text_Value->SetText(FText::AsNumber(InValue));
	}

	RefreshBand();
}

void UFF_MetricRowWidget::SetBandVisible(bool bVisible)
{
	bBandVisible = bVisible;

	// Collapsed를 쓴다. Hidden은 자리를 남겨서 금액 줄의 오른쪽에 빈 칸이 생긴다.
	const ESlateVisibility TargetVisibility = bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;

	if (Text_Band)
	{
		Text_Band->SetVisibility(TargetVisibility);
	}

	if (Image_Band)
	{
		Image_Band->SetVisibility(TargetVisibility);
	}

	RefreshBand();
}

void UFF_MetricRowWidget::RefreshBand()
{
	if (!bBandVisible)
	{
		// 숨긴 상태에서 구간을 계산하지 않는다. 금액은 0~100 기준에 맞지 않아 항상 "나쁨"이 된다.
		return;
	}

	ApplyBand(CurrentValue, Text_Band, Image_Band);
}

void UFF_MetricRowWidget::ApplyBand(int32 Value, UTextBlock* BandLabel, UImage* BandIcon)
{
	if (!BandLabel && !BandIcon)
	{
		return;
	}

	const UFF_UIMetricBandSettings* Settings = UFF_UIMetricBandSettings::Get();
	const FFF_UIMetricBand* Band = Settings ? Settings->FindBand(Value) : nullptr;

	if (BandLabel)
	{
		// 구간을 못 찾으면 비운다. 마지막 구간의 라벨이 남아 있으면 틀린 상태를 보여주게 된다.
		BandLabel->SetText(Band ? Band->Label : FText::GetEmpty());
	}

	if (!BandIcon)
	{
		return;
	}

	if (!Band)
	{
		BandIcon->SetBrushFromTexture(nullptr);
		return;
	}

	// 아이콘 에셋이 아직 없을 수 있다. 그래도 색은 반영한다.
	// 작은 UI 아이콘이라 동기 로드로 둔다. 개수가 늘면 비동기로 바꾼다.
	if (UTexture2D* IconTexture = Band->Icon.LoadSynchronous())
	{
		BandIcon->SetBrushFromTexture(IconTexture);
	}

	// 흰색 단색 텍스처를 구간 색으로 틴트한다. 텍스처를 구간마다 따로 칠하지 않는다.
	BandIcon->SetColorAndOpacity(Band->Color);
}
