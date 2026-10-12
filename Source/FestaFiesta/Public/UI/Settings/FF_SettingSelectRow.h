// FF_SettingSelectRow.h
// 역할: 여러 선택지 중 하나를 고르는 설정 행을 표시한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_SettingSelectRow.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FFF_OnSettingSelectionChanged, FName, SettingId, int32, SelectedIndex);

/**
 * 해상도·화면 모드·품질 단계처럼 유한한 선택지를 순환하는 Light Teal 설정 행.
 * 옵션 의미는 화면이 소유하고, 이 위젯은 표시와 이전/다음 탐색만 담당한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Setting Select Row"))
class FESTAFIESTA_API UFF_SettingSelectRow : public UFF_FocusableWidgetBase
{
	GENERATED_BODY()

public:
	UFF_SettingSelectRow(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Settings")
	void Configure(FName InSettingId, const FText& InLabel, const FText& InDescription,
		const TArray<FText>& InOptions, int32 InitialIndex);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Settings")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Settings")
	FFF_OnSettingSelectionChanged OnSelectionChanged;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandlePreviousClicked();

	UFUNCTION()
	void HandleNextClicked();

	void MoveSelection(int32 Delta);
	void RefreshValue();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Description;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UTextBlock> Text_Value;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Previous;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|Settings")
	TObjectPtr<UButton> Button_Next;

private:
	FName SettingId;
	TArray<FText> Options;
	int32 SelectedIndex = INDEX_NONE;
};
