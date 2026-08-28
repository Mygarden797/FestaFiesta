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
	RejectedInvalidCoordinate,
	/** A projectile was accepted and will resolve when it reaches its aim point. */
	ProjectileFired,
	RejectedProjectileInFlight
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|1. 과녁 노출 시간",
		meta = (ClampMin = "0.05", UIMin = "0.05", Units = "s", DisplayName = "초반 과녁 유지 시간", ToolTip = "게임 시작 직후 과녁 하나가 화면에 나타나 있는 시간입니다. 이 시간이 지나면 점수 감점 없이 놓친 과녁 수가 늘고 콤보가 초기화됩니다."))
	float TargetLifetimeStartSeconds = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|1. 과녁 노출 시간",
		meta = (ClampMin = "0.05", UIMin = "0.05", Units = "s", DisplayName = "후반 과녁 유지 시간", ToolTip = "게임 종료에 가까워졌을 때 과녁 하나가 화면에 나타나 있는 시간입니다. 초반 값보다 클 수 없으며, 만료 시 점수 감점 없이 콤보가 초기화됩니다."))
	float TargetLifetimeEndSeconds = 0.7f;

	/** Ellipse radii relative to the play area's width and height. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|2. 과녁 크기·위치",
		meta = (ClampMin = "0.001", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.25", DisplayName = "초반 과녁 반지름", ToolTip = "게임 시작 직후 과녁의 가로(X)·세로(Y) 반지름입니다. 플레이 영역 크기를 1로 본 비율이며, 예: X=0.075는 화면 너비의 7.5%입니다."))
	FVector2D TargetRadiusStartNormalized = FVector2D(0.075f, 0.125f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|2. 과녁 크기·위치",
		meta = (ClampMin = "0.001", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.25", DisplayName = "후반 과녁 반지름", ToolTip = "게임 종료에 가까워졌을 때 과녁의 가로(X)·세로(Y) 반지름입니다. 초반 크기보다 클 수 없습니다."))
	FVector2D TargetRadiusEndNormalized = FVector2D(0.050f, 0.085f);

	/** Keeps targets clear of the top HUD, close button and screen edges. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|2. 과녁 크기·위치",
		meta = (DisplayName = "과녁 생성 안전 여백", ToolTip = "과녁이 HUD, 닫기 버튼, 물총과 겹치지 않도록 비워 두는 화면 비율입니다. Left/Top/Right/Bottom 순서이며, 과녁 전체가 이 여백 안쪽에 생성됩니다."))
	FMargin SpawnSafeMarginNormalized = FMargin(0.04f, 0.18f, 0.04f, 0.24f);

	/** Bottom-center muzzle used by the normalized 2D projectile simulation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "슈팅 세부 설정|7. 고급 연출 설정",
		meta = (DisplayName = "물 탄환 시작 위치", ToolTip = "물 탄환이 출발하는 총구 위치입니다. 플레이 영역 왼쪽 위가 (0,0), 오른쪽 아래가 (1,1)입니다."))
	FVector2D ProjectileOriginNormalized = FVector2D(0.5f, 0.68f);

	/** Travel speed in normalized play-area units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "슈팅 세부 설정|7. 고급 연출 설정",
		meta = (ClampMin = "0.1", ClampMax = "100.0", UIMin = "0.1", UIMax = "5.0", DisplayName = "물 탄환 이동 속도", ToolTip = "2D 물 탄환이 초당 이동하는 화면 비율입니다. 과녁 위치와 반응 점수는 발사 시점 기준으로 예약되지만, 느릴수록 결과 확정과 다음 과녁 등장이 늦어져 한 판의 득점 기회가 줄 수 있습니다."))
	float ProjectileSpeedNormalizedPerSecond = 1.8f;

	/** Prevents very short shots from resolving in the same frame as the fire input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = "슈팅 세부 설정|7. 고급 연출 설정",
		meta = (ClampMin = "0.01", ClampMax = "0.5", UIMin = "0.01", UIMax = "0.5", Units = "s", DisplayName = "최소 탄환 비행 시간", ToolTip = "아주 가까운 곳을 쏴도 탄환이 최소한 화면에 보이는 시간입니다. 과녁은 예약되지만 결과 확정과 다음 과녁 등장은 이 시간만큼 늦어집니다."))
	float ProjectileMinTravelSeconds = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|3. 기본 점수",
		meta = (ClampMin = "1", UIMin = "1", DisplayName = "명중 기본 점수", ToolTip = "과녁 바깥쪽 유효 영역을 한 번 맞혔을 때 받는 기본 점수입니다."))
	int32 HitBaseScore = 100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|3. 기본 점수",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "연속 명중당 추가 점수", ToolTip = "현재 연속 명중 수 1단계마다 기본 점수에 더하는 점수입니다."))
	int32 ComboBonusPerHit = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|3. 기본 점수",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "연속 명중 추가 점수 상한", ToolTip = "콤보로 한 발에 추가할 수 있는 최대 점수입니다."))
	int32 MaxComboBonus = 50;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|3. 기본 점수",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "빗나감 감점", ToolTip = "과녁을 빗나갔을 때 차감하는 점수입니다. 실제 점수는 0 아래로 내려가지 않으며 콤보도 초기화됩니다."))
	int32 MissPenalty = 25;

	/** Ellipse-distance ratio that begins the precision scoring ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|4. 정밀 명중",
		meta = (ClampMin = "0.01", ClampMax = "0.99", UIMin = "0.01", UIMax = "0.99", DisplayName = "정밀 명중 반지름 비율", ToolTip = "과녁 전체 반지름 중 정밀 명중 영역이 차지하는 비율입니다. 값이 작을수록 정밀 명중이 어려워집니다."))
	float PrecisionRadiusRatio = 0.58f;

	/** Ellipse-distance ratio that begins the bullseye scoring ring. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|4. 정밀 명중",
		meta = (ClampMin = "0.01", ClampMax = "0.99", UIMin = "0.01", UIMax = "0.99", DisplayName = "정중앙 반지름 비율", ToolTip = "과녁 전체 반지름 중 정중앙 영역이 차지하는 비율입니다. '정밀 명중 반지름 비율'보다 작아야 합니다."))
	float BullseyeRadiusRatio = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|4. 정밀 명중",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "정밀 명중 추가 점수", ToolTip = "정밀 명중 영역을 맞혔을 때 기본 점수에 더하는 점수입니다."))
	int32 PrecisionScoreBonus = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|4. 정밀 명중",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "정중앙 추가 점수", ToolTip = "정중앙 영역을 맞혔을 때 기본 점수에 더하는 점수입니다. 정밀 명중 추가 점수보다 작을 수 없습니다."))
	int32 BullseyeScoreBonus = 50;

	/** Hits at or below this age receive the full reaction bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|5. 반응 속도 보너스",
		meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s", DisplayName = "최고 반응 판정 시간", ToolTip = "과녁 등장 후 이 시간 안에 발사하면 반응 속도 추가 점수를 전부 받습니다."))
	float FastReactionSeconds = 0.20f;

	/** Hits at or above this age receive no reaction bonus. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|5. 반응 속도 보너스",
		meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s", DisplayName = "반응 보너스 종료 시간", ToolTip = "과녁 등장 후 이 시간 이상 지나 발사하면 반응 속도 추가 점수를 받지 못합니다. 최고 반응 판정 시간보다 커야 합니다."))
	float SlowReactionSeconds = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|5. 반응 속도 보너스",
		meta = (ClampMin = "0", UIMin = "0", DisplayName = "반응 속도 최대 추가 점수", ToolTip = "가장 빠르게 맞혔을 때 기본 점수에 더하는 최대 반응 속도 점수입니다."))
	int32 MaxReactionScoreBonus = 50;

	/** Deterministic per-target chance for a short-lived golden target. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|6. 보너스 과녁",
		meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "보너스 과녁 등장 확률", ToolTip = "새 과녁이 황금 보너스 과녁으로 등장할 확률입니다. 0은 0%, 1은 100%입니다."))
	float BonusTargetChance = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|6. 보너스 과녁",
		meta = (ClampMin = "1.0", ClampMax = "100.0", UIMin = "1.0", UIMax = "5.0", DisplayName = "보너스 과녁 점수 배율", ToolTip = "보너스 과녁 명중 시 최종 획득 점수에 곱하는 배율입니다."))
	float BonusTargetScoreMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "슈팅 세부 설정|6. 보너스 과녁",
		meta = (ClampMin = "0.1", ClampMax = "1.0", UIMin = "0.1", UIMax = "1.0", DisplayName = "보너스 과녁 유지 시간 배율", ToolTip = "일반 과녁 유지 시간에 곱하는 값입니다. 0.72이면 일반 과녁 시간의 72% 동안 표시됩니다."))
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

	/** Monotonic id for accepted projectiles. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 ProjectileSerial = 0;

	/** Only one projectile may be active, keeping every shot deterministic. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	bool bProjectileInFlight = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	int32 ProjectileTargetSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D ProjectileStartNormalized = FVector2D(0.5f, 0.96f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D ProjectileAimPointNormalized = FVector2D(0.5f, 0.5f);

	/** Interpolated projectile position supplied directly to presentation code. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	FVector2D ProjectilePositionNormalized = FVector2D(0.5f, 0.96f);

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float ProjectileTravelSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float ProjectileRemainingSeconds = 0.0f;

	/** Target age captured at fire time so travel distance cannot distort reaction scoring. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Target Shooting")
	float ProjectileTargetAgeAtFireSeconds = 0.0f;

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
