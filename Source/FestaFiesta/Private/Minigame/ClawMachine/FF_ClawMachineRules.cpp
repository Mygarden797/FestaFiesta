// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/ClawMachine/FF_ClawMachineRules.h"

namespace
{
	bool IsFiniteVector2D(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}
}

bool FFF_ClawMachinePrizeTuning::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (PrizeId.IsNone())
	{
		return Fail(TEXT("PrizeId is required"));
	}
	if (DisplayName.IsEmpty())
	{
		return Fail(TEXT("DisplayName is required"));
	}
	if (ScoreValue <= 0 || !FMath::IsFinite(SpawnWeight) || SpawnWeight <= 0.0f)
	{
		return Fail(TEXT("ScoreValue and SpawnWeight must be positive"));
	}
	if (!FMath::IsFinite(GripDifficulty) || GripDifficulty < 0.0f || GripDifficulty > 1.0f)
	{
		return Fail(TEXT("GripDifficulty must be in [0, 1]"));
	}
	if (!FMath::IsFinite(CaptureRadiusNormalized)
		|| CaptureRadiusNormalized <= 0.0f
		|| CaptureRadiusNormalized > 0.3f)
	{
		return Fail(TEXT("CaptureRadiusNormalized must be in (0, 0.3]"));
	}
	if (!FMath::IsFinite(MassKg) || MassKg < 0.05f || MassKg > 3.0f
		|| !FMath::IsFinite(SurfaceFriction) || SurfaceFriction < 0.0f || SurfaceFriction > 1.0f
		|| !FMath::IsFinite(GripCompliance) || GripCompliance < 0.0f || GripCompliance > 1.0f
		|| !IsFiniteVector2D(CenterOfMassOffsetNormalized)
		|| CenterOfMassOffsetNormalized.Size() > 0.5f)
	{
		return Fail(TEXT("physical mass/friction/compliance/center-of-mass tuning is invalid"));
	}
	if (!FMath::IsFinite(VisualScale.X)
		|| !FMath::IsFinite(VisualScale.Y)
		|| !FMath::IsFinite(VisualScale.Z)
		|| VisualScale.X <= 0.0f || VisualScale.Y <= 0.0f || VisualScale.Z <= 0.0f)
	{
		return Fail(TEXT("VisualScale components must be finite and positive"));
	}
	return true;
}

bool FFF_ClawMachineRules::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const FString& Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!FMath::IsFinite(MovementSpeedNormalizedPerSecond)
		|| MovementSpeedNormalizedPerSecond <= 0.0f)
	{
		return Fail(TEXT("MovementSpeedNormalizedPerSecond must be positive"));
	}
	if (!FFClawMachineRules::IsNormalizedCoordinate(MovementMinNormalized)
		|| !FFClawMachineRules::IsNormalizedCoordinate(MovementMaxNormalized)
		|| MovementMinNormalized.X >= MovementMaxNormalized.X
		|| MovementMinNormalized.Y >= MovementMaxNormalized.Y)
	{
		return Fail(TEXT("movement bounds must form a non-empty normalized rectangle"));
	}
	if (!FFClawMachineRules::IsNormalizedCoordinate(HomePositionNormalized)
		|| HomePositionNormalized.X < MovementMinNormalized.X
		|| HomePositionNormalized.X > MovementMaxNormalized.X
		|| HomePositionNormalized.Y < MovementMinNormalized.Y
		|| HomePositionNormalized.Y > MovementMaxNormalized.Y)
	{
		return Fail(TEXT("HomePositionNormalized must be inside movement bounds"));
	}
	if (!FMath::IsFinite(GripRadiusNormalized) || GripRadiusNormalized <= 0.0f
		|| !FMath::IsFinite(GripPower) || GripPower < 0.0f
		|| !FMath::IsFinite(DeterministicGripVariance) || DeterministicGripVariance < 0.0f
		|| !FMath::IsFinite(RequiredGripQuality) || RequiredGripQuality < 0.0f
		|| RequiredGripQuality > 1.0f)
	{
		return Fail(TEXT("grip tuning contains an invalid value"));
	}
	if (MaxAttempts <= 0 || MaxAttempts > 10 || PrizeCount <= 0 || PrizeCount > 64)
	{
		return Fail(TEXT("MaxAttempts or PrizeCount is outside its supported range"));
	}
	if (!FMath::IsFinite(PerfectAlignmentThreshold) || PerfectAlignmentThreshold < 0.5f
		|| PerfectAlignmentThreshold > 1.0f || PerfectGrabBonusScore < 0)
	{
		return Fail(TEXT("perfect-grab score tuning is invalid"));
	}
	for (const float Duration : {
		DropDurationSeconds, CloseDurationSeconds, LiftDurationSeconds,
		ReturnDurationSeconds, ReleaseDurationSeconds })
	{
		if (!FMath::IsFinite(Duration) || Duration < 0.05f)
		{
			return Fail(TEXT("every mechanical phase duration must be at least 0.05 seconds"));
		}
	}
	if (!FMath::IsFinite(GantryResponseFrequencyHz) || GantryResponseFrequencyHz < 1.0f
		|| !FMath::IsFinite(GantryDampingRatio) || GantryDampingRatio < 0.2f
		|| !FMath::IsFinite(PendulumLengthCm) || PendulumLengthCm < 80.0f
		|| !FMath::IsFinite(PendulumDampingPerSecond) || PendulumDampingPerSecond < 0.1f
		|| !FMath::IsFinite(MaxPendulumSwingDegrees) || MaxPendulumSwingDegrees < 1.0f
		|| !FMath::IsFinite(HeldPrizeResponseFrequencyHz) || HeldPrizeResponseFrequencyHz < 1.0f)
	{
		return Fail(TEXT("physical digital-twin tuning is invalid"));
	}
	if (!FFClawMachineRules::IsNormalizedCoordinate(PrizeSpawnMinNormalized)
		|| !FFClawMachineRules::IsNormalizedCoordinate(PrizeSpawnMaxNormalized)
		|| PrizeSpawnMinNormalized.X >= PrizeSpawnMaxNormalized.X
		|| PrizeSpawnMinNormalized.Y >= PrizeSpawnMaxNormalized.Y
		|| !FMath::IsFinite(PrizeMinSeparationNormalized)
		|| PrizeMinSeparationNormalized < 0.0f)
	{
		return Fail(TEXT("prize spawn bounds or separation is invalid"));
	}
	if (PrizePool.IsEmpty())
	{
		return Fail(TEXT("PrizePool requires at least one archetype"));
	}

	TSet<FName> SeenIds;
	for (const FFF_ClawMachinePrizeTuning& Prize : PrizePool)
	{
		FString PrizeFailure;
		if (!Prize.IsValid(&PrizeFailure))
		{
			return Fail(FString::Printf(TEXT("invalid prize '%s': %s"),
				*Prize.PrizeId.ToString(), *PrizeFailure));
		}
		if (SeenIds.Contains(Prize.PrizeId))
		{
			return Fail(FString::Printf(TEXT("duplicate PrizeId '%s'"), *Prize.PrizeId.ToString()));
		}
		SeenIds.Add(Prize.PrizeId);
	}
	return true;
}

const FFF_ClawMachinePrizeTuning* FFF_ClawMachineRules::FindPrize(const FName PrizeId) const
{
	return PrizePool.FindByPredicate([PrizeId](const FFF_ClawMachinePrizeTuning& Prize)
	{
		return Prize.PrizeId == PrizeId;
	});
}

bool FFClawMachineRules::IsNormalizedCoordinate(const FVector2D& Point)
{
	return IsFiniteVector2D(Point)
		&& Point.X >= 0.0f && Point.X <= 1.0f
		&& Point.Y >= 0.0f && Point.Y <= 1.0f;
}

float FFClawMachineRules::CalculateAlignment(
	const FVector2D& ClawPoint,
	const FFF_ClawMachinePrizeSnapshot& Prize,
	const FFF_ClawMachinePrizeTuning& Tuning,
	const float MachineGripRadius)
{
	const float EffectiveRadius = FMath::Max(
		UE_SMALL_NUMBER,
		FMath::Min(MachineGripRadius, Tuning.CaptureRadiusNormalized));
	return 1.0f - FMath::Clamp(
		FVector2D::Distance(ClawPoint, Prize.PositionNormalized) / EffectiveRadius,
		0.0f,
		1.0f);
}

float FFClawMachineRules::CalculateGripQuality(
	const float Alignment,
	const float GripPower,
	const float PrizeDifficulty,
	const float DeterministicVariance)
{
	// Alignment is intentionally the largest term: the machine has character, but player skill wins.
	return FMath::Clamp(
		FMath::Clamp(Alignment, 0.0f, 1.0f) * 0.72f
			+ FMath::Max(0.0f, GripPower) * 0.42f
			- FMath::Clamp(PrizeDifficulty, 0.0f, 1.0f) * 0.38f
			+ DeterministicVariance,
		0.0f,
		1.0f);
}

float FFClawMachineRules::CalculatePhysicalGripQuality(
	const float Alignment,
	const float GripPower,
	const FFF_ClawMachinePrizeTuning& Tuning,
	const float DeterministicVariance)
{
	const float BaseQuality = CalculateGripQuality(
		Alignment,
		GripPower,
		Tuning.GripDifficulty,
		DeterministicVariance);
	const float MassAlpha = FMath::Clamp((Tuning.MassKg - 0.15f) / 1.35f, 0.0f, 1.0f);
	const float CenterOfMassAlpha = FMath::Clamp(
		Tuning.CenterOfMassOffsetNormalized.Size() / 0.25f,
		0.0f,
		1.0f);
	const float ContactAssist = FMath::Clamp(Tuning.SurfaceFriction, 0.0f, 1.0f) * 0.075f
		+ FMath::Clamp(Tuning.GripCompliance, 0.0f, 1.0f) * 0.055f;
	const float LoadPenalty = MassAlpha * 0.13f + CenterOfMassAlpha * 0.085f;
	return FMath::Clamp(BaseQuality + ContactAssist - LoadPenalty, 0.0f, 1.0f);
}

int32 FFClawMachineRules::AddScoreSaturated(const int32 CurrentScore, const int32 Delta)
{
	const int64 Sum = static_cast<int64>(FMath::Max(0, CurrentScore))
		+ static_cast<int64>(Delta);
	return static_cast<int32>(FMath::Clamp<int64>(Sum, 0, MAX_int32));
}
