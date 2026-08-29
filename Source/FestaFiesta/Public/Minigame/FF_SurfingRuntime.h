// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "FF_SurfingRuntime.generated.h"

class UFF_SurfingDefinition;

/** One DataTable-authored template reused by the endless segment pool. */
struct FFF_SurfingCourseTemplate
{
	EFF_SurfingObjectType Type = EFF_SurfingObjectType::Rock;
	FVector2D PositionNormalized = FVector2D::ZeroVector;
	float CollisionRadiusNormalized = 0.0f;
	float ScoreValueOverride = 0.0f;
};

/** Runtime-only pooled course item. UI receives a camera-relative snapshot copy. */
struct FFF_SurfingCourseObject
{
	uint64 ObjectKey = 0;
	int32 ObjectId = 0;
	int32 SegmentIndex = 0;
	int32 TemplateIndex = 0;
	EFF_SurfingObjectType Type = EFF_SurfingObjectType::Rock;
	/** Absolute course coordinate; unlike the UI snapshot, Y is unbounded. */
	FVector2D PositionNormalized = FVector2D::ZeroVector;
	float CollisionRadiusNormalized = 0.0f;
	float ScoreValueOverride = 0.0f;
	bool bResolved = false;
	bool bPlayerOverlapping = false;
	int32 LastContactSimulationTick = INDEX_NONE;
};

/** Deterministic free ride over a camera-relative, endlessly recycled course. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_SurfingRuntime final : public UFF_MinigameRuntime
{
	GENERATED_BODY()

public:
	/**
	 * Publishes finite input intent in [-1, 1] immediately. The next fixed tick
	 * consumes the latest intent, so presentation stays responsive without making
	 * movement or collision results frame-rate dependent. X steers while the course
	 * always advances; Y trims speed without stopping it.
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

	int32 GetActiveCourseObjectCount() const { return CourseObjects.Num(); }
	int32 GetActiveCourseSegmentCount() const;
	int32 GetResolvedCourseObjectCount() const { return ResolvedCourseObjectKeys.Num(); }

protected:
	virtual bool OnInitialize(FString& OutFailureReason) override;
	virtual void OnAdvancePlaying(float DeltaSeconds, float StartingElapsedSeconds) override;
	virtual void OnFinished(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason) override;

private:
	bool LoadCourseLayout(FString& OutFailureReason);
	void RefreshCourseObjectPool(double MinimumCourseY, double MaximumCourseY);
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
	int32 GetCourseSegmentIndex(double CourseY) const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_SurfingDefinition> SurfingDefinition;

	UPROPERTY(Transient)
	FFF_SurfingSnapshot SurfingSnapshot;

	TArray<FFF_SurfingCourseTemplate> CourseTemplates;
	TArray<FFF_SurfingCourseObject> CourseObjects;
	TSet<uint64> ResolvedCourseObjectKeys;
	FVector2D PlayerCoursePosition = FVector2D::ZeroVector;
	int32 ActiveFirstSegment = 1;
	int32 ActiveLastSegment = 0;
	double DistanceMetersExact = 0.0;
	double SimulationAccumulatorSeconds = 0.0;
	int32 SimulationTick = 0;
	int32 MaxSimulationTicks = 0;
	bool bBoostActivationPending = false;
	float ScoreAccumulator = 0.0f;
};
