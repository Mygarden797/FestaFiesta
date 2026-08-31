// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_ClawMachineJoystickWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnClawJoystickAxisChanged,
	FVector2D,
	InputAxis);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnClawJoystickDropRequested);

/**
 * Designer-owned virtual joystick presentation shared by keyboard, gamepad and pointer input.
 * WBP art may replace every brush while the normalized input contract remains native.
 */
UCLASS(BlueprintType, meta = (DisplayName = "FF Claw Machine Joystick"))
class FESTAFIESTA_API UFF_ClawMachineJoystickWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_ClawMachineJoystickWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Minigame|Claw Machine|Joystick")
	FFF_OnClawJoystickAxisChanged OnInputAxisChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Minigame|Claw Machine|Joystick")
	FFF_OnClawJoystickDropRequested OnDropRequested;

	/** Updates the visual position for keyboard/gamepad input without emitting a new input event. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine|Joystick")
	void SetDisplayedInputAxis(FVector2D InputAxis);

	/** Releases pointer capture and returns the control to neutral. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine|Joystick")
	void CancelInput();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Claw Machine|Joystick")
	FVector2D GetDisplayedInputAxis() const { return TargetInputAxis; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	bool BuildNativeWidgetTree();
	void ResolveBindings();
	FVector2D CalculatePointerAxis(const FGeometry& Geometry, const FPointerEvent& Event) const;
	void ApplyPointerAxis(const FGeometry& Geometry, const FPointerEvent& Event);
	void ReleasePointer();

	UFUNCTION()
	void HandleDropPressed();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UCanvasPanel> Canvas_Joystick;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UBorder> Panel_JoystickKnob;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|Minigame|UI")
	TObjectPtr<UButton> Button_JoystickDrop;

	FVector2D TargetInputAxis = FVector2D::ZeroVector;
	FVector2D SmoothedInputAxis = FVector2D::ZeroVector;
	bool bPointerCaptured = false;
};
