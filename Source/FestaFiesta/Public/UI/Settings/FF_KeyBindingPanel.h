// FF_KeyBindingPanel.h
// 역할: 키 변경 입력을 받고 충돌·초기화 결과를 사용자에게 보여 준다.

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "InputCoreTypes.h"
#include "FF_KeyBindingPanel.generated.h"

class UEnhancedInputUserSettings;
class UFF_KeyBindingRuntimeSubsystem;
class UImage;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_KeyBindingKeyChosen, FKey, Key);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_KeyBindingMappingChosen, FName, MappingName);

/** A dynamically-created keyboard key used by the key-binding panel. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "FF Key Binding Key Button"))
class FESTAFIESTA_API UFF_KeyBindingKeyButton : public UButton
{
	GENERATED_BODY()

public:
	/** Updates this button's key, label, reserved/fixed-state hint, and LightTeal presentation. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Key Binding")
	void Configure(FKey InKey, FText InLabel, bool bInReserved, bool bInFixedAction = false);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Key Binding")
	FFF_KeyBindingKeyChosen OnKeyChosen;

	FKey GetConfiguredKey() const { return ConfiguredKey; }
	bool IsReservedKey() const { return bReservedKey; }
	bool IsFixedActionKey() const { return bFixedActionKey; }

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	FKey ConfiguredKey;

	UPROPERTY(Transient)
	bool bReservedKey = false;

	UPROPERTY(Transient)
	bool bFixedActionKey = false;
};

/** A row-sized button that selects which player-mappable action will be rebound. */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "FF Key Binding Action Button"))
class FESTAFIESTA_API UFF_KeyBindingActionButton : public UButton
{
	GENERATED_BODY()

public:
	/** Updates this button's mapping identity, localized action label, current key, and read-only state. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Key Binding")
	void Configure(FName InMappingName, FText InActionLabel, FKey InCurrentKey, bool bInReadOnly = false);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Key Binding")
	FFF_KeyBindingMappingChosen OnMappingChosen;

	FName GetMappingName() const { return MappingName; }
	FKey GetCurrentKey() const { return CurrentKey; }
	bool IsReadOnlyBinding() const { return bReadOnlyBinding; }

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	FName MappingName = NAME_None;

	UPROPERTY(Transient)
	FKey CurrentKey;

	UPROPERTY(Transient)
	bool bReadOnlyBinding = false;
};

/**
 * Enhanced Input key-binding UI shared by a designer WBP and a native runtime fallback.
 *
 * The panel edits the First player-mappable slot. With no action selected, pressing or clicking
 * a key inspects every current action assigned to it. Raw fixed inputs are displayed read-only.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "FF Key Binding Panel"))
class FESTAFIESTA_API UFF_KeyBindingPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_KeyBindingPanel(const FObjectInitializer& ObjectInitializer);

	/** Re-reads the active Enhanced Input profile and rebuilds the action rows immediately. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Key Binding")
	void RefreshBindings();

	/** Handles a physical key routed by this panel or its owning settings screen. */
	bool HandlePhysicalKey(FKey Key);

	/** True only while an action row is waiting for the next physical keyboard key. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Key Binding")
	bool IsCapturingKey() const { return !SelectedMappingName.IsNone(); }

	/**
	 * Explicitly releases the runtime option-key capture lease.
	 * The owner calls this before removing/rebuilding the nested panel so input
	 * restoration never depends on deferred UUserWidget destruction.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Key Binding")
	void CancelKeyCapture();

	/** Returns every current action assigned to Key, including fixed raw-input actions. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Key Binding")
	FText GetCurrentBindingSummary(FKey Key) const;

	/** The build-mode key is a raw fixed input until that consumer moves to Enhanced Input. */
	static FKey GetFixedBuildModeKey();
	static bool IsFixedActionKey(const FKey& Key);

	/** Project-authored fallback used when the local profile has no valid override. */
	static FKey GetDefaultKeyForMapping(FName MappingName);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

private:
	bool BuildFallbackWidgetTree();
	void ResolveWidgetBindings();
	void InitializeUserSettings();
	void BuildKeyboardRows();
	void BuildBindingRows();
	void BuildFixedBindingRows();
	void InspectKey(const FKey& Key);
	void TryAssignKey(const FKey& NewKey);
	void SetCaptureInProgress(bool bInCaptureInProgress);
	bool IsSupportedKeyboardKey(const FKey& Key) const;
	bool IsSystemOptionKey(const FKey& Key) const;
	bool FindConflictingMapping(const FKey& Key, FName& OutMappingName) const;
	FKey FindCurrentKey(FName MappingName) const;
	FText FindActionLabel(FName MappingName) const;
	UFF_KeyBindingRuntimeSubsystem* ResolveRuntimeSubsystem() const;
	void PersistSettings();
	void SetStatus(const FText& Message, bool bIsError = false);

	UFUNCTION()
	void HandleKeyChosen(FKey Key);

	UFUNCTION()
	void HandleMappingChosen(FName MappingName);

	UFUNCTION()
	void HandleResetAllClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UImage> Image_KeyboardArtwork;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UVerticalBox> VerticalBox_KeyboardRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UVerticalBox> VerticalBox_BindingRows;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UTextBlock> Text_CaptureStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UButton> Button_ResetAll;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"), Category = "FestaFiesta|UI|Key Binding")
	TObjectPtr<UTextBlock> Text_ResetAll;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputUserSettings> UserSettings;

	UPROPERTY(Transient)
	FName SelectedMappingName = NAME_None;
};
