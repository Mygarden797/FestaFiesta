// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/Audio/FF_MinigameAudioTypes.h"
#include "Minigame/FF_MinigameTypes.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_MinigameScreenBase.generated.h"

class UFF_MinigameScreenBase;
class UFF_MinigameRuntime;

/**
 * A screen only asks to close. The minigame subsystem remains the single owner of
 * cancellation, rewards and input/camera teardown.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
	FFF_OnMinigameScreenCloseRequested,
	UFF_MinigameScreenBase* /* Screen */,
	FGuid /* SessionId */);

/**
 * Common presentation boundary for facility minigames.
 *
 * This class owns screen-level input behaviour and the close intent only. It does
 * not own a runtime strategy and must never decide whether a session is cancelled
 * or rewarded.
 */
UCLASS(Abstract, BlueprintType)
class FESTAFIESTA_API UFF_MinigameScreenBase : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UFF_MinigameScreenBase(const FObjectInitializer& ObjectInitializer);

	/** Associates this reusable screen instance with one authoritative session. */
	void SetSessionId(const FGuid& InSessionId);

	/** Generic coordinator hook. Concrete screens accept only their matching runtime type. */
	virtual bool BindRuntime(UFF_MinigameRuntime* InRuntime);

	/** Generic coordinator hook. Base screens have no result presentation. */
	virtual void ShowResult(const FFF_MinigameResult& Result);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|UI")
	FGuid GetSessionId() const { return SessionId; }

	/** Native-only intent consumed by UFF_MinigameSubsystem. */
	FFF_OnMinigameScreenCloseRequested OnCloseRequested;

protected:
	/** Broadcasts at most once for the configured session. */
	void RequestClose();

	/** Emits a semantic event through the world audio subsystem. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Audio")
	bool PlayMinigameSound(EFF_MinigameAudioEvent Event);

	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;

private:
	FGuid SessionId;
	bool bCloseRequested = false;
};
