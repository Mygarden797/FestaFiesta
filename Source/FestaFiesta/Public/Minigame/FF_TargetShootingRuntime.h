// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "FF_TargetShootingRuntime.generated.h"

class UFF_TargetShootingDefinition;

/** Deterministic one-target-at-a-time flick trainer with precision and reaction scoring. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_TargetShootingRuntime final : public UFF_MinigameRuntime
{
	GENERATED_BODY()

public:
	/**
	 * Accepts one shot for the currently displayed TargetSerial.
	 * A hit consumes that serial and immediately spawns the next target. A miss keeps
	 * the current target in place so the player can correct their aim without a random reset.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Target Shooting")
	EFF_TargetShootingShotResult SubmitShot(
		const FGuid& SessionId,
		const FVector2D& NormalizedPoint,
		int32 TargetSerial);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Target Shooting")
	FFF_TargetShootingSnapshot GetTargetSnapshot() const;

protected:
	virtual bool OnInitialize(FString& OutFailureReason) override;
	virtual void OnAdvancePlaying(float DeltaSeconds, float StartingElapsedSeconds) override;
	virtual void OnFinished(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason) override;

private:
	void SpawnTarget(float SessionElapsedSeconds);
	void ConsumeCurrentTargetAndSpawn(float SessionElapsedSeconds);
	void RecordFeedback(
		EFF_TargetShootingFeedbackType FeedbackType,
		const FVector2D& PointNormalized,
		int32 ScoreDelta,
		float ReactionSeconds,
		bool bWasBonusTarget);

	UPROPERTY(Transient)
	TObjectPtr<UFF_TargetShootingDefinition> TargetDefinition;

	UPROPERTY(Transient)
	FFF_TargetShootingSnapshot TargetSnapshot;

	FRandomStream RandomStream;
	double TotalHitReactionSeconds = 0.0;
};
