// FF_SettingSliderRow.h
// 역할: 범위 안의 값을 슬라이더로 바꾸는 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_SettingSliderRow.generated.h"

class USlider;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSettingSliderChanged, float, Value);

/** Light Teal 설정 화면의 슬라이더 한 행. 값 범위와 문구는 부모 WBP 기본값에서 정한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Setting Slider Row"))
class FESTAFIESTA_API UFF_SettingSliderRow : public UFF_FocusableWidgetBase
{
	GENERATED_BODY()

public:
	UFF_SettingSliderRow(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void Configure(const FText& InLabel, const FText& InDescription, float InitialValue,
		float MinValue, float MaxValue, float StepSize, bool bInDisplayAsPercent = true);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnSettingSliderChanged OnValueChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleSliderChanged(float Value);

	void RefreshValueText(float Value);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<USlider> Slider_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Value;

private:
	bool bDisplayAsPercent = true;
};
