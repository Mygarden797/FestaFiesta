// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "FF_TargetShootingRuntime.generated.h"

class UFF_TargetShootingDefinition;

/** Deterministic one-target-at-a-time projectile trainer with precision and reaction scoring. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_TargetShootingRuntime final : public UFF_MinigameRuntime
{
	GENERATED_BODY()

public:
	/**
	 * Starts one visible projectile toward the captured aim point. Scoring is deferred
	 * until the projectile reaches that point while the same target serial is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Target Shooting")
	EFF_TargetShootingShotResult FireProjectile(
		const FGuid& SessionId,
		const FVector2D& NormalizedPoint,
		int32 TargetSerial);

	/**
	 * Starts a projectile from the weapon's current visual muzzle toward its captured
	 * aim direction. This keeps asset-driven muzzle placement and authoritative
	 * impact resolution separate without treating a pointer position as a hit.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Target Shooting")
	EFF_TargetShootingShotResult FireProjectileFromMuzzle(
		const FGuid& SessionId,
		const FVector2D& MuzzleNormalized,
		const FVector2D& AimPointNormalized,
		int32 TargetSerial);

	/**
	 * Immediately resolves one shot for integrations that do not render projectiles.
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
	EFF_TargetShootingShotResult ResolveShot(
		const FVector2D& NormalizedPoint,
		int32 TargetSerial,
		float SessionElapsedSeconds,
		float ReactionSeconds);
	void CompleteProjectile(float SessionElapsedSeconds);
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
