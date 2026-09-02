// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/Atlas/FF_AtlasScreenGeo.h"
#include "UI/ClosingHUD/FF_ClosingHudTypes.h"
#include "FF_ClosingPanelBase.generated.h"

class UButton;
class UFF_ClosingHudScreen;
class UTextBlock;

/**
 * 결산 패널의 공통 껍데기. 제목·닫기·수명·끌어 옮기기를 맡는다.
 *
 * === 끌어 옮기기 (2026-09-02 추가) ===
 * 처음에는 "결산은 읽기만 하니 옮길 이유가 없다" 고 뺐는데, 실기에서 **패널이 뒤의
 * 결과 화면을 가려서** 옮기고 싶어졌다. 준비·라이브와 같은 구현이다.
 *
 * 🔴 함정은 `docs/memory/prep-hud-reference.md` 3절에 있다 — **캡처하지 않고**,
 * 이동은 `NativeTick` 에서 커서를 직접 읽는다. 이 게임의 입력 모드에서는 마우스 이동
 * 이벤트가 위젯까지 안 오는 구간이 있다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Closing Panel Base"))
class FESTAFIESTA_API UFF_ClosingPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 화면이 만들자마자 부른다. 소유자와 제목을 넘긴다. */
	void InitializePanel(UFF_ClosingHudScreen* InOwningScreen, EFF_ClosingTab InTab, const FText& InTitle);

	/** 화면이 열고 닫을 때 부른다. **패널이 스스로 열고 닫지 않는다.** */
	void HandlePanelOpened();
	void HandlePanelClosed();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Closing")
	EFF_ClosingTab GetTab() const { return Tab; }

protected:
	virtual void NativeOnInitialized() override;

	/** C++ 파생용. `Super::` 를 부른 뒤 자기 것을 한다. */
	virtual void NativeOnPanelOpened() {}
	virtual void NativeOnPanelClosed() {}

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Closing", meta = (DisplayName = "On Panel Opened"))
	void K2_OnPanelOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Closing", meta = (DisplayName = "On Panel Closed"))
	void K2_OnPanelClosed();

	UFUNCTION()
	void HandleCloseClicked();

	/** 값이 비면 칸째 접는다. 빈 줄을 남기면 고장난 것으로 보인다. */
	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	// --- 끌어 옮기기. 자세한 경위는 `prep-hud-reference.md` 3절 ---

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 끌어 옮기기를 이 패널에서 허용할지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing")
	bool bAllowDragToMove = true;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing")
	TObjectPtr<UTextBlock> Text_PanelTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing")
	TObjectPtr<UButton> Button_Close;

	/** 소유 화면. 닫기는 여기로 요청한다 — 스스로 숨으면 탭이 눌린 채로 남는다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Closing")
	TWeakObjectPtr<UFF_ClosingHudScreen> OwningScreen;

	EFF_ClosingTab Tab = EFF_ClosingTab::Report;

	/** 임계 판정과 이동량 누적. 준비·라이브와 같은 것을 쓴다. */
	FFF_AtlasDragTracker DragTracker;
};

/**
 * 아직 안 만든 탭에 띄우는 자리표시.
 *
 * 빈 화면보다 "무엇이 들어올 예정인지" 적힌 쪽이 낫다.
 */
UCLASS(meta = (DisplayName = "FF Closing Placeholder Panel"))
class FESTAFIESTA_API UFF_ClosingPlaceholderPanel : public UFF_ClosingPanelBase
{
	GENERATED_BODY()

public:
	void SetPlaceholderMessage(const FText& InMessage);

protected:
	virtual void NativeOnPanelOpened() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing")
	TObjectPtr<UTextBlock> Text_Placeholder;

private:
	FText PlaceholderMessage;
};
