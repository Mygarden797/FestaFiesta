// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/TargetShooting/FF_TargetShootingRules.h"

namespace
{
	constexpr float MinTargetLifetimeSeconds = 0.05f;
	constexpr float MinEffectiveRulesTargetLifetimeSeconds = 0.01f;

	bool IsFinitePositiveRadius(const FVector2D& Radius)
	{
		return FMath::IsFinite(Radius.X)
			&& FMath::IsFinite(Radius.Y)
			&& Radius.X > 0.0f
			&& Radius.Y > 0.0f
			&& Radius.X <= 0.5f
			&& Radius.Y <= 0.5f;
	}

	bool IsFiniteSafeMargin(const FMargin& Margin)
	{
		return FMath::IsFinite(Margin.Left)
			&& FMath::IsFinite(Margin.Top)
			&& FMath::IsFinite(Margin.Right)
			&& FMath::IsFinite(Margin.Bottom)
			&& Margin.Left >= 0.0f
			&& Margin.Top >= 0.0f
			&& Margin.Right >= 0.0f
			&& Margin.Bottom >= 0.0f
			&& Margin.Left + Margin.Right < 1.0f
			&& Margin.Top + Margin.Bottom < 1.0f;
	}
}

bool FFF_TargetShootingRules::IsValid(FString* OutFailureReason) const
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (!FMath::IsFinite(TargetLifetimeStartSeconds)
		|| !FMath::IsFinite(TargetLifetimeEndSeconds)
		|| TargetLifetimeStartSeconds < MinTargetLifetimeSeconds
		|| TargetLifetimeEndSeconds < MinTargetLifetimeSeconds
		|| TargetLifetimeEndSeconds > TargetLifetimeStartSeconds)
	{
		return Fail(TEXT("과녁 유지 시간은 0.05초 이상이어야 하며 후반 값이 초반 값보다 길 수 없습니다."));
	}

	if (!IsFinitePositiveRadius(TargetRadiusStartNormalized)
		|| !IsFinitePositiveRadius(TargetRadiusEndNormalized)
		|| TargetRadiusEndNormalized.X > TargetRadiusStartNormalized.X
		|| TargetRadiusEndNormalized.Y > TargetRadiusStartNormalized.Y)
	{
		return Fail(TEXT("과녁 반지름은 0보다 크고 0.5 이하여야 하며 후반 크기가 초반 크기보다 클 수 없습니다."));
	}

	if (!IsFiniteSafeMargin(SpawnSafeMarginNormalized))
	{
		return Fail(TEXT("과녁 생성 안전 여백은 0 이상이어야 하며 실제 플레이 공간을 남겨야 합니다."));
	}
	if (SpawnSafeMarginNormalized.Left + SpawnSafeMarginNormalized.Right
			+ 2.0f * TargetRadiusStartNormalized.X >= 1.0f
		|| SpawnSafeMarginNormalized.Top + SpawnSafeMarginNormalized.Bottom
			+ 2.0f * TargetRadiusStartNormalized.Y >= 1.0f)
	{
		return Fail(TEXT("과녁 생성 안전 여백 안에 가장 큰 과녁 전체가 들어갈 공간이 필요합니다."));
	}

	if (!FFTargetShootingRules::IsNormalizedCoordinate(ProjectileOriginNormalized)
		|| !FMath::IsFinite(ProjectileSpeedNormalizedPerSecond)
		|| ProjectileSpeedNormalizedPerSecond < 0.1f
		|| ProjectileSpeedNormalizedPerSecond > 100.0f
		|| !FMath::IsFinite(ProjectileMinTravelSeconds)
		|| ProjectileMinTravelSeconds < 0.01f
		|| ProjectileMinTravelSeconds > 0.5f)
	{
		return Fail(TEXT("물 탄환 시작 위치, 이동 속도 또는 최소 비행 시간이 허용 범위를 벗어났습니다."));
	}

	if (HitBaseScore <= 0 || ComboBonusPerHit < 0 || MaxComboBonus < 0 || MissPenalty < 0)
	{
		return Fail(TEXT("명중 기본 점수는 1 이상이어야 하며 콤보·감점 값은 0 이상이어야 합니다."));
	}

	if (!FMath::IsFinite(PrecisionRadiusRatio)
		|| !FMath::IsFinite(BullseyeRadiusRatio)
		|| PrecisionRadiusRatio <= 0.0f
		|| PrecisionRadiusRatio >= 1.0f
		|| BullseyeRadiusRatio <= 0.0f
		|| BullseyeRadiusRatio >= PrecisionRadiusRatio
		|| PrecisionScoreBonus < 0
		|| BullseyeScoreBonus < PrecisionScoreBonus)
	{
		return Fail(TEXT("정중앙 영역은 정밀 영역보다 작아야 하며 정중앙 추가 점수는 정밀 추가 점수 이상이어야 합니다."));
	}

	if (!FMath::IsFinite(FastReactionSeconds)
		|| !FMath::IsFinite(SlowReactionSeconds)
		|| FastReactionSeconds <= 0.0f
		|| SlowReactionSeconds <= FastReactionSeconds
		|| MaxReactionScoreBonus < 0)
	{
		return Fail(TEXT("반응 시간은 0보다 커야 하며 보너스 종료 시간이 최고 반응 판정 시간보다 길어야 합니다."));
	}

	if (!FMath::IsFinite(BonusTargetChance)
		|| BonusTargetChance < 0.0f
		|| BonusTargetChance > 1.0f
		|| !FMath::IsFinite(BonusTargetScoreMultiplier)
		|| BonusTargetScoreMultiplier < 1.0f
		|| BonusTargetScoreMultiplier > 100.0f
		|| !FMath::IsFinite(BonusTargetLifetimeScale)
		|| BonusTargetLifetimeScale < 0.1f
		|| BonusTargetLifetimeScale > 1.0f
		|| TargetLifetimeEndSeconds * BonusTargetLifetimeScale
			< MinEffectiveRulesTargetLifetimeSeconds)
	{
		return Fail(TEXT("보너스 과녁의 등장 확률, 점수 배율 또는 유지 시간 배율이 허용 범위를 벗어났습니다."));
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

namespace FFTargetShootingRules
{
	bool IsNormalizedCoordinate(const FVector2D& Coordinate)
	{
		return FMath::IsFinite(Coordinate.X)
			&& FMath::IsFinite(Coordinate.Y)
			&& Coordinate.X >= 0.0f
			&& Coordinate.X <= 1.0f
			&& Coordinate.Y >= 0.0f
			&& Coordinate.Y <= 1.0f;
	}

	bool IsInsideTarget(
		const FVector2D& Coordinate,
		const FVector2D& Center,
		const FVector2D& Radius)
	{
		if (!IsNormalizedCoordinate(Coordinate)
			|| !IsNormalizedCoordinate(Center)
			|| !IsFinitePositiveRadius(Radius))
		{
			return false;
		}

		const FVector2D NormalizedDelta(
			(Coordinate.X - Center.X) / Radius.X,
			(Coordinate.Y - Center.Y) / Radius.Y);
		return NormalizedDelta.SizeSquared() <= 1.0f + UE_KINDA_SMALL_NUMBER;
	}

	EFF_TargetShootingFeedbackType GetHitQuality(
		const FFF_TargetShootingRules& Rules,
		const FVector2D& Coordinate,
		const FVector2D& Center,
		const FVector2D& Radius)
	{
		if (!IsNormalizedCoordinate(Coordinate)
			|| !IsNormalizedCoordinate(Center)
			|| !IsFinitePositiveRadius(Radius))
		{
			return EFF_TargetShootingFeedbackType::Miss;
		}

		const FVector2D NormalizedDelta(
			(Coordinate.X - Center.X) / Radius.X,
			(Coordinate.Y - Center.Y) / Radius.Y);
		const float RadialDistance = NormalizedDelta.Size();
		if (RadialDistance > 1.0f + UE_KINDA_SMALL_NUMBER)
		{
			return EFF_TargetShootingFeedbackType::Miss;
		}
		if (RadialDistance <= Rules.BullseyeRadiusRatio + UE_KINDA_SMALL_NUMBER)
		{
			return EFF_TargetShootingFeedbackType::Bullseye;
		}
		if (RadialDistance <= Rules.PrecisionRadiusRatio + UE_KINDA_SMALL_NUMBER)
		{
			return EFF_TargetShootingFeedbackType::Precision;
		}
		return EFF_TargetShootingFeedbackType::Hit;
	}

	float GetDifficultyAlpha(const float ElapsedSeconds, const float DurationSeconds)
	{
		if (!FMath::IsFinite(ElapsedSeconds)
			|| !FMath::IsFinite(DurationSeconds)
			|| DurationSeconds <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
	}

	float GetTargetLifetime(
		const FFF_TargetShootingRules& Rules,
		const float DifficultyAlpha)
	{
		return FMath::Lerp(
			Rules.TargetLifetimeStartSeconds,
			Rules.TargetLifetimeEndSeconds,
			FMath::Clamp(DifficultyAlpha, 0.0f, 1.0f));
	}

	FVector2D GetTargetRadius(
		const FFF_TargetShootingRules& Rules,
		const float DifficultyAlpha)
	{
		return FMath::Lerp(
			Rules.TargetRadiusStartNormalized,
			Rules.TargetRadiusEndNormalized,
			FMath::Clamp(DifficultyAlpha, 0.0f, 1.0f));
	}

	FVector2D GenerateTargetCenter(
		FRandomStream& Stream,
		const FVector2D& Radius,
		const FMargin& SafeMarginNormalized)
	{
		const FVector2D SafeRadius(
			FMath::Clamp(Radius.X, UE_SMALL_NUMBER, 0.5f),
			FMath::Clamp(Radius.Y, UE_SMALL_NUMBER, 0.5f));
		const FMargin SafeMargin = IsFiniteSafeMargin(SafeMarginNormalized)
			? SafeMarginNormalized
			: FMargin(0.0f);
		float MinX = SafeRadius.X + static_cast<float>(SafeMargin.Left);
		float MaxX = 1.0f - SafeRadius.X - static_cast<float>(SafeMargin.Right);
		float MinY = SafeRadius.Y + static_cast<float>(SafeMargin.Top);
		float MaxY = 1.0f - SafeRadius.Y - static_cast<float>(SafeMargin.Bottom);
		if (MinX > MaxX)
		{
			MinX = MaxX = 0.5f;
		}
		if (MinY > MaxY)
		{
			MinY = MaxY = 0.5f;
		}
		const float UnitX = Stream.FRand();
		const float UnitY = Stream.FRand();
		return FVector2D(
			FMath::Lerp(MinX, MaxX, UnitX),
			FMath::Lerp(MinY, MaxY, UnitY));
	}

	int32 CalculateHitScore(const FFF_TargetShootingRules& Rules, const int32 CurrentCombo)
	{
		const int64 ComboBonus = FMath::Min<int64>(
			static_cast<int64>(FMath::Max(0, CurrentCombo)) * Rules.ComboBonusPerHit,
			Rules.MaxComboBonus);
		return static_cast<int32>(FMath::Min<int64>(
			static_cast<int64>(Rules.HitBaseScore) + ComboBonus,
			MAX_int32));
	}

	int32 CalculateReactionScoreBonus(
		const FFF_TargetShootingRules& Rules,
		const float ReactionSeconds)
	{
		if (!FMath::IsFinite(ReactionSeconds)
			|| ReactionSeconds < 0.0f
			|| Rules.MaxReactionScoreBonus <= 0)
		{
			return 0;
		}
		if (ReactionSeconds <= Rules.FastReactionSeconds)
		{
			return Rules.MaxReactionScoreBonus;
		}
		if (ReactionSeconds >= Rules.SlowReactionSeconds)
		{
			return 0;
		}

		const double BonusAlpha = 1.0 - static_cast<double>(
			(ReactionSeconds - Rules.FastReactionSeconds)
			/ (Rules.SlowReactionSeconds - Rules.FastReactionSeconds));
		return FMath::Clamp(
			FMath::RoundToInt(static_cast<double>(Rules.MaxReactionScoreBonus) * BonusAlpha),
			0,
			Rules.MaxReactionScoreBonus);
	}

	int32 CalculateShotScore(
		const FFF_TargetShootingRules& Rules,
		const int32 CurrentCombo,
		const EFF_TargetShootingFeedbackType HitQuality,
		const float ReactionSeconds,
		const bool bBonusTarget)
	{
		int64 QualityBonus = 0;
		switch (HitQuality)
		{
		case EFF_TargetShootingFeedbackType::Bullseye:
			QualityBonus = Rules.BullseyeScoreBonus;
			break;
		case EFF_TargetShootingFeedbackType::Precision:
			QualityBonus = Rules.PrecisionScoreBonus;
			break;
		case EFF_TargetShootingFeedbackType::Hit:
			break;
		default:
			return 0;
		}

		const int64 RawScore = static_cast<int64>(CalculateHitScore(Rules, CurrentCombo))
			+ QualityBonus
			+ CalculateReactionScoreBonus(Rules, ReactionSeconds);
		const double MultipliedScore = static_cast<double>(RawScore)
			* (bBonusTarget ? static_cast<double>(Rules.BonusTargetScoreMultiplier) : 1.0);
		return static_cast<int32>(FMath::Clamp<int64>(
			FMath::RoundToInt64(MultipliedScore),
			0,
			MAX_int32));
	}

	int32 AddScoreSaturated(const int32 CurrentScore, const int32 Delta)
	{
		return static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(FMath::Max(0, CurrentScore)) + Delta,
			0,
			MAX_int32));
	}

	float CalculateAccuracyPercent(const int32 HitCount, const int32 MissCount)
	{
		const int64 SafeHits = FMath::Max(0, HitCount);
		const int64 ShotCount = SafeHits + FMath::Max(0, MissCount);
		return ShotCount > 0
			? static_cast<float>(100.0 * static_cast<double>(SafeHits) / static_cast<double>(ShotCount))
			: 0.0f;
	}

	float CalculateHitsPerSecond(const int32 HitCount, const float ElapsedSeconds)
	{
		if (HitCount <= 0 || !FMath::IsFinite(ElapsedSeconds) || ElapsedSeconds <= UE_SMALL_NUMBER)
		{
			return 0.0f;
		}
		return static_cast<float>(HitCount) / ElapsedSeconds;
	}

	int32 CalculateRewardGold(
		const int32 Score,
		const int32 ScoreForMaxReward,
		const int32 MaxRewardGold)
	{
		if (Score <= 0 || ScoreForMaxReward <= 0 || MaxRewardGold <= 0)
		{
			return 0;
		}

		const double Ratio = FMath::Clamp(
			static_cast<double>(Score) / static_cast<double>(ScoreForMaxReward),
			0.0,
			1.0);
		return FMath::Clamp(
			FMath::RoundToInt(static_cast<double>(MaxRewardGold) * Ratio),
			0,
			MaxRewardGold);
	}
}
