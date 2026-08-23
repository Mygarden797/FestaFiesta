// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_OptionRowToggle.generated.h"

class UCheckBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnOptionToggled, bool, bEnabled);

/**
 * 옵션의 켜고/끄는 설정 한 줄.
 *
 * `UFF_FocusableWidgetBase` 를 상속한다 — 전역 `RenderFocusRule=Never` 라 체크박스에
 * 포커스가 가도 화면에 표시가 없다. **행이 포커스를 받고 링을 그린다** (C-024).
 *
 * === 설명 문구를 함께 둔다 ===
 * `움직임 감소` 같은 이름만으로는 무엇이 바뀌는지 알 수 없다. 켜면 무슨 일이 생기는지 적는다.
 * 값이 없으면 칸을 숨긴다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Option Row Toggle"))
class FESTAFIESTA_API UFF_OptionRowToggle : public UFF_FocusableWidgetBase
{
	GENERATED_BODY()

public:
	UFF_OptionRowToggle(const FObjectInitializer& ObjectInitializer);

	/** 한 번에 전부 넣는다. 라벨만 바뀌고 값이 안 바뀌는 상태를 만들지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void Configure(const FText& InLabel, const FText& InDescription, bool bInitialValue);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnOptionToggled OnOptionToggled;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleCheckStateChanged(bool bIsChecked);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UCheckBox> CheckBox_Value;
};
