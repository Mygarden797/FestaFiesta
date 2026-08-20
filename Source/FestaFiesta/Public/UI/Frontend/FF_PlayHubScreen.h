// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_PlayHubScreen.generated.h"

class UFF_CommonButtonBase;
class UImage;
class UPanelWidget;
class UTextBlock;

/** 플레이 허브의 메뉴 요청. 인자가 없다 — 어느 항목인지는 델리게이트가 구분한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPlayHubMenuRequested);

/**
 * `플레이` 안에서 무엇을 할지 고르는 화면.
 *
 * 타이틀 최상위 메뉴를 3개(`플레이`·`옵션`·`게임 종료`)로 유지하기 위해
 * 이어하기·새 게임을 이 아래로 내렸다 (`docs/ui/main-menu-and-save.md` 1절).
 *
 * === 저장 기록이 없는 상태가 기본이다 ===
 * 첫 실행이 기본이다. `최근 축제 계속하기` 와 `저장 기록 선택` 은 잠기고,
 * **자물쇠 자리 + `기록 없음` 문구를 함께** 띄운다. 색이나 흐린 글자만으로 잠금을 알리지 않는다 (AT-011).
 *
 * === 기본 포커스가 상태에 따라 다르다 ===
 * 저장이 없으면 `새 축제 시작`, 있으면 `최근 축제 계속하기` 다.
 * 잠긴 버튼에 포커스를 주면 게임패드로 아무것도 할 수 없는 화면이 된다.
 *
 * === 목적지를 모른다 ===
 * `UFF_TitleScreen` 과 같은 구조다. 델리게이트만 알리고 화면을 띄운
 * `UFF_UIFlowSubsystem` 이 다음 화면을 정한다. 그래야 축제·화면이 늘어도 이 클래스를 고치지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Play Hub Screen"))
class FESTAFIESTA_API UFF_PlayHubScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	/** `최근 축제 계속하기`. 저장이 없으면 발생하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnPlayHubMenuRequested OnContinueRequested;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnPlayHubMenuRequested OnNewFestivalRequested;

	/** `저장 기록 선택`. 저장이 없으면 발생하지 않는다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnPlayHubMenuRequested OnSaveListRequested;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnPlayHubMenuRequested OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	/** 저장이 없으면 `새 축제 시작`. 잠긴 버튼에 포커스를 주지 않는다. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleButtonClicked(UFF_CommonButtonBase* Button);

	/** 저장 기록 유무를 읽어 잠금 표시를 전부 다시 선언한다. 멱등이다. */
	void RefreshFromSaveList();

	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Continue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_NewFestival;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_SaveList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	/** `기록 없음`. 자물쇠 아이콘과 **함께** 쓴다 — 문구만으로도 잠금을 알 수 있어야 한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_LockedNotice;

	/** 자물쇠 자리. 지금은 빈 사각형이고 아이콘은 나중에 넣는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_LockedIcon;

	/** 우측 최근 기록 목록 자리. 기록이 없으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> Panel_Recent;

	/** `저장 기록이 없습니다` 빈 상태 문구. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_RecentEmpty;

private:
	/** 마지막으로 계산한 저장 유무. 포커스 대상을 정하는 데 쓴다. */
	bool bHasSave = false;
};
