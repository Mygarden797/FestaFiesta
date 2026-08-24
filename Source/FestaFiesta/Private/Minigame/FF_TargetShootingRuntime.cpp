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
	TotalHitReactionSeconds = 0.0;
	RandomStream.Initialize(GetSnapshot().Context.RandomSeed);
	SpawnTarget(0.0f);
	return true;
}

void UFF_TargetShootingRuntime::OnAdvancePlaying(
	const float DeltaSeconds,
	const float StartingElapsedSeconds)
{
	if (!TargetDefinition || !TargetSnapshot.bTargetVisible)
	{
		return;
	}

	float SecondsLeft = DeltaSeconds;
	float CursorSeconds = StartingElapsedSeconds;
	int32 ExpirationCount = 0;
	while (SecondsLeft > UE_SMALL_NUMBER
		&& TargetSnapshot.bTargetVisible
		&& ExpirationCount < MaxTargetExpirationsPerAdvance)
	{
		const float StepSeconds = FMath::Min(SecondsLeft, TargetSnapshot.TargetRemainingSeconds);
		TargetSnapshot.TargetRemainingSeconds = FMath::Max(
			0.0f,
			TargetSnapshot.TargetRemainingSeconds - StepSeconds);
		TargetSnapshot.TargetAgeSeconds = FMath::Max(
			0.0f,
			TargetSnapshot.TargetAgeSeconds + StepSeconds);
		SecondsLeft = FMath::Max(0.0f, SecondsLeft - StepSeconds);
		CursorSeconds += StepSeconds;

		if (TargetSnapshot.TargetRemainingSeconds > UE_SMALL_NUMBER)
		{
			break;
		}

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
		const float ReactionSeconds = FMath::Max(0.0f, TargetSnapshot.TargetAgeSeconds);
		const int32 RequestedScoreDelta = FFTargetShootingRules::CalculateShotScore(
			TargetDefinition->Rules,
			TargetSnapshot.CurrentCombo,
			HitQuality,
			ReactionSeconds,
			TargetSnapshot.bBonusTarget);
		const int32 PreviousScore = GetSnapshot().Score;
		SetScore(FFTargetShootingRules::AddScoreSaturated(PreviousScore, RequestedScoreDelta));
		TargetSnapshot.HitCount = FMath::Min(TargetSnapshot.HitCount + 1, MAX_int32);
		TargetSnapshot.CurrentCombo = FMath::Min(TargetSnapshot.CurrentCombo + 1, MAX_int32);
		TargetSnapshot.BestCombo = FMath::Max(
			TargetSnapshot.BestCombo,
			TargetSnapshot.CurrentCombo);
		TotalHitReactionSeconds += static_cast<double>(ReactionSeconds);
		RecordFeedback(
			HitQuality,
			NormalizedPoint,
			GetSnapshot().Score - PreviousScore,
			ReactionSeconds,
			TargetSnapshot.bBonusTarget);
		ConsumeCurrentTargetAndSpawn(GetSnapshot().ElapsedSeconds);
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
			TargetSnapshot.TargetAgeSeconds,
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
	SpawnTarget(SessionElapsedSeconds);
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
