// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Minigame/FF_MinigameTypes.h"
#include "FF_SurfingTypes.generated.h"

/** Stable outcomes for continuous endless-course movement input. */
UENUM(BlueprintType)
enum class EFF_SurfingSteerResult : uint8
{
	Applied,
	RejectedNotPlaying,
	RejectedWrongSession,
	RejectedInvalidDirection,
	/** Kept for serialized v1 widgets; the arena game never enters recovery. */
	RejectedRecovering UMETA(Hidden),
	RejectedInvalidAxis
};

UENUM(BlueprintType)
enum class EFF_SurfingBoostResult : uint8
{
	Activated,
	RejectedNotPlaying,
	RejectedWrongSession,
	RejectedNoCharge,
	RejectedAlreadyBoosting
};

UENUM(BlueprintType)
enum class EFF_SurfingObjectType : uint8
{
	Rock,
	Log,
	Buoy,
	Coin,
	Heart,
	BoostPickup
};

UENUM(BlueprintType)
enum class EFF_SurfingEventType : uint8
{
	None,
	/** Legacy event kept for serialized v1 widgets. */
	ObstacleAvoided UMETA(Hidden),
	ObstacleHit,
	/** A boosted contact. The obstacle remains in its current course segment. */
	ObstacleSmashed,
	CoinCollected,
	HeartCollected,
	BoostPickupCollected,
	BoostActivated
};

/** One authored object in a repeatable two-dimensional surfing course segment. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingCourseRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfing Course")
	EFF_SurfingObjectType Type = EFF_SurfingObjectType::Rock;

	/** Segment-local location in [-1, 1]. +Y is course-forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfing Course")
	FVector2D PositionNormalized = FVector2D::ZeroVector;

	/** Visually circular radius in the aspect-corrected arena metric. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfing Course", meta = (ClampMin = "0.001"))
	float CollisionRadiusNormalized = 0.1f;

	/** Zero uses the matching Rules value; positive values override this object's score magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfing Course", meta = (ClampMin = "0.0"))
	float ScoreValueOverride = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfing Course")
	bool bEnabled = true;
};

/** Data-authored tuning for the deterministic endless-course surfing runtime. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float CharacterSpeedNormalizedPerSecond = 0.7f;

	/** Display metric width per one normalized X unit (arena aspect ratio). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.01"))
	float ArenaHorizontalMetricScale = 2.18f;

	/** A/D steering contribution while the course keeps moving forward. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SteeringStrength = 0.55f;

	/** W/S speed adjustment. Values below one guarantee uninterrupted forward travel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Movement", meta = (ClampMin = "0.0", ClampMax = "0.75"))
	float ForwardInputSpeedInfluence = 0.25f;

	/** Length of one repeatable DataTable course segment in normalized forward units. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Course", meta = (ClampMin = "2.01", ClampMax = "10.0"))
	float CourseSegmentLengthNormalized = 2.4f;

	/** Recycled segments retained behind the surfer for smooth reverse turns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Course", meta = (ClampMin = "1", ClampMax = "8"))
	int32 CourseSegmentsBehind = 2;

	/** Recycled segments prepared in front of the surfer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Course", meta = (ClampMin = "2", ClampMax = "12"))
	int32 CourseSegmentsAhead = 4;

	/** Player radius in display-metric units, not raw normalized X distance. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float PlayerCollisionRadiusNormalized = 0.065f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Collision", meta = (ClampMin = "0.01"))
	float HitInvincibilitySeconds = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0.01"))
	float BoostDurationSeconds = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "1.0"))
	float BoostSpeedMultiplier = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player", meta = (ClampMin = "1"))
	int32 MaxLives = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Player", meta = (ClampMin = "0"))
	int32 InitialLives = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "1"))
	int32 MaxBoostCharges = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boost", meta = (ClampMin = "0"))
	int32 InitialBoostCharges = 3;

	/** Score per unit of actual two-dimensional path length. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float DistanceScorePerMeter = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float CoinScore = 75.0f;

	/** Reward for contacting an obstacle while boosted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float ObstacleDestroyScore = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score", meta = (ClampMin = "0.0"))
	float CollisionPenalty = 100.0f;

	// Serialized v1 scrolling-course assets remain loadable while the segment
	// based runtime uses the explicit settings above.
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CharacterSpeedNormalizedPerSecond."))
	float LateralSpeedNormalizedPerSecond = 1.6f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Fixed-arena surfing has no world scroll speed."))
	float ForwardSpeedStartMetersPerSecond = 8.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Fixed-arena surfing has no world scroll speed."))
	float ForwardSpeedEndMetersPerSecond = 12.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Fixed-arena surfing has no view distance."))
	float ViewDistanceMeters = 35.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable positions."))
	float RowSpacingStartMeters = 7.0f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable positions."))
	float RowSpacingEndMeters = 4.5f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	int32 SpawnColumnCount = 7;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	int32 ObstacleCountStart = 1;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	int32 ObstacleCountEnd = 3;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	float CoinRowProbability = 0.55f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	float HeartRowProbability = 0.08f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CourseLayoutTable rows."))
	float BoostRowProbability = 0.06f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use each course row's collision radius."))
	float ObjectCollisionRadiusNormalized = 0.085f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Fixed obstacles are not scored as avoided."))
	float ObstacleAvoidScore = 2.5f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Boost speed already increases scored path length."))
	float BoostDistanceScoreMultiplier = 1.1f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Balance gameplay was replaced by arena surfing."))
	float PerfectBalanceThreshold = 0.18f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Balance gameplay was replaced by arena surfing."))
	float SafeBalanceThreshold = 0.55f;

	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Balance gameplay was replaced by arena surfing."))
	float WipeoutThreshold = 1.0f;

	bool IsValid(FString* OutFailureReason = nullptr) const;
};

/** One active recycled-course object projected into camera-relative coordinates. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingObjectSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 ObjectId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingObjectType Type = EFF_SurfingObjectType::Rock;

	/** Camera-relative position. Y=0 is the surfer and +Y is farther ahead. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	FVector2D PositionNormalized = FVector2D::ZeroVector;

	/** Visually circular radius in the aspect-corrected arena metric. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float CollisionRadiusNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float LateralPositionNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float ForwardPositionNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float DistanceAheadMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float ForwardAlpha = 0.0f;
};

/** Authoritative read model for the continuously recycled surfing course. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_SurfingSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	FFF_MinigameSessionSnapshot Session;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	FVector2D PlayerPositionNormalized = FVector2D::ZeroVector;

	/** Unbounded course-forward coordinate. Presentation remains camera-relative. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	double CourseProgressNormalized = 0.0;

	/** Repeatable segment currently underneath the surfer. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 CourseSegmentIndex = 0;

	/** Horizontal display metric used by authoritative movement and collision. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float ArenaHorizontalMetricScale = 2.18f;

	/** Last raw input vector. Zero recentres steering while automatic forward travel continues. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	FVector2D MovementInput = FVector2D::ZeroVector;

	/** Auto-forward steering direction under the aspect-corrected arena metric. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	FVector2D CurrentHeading = FVector2D(0.0, 1.0);

	/** Accumulated two-dimensional path length. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float DistanceMeters = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float CurrentSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 Lives = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 MaxLives = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 BoostCharges = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 MaxBoostCharges = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float BoostRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	bool bBoosting = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	float HitInvincibilityRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	bool bInvincible = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 CollisionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 SmashedCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 CoinCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 HeartCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 BoostPickupCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 SteerCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	int32 LatestEventSerial = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	EFF_SurfingEventType LatestEventType = EFF_SurfingEventType::None;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Minigame|Surfing")
	TArray<FFF_SurfingObjectSnapshot> VisibleObjects;

	// Compatibility aliases for the independently migrated one-axis/scrolling UI.
	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float PlayerLateralNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float PlayerForwardNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float SteerAxis = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float ForwardAxis = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	int32 AvoidedCount = 0;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float Balance = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float WaveForcePerSecond = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float WaveChangeRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float RecoveryRemainingSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	bool bRecovering = false;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	int32 WipeoutCount = 0;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float StableRideSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float PerfectRideSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float CurrentFlowSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, meta = (DeprecatedProperty))
	float BestFlowSeconds = 0.0f;
};
