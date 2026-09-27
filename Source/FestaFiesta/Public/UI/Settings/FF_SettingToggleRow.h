// FF_SettingToggleRow.h
// 역할: 켜기·끄기 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_SettingToggleRow.generated.h"

class UCheckBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSettingToggleChanged, bool, bEnabled);

/** Light Teal 설정 화면의 토글 한 행. 표현은 WBP, 상태 연결은 C++이 맡는다. */
UCLASS(Abstract, meta = (DisplayName = "FF Setting Toggle Row"))
class FESTAFIESTA_API UFF_SettingToggleRow : public UFF_FocusableWidgetBase
{
	GENERATED_BODY()

public:
	UFF_SettingToggleRow(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void Configure(const FText& InLabel, const FText& InDescription, bool bInitialValue);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnSettingToggleChanged OnValueChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleCheckStateChanged(bool bChecked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UCheckBox> CheckBox_Value;
};
