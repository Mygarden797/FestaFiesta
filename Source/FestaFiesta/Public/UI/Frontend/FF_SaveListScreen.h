// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_SaveListScreen.generated.h"

class UFF_CommonButtonBase;
class UFF_SaveSlotCard;
class UPanelWidget;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnSaveListBackRequested);

/**
 * 저장 기록 목록. 자동 저장과 수동 저장을 그룹으로 나눠 보여준다.
 *
 * === 불러오기를 하지 않는다 ===
 * 저장 원본과 불러오기 실행은 `UFF_FestivalProgressionSubsystem`(김시리) 이 소유하고
 * **그 API 가 아직 없다.** 이 화면은 목록을 표시하고 선택을 알리는 데까지만 한다.
 * `IsBackedByRealSaveSystem()` 이 false 인 동안 **`저장 기능 준비 중` 을 화면에 함께 띄운다** —
 * 진짜처럼 보이는 가짜 목록은 시연에서 오해를 만든다.
 *
 * === 그룹이 비면 그룹째로 숨긴다 ===
 * 자동 저장만 있으면 `수동 저장` 머리글을 남기지 않는다. 빈 칸을 남기지 않는 규칙과 같다.
 *
 * === 카드는 색으로 자동/수동을 구분하지 않는다 ===
 * `UFF_SaveSlotCard` 가 아이콘 자리 + 라벨을 함께 쓴다 (AT-011).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Save List Screen"))
class FESTAFIESTA_API UFF_SaveListScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnSaveListBackRequested OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleBackClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleSlotSelected(FName SlotId);

	/** 목록이 바뀌면 다시 그린다. Provider 가 알려준다. */
	UFUNCTION()
	void HandleSaveListChanged();

	/** 목록 전체를 다시 만든다. 부분 갱신을 하지 않는다. */
	void RebuildList();

	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UVerticalBox> VerticalBox_Auto;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UVerticalBox> VerticalBox_Manual;

	/** `자동 저장` 그룹 머리글. 그룹이 비면 함께 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> Panel_AutoGroup;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> Panel_ManualGroup;

	/** 기록이 하나도 없을 때만 보인다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Empty;

	/** `저장 기능 준비 중`. 실제 저장 시스템이 붙으면 숨는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SaveSystemNotice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	/** 카드에 쓸 위젯. 보통 `WBP_FF_SaveSlotCard`. 없으면 목록을 그리지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	TSubclassOf<UFF_SaveSlotCard> CardWidgetClass;
};
