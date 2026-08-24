// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Frontend/FF_SaveListProvider.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_SaveSlotCard.generated.h"

class UImage;
class UTextBlock;

/** 어느 슬롯이 선택됐는지 알린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSaveSlotSelected, FName, SlotId);

/**
 * 저장 기록 한 건을 그리는 카드.
 *
 * `UFF_FocusableWidgetBase` 를 상속해 게임패드 포커스가 보이게 한다 —
 * 버튼이 아니라 카드이므로 전역 `RenderFocusRule=Never` 아래에서는 직접 그려야 한다 (C-024).
 *
 * === 자동/수동을 색으로 구분하지 않는다 ===
 * 아이콘 자리 + 라벨(`자동 저장` / `수동 저장`)을 함께 쓴다 (AT-011).
 * 아이콘은 지금 빈 사각형이고 나중에 교체한다.
 *
 * === 불러오기는 아직 못 한다 ===
 * 선택은 알리지만 실제 불러오기는 `UFF_FestivalProgressionSubsystem` 이 소유하고
 * 그 API 가 아직 없다. 화면이 `저장 기능 준비 중` 을 함께 띄운다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Save Slot Card"))
class FESTAFIESTA_API UFF_SaveSlotCard : public UFF_FocusableWidgetBase
{
	GENERATED_BODY()

public:
	UFF_SaveSlotCard(const FObjectInitializer& ObjectInitializer);

	/** 카드 내용을 채운다. 한 번에 전부 넣는다 — 부분 세팅으로 틀린 조합을 만들지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetEntry(const FFF_SaveSlotEntry& InEntry);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnSaveSlotSelected OnSaveSlotSelected;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_FestivalName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Region;

	/** `Day 2 · 모닝체크`. 일차와 단계를 한 줄로 묶는다 — 따로 두면 빈 칸이 생긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_DayStage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SavedAt;

	/** `자동 저장` / `수동 저장`. **색과 함께 반드시 이 문구를 쓴다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Kind;

	/** 자동/수동 아이콘 자리. 흑백으로 뽑았을 때 실루엣이 달라야 한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_KindIcon;

private:
	FName CachedSlotId;
};
