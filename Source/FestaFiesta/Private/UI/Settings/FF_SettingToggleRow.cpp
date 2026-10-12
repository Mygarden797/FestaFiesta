// FF_SettingToggleRow.cpp
// 역할: 켜기·끄기 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingToggleRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Settings/FF_FestivalOptionsStyle.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"

UFF_SettingToggleRow::UFF_SettingToggleRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFrontendPalette = false;
}

void UFF_SettingToggleRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (CheckBox_Value)
	{
		CheckBox_Value->OnCheckStateChanged.AddUniqueDynamic(
			this, &UFF_SettingToggleRow::HandleCheckStateChanged);
	}
}

void UFF_SettingToggleRow::NativePreConstruct()
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
	for (UTextBlock* Text : { Text_Label.Get(), Text_Description.Get() })
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
	if (CheckBox_Value)
	{
		FCheckBoxStyle Style = CheckBox_Value->GetWidgetStyle();
		Style.UncheckedImage = FFFestivalOptionsStyle::Rounded(T.White, T.LineStrong, 6.f, 2.f);
		Style.UncheckedImage.ImageSize = FVector2D(26.f, 26.f);
		Style.UncheckedHoveredImage = FFFestivalOptionsStyle::Rounded(T.Hover, T.Green, 6.f, 2.f);
		Style.UncheckedHoveredImage.ImageSize = FVector2D(26.f, 26.f);
		Style.UncheckedPressedImage = Style.UncheckedHoveredImage;
		// Preserve the authored check glyph so the checked state is not conveyed by color alone.
		Style.CheckedImage.TintColor = T.Green;
		Style.CheckedImage.ImageSize = FVector2D(26.f, 26.f);
		Style.CheckedHoveredImage = Style.CheckedImage;
		Style.CheckedHoveredImage.TintColor = T.GreenDeep;
		Style.CheckedPressedImage = Style.CheckedHoveredImage;
		Style.Padding = FMargin(12.f, 10.f, 8.f, 10.f);
		CheckBox_Value->SetWidgetStyle(Style);

		// A state label makes the value readable and extends the click target beyond the small glyph.
		UTextBlock* State = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_FestivalToggleState")));
		if (!State && !CheckBox_Value->GetContent())
		{
			State = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_FestivalToggleState"));
			CheckBox_Value->SetContent(State);
		}
		if (State)
		{
			FSlateFontInfo Font = Text_Label ? Text_Label->GetFont() : State->GetFont();
			Font.Size = 16;
			Font.TypefaceFontName = TEXT("SemiBold");
			State->SetFont(Font);
			State->SetMinDesiredWidth(42.f);
			State->SetColorAndOpacity(T.Ink);
			State->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		RefreshFestivalToggleState();
	}
	if (Image_FocusRing)
	{
		Image_FocusRing->SetBrush(FFFestivalOptionsStyle::Rounded(FLinearColor::Transparent, T.Green, 12.f, 2.f));
		Image_FocusRing->SetColorAndOpacity(FLinearColor::White);
	}
}

void UFF_SettingToggleRow::NativeDestruct()
{
	if (CheckBox_Value)
	{
		CheckBox_Value->OnCheckStateChanged.RemoveDynamic(
			this, &UFF_SettingToggleRow::HandleCheckStateChanged);
	}
	Super::NativeDestruct();
}

void UFF_SettingToggleRow::Configure(const FText& InLabel, const FText& InDescription, bool bInitialValue)
{
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
	if (CheckBox_Value)
	{
		CheckBox_Value->SetIsChecked(bInitialValue);
	}
	RefreshFestivalToggleState();
}

void UFF_SettingToggleRow::HandleCheckStateChanged(bool bChecked)
{
	RefreshFestivalToggleState();
	OnValueChanged.Broadcast(bChecked);
}

void UFF_SettingToggleRow::RefreshFestivalToggleState()
{
	if (!GetTypedOuter<UFF_FestivalSettingsScreen>() || !WidgetTree || !CheckBox_Value)
	{
		return;
	}
	if (UTextBlock* State = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Text_FestivalToggleState"))))
	{
		State->SetText(CheckBox_Value->IsChecked()
			? NSLOCTEXT("FestaFiestaFestivalSettings", "ToggleOn", "켜짐")
			: NSLOCTEXT("FestaFiestaFestivalSettings", "ToggleOff", "꺼짐"));
	}
}
