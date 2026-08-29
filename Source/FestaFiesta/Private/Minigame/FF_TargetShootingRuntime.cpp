// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_TargetShootingRuntime.h"

#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/TargetShooting/FF_TargetShootingRules.h"

namespace
{
	constexpr float MinEffectiveTargetLifetimeSeconds = 0.01f;
	constexpr int32 MaxTargetExpirationsPerAdvance = 4096;
}

bool UFF_TargetShootingRuntime::OnInitialize(FString& OutFailureReason)
{
	TargetDefinition = Cast<UFF_TargetShootingDefinition>(GetDefinition());
	if (!TargetDefinition)
	{
		OutFailureReason = TEXT("target-shooting runtime requires UFF_TargetShootingDefinition");
		return false;
	}

	TargetSnapshot = FFF_TargetShootingSnapshot();
	TargetSnapshot.Session = GetSnapshot();
	TargetSnapshot.ProjectileStartNormalized = TargetDefinition->Rules.ProjectileOriginNormalized;
	TargetSnapshot.ProjectilePositionNormalized = TargetDefinition->Rules.ProjectileOriginNormalized;
	TotalHitReactionSeconds = 0.0;
	RandomStream.Initialize(GetSnapshot().Context.RandomSeed);
	SpawnTarget(0.0f);
	return true;
}

void UFF_TargetShootingRuntime::OnAdvancePlaying(
	const float DeltaSeconds,
	const float StartingElapsedSeconds)
{
	if (!TargetDefinition)
	{
		return;
	}

	float SecondsLeft = DeltaSeconds;
	float CursorSeconds = StartingElapsedSeconds;
	int32 ExpirationCount = 0;
	while (SecondsLeft > UE_SMALL_NUMBER
		&& ExpirationCount < MaxTargetExpirationsPerAdvance)
	{
		float StepSeconds = SecondsLeft;
		// An accepted shot reserves its target until impact. Projectile travel is a
		// presentation delay and must not shorten the player's authored reaction window.
		const bool bTargetCountdownPaused = TargetSnapshot.bProjectileInFlight;
		if (TargetSnapshot.bTargetVisible && !bTargetCountdownPaused)
		{
			StepSeconds = FMath::Min(
				StepSeconds,
				FMath::Max(0.0f, TargetSnapshot.TargetRemainingSeconds));
		}
		if (TargetSnapshot.bProjectileInFlight)
		{
			StepSeconds = FMath::Min(
				StepSeconds,
				FMath::Max(0.0f, TargetSnapshot.ProjectileRemainingSeconds));
		}

		if (StepSeconds > UE_SMALL_NUMBER)
		{
			if (TargetSnapshot.bTargetVisible && !bTargetCountdownPaused)
			{
				TargetSnapshot.TargetRemainingSeconds = FMath::Max(
					0.0f,
					TargetSnapshot.TargetRemainingSeconds - StepSeconds);
				TargetSnapshot.TargetAgeSeconds = FMath::Max(
					0.0f,
					TargetSnapshot.TargetAgeSeconds + StepSeconds);
			}
			if (TargetSnapshot.bProjectileInFlight)
			{
				TargetSnapshot.ProjectileRemainingSeconds = FMath::Max(
					0.0f,
					TargetSnapshot.ProjectileRemainingSeconds - StepSeconds);
			}
			SecondsLeft = FMath::Max(0.0f, SecondsLeft - StepSeconds);
			CursorSeconds += StepSeconds;
		}

		bool bProcessedEvent = false;
		// A projectile arriving exactly on the deadline gets the first opportunity
		// to hit. A miss can still be followed by the target expiring at that instant.
		if (TargetSnapshot.bProjectileInFlight
			&& TargetSnapshot.ProjectileRemainingSeconds <= UE_SMALL_NUMBER)
		{
			CompleteProjectile(CursorSeconds);
			bProcessedEvent = true;
		}

		if (TargetSnapshot.bTargetVisible
			&& TargetSnapshot.TargetRemainingSeconds <= UE_SMALL_NUMBER)
		{
			TargetSnapshot.TargetRemainingSeconds = 0.0f;
			TargetSnapshot.bTargetVisible = false;
			++ExpirationCount;
			TargetSnapshot.EscapedTargetCount = FMath::Min(
				TargetSnapshot.EscapedTargetCount + 1,
				MAX_int32);
			TargetSnapshot.CurrentCombo = 0;
			RecordFeedback(
				EFF_TargetShootingFeedbackType::Escaped,
				TargetSnapshot.TargetCenterNormalized,
				0,
				TargetSnapshot.TargetAgeSeconds,
				TargetSnapshot.bBonusTarget);

			if (CursorSeconds < TargetDefinition->DurationSeconds)
			{
				SpawnTarget(CursorSeconds);
			}
			bProcessedEvent = true;
		}

		if (!bProcessedEvent && StepSeconds <= UE_SMALL_NUMBER)
		{
			break;
		}
	}

	if (ExpirationCount >= MaxTargetExpirationsPerAdvance && TargetSnapshot.bTargetVisible)
	{
		TargetSnapshot.TargetLifetimeSeconds = FMath::Max(
			TargetSnapshot.TargetLifetimeSeconds,
			MinEffectiveTargetLifetimeSeconds);
		TargetSnapshot.TargetRemainingSeconds = FMath::Max(
			TargetSnapshot.TargetRemainingSeconds,
			MinEffectiveTargetLifetimeSeconds);
	}
}

void UFF_TargetShootingRuntime::OnFinished(
	const EFF_MinigameSessionState FinalState,
	const EFF_MinigameEndReason EndReason)
{
	(void)FinalState;
	(void)EndReason;
	TargetSnapshot.bTargetVisible = false;
	TargetSnapshot.TargetRemainingSeconds = 0.0f;
	TargetSnapshot.bProjectileInFlight = false;
	TargetSnapshot.ProjectileRemainingSeconds = 0.0f;
}

EFF_TargetShootingShotResult UFF_TargetShootingRuntime::FireProjectile(
	const FGuid& SessionId,
	const FVector2D& NormalizedPoint,
	const int32 TargetSerial)
{
	const FVector2D MuzzleNormalized = TargetDefinition
		? TargetDefinition->Rules.ProjectileOriginNormalized
		: FVector2D(0.5f, 0.68f);
	return FireProjectileFromMuzzle(
		SessionId,
		MuzzleNormalized,
		NormalizedPoint,
		TargetSerial);
}

EFF_TargetShootingShotResult UFF_TargetShootingRuntime::FireProjectileFromMuzzle(
	const FGuid& SessionId,
	const FVector2D& MuzzleNormalized,
	const FVector2D& AimPointNormalized,
	const int32 TargetSerial)
{
	if (!IsPlaying())
	{
		return EFF_TargetShootingShotResult::RejectedNotPlaying;
	}

	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_TargetShootingShotResult::RejectedWrongSession;
	}

	if (!FFTargetShootingRules::IsNormalizedCoordinate(MuzzleNormalized)
		|| !FFTargetShootingRules::IsNormalizedCoordinate(AimPointNormalized))
	{
		return EFF_TargetShootingShotResult::RejectedInvalidCoordinate;
	}

	if (!TargetSnapshot.bTargetVisible || TargetSerial != TargetSnapshot.TargetSerial)
	{
		return EFF_TargetShootingShotResult::RejectedStaleTarget;
	}

	if (TargetSnapshot.bProjectileInFlight)
	{
		return EFF_TargetShootingShotResult::RejectedProjectileInFlight;
	}

	const FFF_TargetShootingRules& Rules = TargetDefinition->Rules;
	TargetSnapshot.ProjectileSerial = TargetSnapshot.ProjectileSerial == MAX_int32
		? 1
		: TargetSnapshot.ProjectileSerial + 1;
	TargetSnapshot.ProjectileTargetSerial = TargetSerial;
	TargetSnapshot.ProjectileStartNormalized = MuzzleNormalized;
	TargetSnapshot.ProjectileAimPointNormalized = AimPointNormalized;
	TargetSnapshot.ProjectilePositionNormalized = MuzzleNormalized;
	const float ProjectileDistance = FVector2D::Distance(
		MuzzleNormalized,
		AimPointNormalized);
	TargetSnapshot.ProjectileTravelSeconds = FMath::Max(
		Rules.ProjectileMinTravelSeconds,
		ProjectileDistance / FMath::Max(Rules.ProjectileSpeedNormalizedPerSecond, 0.1f));
	TargetSnapshot.ProjectileRemainingSeconds = TargetSnapshot.ProjectileTravelSeconds;
	TargetSnapshot.ProjectileTargetAgeAtFireSeconds = FMath::Max(
		0.0f,
		TargetSnapshot.TargetAgeSeconds);
	TargetSnapshot.bProjectileInFlight = true;
	return EFF_TargetShootingShotResult::ProjectileFired;
}

EFF_TargetShootingShotResult UFF_TargetShootingRuntime::SubmitShot(
	const FGuid& SessionId,
	const FVector2D& NormalizedPoint,
	const int32 TargetSerial)
{
	if (!IsPlaying())
	{
		return EFF_TargetShootingShotResult::RejectedNotPlaying;
	}

	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_TargetShootingShotResult::RejectedWrongSession;
	}

	if (!FFTargetShootingRules::IsNormalizedCoordinate(NormalizedPoint))
	{
		return EFF_TargetShootingShotResult::RejectedInvalidCoordinate;
	}

	if (!TargetSnapshot.bTargetVisible || TargetSerial != TargetSnapshot.TargetSerial)
	{
		return EFF_TargetShootingShotResult::RejectedStaleTarget;
	}
	if (TargetSnapshot.bProjectileInFlight)
	{
		return EFF_TargetShootingShotResult::RejectedProjectileInFlight;
	}

	return ResolveShot(
		NormalizedPoint,
		TargetSerial,
		GetSnapshot().ElapsedSeconds,
		TargetSnapshot.TargetAgeSeconds);
}

EFF_TargetShootingShotResult UFF_TargetShootingRuntime::ResolveShot(
	const FVector2D& NormalizedPoint,
	const int32 TargetSerial,
	const float SessionElapsedSeconds,
	const float ReactionSeconds)
{
	if (!TargetSnapshot.bTargetVisible || TargetSerial != TargetSnapshot.TargetSerial)
	{
		return EFF_TargetShootingShotResult::RejectedStaleTarget;
	}

	const EFF_TargetShootingFeedbackType HitQuality = FFTargetShootingRules::GetHitQuality(
		TargetDefinition->Rules,
		NormalizedPoint,
		TargetSnapshot.TargetCenterNormalized,
		TargetSnapshot.TargetRadiusNormalized);
	const bool bHit = HitQuality == EFF_TargetShootingFeedbackType::Hit
		|| HitQuality == EFF_TargetShootingFeedbackType::Precision
		|| HitQuality == EFF_TargetShootingFeedbackType::Bullseye;

	if (bHit)
	{
		const float SafeReactionSeconds = FMath::IsFinite(ReactionSeconds)
			? FMath::Max(0.0f, ReactionSeconds)
			: 0.0f;
		const int32 RequestedScoreDelta = FFTargetShootingRules::CalculateShotScore(
			TargetDefinition->Rules,
			TargetSnapshot.CurrentCombo,
			HitQuality,
			SafeReactionSeconds,
			TargetSnapshot.bBonusTarget);
		const int32 PreviousScore = GetSnapshot().Score;
		SetScore(FFTargetShootingRules::AddScoreSaturated(PreviousScore, RequestedScoreDelta));
		TargetSnapshot.HitCount = FMath::Min(TargetSnapshot.HitCount + 1, MAX_int32);
		TargetSnapshot.CurrentCombo = FMath::Min(TargetSnapshot.CurrentCombo + 1, MAX_int32);
		TargetSnapshot.BestCombo = FMath::Max(
			TargetSnapshot.BestCombo,
			TargetSnapshot.CurrentCombo);
		TotalHitReactionSeconds += static_cast<double>(SafeReactionSeconds);
		RecordFeedback(
			HitQuality,
			NormalizedPoint,
			GetSnapshot().Score - PreviousScore,
			SafeReactionSeconds,
			TargetSnapshot.bBonusTarget);
		ConsumeCurrentTargetAndSpawn(SessionElapsedSeconds);
	}
	else
	{
		const int32 PreviousScore = GetSnapshot().Score;
		SetScore(FFTargetShootingRules::AddScoreSaturated(
			PreviousScore,
			-TargetDefinition->Rules.MissPenalty));
		TargetSnapshot.MissCount = FMath::Min(TargetSnapshot.MissCount + 1, MAX_int32);
		TargetSnapshot.CurrentCombo = 0;
		RecordFeedback(
			EFF_TargetShootingFeedbackType::Miss,
			NormalizedPoint,
			GetSnapshot().Score - PreviousScore,
			FMath::IsFinite(ReactionSeconds)
				? FMath::Max(0.0f, ReactionSeconds)
				: 0.0f,
			TargetSnapshot.bBonusTarget);
	}

	return bHit
		? EFF_TargetShootingShotResult::Hit
		: EFF_TargetShootingShotResult::Miss;
}

FFF_TargetShootingSnapshot UFF_TargetShootingRuntime::GetTargetSnapshot() const
{
	FFF_TargetShootingSnapshot Result = TargetSnapshot;
	Result.Session = GetSnapshot();
	if (TargetDefinition)
	{
		Result.PrecisionRadiusRatio = TargetDefinition->Rules.PrecisionRadiusRatio;
		Result.BullseyeRadiusRatio = TargetDefinition->Rules.BullseyeRadiusRatio;
	}
	if (Result.bProjectileInFlight && Result.ProjectileTravelSeconds > UE_SMALL_NUMBER)
	{
		const float TravelAlpha = 1.0f - FMath::Clamp(
			Result.ProjectileRemainingSeconds / Result.ProjectileTravelSeconds,
			0.0f,
			1.0f);
		Result.ProjectilePositionNormalized = FMath::Lerp(
			Result.ProjectileStartNormalized,
			Result.ProjectileAimPointNormalized,
			TravelAlpha);
	}
	Result.AccuracyPercent = FFTargetShootingRules::CalculateAccuracyPercent(
		Result.HitCount,
		Result.MissCount);
	Result.HitsPerSecond = FFTargetShootingRules::CalculateHitsPerSecond(
		Result.HitCount,
		Result.Session.ElapsedSeconds);
	Result.AverageHitReactionSeconds = Result.HitCount > 0
		? static_cast<float>(TotalHitReactionSeconds / static_cast<double>(Result.HitCount))
		: 0.0f;
	return Result;
}

void UFF_TargetShootingRuntime::CompleteProjectile(const float SessionElapsedSeconds)
{
	const FVector2D AimPoint = TargetSnapshot.ProjectileAimPointNormalized;
	const int32 TargetSerial = TargetSnapshot.ProjectileTargetSerial;
	TargetSnapshot.bProjectileInFlight = false;
	TargetSnapshot.ProjectileRemainingSeconds = 0.0f;
	TargetSnapshot.ProjectilePositionNormalized = AimPoint;

	// FireProjectile reserves this serial by pausing its target countdown. Resolve at
	// impact for presentation timing while scoring still uses the captured fire-time
	// reaction value.
	ResolveShot(
		AimPoint,
		TargetSerial,
		SessionElapsedSeconds,
		TargetSnapshot.ProjectileTargetAgeAtFireSeconds);
}

void UFF_TargetShootingRuntime::SpawnTarget(const float SessionElapsedSeconds)
{
	if (!TargetDefinition)
	{
		return;
	}

	const float DifficultyAlpha = FFTargetShootingRules::GetDifficultyAlpha(
		SessionElapsedSeconds,
		TargetDefinition->DurationSeconds);
	TargetSnapshot.TargetRadiusNormalized = FFTargetShootingRules::GetTargetRadius(
		TargetDefinition->Rules,
		DifficultyAlpha);
	TargetSnapshot.bBonusTarget = RandomStream.FRand() < TargetDefinition->Rules.BonusTargetChance;
	const float BaseLifetimeSeconds = FFTargetShootingRules::GetTargetLifetime(
		TargetDefinition->Rules,
		DifficultyAlpha);
	TargetSnapshot.TargetLifetimeSeconds = FMath::Max(
		BaseLifetimeSeconds
		* (TargetSnapshot.bBonusTarget
			? TargetDefinition->Rules.BonusTargetLifetimeScale
			: 1.0f),
		MinEffectiveTargetLifetimeSeconds);
	TargetSnapshot.TargetRemainingSeconds = TargetSnapshot.TargetLifetimeSeconds;
	TargetSnapshot.TargetAgeSeconds = 0.0f;
	TargetSnapshot.TargetCenterNormalized = FFTargetShootingRules::GenerateTargetCenter(
		RandomStream,
		TargetSnapshot.TargetRadiusNormalized,
		TargetDefinition->Rules.SpawnSafeMarginNormalized);
	TargetSnapshot.TargetSerial = TargetSnapshot.TargetSerial == MAX_int32
		? 1
		: TargetSnapshot.TargetSerial + 1;
	TargetSnapshot.bTargetVisible = true;
}

void UFF_TargetShootingRuntime::ConsumeCurrentTargetAndSpawn(const float SessionElapsedSeconds)
{
	TargetSnapshot.bTargetVisible = false;
	TargetSnapshot.TargetRemainingSeconds = 0.0f;
	if (TargetDefinition && SessionElapsedSeconds < TargetDefinition->DurationSeconds)
	{
		SpawnTarget(SessionElapsedSeconds);
	}
}

void UFF_TargetShootingRuntime::RecordFeedback(
	const EFF_TargetShootingFeedbackType FeedbackType,
	const FVector2D& PointNormalized,
	const int32 ScoreDelta,
	const float ReactionSeconds,
	const bool bWasBonusTarget)
{
	TargetSnapshot.LatestFeedbackSerial = TargetSnapshot.LatestFeedbackSerial == MAX_int32
		? 1
		: TargetSnapshot.LatestFeedbackSerial + 1;
	TargetSnapshot.LatestFeedbackType = FeedbackType;
	TargetSnapshot.LatestFeedbackPointNormalized = PointNormalized;
	TargetSnapshot.LatestScoreDelta = ScoreDelta;
	TargetSnapshot.LatestReactionSeconds = FMath::IsFinite(ReactionSeconds)
		? FMath::Max(0.0f, ReactionSeconds)
		: 0.0f;
	TargetSnapshot.bLatestFeedbackWasBonusTarget = bWasBonusTarget;
}
