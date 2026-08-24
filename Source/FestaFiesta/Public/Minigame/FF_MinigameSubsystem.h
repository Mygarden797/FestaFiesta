// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Minigame/FF_MinigameTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Time/FF_FestivalStage.h"
#include "FF_MinigameSubsystem.generated.h"

class AFF_FestivalPlayerController;
class APlayerController;
class UFF_MinigameDefinition;
class UFF_MinigameRuntime;
class UFF_MinigameScreenBase;
struct FFFZSWorldChangeEvent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnMinigameStarted,
	FGuid, SessionId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FFF_OnMinigameFinished,
	const FFF_MinigameResult&, Result);

/**
 * World-scoped coordinator for facility minigames.
 *
 * Definitions select replaceable runtime and screen strategies. This class is the single owner
 * of start validation, session lifetime, modal input and exactly-once economy rewards.
 */
UCLASS()
class FESTAFIESTA_API UFF_MinigameSubsystem final : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Authoritative start boundary used by facility UI and future interaction adapters. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame")
	EFF_MinigameStartResult StartMinigame(
		APlayerController* OwningController,
		FGuid FacilityInstanceId);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame")
	void CancelActiveMinigame(
		EFF_MinigameEndReason Reason = EFF_MinigameEndReason::PlayerCancelled);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool HasActiveSession() const { return Runtime != nullptr; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	FFF_MinigameSessionSnapshot GetActiveSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	const UFF_MinigameDefinition* GetActiveDefinition() const { return ActiveDefinition; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	static FText GetStartResultText(EFF_MinigameStartResult Result);

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Minigame")
	FFF_OnMinigameStarted OnMinigameStarted;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|Minigame")
	FFF_OnMinigameFinished OnMinigameFinished;

private:
	void HandleFacilityMinigameRequested(FGuid FacilityInstanceId);
	void HandleScreenCloseRequested(UFF_MinigameScreenBase* RequestingScreen, FGuid SessionId);
	void CompleteActiveSession();
	void TeardownSession();
	void RestoreControllerInput();

	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleWorldSnapshotRestored();

	UPROPERTY(Transient)
	TObjectPtr<UFF_MinigameDefinition> ActiveDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UFF_MinigameRuntime> Runtime;

	UPROPERTY(Transient)
	TObjectPtr<UFF_MinigameScreenBase> Screen;

	TWeakObjectPtr<AFF_FestivalPlayerController> ModalController;
	bool bPreviousCursorVisible = false;
	bool bRewardCommitted = false;
	bool bCompletionHandled = false;
};
