// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/Surfing/FF_SurfingRules.h"

namespace
{
	bool IsFiniteNonNegative(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f;
	}
}

bool FFF_SurfingRules::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!FMath::IsFinite(CharacterSpeedNormalizedPerSecond)
		|| CharacterSpeedNormalizedPerSecond <= 0.0f)
	{
		return Fail(TEXT("character speed must be finite and greater than zero"));
	}
	if (!FMath::IsFinite(ArenaHorizontalMetricScale)
		|| ArenaHorizontalMetricScale <= 0.0f)
	{
		return Fail(TEXT("arena horizontal metric scale must be finite and greater than zero"));
	}
	if (!FMath::IsFinite(SteeringStrength)
		|| SteeringStrength < 0.05f
		|| SteeringStrength > 1.0f
		|| !FMath::IsFinite(ForwardInputSpeedInfluence)
		|| ForwardInputSpeedInfluence < 0.0f
		|| ForwardInputSpeedInfluence >= 1.0f)
	{
		return Fail(TEXT("endless steering and forward speed influence are invalid"));
	}
	if (!FMath::IsFinite(CourseSegmentLengthNormalized)
		|| CourseSegmentLengthNormalized <= 2.0f
		|| CourseSegmentsBehind < 1
		|| CourseSegmentsBehind > 8
		|| CourseSegmentsAhead < 2
		|| CourseSegmentsAhead > 12)
	{
		return Fail(TEXT("endless course segment length and recycling window are invalid"));
	}

	if (!FMath::IsFinite(PlayerCollisionRadiusNormalized)
		|| PlayerCollisionRadiusNormalized <= 0.0f
		|| PlayerCollisionRadiusNormalized > 1.0f)
	{
		return Fail(TEXT("player collision radius must be finite and inside (0, 1]"));
	}

	if (!FMath::IsFinite(HitInvincibilitySeconds)
		|| HitInvincibilitySeconds <= 0.0f
		|| !FMath::IsFinite(BoostDurationSeconds)
		|| BoostDurationSeconds <= 0.0f
		|| !FMath::IsFinite(BoostSpeedMultiplier)
		|| BoostSpeedMultiplier <= 1.0f)
	{
		return Fail(TEXT("invincibility and boost tuning must be positive, and boost must increase speed"));
	}

	// One fixed tick may cross several course segments. Include the maximum W input
	// speed trim so authored tuning cannot exceed the bounded collision workload.
	const double MaximumMetricTickTravel = static_cast<double>(CharacterSpeedNormalizedPerSecond)
		* static_cast<double>(BoostSpeedMultiplier)
		* (1.0 + static_cast<double>(ForwardInputSpeedInfluence))
		* FFSurfingRules::FixedSimulationStepSeconds;
	const double MaximumCoordinateTickTravel = MaximumMetricTickTravel
		/ FMath::Min(static_cast<double>(ArenaHorizontalMetricScale), 1.0);
	if (!FMath::IsFinite(MaximumCoordinateTickTravel)
		|| MaximumCoordinateTickTravel > 64.0)
	{
		return Fail(TEXT("boosted character travel must not exceed 64 normalized arena units per fixed tick"));
	}

	if (MaxLives <= 0
		|| InitialLives < 0
		|| InitialLives > MaxLives
		|| (bEndRunWhenLivesDepleted && InitialLives == 0))
	{
		return Fail(TEXT("initial lives must be inside [0, MaxLives], positive when depletion ends the run, and MaxLives must be positive"));
	}

	if (MaxBoostCharges <= 0
		|| InitialBoostCharges < 0
		|| InitialBoostCharges > MaxBoostCharges)
	{
		return Fail(TEXT("initial boost charges must be inside [0, MaxBoostCharges]"));
	}

	if (!FMath::IsFinite(DistanceScorePerMeter)
		|| DistanceScorePerMeter <= 0.0f
		|| !IsFiniteNonNegative(CoinScore)
		|| !IsFiniteNonNegative(ObstacleDestroyScore)
		|| !IsFiniteNonNegative(CollisionPenalty))
	{
		return Fail(TEXT("score tuning must be finite and non-negative with positive path scoring"));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

namespace FFSurfingRules
{
	bool IsValidSteerAxis(const float Axis)
	{
		return FMath::IsFinite(Axis) && Axis >= -1.0f && Axis <= 1.0f;
	}

	bool IsValidMovementInput(const float LateralAxis, const float ForwardAxis)
	{
		return IsValidSteerAxis(LateralAxis) && IsValidSteerAxis(ForwardAxis);
	}

	bool IsValidSteerDirection(const int32 Direction)
	{
		return Direction == -1 || Direction == 1;
	}

	float ResolveDigitalAxis(
		const bool bNegativeHeld,
		const bool bPositiveHeld,
		const int32 MostRecentDirection)
	{
		if (bNegativeHeld != bPositiveHeld)
		{
			return bPositiveHeld ? 1.0f : -1.0f;
		}
		if (bNegativeHeld && bPositiveHeld && IsValidSteerDirection(MostRecentDirection))
		{
			return static_cast<float>(MostRecentDirection);
		}
		return 0.0f;
	}

	float CalculateExponentialResponse(
		const float ResponsePerSecond,
		const float DeltaSeconds)
	{
		if (!FMath::IsFinite(ResponsePerSecond)
			|| !FMath::IsFinite(DeltaSeconds)
			|| ResponsePerSecond <= 0.0f
			|| DeltaSeconds <= 0.0f)
		{
			return 0.0f;
		}

		const double Exponent = FMath::Min(
			static_cast<double>(ResponsePerSecond)
				* static_cast<double>(DeltaSeconds),
			80.0);
		return static_cast<float>(1.0 - FMath::Exp(-Exponent));
	}

	bool IsObstacleType(const EFF_SurfingObjectType Type)
	{
		return Type == EFF_SurfingObjectType::Rock
			|| Type == EFF_SurfingObjectType::Log
			|| Type == EFF_SurfingObjectType::Buoy;
	}

	FVector2D NormalizeHeading(
		const float LateralAxis,
		const float ForwardAxis,
		const float ArenaHorizontalMetricScale)
	{
		if (!IsValidMovementInput(LateralAxis, ForwardAxis)
			|| !FMath::IsFinite(ArenaHorizontalMetricScale)
			|| ArenaHorizontalMetricScale <= 0.0f
			|| (LateralAxis == 0.0f && ForwardAxis == 0.0f))
		{
			return FVector2D::ZeroVector;
		}

		const double MetricLateral = static_cast<double>(LateralAxis)
			* static_cast<double>(ArenaHorizontalMetricScale);
		const double Length = FMath::Sqrt(
			MetricLateral * MetricLateral
			+ static_cast<double>(ForwardAxis) * static_cast<double>(ForwardAxis));
		if (!FMath::IsFinite(Length) || Length <= 0.0)
		{
			return FVector2D::ZeroVector;
		}
		return FVector2D(
			static_cast<double>(LateralAxis) / Length,
			static_cast<double>(ForwardAxis) / Length);
	}

	bool GetSegmentCircleHitTime(
		const FVector2D& SegmentStart,
		const FVector2D& SegmentEnd,
		const FVector2D& CircleCenter,
		const float CombinedRadius,
		const float ArenaHorizontalMetricScale,
		double& OutTimeOfImpact)
	{
		OutTimeOfImpact = 0.0;
		if (!FMath::IsFinite(SegmentStart.X)
			|| !FMath::IsFinite(SegmentStart.Y)
			|| !FMath::IsFinite(SegmentEnd.X)
			|| !FMath::IsFinite(SegmentEnd.Y)
			|| !FMath::IsFinite(CircleCenter.X)
			|| !FMath::IsFinite(CircleCenter.Y)
			|| !FMath::IsFinite(CombinedRadius)
			|| CombinedRadius < 0.0f
			|| !FMath::IsFinite(ArenaHorizontalMetricScale)
			|| ArenaHorizontalMetricScale <= 0.0f)
		{
			return false;
		}

		const FVector2D MetricStart(
			SegmentStart.X * static_cast<double>(ArenaHorizontalMetricScale),
			SegmentStart.Y);
		const FVector2D MetricEnd(
			SegmentEnd.X * static_cast<double>(ArenaHorizontalMetricScale),
			SegmentEnd.Y);
		const FVector2D MetricCenter(
			CircleCenter.X * static_cast<double>(ArenaHorizontalMetricScale),
			CircleCenter.Y);
		const FVector2D FromCenter = MetricStart - MetricCenter;
		const double RadiusSquared = static_cast<double>(CombinedRadius)
			* static_cast<double>(CombinedRadius);
		const double StartDistanceSquared = FromCenter.SquaredLength();
		if (StartDistanceSquared <= RadiusSquared)
		{
			return true;
		}

		const FVector2D Segment = MetricEnd - MetricStart;
		const double A = Segment.SquaredLength();
		if (A <= UE_DOUBLE_SMALL_NUMBER)
		{
			return false;
		}

		const double B = 2.0 * FVector2D::DotProduct(FromCenter, Segment);
		const double C = StartDistanceSquared - RadiusSquared;
		const double Discriminant = B * B - 4.0 * A * C;
		if (Discriminant < 0.0)
		{
			return false;
		}

		const double FirstTime = (-B - FMath::Sqrt(FMath::Max(0.0, Discriminant)))
			/ (2.0 * A);
		if (FirstTime < -UE_DOUBLE_KINDA_SMALL_NUMBER
			|| FirstTime > 1.0 + UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			return false;
		}

		OutTimeOfImpact = FMath::Clamp(FirstTime, 0.0, 1.0);
		return true;
	}

	float AddScoreSaturated(const float CurrentScore, const float Delta)
	{
		const float SafeCurrent = FMath::IsFinite(CurrentScore)
			? FMath::Max(0.0f, CurrentScore)
			: 0.0f;
		if (!FMath::IsFinite(Delta))
		{
			return SafeCurrent;
		}

		const double Result = FMath::Clamp(
			static_cast<double>(SafeCurrent) + static_cast<double>(Delta),
			0.0,
			static_cast<double>(MAX_int32));
		return static_cast<float>(Result);
	}

	int32 QuantizeScore(const float ScoreAccumulator)
	{
		if (!FMath::IsFinite(ScoreAccumulator) || ScoreAccumulator <= 0.0f)
		{
			return 0;
		}
		if (static_cast<double>(ScoreAccumulator) >= static_cast<double>(MAX_int32))
		{
			return MAX_int32;
		}
		return FMath::FloorToInt(ScoreAccumulator);
	}
}
