// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_SurfingRuntime.h"

#include "Engine/DataTable.h"
#include "Minigame/FF_MinigameDefinition.h"
#include "Minigame/Surfing/FF_SurfingRules.h"

namespace
{
	constexpr double FixedTickToleranceSeconds = 1.0e-9;
	constexpr double MovementTolerance = 1.0e-10;
	constexpr double VisibleCourseBehindNormalized = 1.15;
	constexpr double VisibleCourseAheadNormalized = 1.35;

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

	uint64 MakeCourseObjectKey(const int32 SegmentIndex, const int32 TemplateIndex)
	{
		return (static_cast<uint64>(static_cast<uint32>(SegmentIndex)) << 32)
			| static_cast<uint64>(static_cast<uint32>(TemplateIndex));
	}

	int32 GetSegmentIndexFromCourseObjectKey(const uint64 ObjectKey)
	{
		return static_cast<int32>(static_cast<uint32>(ObjectKey >> 32));
	}

	int32 MakeCourseObjectId(
		const int32 SegmentIndex,
		const int32 TemplateIndex,
		const int32 TemplateCount,
		const int32 PoolSegmentCount)
	{
		const int32 SafePoolCount = FMath::Max(1, PoolSegmentCount);
		const int32 SlotIndex = ((SegmentIndex % SafePoolCount) + SafePoolCount)
			% SafePoolCount;
		return SlotIndex * FMath::Max(1, TemplateCount) + TemplateIndex + 1;
	}
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
	SurfingSnapshot.CourseProgressNormalized = 0.0;
	SurfingSnapshot.CourseSegmentIndex = 0;
	SurfingSnapshot.ArenaHorizontalMetricScale = SurfingDefinition->Rules.ArenaHorizontalMetricScale;
	SurfingSnapshot.MovementInput = FVector2D::ZeroVector;
	SurfingSnapshot.CurrentHeading = FVector2D(0.0, 1.0);
	SurfingSnapshot.Lives = SurfingDefinition->Rules.InitialLives;
	SurfingSnapshot.MaxLives = SurfingDefinition->Rules.MaxLives;
	SurfingSnapshot.BoostCharges = SurfingDefinition->Rules.InitialBoostCharges;
	SurfingSnapshot.MaxBoostCharges = SurfingDefinition->Rules.MaxBoostCharges;
	SurfingSnapshot.CurrentSpeed = SurfingDefinition->Rules.CharacterSpeedNormalizedPerSecond;

	CourseTemplates.Reset();
	CourseObjects.Reset();
	ResolvedCourseObjectKeys.Reset();
	PlayerCoursePosition = FVector2D::ZeroVector;
	ActiveFirstSegment = 1;
	ActiveLastSegment = 0;
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
	RefreshCourseObjectPool(PlayerCoursePosition.Y, PlayerCoursePosition.Y);

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

int32 UFF_SurfingRuntime::GetActiveCourseSegmentCount() const
{
	return ActiveLastSegment >= ActiveFirstSegment
		? ActiveLastSegment - ActiveFirstSegment + 1
		: 0;
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

		FFF_SurfingCourseTemplate& Object = CourseTemplates.AddDefaulted_GetRef();
		Object.Type = Row->Type;
		Object.PositionNormalized = Row->PositionNormalized;
		Object.CollisionRadiusNormalized = Row->CollisionRadiusNormalized;
		Object.ScoreValueOverride = Row->ScoreValueOverride;
	}

	if (CourseTemplates.IsEmpty())
	{
		OutFailureReason = TEXT("surfing course contains no enabled objects");
		return false;
	}
	OutFailureReason.Reset();
	return true;
}

int32 UFF_SurfingRuntime::GetCourseSegmentIndex(const double CourseY) const
{
	const double SegmentLength = SurfingDefinition
		? FMath::Max(2.01, static_cast<double>(
			SurfingDefinition->Rules.CourseSegmentLengthNormalized))
		: 2.4;
	return FMath::FloorToInt((CourseY + SegmentLength * 0.5) / SegmentLength);
}

void UFF_SurfingRuntime::RefreshCourseObjectPool(
	const double MinimumCourseY,
	const double MaximumCourseY)
{
	if (!SurfingDefinition || CourseTemplates.IsEmpty())
	{
		CourseObjects.Reset();
		ActiveFirstSegment = 1;
		ActiveLastSegment = 0;
		return;
	}

	const FFF_SurfingRules& Rules = SurfingDefinition->Rules;
	const int32 PresentationPoolSegmentCount = Rules.CourseSegmentsBehind
		+ Rules.CourseSegmentsAhead + 1;
	const double SafeMinimum = FMath::Min(MinimumCourseY, MaximumCourseY);
	const double SafeMaximum = FMath::Max(MinimumCourseY, MaximumCourseY);
	const int32 DesiredFirst = GetCourseSegmentIndex(SafeMinimum)
		- Rules.CourseSegmentsBehind;
	const int32 DesiredLast = GetCourseSegmentIndex(SafeMaximum)
		+ Rules.CourseSegmentsAhead;
	for (auto ResolvedIt = ResolvedCourseObjectKeys.CreateIterator(); ResolvedIt; ++ResolvedIt)
	{
		if (GetSegmentIndexFromCourseObjectKey(*ResolvedIt) < DesiredFirst)
		{
			ResolvedIt.RemoveCurrent();
		}
	}
	if (DesiredFirst == ActiveFirstSegment && DesiredLast == ActiveLastSegment)
	{
		return;
	}

	TMap<uint64, FFF_SurfingCourseObject> ExistingObjects;
	ExistingObjects.Reserve(CourseObjects.Num());
	for (FFF_SurfingCourseObject& Object : CourseObjects)
	{
		ExistingObjects.Add(Object.ObjectKey, MoveTemp(Object));
	}

	TArray<FFF_SurfingCourseObject> RecycledObjects;
	const int32 SegmentCount = FMath::Max(0, DesiredLast - DesiredFirst + 1);
	RecycledObjects.Reserve(SegmentCount * CourseTemplates.Num());
	for (int32 SegmentIndex = DesiredFirst; SegmentIndex <= DesiredLast; ++SegmentIndex)
	{
		for (int32 TemplateIndex = 0; TemplateIndex < CourseTemplates.Num(); ++TemplateIndex)
		{
			const uint64 ObjectKey = MakeCourseObjectKey(SegmentIndex, TemplateIndex);
			if (FFF_SurfingCourseObject* Existing = ExistingObjects.Find(ObjectKey))
			{
				RecycledObjects.Add(MoveTemp(*Existing));
				continue;
			}

			const FFF_SurfingCourseTemplate& Template = CourseTemplates[TemplateIndex];
			FFF_SurfingCourseObject& Object = RecycledObjects.AddDefaulted_GetRef();
			Object.ObjectKey = ObjectKey;
			Object.ObjectId = MakeCourseObjectId(
				SegmentIndex,
				TemplateIndex,
				CourseTemplates.Num(),
				PresentationPoolSegmentCount);
			Object.SegmentIndex = SegmentIndex;
			Object.TemplateIndex = TemplateIndex;
			Object.Type = Template.Type;
			Object.PositionNormalized = FVector2D(
				Template.PositionNormalized.X,
				Template.PositionNormalized.Y
					+ static_cast<double>(SegmentIndex)
						* static_cast<double>(Rules.CourseSegmentLengthNormalized));
			Object.CollisionRadiusNormalized = Template.CollisionRadiusNormalized;
			Object.ScoreValueOverride = Template.ScoreValueOverride;
			Object.bResolved = ResolvedCourseObjectKeys.Contains(ObjectKey);
		}
	}

	CourseObjects = MoveTemp(RecycledObjects);
	ActiveFirstSegment = DesiredFirst;
	ActiveLastSegment = DesiredLast;
}

void UFF_SurfingRuntime::SimulateFixedTick()
{
	ApplyPendingCommands();
	const FFF_SurfingRules& Rules = SurfingDefinition->Rules;
	const bool bBoostingThisTick = SurfingSnapshot.BoostRemainingSeconds > 0.0f;
	const float Speed = Rules.CharacterSpeedNormalizedPerSecond
		* (bBoostingThisTick ? Rules.BoostSpeedMultiplier : 1.0f)
		* (1.0f + static_cast<float>(SurfingSnapshot.MovementInput.Y)
			* Rules.ForwardInputSpeedInfluence);
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
		const FVector2D NewHeading = FFSurfingRules::NormalizeHeading(
			static_cast<float>(PendingMovementInput.X)
				* SurfingDefinition->Rules.SteeringStrength,
			1.0f,
			SurfingDefinition->Rules.ArenaHorizontalMetricScale);
		if (!NewHeading.IsZero())
		{
			SurfingSnapshot.CurrentHeading = NewHeading;
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
	FVector2D Position = PlayerCoursePosition;
	const FVector2D Heading = SurfingSnapshot.CurrentHeading;
	double RemainingDistance = FMath::Max(0.0, TravelDistance);
	RefreshCourseObjectPool(Position.Y, Position.Y + RemainingDistance);
	Position.X = FMath::Clamp(Position.X, -1.0, 1.0);

	const double DistanceToVerticalWall = FMath::Abs(Heading.X) > MovementTolerance
		? FMath::Max(
			0.0,
			((Heading.X > 0.0 ? 1.0 : -1.0) - Position.X) / Heading.X)
		: TNumericLimits<double>::Max();
	const double SegmentDistance = FMath::Min(RemainingDistance, DistanceToVerticalWall);
	if (SegmentDistance > MovementTolerance)
	{
		const FVector2D SegmentEnd = Position + Heading * SegmentDistance;
		ProcessMovementSegment(Position, SegmentEnd);
		Position = SegmentEnd;
		RemainingDistance = FMath::Max(0.0, RemainingDistance - SegmentDistance);
	}

	const bool bReachedWall = DistanceToVerticalWall <= SegmentDistance + MovementTolerance;
	if (bReachedWall && RemainingDistance > MovementTolerance)
	{
		Position.X = Heading.X > 0.0 ? 1.0 : -1.0;
		const double ForwardDirection = Heading.Y >= 0.0 ? 1.0 : -1.0;
		const FVector2D SlideEnd(
			Position.X,
			Position.Y + ForwardDirection * RemainingDistance);
		ProcessMovementSegment(Position, SlideEnd);
		Position = SlideEnd;
	}

	PlayerCoursePosition = FVector2D(
		FMath::Clamp(Position.X, -1.0, 1.0),
		Position.Y);
	RefreshCourseObjectPool(PlayerCoursePosition.Y, PlayerCoursePosition.Y);
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
		return CourseObjects[A.CourseObjectIndex].ObjectKey
			< CourseObjects[B.CourseObjectIndex].ObjectKey;
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
	ResolvedCourseObjectKeys.Add(Object.ObjectKey);
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
		const double RelativeForward = Object.PositionNormalized.Y
			- PlayerCoursePosition.Y;
		if (RelativeForward < -VisibleCourseBehindNormalized
			|| RelativeForward > VisibleCourseAheadNormalized)
		{
			continue;
		}

		FFF_SurfingObjectSnapshot& Visible = SurfingSnapshot.VisibleObjects.AddDefaulted_GetRef();
		Visible.ObjectId = Object.ObjectId;
		Visible.Type = Object.Type;
		Visible.PositionNormalized = FVector2D(
			Object.PositionNormalized.X,
			RelativeForward);
		Visible.CollisionRadiusNormalized = Object.CollisionRadiusNormalized;
		Visible.LateralPositionNormalized = static_cast<float>(Visible.PositionNormalized.X);
		Visible.ForwardPositionNormalized = static_cast<float>(Visible.PositionNormalized.Y);
		Visible.DistanceAheadMeters = static_cast<float>(RelativeForward);
		Visible.ForwardAlpha = static_cast<float>(
			FMath::Clamp((RelativeForward + 1.0) * 0.5, 0.0, 1.0));
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

	SurfingSnapshot.PlayerPositionNormalized = FVector2D(
		FMath::Clamp(PlayerCoursePosition.X, -1.0, 1.0),
		0.0);
	SurfingSnapshot.CourseProgressNormalized = PlayerCoursePosition.Y;
	SurfingSnapshot.CourseSegmentIndex = GetCourseSegmentIndex(PlayerCoursePosition.Y);
	SurfingSnapshot.PlayerLateralNormalized = static_cast<float>(
		SurfingSnapshot.PlayerPositionNormalized.X);
	SurfingSnapshot.PlayerForwardNormalized = 0.0f;
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
