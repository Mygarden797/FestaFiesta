// FF_SettingToggleRow.cpp
// 역할: 켜기·끄기 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_SettingToggleRow.h"

#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

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
}

void UFF_SettingToggleRow::HandleCheckStateChanged(bool bChecked)
{
	OnValueChanged.Broadcast(bChecked);
}
