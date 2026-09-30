// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Frontend/FF_SaveListProvider.h"
#include "UI/Widgets/FF_FocusableWidgetBase.h"
#include "FF_SaveSlotCard.generated.h"

class UButton;
class UImage;
class UTextBlock;

/** 어느 슬롯이 선택됐는지 알린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSaveSlotSelected, FName, SlotId);

/** 이 슬롯을 지워 달라고 알린다. 확인(두 번 누르기)을 거친 뒤에만 울린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSaveSlotDeleteRequested, FName, SlotId);

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

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnSaveSlotDeleteRequested OnSaveSlotDeleteRequested;

protected:
	// 디자이너에서도 삭제 버튼 모양이 보이게 여기서 한 번 입힌다.
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// 카드에서 마우스가 나가면 삭제 확인을 푼다. 한 번 눌러 둔 채 다른 카드로 가서 잊는 것을 막는다.
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

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

	/**
	 * 삭제 버튼. 없으면 삭제를 제공하지 않는다(구형 WBP 도 그대로 돈다).
	 *
	 * 한 번 누르면 문구가 `한 번 더 누르면 삭제` 로 바뀌고, 한 번 더 눌러야 지운다.
	 * 되돌릴 수 없는 동작이라 한 번에 지우지 않는다. 확인 창을 따로 띄우지 않는 이유는
	 * 설정 화면의 `변경 버리기` 와 같은 방식으로 맞추려는 것이다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Delete;

	/** 삭제 버튼 문구. 없으면 버튼 안의 글자를 찾아 쓴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Delete;

private:
	UFUNCTION()
	void HandleDeleteClicked();

	// 삭제 확인 상태를 바꾸고 문구와 모양을 맞춘다.
	void SetDeleteArmed(bool bArmed);

	/* 삭제 버튼 모양을 코드에서 입힌다. 평소에는 옅은 테두리의 작은 알약, 확인 중에는 붉은 알약.

	   WBP 에 버튼을 넣으면 엔진 기본 회색 버튼이 되어 크림색 카드에서 튄다. 색을 손으로
	   맞추면 다른 카드·화면과 조금씩 어긋나서, 팀 팔레트(UFF_UIPalette InGame)에서 읽어 입힌다.
	   확인 상태를 색으로도 구분하는 이유는 문구만 바뀌면 한 번 더 누르면 지워진다는 것을 놓치기 쉬워서다.
	*/
	void ApplyDeleteButtonStyle(bool bArmed);

	UTextBlock* ResolveDeleteLabel() const;

	FName CachedSlotId;

	// 삭제를 한 번 눌러 둔 상태. 한 번 더 누르면 지운다.
	bool bDeleteArmed = false;
};
