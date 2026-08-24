// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Frontend/FF_OptionRowToggle.h"

#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"

UFF_OptionRowToggle::UFF_OptionRowToggle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bUseFrontendPalette = true;
}

void UFF_OptionRowToggle::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (CheckBox_Value)
	{
		CheckBox_Value->OnCheckStateChanged.AddUniqueDynamic(
			this, &UFF_OptionRowToggle::HandleCheckStateChanged);
	}
	else
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("%s: CheckBox_Value 바인딩이 없어 값을 바꿀 수 없다."), *GetClass()->GetName());
	}
}

void UFF_OptionRowToggle::NativeDestruct()
{
	// 해제는 조건부, 정리는 무조건. Clear() 를 쓰지 않는다.
	if (CheckBox_Value)
	{
		CheckBox_Value->OnCheckStateChanged.RemoveDynamic(
			this, &UFF_OptionRowToggle::HandleCheckStateChanged);
	}

	Super::NativeDestruct();
}

void UFF_OptionRowToggle::Configure(const FText& InLabel, const FText& InDescription, bool bInitialValue)
{
	if (Text_Label)
	{
		Text_Label->SetText(InLabel);
	}

	if (Text_Description)
	{
		// 값이 없으면 칸을 숨긴다.
		Text_Description->SetText(InDescription);
		Text_Description->SetVisibility(
			InDescription.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	if (CheckBox_Value)
	{
		// SetIsChecked 는 OnCheckStateChanged 를 울리지 않는다(직접 세터다).
		// 그래서 초기값을 넣어도 OnOptionToggled 가 나가지 않는다 —
		// 나갔다면 화면을 열자마자 설정이 저장되고 구독자가 "사용자가 바꿨다" 로 오해했을 것이다.
		CheckBox_Value->SetIsChecked(bInitialValue);
	}
}

void UFF_OptionRowToggle::HandleCheckStateChanged(bool bIsChecked)
{
	OnOptionToggled.Broadcast(bIsChecked);
}
