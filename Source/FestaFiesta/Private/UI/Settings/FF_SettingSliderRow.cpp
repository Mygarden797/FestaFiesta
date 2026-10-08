// FF_SettingSliderRow.cpp
// 역할: 범위 안의 값을 슬라이더로 바꾸는 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingSliderRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Settings/FF_FestivalOptionsStyle.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"

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

void UFF_SettingSliderRow::NativePreConstruct()
{
	Super::NativePreConstruct();
	// The row Blueprint is shared with the title menu; only festival instances receive this style.
	if (!GetTypedOuter<UFF_FestivalSettingsScreen>() || !WidgetTree)
	{
		return;
	}

	const FFFestivalOptionsStyle::FTokens T = FFFestivalOptionsStyle::Get();
	if (USizeBox* Root = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("SizeBox_Root"))))
	{
		Root->ClearHeightOverride();
		Root->SetMinDesiredHeight(76.f);
	}
	if (UBorder* Background = Cast<UBorder>(WidgetTree->FindWidget(TEXT("Border_Background"))))
	{
		Background->SetBrush(FFFestivalOptionsStyle::Rounded(T.White, T.Line));
		Background->SetBrushColor(FLinearColor::White);
		Background->SetPadding(FMargin(20.f, 12.f));
	}
	for (UTextBlock* Text : { Text_Label.Get(), Text_Description.Get(), Text_Value.Get() })
	{
		if (!Text)
		{
			continue;
		}
		const bool bDescription = Text == Text_Description;
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = bDescription ? 14 : 18;
		Font.TypefaceFontName = bDescription ? TEXT("Medium") : TEXT("SemiBold");
		Text->SetFont(Font);
		Text->SetColorAndOpacity(bDescription ? T.Muted : T.Ink);
		Text->SetAutoWrapText(bDescription);
	}
	if (Text_Description)
	{
		if (UVerticalBoxSlot* DescriptionLayout = Cast<UVerticalBoxSlot>(Text_Description->Slot))
		{
			DescriptionLayout->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
		}
	}
	if (USizeBox* SliderBox = Cast<USizeBox>(WidgetTree->FindWidget(TEXT("SizeBox_Slider"))))
	{
		SliderBox->SetWidthOverride(270.f);
		SliderBox->SetHeightOverride(44.f);
	}
	if (Slider_Value)
	{
		FSliderStyle Style = Slider_Value->GetWidgetStyle();
		Style.BarThickness = 4.f;
		Style.NormalBarImage = FFFestivalOptionsStyle::Rounded(T.LineStrong, T.LineStrong, 2.f, 0.f);
		Style.HoveredBarImage = FFFestivalOptionsStyle::Rounded(T.Green, T.Green, 2.f, 0.f);
		Style.DisabledBarImage = FFFestivalOptionsStyle::Rounded(T.Disabled, T.Disabled, 2.f, 0.f);
		Style.NormalThumbImage = FFFestivalOptionsStyle::Rounded(T.Green, T.White, 10.f, 2.f);
		Style.NormalThumbImage.ImageSize = FVector2D(20.f, 20.f);
		Style.HoveredThumbImage = FFFestivalOptionsStyle::Rounded(T.GreenDeep, T.White, 11.f, 2.f);
		Style.HoveredThumbImage.ImageSize = FVector2D(22.f, 22.f);
		Style.DisabledThumbImage = FFFestivalOptionsStyle::Rounded(T.LineStrong, T.White, 10.f, 2.f);
		Style.DisabledThumbImage.ImageSize = FVector2D(20.f, 20.f);
		Slider_Value->SetWidgetStyle(Style);
		Slider_Value->SetSliderBarColor(FLinearColor::White);
		Slider_Value->SetSliderHandleColor(FLinearColor::White);
	}
	if (Text_Value)
	{
		Text_Value->SetMinDesiredWidth(64.f);
		Text_Value->SetJustification(ETextJustify::Right);
	}
	if (Image_FocusRing)
	{
		Image_FocusRing->SetBrush(FFFestivalOptionsStyle::Rounded(FLinearColor::Transparent, T.Green, 12.f, 2.f));
		Image_FocusRing->SetColorAndOpacity(FLinearColor::White);
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
