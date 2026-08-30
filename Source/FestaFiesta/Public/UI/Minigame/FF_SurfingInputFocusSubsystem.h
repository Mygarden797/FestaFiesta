// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_SurfingInputFocusSubsystem.generated.h"

class UFF_SurfingScreen;

/**
 * Keeps the packaged Surfing screen on the local Slate user's focus path.
 *
 * The minigame is intentionally a modal UMG screen and receives movement in
 * NativeOnPreviewKeyDown. During a cooked launch, CommonUI can restore focus to
 * the viewport after the screen's one-shot SetWidgetToFocus call. Once that
 * happens, WASD never reaches the screen. This small world service performs a
 * bounded three-frame retry only while an active Surfing run is visible, the
 * application is foreground, and the game is not paused. Focus already owned
 * by the screen or one of its children is left untouched.
 */
UCLASS()
class FESTAFIESTA_API UFF_SurfingInputFocusSubsystem final
	: public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	UFUNCTION()
	void HandleMinigameStarted(FGuid SessionId);

	void HandleApplicationReactivated();
	void ScheduleFocusRecovery(FGuid SessionId);
	void TryRepairFocus();
	UFF_SurfingScreen* FindActiveSurfingScreen(FGuid SessionId) const;

	TWeakObjectPtr<UFF_SurfingScreen> CachedScreen;
	FGuid PendingSessionId;
	int32 RemainingFocusAttempts = 0;
	bool bFocusAttemptScheduled = false;
};
