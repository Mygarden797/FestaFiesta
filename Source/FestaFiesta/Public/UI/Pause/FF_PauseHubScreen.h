// FF_PauseHubScreen.h
// 역할: 일시정지 메뉴에서 재개·설정·도움말·나가기 항목을 연결한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_PauseHubScreen.generated.h"

class UButton;
class UImage;
class UTextBlock;

/** 일시정지 허브에서 선택할 수 있는 프로젝트 범위의 목적지. */
UENUM(BlueprintType)
enum class EFF_PauseHubAction : uint8
{
	Resume,
	SystemSettings,
	KeyboardGuide,
	Account,
	ExitGame,
	ReturnToAtlas
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnPauseHubActionRequested, EFF_PauseHubAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPauseHubReleased);

/**
 * ESC/F5가 처음 여는 LightTeal 일시정지 허브.
 *
 * 이 화면은 어디로 갈지만 요청한다. Pause/Modal 소유권과 화면 스택 전환은
 * UFF_FestivalSettingsSubsystem 한 곳이 관리해 설정 화면 왕복 중 상태가 풀리지 않게 한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Pause Hub Screen"))
class FESTAFIESTA_API UFF_PauseHubScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Pause")
	FFF_OnPauseHubActionRequested OnActionRequested;

	/** Root/Travel 등 외부 소유자가 화면을 먼저 제거했을 때 세션 lease를 회수하기 위한 신호. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Pause")
	FFF_OnPauseHubReleased OnReleased;

	/** raw 입력과 Slate가 같은 키를 본 경우에도 한 프레임에 한 번만 요청한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Pause")
	bool RequestAction(EFF_PauseHubAction Action);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 현재 축제의 실제 읽기 모델로 우측 운영 카드를 갱신한다. */
	void RefreshFestivalContext();
	void RefreshFocusPresentation();
	void RefreshOptionKeyHints();
	void SetContextCopy(EFF_PauseHubAction Action);

	UFUNCTION() void HandleResumeClicked();
	UFUNCTION() void HandleSettingsClicked();
	UFUNCTION() void HandleKeyboardClicked();
	UFUNCTION() void HandleAccountClicked();
	UFUNCTION() void HandleReturnToAtlasClicked();
	UFUNCTION() void HandleExitClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Resume;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Settings;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Keyboard;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Account;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_ReturnToAtlas;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Exit;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UButton> Button_Close;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusResume;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusSettings;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusKeyboard;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusAccount;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusReturnToAtlas;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UImage> Image_FocusExit;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_HeaderHint;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_GuideBody;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_KeyHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_FestivalName;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_DayStage;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_Clock;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_Gold;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_Visitors;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_Congestion;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_SelectionEyebrow;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_SelectionTitle;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|Pause")
	TObjectPtr<UTextBlock> Text_SelectionDescription;

private:
	EFF_PauseHubAction PresentedAction = EFF_PauseHubAction::Resume;
	TArray<FKey> PresentedOpenCloseKeys;
	uint64 LastActionRequestFrame = MAX_uint64;
};
