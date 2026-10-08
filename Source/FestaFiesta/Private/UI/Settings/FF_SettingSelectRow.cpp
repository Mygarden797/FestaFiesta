// FF_SettingSelectRow.cpp
// 역할: 여러 선택지 중 하나를 고르는 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingSelectRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UI/Settings/FF_FestivalOptionsStyle.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"

UFF_SettingSelectRow::UFF_SettingSelectRow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFrontendPalette = false;
}

void UFF_SettingSelectRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Previous)
	{
		Button_Previous->OnClicked.AddUniqueDynamic(this, &UFF_SettingSelectRow::HandlePreviousClicked);
	}
	if (Button_Next)
	{
		Button_Next->OnClicked.AddUniqueDynamic(this, &UFF_SettingSelectRow::HandleNextClicked);
	}
}

void UFF_SettingSelectRow::NativePreConstruct()
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
	if (Text_Value)
	{
		Text_Value->SetMinDesiredWidth(200.f);
		Text_Value->SetJustification(ETextJustify::Center);
	}
	for (UButton* Button : { Button_Previous.Get(), Button_Next.Get() })
	{
		if (!Button)
		{
			continue;
		}
		FButtonStyle Style = Button->GetStyle();
		Style.Normal = FFFestivalOptionsStyle::Rounded(T.Hover, T.LineStrong, 8.f);
		Style.Hovered = FFFestivalOptionsStyle::Rounded(T.Selected, T.Green, 8.f, 2.f);
		Style.Pressed = FFFestivalOptionsStyle::Rounded(T.Selected, T.GreenDeep, 8.f, 2.f);
		Style.Disabled = FFFestivalOptionsStyle::Rounded(T.Disabled, T.Line, 8.f);
		Style.NormalPadding = FMargin(12.f, 6.f);
		Style.PressedPadding = Style.NormalPadding;
		Button->SetStyle(Style);
		if (UTextBlock* Arrow = Cast<UTextBlock>(Button->GetContent()))
		{
			FSlateFontInfo Font = Arrow->GetFont();
			Font.Size = 24;
			Font.TypefaceFontName = TEXT("SemiBold");
			Arrow->SetFont(Font);
			Arrow->SetColorAndOpacity(T.Ink);
			Arrow->SetMinDesiredWidth(20.f);
			Arrow->SetJustification(ETextJustify::Center);
			if (UButtonSlot* ArrowLayout = Cast<UButtonSlot>(Arrow->Slot))
			{
				ArrowLayout->SetPadding(FMargin(0.f));
			}
		}
		Button->SetToolTipText(Button == Button_Previous
			? NSLOCTEXT("FestaFiestaFestivalSettings", "PreviousValue", "이전 값 선택")
			: NSLOCTEXT("FestaFiestaFestivalSettings", "NextValue", "다음 값 선택"));
	}
	if (Image_FocusRing)
	{
		Image_FocusRing->SetBrush(FFFestivalOptionsStyle::Rounded(FLinearColor::Transparent, T.Green, 12.f, 2.f));
		Image_FocusRing->SetColorAndOpacity(FLinearColor::White);
	}
}

void UFF_SettingSelectRow::NativeDestruct()
{
	if (Button_Previous)
	{
		Button_Previous->OnClicked.RemoveDynamic(this, &UFF_SettingSelectRow::HandlePreviousClicked);
	}
	if (Button_Next)
	{
		Button_Next->OnClicked.RemoveDynamic(this, &UFF_SettingSelectRow::HandleNextClicked);
	}
	Super::NativeDestruct();
}

FReply UFF_SettingSelectRow::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left)
	{
		MoveSelection(-1);
		return FReply::Handled();
	}
	if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right)
	{
		MoveSelection(1);
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_SettingSelectRow::Configure(FName InSettingId, const FText& InLabel,
	const FText& InDescription, const TArray<FText>& InOptions, int32 InitialIndex)
{
	SettingId = InSettingId;
	Options = InOptions;
	SelectedIndex = Options.IsEmpty() ? INDEX_NONE : FMath::Clamp(InitialIndex, 0, Options.Num() - 1);

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

	const bool bCanChoose = Options.Num() > 1;
	if (Button_Previous)
	{
		Button_Previous->SetIsEnabled(bCanChoose);
	}
	if (Button_Next)
	{
		Button_Next->SetIsEnabled(bCanChoose);
	}
	RefreshValue();
}

void UFF_SettingSelectRow::HandlePreviousClicked()
{
	MoveSelection(-1);
}

void UFF_SettingSelectRow::HandleNextClicked()
{
	MoveSelection(1);
}

void UFF_SettingSelectRow::MoveSelection(int32 Delta)
{
	if (Options.Num() <= 1 || Delta == 0)
	{
		return;
	}

	SelectedIndex = (SelectedIndex + Delta + Options.Num()) % Options.Num();
	RefreshValue();
	OnSelectionChanged.Broadcast(SettingId, SelectedIndex);
}

void UFF_SettingSelectRow::RefreshValue()
{
	if (Text_Value)
	{
		Text_Value->SetText(Options.IsValidIndex(SelectedIndex)
			? Options[SelectedIndex] : FText::FromString(TEXT("—")));
	}
}
