// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"

/** Pure fixed-arena surfing rules. These helpers never access a World or UI. */
namespace FFSurfingRules
{
	inline constexpr double FixedSimulationStepSeconds = 1.0 / 60.0;

	FESTAFIESTA_API bool IsValidSteerAxis(float Axis);
	FESTAFIESTA_API bool IsValidMovementInput(float LateralAxis, float ForwardAxis);
	FESTAFIESTA_API bool IsValidSteerDirection(int32 Direction);
	/** Resolves opposing held keys with the most recently pressed direction winning. */
	FESTAFIESTA_API float ResolveDigitalAxis(
		bool bNegativeHeld,
		bool bPositiveHeld,
		int32 MostRecentDirection);
	/** Exact exponential response alpha for frame-rate-independent presentation smoothing. */
	FESTAFIESTA_API float CalculateExponentialResponse(
		float ResponsePerSecond,
		float DeltaSeconds);
	FESTAFIESTA_API bool IsObstacleType(EFF_SurfingObjectType Type);
	FESTAFIESTA_API FVector2D NormalizeHeading(
		float LateralAxis,
		float ForwardAxis,
		float ArenaHorizontalMetricScale);

	/** Returns the earliest normalized time of impact for a segment against a circle. */
	FESTAFIESTA_API bool GetSegmentCircleHitTime(
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		const FVector2D& CircleCenter,
		float CombinedRadius,
		float ArenaHorizontalMetricScale,
		double& OutTimeOfImpact);

	FESTAFIESTA_API float AddScoreSaturated(float CurrentScore, float Delta);
	FESTAFIESTA_API int32 QuantizeScore(float ScoreAccumulator);
}
