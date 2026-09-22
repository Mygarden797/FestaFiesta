// FF_SettingSliderRow.cpp
// 역할: 범위 안의 값을 슬라이더로 바꾸는 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingSliderRow.h"

#include "Components/Slider.h"
#include "Components/TextBlock.h"

UFF_SettingSliderRow::UFF_SettingSliderRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFrontendPalette = false;
}

void UFF_SettingSliderRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Slider_Value)
	{
		Slider_Value->OnValueChanged.AddUniqueDynamic(this, &UFF_SettingSliderRow::HandleSliderChanged);
	}
}

void UFF_SettingSliderRow::NativeDestruct()
{
	if (Slider_Value)
	{
		Slider_Value->OnValueChanged.RemoveDynamic(this, &UFF_SettingSliderRow::HandleSliderChanged);
	}
	Super::NativeDestruct();
}

void UFF_SettingSliderRow::Configure(const FText& InLabel, const FText& InDescription, float InitialValue,
	float MinValue, float MaxValue, float StepSize, bool bInDisplayAsPercent)
{
	bDisplayAsPercent = bInDisplayAsPercent;
	if (Text_Label)
	{
		Text_Label->SetText(InLabel);
	}
	if (Text_Description)
	{
		Text_Description->SetText(InDescription);
		Text_Description->SetVisibility(
			InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (Slider_Value)
	{
		Slider_Value->SetMinValue(MinValue);
		Slider_Value->SetMaxValue(MaxValue);
		Slider_Value->SetStepSize(StepSize);
		Slider_Value->SetValue(FMath::Clamp(InitialValue, MinValue, MaxValue));
	}
	RefreshValueText(InitialValue);
}

void UFF_SettingSliderRow::HandleSliderChanged(float Value)
{
	RefreshValueText(Value);
	OnValueChanged.Broadcast(Value);
}

void UFF_SettingSliderRow::RefreshValueText(float Value)
{
	if (!Text_Value)
	{
		return;
	}

	if (bDisplayAsPercent)
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(0);
		Text_Value->SetText(FText::AsPercent(Value, &Options));
	}
	else
	{
		FNumberFormattingOptions Options;
		Options.SetMaximumFractionalDigits(2);
		Text_Value->SetText(FText::AsNumber(Value, &Options));
	}
}
