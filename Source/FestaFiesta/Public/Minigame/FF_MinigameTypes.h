// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_MinigameTypes.generated.h"

/** Runtime state shared by every facility minigame. */
UENUM(BlueprintType)
enum class EFF_MinigameSessionState : uint8
{
	Inactive,
	Playing,
	Completed,
	Cancelled
};

/** Stable reasons returned at the authoritative start boundary. */
UENUM(BlueprintType)
enum class EFF_MinigameStartResult : uint8
{
	Success,
	InvalidController,
	AnotherSessionActive,
	NotLiveStage,
	FacilityNotFound,
	FacilityNotActive,
	FacilityNeedsRelocation,
	NoMinigameForFacility,
	DefinitionInvalid,
	RuntimeCreationFailed,
	ScreenUnavailable
};

UENUM(BlueprintType)
enum class EFF_MinigameEndReason : uint8
{
	None,
	TimeExpired,
	PlayerCancelled,
	StageChanged,
	FacilityRemoved,
	WorldRestored,
	WorldShutdown
};

UENUM(BlueprintType)
enum class EFF_TargetShootingShotResult : uint8
{
	Hit,
	Miss,
	RejectedNotPlaying,
	RejectedWrongSession,
	RejectedStaleTarget,
	RejectedInvalidCoordinate
};

/** Presentation/scoring tier produced by the authoritative shot calculation. */
UENUM(BlueprintType)
enum class EFF_TargetShootingFeedbackType : uint8
{
	None,
	Miss,
	Hit,
	Precision,
	Bullseye,
	Escaped
};

/** Immutable input supplied when a runtime strategy is created. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MinigameSessionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FGuid SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FGuid FacilityInstanceId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FName FacilityDefinitionId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FName MinigameId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	int32 RandomSeed = 0;
};

/** Generic read model consumed by the coordinator and result UI. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MinigameSessionSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FFF_MinigameSessionContext Context;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	EFF_MinigameSessionState State = EFF_MinigameSessionState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	EFF_MinigameEndReason EndReason = EFF_MinigameEndReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	float ElapsedSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	float RemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	int32 Score = 0;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_MinigameResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FGuid SessionId;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FName MinigameId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	int32 Score = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	int32 RewardGold = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	FText GradeText;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame")
	EFF_MinigameEndReason EndReason = EFF_MinigameEndReason::None;
};

/** Data-authored tuning for the first target-shooting runtime. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_TargetShootingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0.1"))
	float TargetLifetimeStartSeconds = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target", meta = (ClampMin = "0.1"))
	float TargetLifetimeEndSeconds = 0.7f;

	/** Ellipse radii relative to the play area's width and height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FVector2D TargetRadiusStartNormalized = FVector2D(0.060f, 0.105f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FVector2D TargetRadiusEndNormalized = FVector2D(0.038f, 0.067f);

	/** Keeps targets clear of the top HUD, close button and screen edges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	FMargin SpawnSafeMarginNormalized = FMargin(0.04f, 0.18f, 0.04f, 0.06f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "1"))
	int32 HitBaseScore = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
	int32 ComboBonusPerHit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
	int32 MaxComboBonus = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0"))
	int32 MissPenalty = 25;

	/** Ellipse-distance ratio that begins the precision scoring ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Precision", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float PrecisionRadiusRatio = 0.58f;

	/** Ellipse-distance ratio that begins the bullseye scoring ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Precision", meta = (ClampMin = "0.01", ClampMax = "0.99"))
	float BullseyeRadiusRatio = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Precision", meta = (ClampMin = "0"))
	int32 PrecisionScoreBonus = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Precision", meta = (ClampMin = "0"))
	int32 BullseyeScoreBonus = 50;

	/** Hits at or below this age receive the full reaction bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0.01", Units = "s"))
	float FastReactionSeconds = 0.20f;

	/** Hits at or above this age receive no reaction bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0.01", Units = "s"))
	float SlowReactionSeconds = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = "0"))
	int32 MaxReactionScoreBonus = 50;

	/** Deterministic per-target chance for a short-lived golden target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonus Target", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BonusTargetChance = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonus Target", meta = (ClampMin = "1.0", ClampMax = "100.0"))
	float BonusTargetScoreMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bonus Target", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float BonusTargetLifetimeScale = 0.72f;

	bool IsValid(FString* OutFailureReason = nullptr) const;
};

/** Target-shooting-specific snapshot. Coordinates are normalized for DPI independence. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_TargetShootingSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FFF_MinigameSessionSnapshot Session;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 TargetSerial = 0;

	/** False after the session has ended; UI must not accept shots in that state. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	bool bTargetVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D TargetCenterNormalized = FVector2D(0.5f, 0.5f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D TargetRadiusNormalized = FVector2D(0.05f, 0.09f);

	/** Visual scoring rings use the same authoritative radial thresholds as hit testing. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float PrecisionRadiusRatio = 0.58f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float BullseyeRadiusRatio = 0.26f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float TargetRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float TargetLifetimeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float TargetAgeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	bool bBonusTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 HitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 MissCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 EscapedTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 CurrentCombo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 BestCombo = 0;

	/** Monotonic presentation event; accepted shots and escaped targets increment it once. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 LatestFeedbackSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	EFF_TargetShootingFeedbackType LatestFeedbackType = EFF_TargetShootingFeedbackType::None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D LatestFeedbackPointNormalized = FVector2D(0.5f, 0.5f);

	/** Actual score mutation after saturation; misses are zero or negative. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 LatestScoreDelta = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float LatestReactionSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	bool bLatestFeedbackWasBonusTarget = false;

	/** Aim-trainer metrics projected for HUD/result presentation. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float AccuracyPercent = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float HitsPerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float AverageHitReactionSeconds = 0.0f;
};
