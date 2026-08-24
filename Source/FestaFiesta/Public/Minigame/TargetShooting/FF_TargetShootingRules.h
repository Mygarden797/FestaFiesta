// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameTypes.h"

/** Pure target-shooting rules. These functions do not access a World or UI. */
namespace FFTargetShootingRules
{
	FESTAFIESTA_API bool IsNormalizedCoordinate(const FVector2D& Coordinate);

	FESTAFIESTA_API bool IsInsideTarget(
		const FVector2D& Coordinate,
		const FVector2D& Center,
		const FVector2D& Radius);

	FESTAFIESTA_API EFF_TargetShootingFeedbackType GetHitQuality(
		const FFF_TargetShootingRules& Rules,
		const FVector2D& Coordinate,
		const FVector2D& Center,
		const FVector2D& Radius);

	FESTAFIESTA_API float GetDifficultyAlpha(float ElapsedSeconds, float DurationSeconds);

	FESTAFIESTA_API float GetTargetLifetime(
		const FFF_TargetShootingRules& Rules,
		float DifficultyAlpha);

	FESTAFIESTA_API FVector2D GetTargetRadius(
		const FFF_TargetShootingRules& Rules,
		float DifficultyAlpha);

	/** Consumes exactly two values and keeps the full ellipse outside authored HUD safe margins. */
	FESTAFIESTA_API FVector2D GenerateTargetCenter(
		FRandomStream& Stream,
		const FVector2D& Radius,
		const FMargin& SafeMarginNormalized);

	FESTAFIESTA_API int32 CalculateHitScore(
		const FFF_TargetShootingRules& Rules,
		int32 CurrentCombo);

	FESTAFIESTA_API int32 CalculateReactionScoreBonus(
		const FFF_TargetShootingRules& Rules,
		float ReactionSeconds);

	FESTAFIESTA_API int32 CalculateShotScore(
		const FFF_TargetShootingRules& Rules,
		int32 CurrentCombo,
		EFF_TargetShootingFeedbackType HitQuality,
		float ReactionSeconds,
		bool bBonusTarget);

	FESTAFIESTA_API int32 AddScoreSaturated(int32 CurrentScore, int32 Delta);

	FESTAFIESTA_API float CalculateAccuracyPercent(int32 HitCount, int32 MissCount);

	FESTAFIESTA_API float CalculateHitsPerSecond(int32 HitCount, float ElapsedSeconds);

	/** Linear reward mapping clamped to [0, MaxRewardGold]. */
	FESTAFIESTA_API int32 CalculateRewardGold(
		int32 Score,
		int32 ScoreForMaxReward,
		int32 MaxRewardGold);
}
