// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"
#include "UI/Minigame/FF_MinigameScreenBase.h"
#include "FF_ClawMachineScreen.generated.h"

class AFF_ClawMachineArena3D;
class UBorder;
class UButton;
class UProgressBar;
class UTextBlock;
class UViewport;
class UWidget;
class UFF_ClawMachineRuntime;
class UFF_ClawMachineJoystickWidget;

/** Designer-editable presentation tokens. Gameplay values remain on UFF_ClawMachineDefinition. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_ClawMachineVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena",
		meta = (DisplayName = "3D 기계 클래스", ToolTip = "전체 기계 외형을 바꾸려면 FF Claw Machine 3D Arena의 Blueprint 자식 클래스를 지정합니다."))
	TSubclassOf<AFF_ClawMachineArena3D> ArenaActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FLinearColor ViewportBackgroundColor = FLinearColor(0.002f, 0.008f, 0.015f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float KeyLightIntensity = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float SkyLightIntensity = 0.52f;

	/** Front-on gameplay camera: depth stays readable without obscuring alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FVector CameraLocation = FVector(-1500.0f, 0.0f, 220.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena")
	FRotator CameraRotation = FRotator(-4.2f, 0.0f, 0.0f);

	/**
	 * Versioned close gameplay preset. Keeping this separate from the legacy wide-shot
	 * fields lets existing designer WBPs adopt the improved framing without rewriting
	 * their serialized VisualStyle struct.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Gameplay Camera",
		meta = (DisplayName = "근접 플레이 구도 사용", ToolTip = "유리 전면에서 인형과 집게를 크게 보여주는 플레이 전용 구도를 사용합니다. 끄면 기존 Camera Location/Rotation을 사용합니다."))
	bool bUseCloseGameplayView = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Gameplay Camera",
		meta = (DisplayName = "근접 카메라 위치"))
	FVector CloseGameplayCameraLocation = FVector(-880.0f, 0.0f, 175.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Gameplay Camera",
		meta = (DisplayName = "근접 카메라 회전"))
	FRotator CloseGameplayCameraRotation = FRotator(-6.5f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Gameplay Camera",
		meta = (ClampMin = "80.0", ClampMax = "500.0", Units = "cm", DisplayName = "캐비닛 안전 이격거리", ToolTip = "하강 확대 카메라가 유리와 조작 패널을 관통하지 않도록 보장하는 최소 거리입니다."))
	float CameraMinimumCabinetStandoffCm = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (DisplayName = "집기 확인 카메라", ToolTip = "조준 중에는 정면 고정하고, 하강·닫힘·초기 상승 구간에서만 집게 위치를 확대 추적합니다."))
	bool bEnableOperatorCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "350.0", Units = "cm", DisplayName = "집기 위치 좌우 보정"))
	float CameraLateralTrackingCm = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "250.0", Units = "cm", DisplayName = "집기 위치 원근 보정"))
	float CameraDepthTrackingCm = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "500.0", Units = "cm", DisplayName = "집기 확대 돌리 거리", ToolTip = "하강한 집게와 인형의 접촉을 확인하기 위해 카메라가 기계 쪽으로 이동하는 거리입니다."))
	float CameraActionDollyCm = 310.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "20.0", Units = "deg", DisplayName = "집기 확인 최대 좌우 회전"))
	float CameraMaxYawDegrees = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "25.0", Units = "deg", DisplayName = "집기 확인 최대 상하 회전"))
	float CameraMaxPitchDegrees = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.1", ClampMax = "1.0", DisplayName = "하강 확대 완료 비율", ToolTip = "하강 진행도의 이 비율에서 최대 확대에 도달합니다."))
	float CameraDropZoomCompletionRatio = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "0.0", ClampMax = "0.9", DisplayName = "상승 근접 유지 비율", ToolTip = "상승 초반에 집힌 인형을 근접 화면으로 유지하는 구간입니다."))
	float CameraLiftCloseupHoldRatio = 0.38f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "3D Arena|Operator Camera",
		meta = (ClampMin = "1.0", ClampMax = "20.0", DisplayName = "카메라 추종 속도"))
	float CameraFollowSpeed = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD|Joystick",
		meta = (DisplayName = "조이스틱 WBP", ToolTip = "하단 조작 패널의 디자이너 소유 WBP입니다. 로드에 실패하면 완성된 네이티브 조이스틱으로 폴백합니다."))
	TSoftClassPtr<UFF_ClawMachineJoystickWidget> JoystickWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor ScreenWashColor = FLinearColor(0.004f, 0.018f, 0.025f, 0.08f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor PanelColor = FLinearColor(0.012f, 0.065f, 0.080f, 0.97f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor PanelMutedColor = FLinearColor(0.018f, 0.095f, 0.110f, 0.94f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor AccentColor = FLinearColor(0.12f, 0.92f, 0.88f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor CoralColor = FLinearColor(1.0f, 0.30f, 0.24f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor GoldColor = FLinearColor(1.0f, 0.68f, 0.12f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor PrimaryTextColor = FLinearColor(0.94f, 1.0f, 0.98f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HUD")
	FLinearColor SecondaryTextColor = FLinearColor(0.56f, 0.86f, 0.84f, 1.0f);
};

/**
 * Keyboard/gamepad-first claw-machine screen with a complete native fallback.
 * WBP designers may replace the layout by providing widgets with the optional names below.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Claw Machine Screen"))
class FESTAFIESTA_API UFF_ClawMachineScreen : public UFF_MinigameScreenBase
{
	GENERATED_BODY()

public:
	UFF_ClawMachineScreen(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine")
	bool SetRuntime(UFF_ClawMachineRuntime* InRuntime);

	virtual bool BindRuntime(UFF_MinigameRuntime* InRuntime) override;
	virtual void ShowResult(const FFF_MinigameResult& Result) override;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine")
	void ApplySnapshot(const FFF_ClawMachineSnapshot& Snapshot);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Claw Machine")
	const FFF_ClawMachineSnapshot& GetDisplayedSnapshot() const { return DisplayedSnapshot; }

	/** True only when the loaded WBP supplied the active WidgetTree (visual-QA contract). */
	bool IsUsingDesignerWidgetTree() const { return !bBuiltNativeWidgetTree; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|Minigame|Claw Machine|Visual Style")
	FFF_ClawMachineVisualStyle VisualStyle;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyUp(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnAnalogValueChanged(const FGeometry& InGeometry, const FAnalogInputEvent& InAnalogEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

private:
	bool BuildNativeWidgetTree();
	void ResolveBindings();
	bool EnsureJoystickWidget();
	void InitializeBindings();
	bool HasRequiredBindings() const;
	bool TryInitializeArena();
	void RefreshPresentation();
	void UpdateOperatorCamera(float DeltaSeconds);
	FVector ResolveGameplayCameraLocation() const;
	FRotator ResolveGameplayCameraRotation() const;
	void UpdateMovementInput();
	FVector2D BuildDigitalMovementAxis() const;
	static FText GetPhaseText(EFF_ClawMachinePhase Phase);
	FText GetFeedbackText() const;

	UFUNCTION()
	void HandleDropClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleVirtualJoystickAxisChanged(FVector2D InputAxis);

	UPROPERTY(Transient)
	TObjectPtr<UFF_ClawMachineRuntime> Runtime;

	UPROPERTY(Transient)
	FFF_ClawMachineSnapshot DisplayedSnapshot;

	UPROPERTY(Transient)
	FFF_MinigameResult DisplayedResult;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UViewport> Viewport_ClawMachine3D;

	UPROPERTY(Transient)
	TObjectPtr<AFF_ClawMachineArena3D> ArenaActor;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Score;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Attempts;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Feedback;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UProgressBar> Progress_Phase;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_Drop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Panel_Result;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UTextBlock> Text_Result;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_DropLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_CloseLabel;

	UPROPERTY(Transient)
	TObjectPtr<UFF_ClawMachineJoystickWidget> JoystickWidget;

	bool bBuiltNativeWidgetTree = false;
	bool bBindingsInitialized = false;
	bool bArenaReady = false;
	bool bArenaConfigured = false;
	bool bHasSnapshot = false;
	bool bHasResult = false;
	bool bResultFocusApplied = false;
	bool bMoveLeft = false;
	bool bMoveRight = false;
	bool bMoveForward = false;
	bool bMoveBackward = false;
	FVector2D AnalogMovement = FVector2D::ZeroVector;
	FVector2D VirtualJoystickMovement = FVector2D::ZeroVector;
	int32 PresentedFeedbackSerial = 0;
	float FeedbackRemainingSeconds = 0.0f;
	FVector CurrentCameraLocation = FVector::ZeroVector;
	FRotator CurrentCameraRotation = FRotator::ZeroRotator;
	bool bCameraRigInitialized = false;
};
