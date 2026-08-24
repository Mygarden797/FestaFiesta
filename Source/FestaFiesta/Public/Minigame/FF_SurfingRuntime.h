// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "FF_SurfingRuntime.generated.h"

class UFF_SurfingDefinition;

/** Runtime-only fixed arena item. UI receives an immutable snapshot copy. */
struct FFF_SurfingCourseObject
{
	int32 ObjectId = 0;
	EFF_SurfingObjectType Type = EFF_SurfingObjectType::Rock;
	FVector2D PositionNormalized = FVector2D::ZeroVector;
	float CollisionRadiusNormalized = 0.0f;
	float ScoreValueOverride = 0.0f;
	bool bResolved = false;
	bool bPlayerOverlapping = false;
	int32 LastContactSimulationTick = INDEX_NONE;
};

/** Deterministic thirty-second free ride in a fixed two-dimensional arena. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_SurfingRuntime final : public UFF_MinigameRuntime
{
	GENERATED_BODY()

public:
	/**
	 * Queues finite axes in [-1, 1] for the next fixed tick. A non-zero vector
	 * becomes the normalized heading; zero keeps the previous heading. +Y is up.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingSteerResult SetMovementInput(
		const FGuid& SessionId,
		float LateralAxis,
		float ForwardAxis);

	/** Horizontal compatibility wrapper that preserves the current forward axis. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingSteerResult SetSteerInput(const FGuid& SessionId, float Axis);

	/** Compatibility wrapper that supplies a persistent -1/+1 horizontal axis. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingSteerResult SubmitSteer(const FGuid& SessionId, int32 Direction);

	/** Queues one boost charge for activation on the next fixed tick. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingBoostResult SubmitBoost(const FGuid& SessionId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Surfing")
	FFF_SurfingSnapshot GetSurfingSnapshot() const;

protected:
	virtual bool OnInitialize(FString& OutFailureReason) override;
	virtual void OnAdvancePlaying(float DeltaSeconds, float StartingElapsedSeconds) override;
	virtual void OnFinished(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason) override;

private:
	bool LoadCourseLayout(FString& OutFailureReason);
	void SimulateFixedTick();
	void ApplyPendingCommands();
	void MovePlayerAndProcessCollisions(double TravelDistance);
	void ProcessMovementSegment(
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd);
	void ProcessObjectContact(FFF_SurfingCourseObject& Object);
	void RefreshVisibleObjects();
	void RecordEvent(EFF_SurfingEventType EventType);
	void SynchronizeScore();
	void SynchronizeDerivedAndLegacyState();

	UPROPERTY(Transient)
	TObjectPtr<UFF_SurfingDefinition> SurfingDefinition;

	UPROPERTY(Transient)
	FFF_SurfingSnapshot SurfingSnapshot;

	TArray<FFF_SurfingCourseObject> CourseObjects;
	double DistanceMetersExact = 0.0;
	double SimulationAccumulatorSeconds = 0.0;
	int32 SimulationTick = 0;
	int32 MaxSimulationTicks = 0;
	FVector2D PendingMovementInput = FVector2D::ZeroVector;
	bool bHasPendingMovementInput = false;
	bool bBoostActivationPending = false;
	float ScoreAccumulator = 0.0f;
};
