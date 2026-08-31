// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputAction.h"
#include "Subsystems/EngineSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "FF_KeyBindingRuntimeSubsystem.generated.h"

class AGameModeBase;
class APlayerController;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputUserSettings;
class UInputMappingContext;

/** Native notification used by the owning settings screen to mirror its Save/Discard state. */
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_KeyBindingDraftChanged, bool /* bHasPendingChanges */);

/**
 * Enables the official Enhanced Input user-settings service before any local
 * player subsystem is created. This is deliberately a runtime-only CDO change:
 * it avoids taking ownership of the team's DefaultInput.ini while still making
 * packaged builds and PIE follow the same code path.
 */
UCLASS()
class FESTAFIESTA_API UFF_EnhancedInputUserSettingsBootstrapSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};

/**
 * Transient action used solely to register stable Player Mappable rows.
 * Gameplay continues to use the team's original Input Actions and modifiers.
 */
UCLASS(Transient)
class FESTAFIESTA_API UFF_KeyBindingProfileAction : public UInputAction
{
	GENERATED_BODY()

public:
	void Configure(FName MappingName, const FText& DisplayName, const FText& DisplayCategory,
		bool bInTriggerWhenPaused);
};

/**
 * Bridges Enhanced Input User Settings to existing game input without saving or
 * dirtying team-owned IMC assets.
 *
 * A transient profile context owns Player Mappable metadata and persistence.
 * Saved keys are mirrored into the already-loaded gameplay IMCs in memory only;
 * their original keys are restored when this local-player subsystem shuts down.
 * The two system option keys are implemented as paused raw bindings so they keep
 * working while the menu is open and can themselves be remapped.
 */
UCLASS()
class FESTAFIESTA_API UFF_KeyBindingRuntimeSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

	/** Creates/loads the profile and applies its current mappings. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	bool EnsureReady();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Input|User Settings")
	UEnhancedInputUserSettings* GetUserSettings() const { return UserSettings; }

	/** Current saved key, falling back to the project-authored default. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Input|User Settings")
	FKey GetCurrentKey(FName MappingName) const;

	/** Current valid option-toggle keys in primary/secondary display order. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Input|User Settings")
	TArray<FKey> GetOpenCloseKeys() const;

	/** Re-applies profile keys to gameplay and option-toggle bindings. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	void RefreshMappings();

	/**
	 * Temporarily releases system option keys while a key-capture row is waiting.
	 * This lets Esc/F5 (or their remapped replacements) reach the settings widget.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	void SetCaptureInProgress(bool bInCaptureInProgress);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Input|User Settings")
	bool IsCaptureInProgress() const { return bCaptureInProgress; }

	/**
	 * Starts one explicit key-edit transaction and snapshots every shared definition row.
	 * Repeated calls are idempotent so a rebuilt key panel cannot replace the original baseline.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	bool BeginKeyBindingEdit();

	/** Applies the in-memory draft to live controls without writing the user-settings save game. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	bool ApplyKeyBindingDraft();

	/** Applies and persists the current draft, then closes the transaction. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	bool CommitKeyBindingEdit();

	/** Restores the transaction snapshot through Enhanced Input and never writes it to disk. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Input|User Settings")
	bool RollbackKeyBindingEdit();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Input|User Settings")
	bool HasPendingKeyBindingChanges() const { return bHasPendingKeyBindingChanges; }

	/** Native-only dirty-state signal. Bind this into the outer screen's unified Save/Discard state. */
	FFF_KeyBindingDraftChanged& OnKeyBindingDraftChanged() { return KeyBindingDraftChanged; }

private:
	struct FGameplayMappingTarget
	{
		TWeakObjectPtr<UInputMappingContext> Context;
		int32 MappingIndex = INDEX_NONE;
		FName MappingName;
		FKey OriginalKey;
	};

	void BuildProfileContext();
	void ResolveGameplayMappingTargets();
	void ApplyGameplayMappings();
	void RestoreGameplayMappings();
	void ApplyOptionToggleBindings();
	void RemoveOptionToggleBindings(APlayerController* Controller);
	void RemoveConflictingDebugBindings(APlayerController* Controller) const;
	APlayerController* ResolvePlayerController() const;
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void HandleDeferredControllerReady();
	bool CaptureKeyBindingEditSnapshot();
	bool IsCurrentDraftDifferentFromSnapshot() const;
	void SetPendingKeyBindingChanges(bool bInHasPendingChanges);
	void ResetKeyBindingEditState();

	UFUNCTION()
	void HandleUserSettingsChanged(UEnhancedInputUserSettings* ChangedSettings);

	void HandleOptionsToggleInput(FKey PressedKey);

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputUserSettings> UserSettings;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> ProfileMappingContext;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_KeyBindingProfileAction>> ProfileActions;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CameraMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CharacterMappingContext;

	TArray<FGameplayMappingTarget> GameplayMappingTargets;
	TMap<FName, FKey> KeyBindingEditSnapshot;
	TWeakObjectPtr<APlayerController> CurrentController;
	FDelegateHandle PostLoginHandle;
	FFF_KeyBindingDraftChanged KeyBindingDraftChanged;
	TArray<FKey> OriginalOpenCloseKeys;
	bool bCapturedOriginalOpenCloseKeys = false;
	bool bProfileContextRegistered = false;
	bool bGameplayTargetsResolved = false;
	bool bCaptureInProgress = false;
	bool bApplyingMappings = false;
	bool bKeyBindingEditActive = false;
	bool bHasPendingKeyBindingChanges = false;
};
