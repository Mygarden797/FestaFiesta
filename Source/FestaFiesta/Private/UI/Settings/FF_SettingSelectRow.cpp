// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingSelectRow.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

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
