// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_SurfingRuntime.h"

#include "Engine/DataTable.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/Surfing/FF_SurfingRules.h"

namespace
{
	constexpr double FixedTickToleranceSeconds = 1.0e-9;
	constexpr double MovementTolerance = 1.0e-10;
	constexpr int32 MaximumReflectedSegmentsPerTick = 128;

	void IncrementSaturated(int32& Value)
	{
		if (Value < MAX_int32)
		{
			++Value;
		}
	}

	bool IsNearlySameInput(const FVector2D& A, const FVector2D& B)
	{
		return A.X == B.X && A.Y == B.Y;
	}

	bool IsInsideCircle(
		const FVector2D& Point,
		const FVector2D& Center,
		const double Radius,
		const double ArenaHorizontalMetricScale)
	{
		const double MetricDeltaX = (Point.X - Center.X) * ArenaHorizontalMetricScale;
		const double DeltaY = Point.Y - Center.Y;
		return MetricDeltaX * MetricDeltaX + DeltaY * DeltaY <= Radius * Radius;
	}

	struct FContactCandidate
	{
		int32 CourseObjectIndex = INDEX_NONE;
		double TimeOfImpact = 0.0;
	};
}

bool UFF_SurfingRuntime::OnInitialize(FString& OutFailureReason)
{
	SurfingDefinition = Cast<UFF_SurfingDefinition>(GetDefinition());
	if (!SurfingDefinition)
	{
		OutFailureReason = TEXT("surfing runtime requires UFF_SurfingDefinition");
		return false;
	}

	SurfingSnapshot = FFF_SurfingSnapshot();
	SurfingSnapshot.Session = GetSnapshot();
	SurfingSnapshot.PlayerPositionNormalized = FVector2D::ZeroVector;
	SurfingSnapshot.ArenaHorizontalMetricScale = SurfingDefinition->Rules.ArenaHorizontalMetricScale;
	SurfingSnapshot.MovementInput = FVector2D::ZeroVector;
	SurfingSnapshot.CurrentHeading = FVector2D(0.0, 1.0);
	SurfingSnapshot.Lives = SurfingDefinition->Rules.InitialLives;
	SurfingSnapshot.MaxLives = SurfingDefinition->Rules.MaxLives;
	SurfingSnapshot.BoostCharges = SurfingDefinition->Rules.InitialBoostCharges;
	SurfingSnapshot.MaxBoostCharges = SurfingDefinition->Rules.MaxBoostCharges;
	SurfingSnapshot.CurrentSpeed = SurfingDefinition->Rules.CharacterSpeedNormalizedPerSecond;

	CourseObjects.Reset();
	DistanceMetersExact = 0.0;
	SimulationAccumulatorSeconds = 0.0;
	SimulationTick = 0;
	MaxSimulationTicks = FMath::Max(
		1,
		FMath::RoundToInt(
			static_cast<double>(SurfingDefinition->DurationSeconds)
			/ FFSurfingRules::FixedSimulationStepSeconds));
	PendingMovementInput = FVector2D::ZeroVector;
	bHasPendingMovementInput = false;
	bBoostActivationPending = false;
	ScoreAccumulator = 0.0f;

	if (!LoadCourseLayout(OutFailureReason))
	{
		return false;
	}

	SynchronizeDerivedAndLegacyState();
	RefreshVisibleObjects();
	return true;
}

void UFF_SurfingRuntime::OnAdvancePlaying(
	const float DeltaSeconds,
	const float StartingElapsedSeconds)
{
	if (!SurfingDefinition)
	{
		return;
	}

	SimulationAccumulatorSeconds += static_cast<double>(DeltaSeconds);
	const bool bFinalAdvance = static_cast<double>(StartingElapsedSeconds)
		+ static_cast<double>(DeltaSeconds)
		>= static_cast<double>(SurfingDefinition->DurationSeconds) - FixedTickToleranceSeconds;

	while (SimulationTick < MaxSimulationTicks
		&& (SimulationAccumulatorSeconds + FixedTickToleranceSeconds
			>= FFSurfingRules::FixedSimulationStepSeconds
			|| bFinalAdvance))
	{
		SimulationAccumulatorSeconds = FMath::Max(
			0.0,
			SimulationAccumulatorSeconds - FFSurfingRules::FixedSimulationStepSeconds);
		SimulateFixedTick();
	}

	RefreshVisibleObjects();
	SynchronizeScore();
}

void UFF_SurfingRuntime::OnFinished(
	const EFF_MinigameSessionState FinalState,
	const EFF_MinigameEndReason EndReason)
{
	(void)FinalState;
	(void)EndReason;
	bHasPendingMovementInput = false;
	bBoostActivationPending = false;
	SurfingSnapshot.MovementInput = FVector2D::ZeroVector;
	SurfingSnapshot.CurrentSpeed = 0.0f;
	SurfingSnapshot.BoostRemainingSeconds = 0.0f;
	SurfingSnapshot.HitInvincibilityRemainingSeconds = 0.0f;
	SurfingSnapshot.VisibleObjects.Reset();
	SynchronizeDerivedAndLegacyState();
	SynchronizeScore();
}

EFF_SurfingSteerResult UFF_SurfingRuntime::SetMovementInput(
	const FGuid& SessionId,
	const float LateralAxis,
	const float ForwardAxis)
{
	if (!IsPlaying())
	{
		return EFF_SurfingSteerResult::RejectedNotPlaying;
	}
	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_SurfingSteerResult::RejectedWrongSession;
	}
	if (!FFSurfingRules::IsValidMovementInput(LateralAxis, ForwardAxis))
	{
		return EFF_SurfingSteerResult::RejectedInvalidAxis;
	}

	const FVector2D NewInput(LateralAxis, ForwardAxis);
	const FVector2D EffectiveInput = bHasPendingMovementInput
		? PendingMovementInput
		: SurfingSnapshot.MovementInput;
	if (IsNearlySameInput(EffectiveInput, NewInput))
	{
		return EFF_SurfingSteerResult::Applied;
	}

	IncrementSaturated(SurfingSnapshot.SteerCount);
	PendingMovementInput = NewInput;
	bHasPendingMovementInput = true;
	return EFF_SurfingSteerResult::Applied;
}

EFF_SurfingSteerResult UFF_SurfingRuntime::SetSteerInput(
	const FGuid& SessionId,
	const float Axis)
{
	const float ForwardAxis = bHasPendingMovementInput
		? static_cast<float>(PendingMovementInput.Y)
		: static_cast<float>(SurfingSnapshot.MovementInput.Y);
	return SetMovementInput(SessionId, Axis, ForwardAxis);
}

EFF_SurfingSteerResult UFF_SurfingRuntime::SubmitSteer(
	const FGuid& SessionId,
	const int32 Direction)
{
	if (!IsPlaying())
	{
		return EFF_SurfingSteerResult::RejectedNotPlaying;
	}
	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_SurfingSteerResult::RejectedWrongSession;
	}
	if (!FFSurfingRules::IsValidSteerDirection(Direction))
	{
		return EFF_SurfingSteerResult::RejectedInvalidDirection;
	}
	return SetSteerInput(SessionId, static_cast<float>(Direction));
}

EFF_SurfingBoostResult UFF_SurfingRuntime::SubmitBoost(const FGuid& SessionId)
{
	if (!IsPlaying())
	{
		return EFF_SurfingBoostResult::RejectedNotPlaying;
	}
	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_SurfingBoostResult::RejectedWrongSession;
	}
	if (SurfingSnapshot.bBoosting || bBoostActivationPending)
	{
		return EFF_SurfingBoostResult::RejectedAlreadyBoosting;
	}
	if (SurfingSnapshot.BoostCharges <= 0)
	{
		return EFF_SurfingBoostResult::RejectedNoCharge;
	}

	bBoostActivationPending = true;
	return EFF_SurfingBoostResult::Activated;
}

FFF_SurfingSnapshot UFF_SurfingRuntime::GetSurfingSnapshot() const
{
	FFF_SurfingSnapshot Result = SurfingSnapshot;
	Result.Session = GetSnapshot();
	return Result;
}

bool UFF_SurfingRuntime::LoadCourseLayout(FString& OutFailureReason)
{
	const UDataTable* CourseTable = SurfingDefinition->CourseLayoutTable.LoadSynchronous();
	if (!CourseTable || CourseTable->GetRowStruct() != FFF_SurfingCourseRow::StaticStruct())
	{
		OutFailureReason = TEXT("surfing runtime requires a valid FFF_SurfingCourseRow table");
		return false;
	}

	TArray<FName> RowNames = CourseTable->GetRowNames();
	RowNames.Sort([](const FName A, const FName B)
	{
		return A.LexicalLess(B);
	});
	for (const FName RowName : RowNames)
	{
		const FFF_SurfingCourseRow* Row = CourseTable->FindRow<FFF_SurfingCourseRow>(
			RowName,
			TEXT("UFF_SurfingRuntime::LoadCourseLayout"),
			false);
		if (!Row || !Row->bEnabled)
		{
			continue;
		}

		FFF_SurfingCourseObject& Object = CourseObjects.AddDefaulted_GetRef();
		Object.ObjectId = CourseObjects.Num();
		Object.Type = Row->Type;
		Object.PositionNormalized = Row->PositionNormalized;
		Object.CollisionRadiusNormalized = Row->CollisionRadiusNormalized;
		Object.ScoreValueOverride = Row->ScoreValueOverride;
	}

	if (CourseObjects.IsEmpty())
	{
		OutFailureReason = TEXT("surfing course contains no enabled objects");
		return false;
	}
	OutFailureReason.Reset();
	return true;
}

void UFF_SurfingRuntime::SimulateFixedTick()
{
	ApplyPendingCommands();
	const FFF_SurfingRules& Rules = SurfingDefinition->Rules;
	const bool bBoostingThisTick = SurfingSnapshot.BoostRemainingSeconds > 0.0f;
	const float Speed = Rules.CharacterSpeedNormalizedPerSecond
		* (bBoostingThisTick ? Rules.BoostSpeedMultiplier : 1.0f);
	SurfingSnapshot.CurrentSpeed = Speed;

	const double TravelDistance = static_cast<double>(Speed)
		* FFSurfingRules::FixedSimulationStepSeconds;
	DistanceMetersExact += TravelDistance;
	ScoreAccumulator = FFSurfingRules::AddScoreSaturated(
		ScoreAccumulator,
		static_cast<float>(TravelDistance) * Rules.DistanceScorePerMeter);
	MovePlayerAndProcessCollisions(TravelDistance);

	SurfingSnapshot.BoostRemainingSeconds = FMath::Max(
		0.0f,
		SurfingSnapshot.BoostRemainingSeconds
			- static_cast<float>(FFSurfingRules::FixedSimulationStepSeconds));
	SurfingSnapshot.HitInvincibilityRemainingSeconds = FMath::Max(
		0.0f,
		SurfingSnapshot.HitInvincibilityRemainingSeconds
			- static_cast<float>(FFSurfingRules::FixedSimulationStepSeconds));
	++SimulationTick;
	SurfingSnapshot.DistanceMeters = static_cast<float>(DistanceMetersExact);
	SynchronizeDerivedAndLegacyState();
}

void UFF_SurfingRuntime::ApplyPendingCommands()
{
	if (bHasPendingMovementInput)
	{
		SurfingSnapshot.MovementInput = PendingMovementInput;
		bHasPendingMovementInput = false;
		if (PendingMovementInput.X != 0.0 || PendingMovementInput.Y != 0.0)
		{
			const FVector2D NewHeading = FFSurfingRules::NormalizeHeading(
				static_cast<float>(PendingMovementInput.X),
				static_cast<float>(PendingMovementInput.Y),
				SurfingDefinition->Rules.ArenaHorizontalMetricScale);
			if (!NewHeading.IsZero())
			{
				SurfingSnapshot.CurrentHeading = NewHeading;
			}
		}
	}

	if (bBoostActivationPending)
	{
		bBoostActivationPending = false;
		if (SurfingSnapshot.BoostCharges > 0
			&& SurfingSnapshot.BoostRemainingSeconds <= 0.0f)
		{
			--SurfingSnapshot.BoostCharges;
			SurfingSnapshot.BoostRemainingSeconds = SurfingDefinition->Rules.BoostDurationSeconds;
			RecordEvent(EFF_SurfingEventType::BoostActivated);
		}
	}
}

void UFF_SurfingRuntime::MovePlayerAndProcessCollisions(const double TravelDistance)
{
	FVector2D Position = SurfingSnapshot.PlayerPositionNormalized;
	FVector2D Heading = SurfingSnapshot.CurrentHeading;
	double RemainingDistance = FMath::Max(0.0, TravelDistance);

	for (int32 SegmentIndex = 0;
		SegmentIndex < MaximumReflectedSegmentsPerTick
			&& RemainingDistance > MovementTolerance;
		++SegmentIndex)
	{
		Position.X = FMath::Clamp(Position.X, -1.0, 1.0);
		Position.Y = FMath::Clamp(Position.Y, -1.0, 1.0);

		const double DistanceToVerticalWall = FMath::Abs(Heading.X) > MovementTolerance
			? FMath::Max(
				0.0,
				((Heading.X > 0.0 ? 1.0 : -1.0) - Position.X) / Heading.X)
			: TNumericLimits<double>::Max();
		const double DistanceToHorizontalWall = FMath::Abs(Heading.Y) > MovementTolerance
			? FMath::Max(
				0.0,
				((Heading.Y > 0.0 ? 1.0 : -1.0) - Position.Y) / Heading.Y)
			: TNumericLimits<double>::Max();
		const double DistanceToWall = FMath::Min(
			DistanceToVerticalWall,
			DistanceToHorizontalWall);
		const double SegmentDistance = FMath::Min(RemainingDistance, DistanceToWall);

		if (SegmentDistance > MovementTolerance)
		{
			const FVector2D SegmentEnd = Position + Heading * SegmentDistance;
			ProcessMovementSegment(Position, SegmentEnd);
			Position = SegmentEnd;
			RemainingDistance = FMath::Max(0.0, RemainingDistance - SegmentDistance);
		}

		const bool bReachedWall = DistanceToWall <= SegmentDistance + MovementTolerance;
		if (!bReachedWall)
		{
			break;
		}

		bool bReflected = false;
		if (DistanceToVerticalWall <= DistanceToWall + MovementTolerance)
		{
			Heading.X = -Heading.X;
			bReflected = true;
		}
		if (DistanceToHorizontalWall <= DistanceToWall + MovementTolerance)
		{
			Heading.Y = -Heading.Y;
			bReflected = true;
		}
		if (!bReflected)
		{
			break;
		}
	}

	SurfingSnapshot.PlayerPositionNormalized = FVector2D(
		FMath::Clamp(Position.X, -1.0, 1.0),
		FMath::Clamp(Position.Y, -1.0, 1.0));
	SurfingSnapshot.CurrentHeading = Heading;
}

void UFF_SurfingRuntime::ProcessMovementSegment(
	const FVector2D& SegmentStart,
	const FVector2D& SegmentEnd)
{
	const double PlayerRadius = SurfingDefinition->Rules.PlayerCollisionRadiusNormalized;
	TArray<FContactCandidate, TInlineAllocator<16>> Candidates;
	for (int32 ObjectIndex = 0; ObjectIndex < CourseObjects.Num(); ++ObjectIndex)
	{
		FFF_SurfingCourseObject& Object = CourseObjects[ObjectIndex];
		if (Object.bResolved)
		{
			continue;
		}

		double TimeOfImpact = 0.0;
		const float CombinedRadius = static_cast<float>(
			PlayerRadius + static_cast<double>(Object.CollisionRadiusNormalized));
		const bool bSegmentHits = FFSurfingRules::GetSegmentCircleHitTime(
			SegmentStart,
			SegmentEnd,
			Object.PositionNormalized,
			CombinedRadius,
			SurfingDefinition->Rules.ArenaHorizontalMetricScale,
			TimeOfImpact);
		if (bSegmentHits
			&& (!FFSurfingRules::IsObstacleType(Object.Type)
				|| (!Object.bPlayerOverlapping
					&& Object.LastContactSimulationTick != SimulationTick)))
		{
			FContactCandidate& Candidate = Candidates.AddDefaulted_GetRef();
			Candidate.CourseObjectIndex = ObjectIndex;
			Candidate.TimeOfImpact = TimeOfImpact;
		}
	}

	Candidates.Sort([this](const FContactCandidate& A, const FContactCandidate& B)
	{
		if (!FMath::IsNearlyEqual(A.TimeOfImpact, B.TimeOfImpact, 1.0e-12))
		{
			return A.TimeOfImpact < B.TimeOfImpact;
		}
		return CourseObjects[A.CourseObjectIndex].ObjectId
			< CourseObjects[B.CourseObjectIndex].ObjectId;
	});

	for (const FContactCandidate& Candidate : Candidates)
	{
		FFF_SurfingCourseObject& Object = CourseObjects[Candidate.CourseObjectIndex];
		if (Object.bResolved)
		{
			continue;
		}
		if (FFSurfingRules::IsObstacleType(Object.Type))
		{
			Object.LastContactSimulationTick = SimulationTick;
		}
		ProcessObjectContact(Object);
	}

	for (FFF_SurfingCourseObject& Object : CourseObjects)
	{
		if (!Object.bResolved && FFSurfingRules::IsObstacleType(Object.Type))
		{
			const double CombinedRadius = PlayerRadius
				+ static_cast<double>(Object.CollisionRadiusNormalized);
			Object.bPlayerOverlapping = IsInsideCircle(
				SegmentEnd,
				Object.PositionNormalized,
				CombinedRadius,
				SurfingDefinition->Rules.ArenaHorizontalMetricScale);
		}
	}
}

void UFF_SurfingRuntime::ProcessObjectContact(FFF_SurfingCourseObject& Object)
{
	const FFF_SurfingRules& Rules = SurfingDefinition->Rules;
	if (FFSurfingRules::IsObstacleType(Object.Type))
	{
		if (SurfingSnapshot.BoostRemainingSeconds > 0.0f)
		{
			IncrementSaturated(SurfingSnapshot.SmashedCount);
			const float Reward = Object.ScoreValueOverride > 0.0f
				? Object.ScoreValueOverride
				: Rules.ObstacleDestroyScore;
			ScoreAccumulator = FFSurfingRules::AddScoreSaturated(ScoreAccumulator, Reward);
			RecordEvent(EFF_SurfingEventType::ObstacleSmashed);
		}
		else if (SurfingSnapshot.HitInvincibilityRemainingSeconds <= 0.0f)
		{
			IncrementSaturated(SurfingSnapshot.CollisionCount);
			SurfingSnapshot.Lives = FMath::Max(0, SurfingSnapshot.Lives - 1);
			SurfingSnapshot.HitInvincibilityRemainingSeconds = Rules.HitInvincibilitySeconds;
			const float Penalty = Object.ScoreValueOverride > 0.0f
				? Object.ScoreValueOverride
				: Rules.CollisionPenalty;
			ScoreAccumulator = FFSurfingRules::AddScoreSaturated(ScoreAccumulator, -Penalty);
			RecordEvent(EFF_SurfingEventType::ObstacleHit);
		}
		return;
	}

	Object.bResolved = true;
	const float OverrideScore = Object.ScoreValueOverride;
	switch (Object.Type)
	{
	case EFF_SurfingObjectType::Coin:
		IncrementSaturated(SurfingSnapshot.CoinCount);
		ScoreAccumulator = FFSurfingRules::AddScoreSaturated(
			ScoreAccumulator,
			OverrideScore > 0.0f ? OverrideScore : Rules.CoinScore);
		RecordEvent(EFF_SurfingEventType::CoinCollected);
		break;
	case EFF_SurfingObjectType::Heart:
		IncrementSaturated(SurfingSnapshot.HeartCount);
		if (SurfingSnapshot.Lives < SurfingSnapshot.MaxLives)
		{
			++SurfingSnapshot.Lives;
		}
		if (OverrideScore > 0.0f)
		{
			ScoreAccumulator = FFSurfingRules::AddScoreSaturated(
				ScoreAccumulator,
				OverrideScore);
		}
		RecordEvent(EFF_SurfingEventType::HeartCollected);
		break;
	case EFF_SurfingObjectType::BoostPickup:
		IncrementSaturated(SurfingSnapshot.BoostPickupCount);
		if (SurfingSnapshot.BoostCharges < SurfingSnapshot.MaxBoostCharges)
		{
			++SurfingSnapshot.BoostCharges;
		}
		if (OverrideScore > 0.0f)
		{
			ScoreAccumulator = FFSurfingRules::AddScoreSaturated(
				ScoreAccumulator,
				OverrideScore);
		}
		RecordEvent(EFF_SurfingEventType::BoostPickupCollected);
		break;
	default:
		break;
	}
}

void UFF_SurfingRuntime::RefreshVisibleObjects()
{
	SurfingSnapshot.VisibleObjects.Reset();
	if (!SurfingDefinition || !IsPlaying())
	{
		return;
	}

	for (const FFF_SurfingCourseObject& Object : CourseObjects)
	{
		if (Object.bResolved)
		{
			continue;
		}

		FFF_SurfingObjectSnapshot& Visible = SurfingSnapshot.VisibleObjects.AddDefaulted_GetRef();
		Visible.ObjectId = Object.ObjectId;
		Visible.Type = Object.Type;
		Visible.PositionNormalized = Object.PositionNormalized;
		Visible.CollisionRadiusNormalized = Object.CollisionRadiusNormalized;
		Visible.LateralPositionNormalized = static_cast<float>(Object.PositionNormalized.X);
		Visible.ForwardPositionNormalized = static_cast<float>(Object.PositionNormalized.Y);
		Visible.DistanceAheadMeters = 0.0f;
		Visible.ForwardAlpha = static_cast<float>(
			FMath::Clamp((Object.PositionNormalized.Y + 1.0) * 0.5, 0.0, 1.0));
	}
}

void UFF_SurfingRuntime::RecordEvent(const EFF_SurfingEventType EventType)
{
	IncrementSaturated(SurfingSnapshot.LatestEventSerial);
	SurfingSnapshot.LatestEventType = EventType;
}

void UFF_SurfingRuntime::SynchronizeScore()
{
	SetScore(FFSurfingRules::QuantizeScore(ScoreAccumulator));
}

void UFF_SurfingRuntime::SynchronizeDerivedAndLegacyState()
{
	SurfingSnapshot.bBoosting = SurfingSnapshot.BoostRemainingSeconds > 0.0f;
	SurfingSnapshot.bInvincible = SurfingSnapshot.bBoosting
		|| SurfingSnapshot.HitInvincibilityRemainingSeconds > 0.0f;

	SurfingSnapshot.PlayerLateralNormalized = static_cast<float>(
		SurfingSnapshot.PlayerPositionNormalized.X);
	SurfingSnapshot.PlayerForwardNormalized = static_cast<float>(
		SurfingSnapshot.PlayerPositionNormalized.Y);
	SurfingSnapshot.SteerAxis = static_cast<float>(SurfingSnapshot.MovementInput.X);
	SurfingSnapshot.ForwardAxis = static_cast<float>(SurfingSnapshot.MovementInput.Y);
	SurfingSnapshot.Balance = SurfingSnapshot.PlayerLateralNormalized;
	SurfingSnapshot.WaveForcePerSecond = 0.0f;
	SurfingSnapshot.WaveChangeRemainingSeconds = 0.0f;
	SurfingSnapshot.RecoveryRemainingSeconds = SurfingSnapshot.HitInvincibilityRemainingSeconds;
	SurfingSnapshot.bRecovering = false;
	SurfingSnapshot.WipeoutCount = SurfingSnapshot.CollisionCount;
	const float SimulationSeconds = static_cast<float>(SimulationTick)
		* static_cast<float>(FFSurfingRules::FixedSimulationStepSeconds);
	SurfingSnapshot.CurrentFlowSeconds = SimulationSeconds;
	SurfingSnapshot.BestFlowSeconds = SimulationSeconds;
}
