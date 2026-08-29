// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_ClawMachineRuntime.h"

#include "Minigame/ClawMachine/FF_ClawMachineRules.h"
#include "Minigame/FF_MinigameDefinition.h"

namespace
{
	constexpr int32 MaxPhaseTransitionsPerAdvance = 32;
	constexpr int32 MaxPrizePlacementAttempts = 48;
}

bool UFF_ClawMachineRuntime::OnInitialize(FString& OutFailureReason)
{
	ClawDefinition = Cast<UFF_ClawMachineDefinition>(GetDefinition());
	if (!ClawDefinition)
	{
		OutFailureReason = TEXT("claw-machine runtime requires UFF_ClawMachineDefinition");
		return false;
	}
	ResolvedRules = ClawDefinition->Rules;
	if (!ClawDefinition->BuildResolvedPrizePool(ResolvedRules.PrizePool, &OutFailureReason))
	{
		return false;
	}

	ClawSnapshot = FFF_ClawMachineSnapshot();
	ClawSnapshot.Session = GetSnapshot();
	ClawSnapshot.Phase = EFF_ClawMachinePhase::Aiming;
	ClawSnapshot.ClawPositionNormalized = ResolvedRules.HomePositionNormalized;
	ClawSnapshot.DropPositionNormalized = ResolvedRules.HomePositionNormalized;
	ClawSnapshot.MaxAttempts = ResolvedRules.MaxAttempts;
	RandomStream.Initialize(GetSnapshot().Context.RandomSeed);
	PopulatePrizes();
	return true;
}

void UFF_ClawMachineRuntime::OnAdvancePlaying(
	const float DeltaSeconds,
	const float StartingElapsedSeconds)
{
	(void)StartingElapsedSeconds;
	if (!ClawDefinition)
	{
		return;
	}

	if (ClawSnapshot.Phase == EFF_ClawMachinePhase::Aiming)
	{
		FVector2D SafeInput = MovementInput.GetClampedToMaxSize(1.0f);
		ClawSnapshot.ClawPositionNormalized += SafeInput
			* ResolvedRules.MovementSpeedNormalizedPerSecond
			* DeltaSeconds;
		ClawSnapshot.ClawPositionNormalized.X = FMath::Clamp(
			ClawSnapshot.ClawPositionNormalized.X,
			ResolvedRules.MovementMinNormalized.X,
			ResolvedRules.MovementMaxNormalized.X);
		ClawSnapshot.ClawPositionNormalized.Y = FMath::Clamp(
			ClawSnapshot.ClawPositionNormalized.Y,
			ResolvedRules.MovementMinNormalized.Y,
			ResolvedRules.MovementMaxNormalized.Y);
		return;
	}

	float SecondsLeft = DeltaSeconds;
	for (int32 TransitionCount = 0;
		SecondsLeft > UE_SMALL_NUMBER && TransitionCount < MaxPhaseTransitionsPerAdvance;
		++TransitionCount)
	{
		const float RemainingInPhase = FMath::Max(0.0f, PhaseDurationSeconds - PhaseElapsedSeconds);
		const float Step = FMath::Min(SecondsLeft, RemainingInPhase);
		PhaseElapsedSeconds += Step;
		SecondsLeft -= Step;
		ClawSnapshot.PhaseAlpha = PhaseDurationSeconds > UE_SMALL_NUMBER
			? FMath::Clamp(PhaseElapsedSeconds / PhaseDurationSeconds, 0.0f, 1.0f)
			: 1.0f;

		if (ClawSnapshot.Phase == EFF_ClawMachinePhase::Returning)
		{
			ClawSnapshot.ClawPositionNormalized = FMath::Lerp(
				ReturnStartPosition,
				ResolvedRules.HomePositionNormalized,
				FMath::InterpEaseInOut(0.0f, 1.0f, ClawSnapshot.PhaseAlpha, 2.0f));
		}

		if (RemainingInPhase <= UE_SMALL_NUMBER
			|| PhaseElapsedSeconds + UE_SMALL_NUMBER >= PhaseDurationSeconds)
		{
			CompleteCurrentPhase();
			if (!IsPlaying() || ClawSnapshot.Phase == EFF_ClawMachinePhase::Aiming)
			{
				break;
			}
		}
		else if (Step <= UE_SMALL_NUMBER)
		{
			break;
		}
	}
}

void UFF_ClawMachineRuntime::OnFinished(
	const EFF_MinigameSessionState FinalState,
	const EFF_MinigameEndReason EndReason)
{
	(void)FinalState;
	(void)EndReason;
	MovementInput = FVector2D::ZeroVector;
	ClawSnapshot.Phase = EFF_ClawMachinePhase::RoundComplete;
	ClawSnapshot.PhaseAlpha = 1.0f;
}

EFF_ClawMachineInputResult UFF_ClawMachineRuntime::SetMovementInput(
	const FGuid& SessionId,
	const FVector2D& InputAxis)
{
	if (!IsPlaying())
	{
		return EFF_ClawMachineInputResult::RejectedNotPlaying;
	}
	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_ClawMachineInputResult::RejectedWrongSession;
	}
	if (!FMath::IsFinite(InputAxis.X) || !FMath::IsFinite(InputAxis.Y))
	{
		return EFF_ClawMachineInputResult::RejectedInvalidInput;
	}
	if (ClawSnapshot.Phase != EFF_ClawMachinePhase::Aiming)
	{
		MovementInput = FVector2D::ZeroVector;
		return EFF_ClawMachineInputResult::RejectedNotAiming;
	}

	MovementInput = InputAxis.GetClampedToMaxSize(1.0f);
	return EFF_ClawMachineInputResult::Applied;
}

EFF_ClawMachineInputResult UFF_ClawMachineRuntime::SubmitDrop(const FGuid& SessionId)
{
	if (!IsPlaying())
	{
		return EFF_ClawMachineInputResult::RejectedNotPlaying;
	}
	if (SessionId != GetSnapshot().Context.SessionId)
	{
		return EFF_ClawMachineInputResult::RejectedWrongSession;
	}
	if (ClawSnapshot.Phase != EFF_ClawMachinePhase::Aiming)
	{
		return EFF_ClawMachineInputResult::RejectedNotAiming;
	}
	if (ClawSnapshot.AttemptsUsed >= ClawSnapshot.MaxAttempts)
	{
		return EFF_ClawMachineInputResult::RejectedNoAttempts;
	}

	MovementInput = FVector2D::ZeroVector;
	ClawSnapshot.DropPositionNormalized = ClawSnapshot.ClawPositionNormalized;
	CandidatePrizeIndex = FindBestPrizeIndex(CandidateAlignment);
	CandidateGripVariance = RandomStream.FRandRange(
		-ResolvedRules.DeterministicGripVariance,
		ResolvedRules.DeterministicGripVariance);
	++ClawSnapshot.AttemptsUsed;
	StartPhase(EFF_ClawMachinePhase::Dropping, ResolvedRules.DropDurationSeconds);
	return EFF_ClawMachineInputResult::DropStarted;
}

FFF_ClawMachineSnapshot UFF_ClawMachineRuntime::GetClawSnapshot() const
{
	FFF_ClawMachineSnapshot Result = ClawSnapshot;
	Result.Session = GetSnapshot();
	return Result;
}

void UFF_ClawMachineRuntime::PopulatePrizes()
{
	const FFF_ClawMachineRules& Rules = ResolvedRules;
	ClawSnapshot.Prizes.Reset(Rules.PrizeCount);
	for (int32 PrizeIndex = 0; PrizeIndex < Rules.PrizeCount; ++PrizeIndex)
	{
		FFF_ClawMachinePrizeSnapshot Prize;
		Prize.PrizeSerial = PrizeIndex + 1;
		const int32 PoolIndex = ChoosePrizePoolIndex();
		Prize.PrizeId = Rules.PrizePool.IsValidIndex(PoolIndex)
			? Rules.PrizePool[PoolIndex].PrizeId
			: Rules.PrizePool[0].PrizeId;
		Prize.RotationDegrees = RandomStream.FRandRange(-24.0f, 24.0f);

		FVector2D Candidate = Rules.HomePositionNormalized;
		for (int32 Attempt = 0; Attempt < MaxPrizePlacementAttempts; ++Attempt)
		{
			Candidate = FVector2D(
				RandomStream.FRandRange(Rules.PrizeSpawnMinNormalized.X, Rules.PrizeSpawnMaxNormalized.X),
				RandomStream.FRandRange(Rules.PrizeSpawnMinNormalized.Y, Rules.PrizeSpawnMaxNormalized.Y));
			const bool bSeparated = ClawSnapshot.Prizes.ContainsByPredicate(
				[&Rules, &Candidate](const FFF_ClawMachinePrizeSnapshot& Existing)
				{
					return FVector2D::Distance(Candidate, Existing.PositionNormalized)
						< Rules.PrizeMinSeparationNormalized;
				});
			if (!bSeparated)
			{
				break;
			}
		}
		Prize.PositionNormalized = Candidate;
		ClawSnapshot.Prizes.Add(Prize);
	}
}

int32 UFF_ClawMachineRuntime::ChoosePrizePoolIndex()
{
	const TArray<FFF_ClawMachinePrizeTuning>& Pool = ResolvedRules.PrizePool;
	float TotalWeight = 0.0f;
	for (const FFF_ClawMachinePrizeTuning& Prize : Pool)
	{
		TotalWeight += FMath::Max(0.0f, Prize.SpawnWeight);
	}
	float Roll = RandomStream.FRandRange(0.0f, TotalWeight);
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		Roll -= FMath::Max(0.0f, Pool[Index].SpawnWeight);
		if (Roll <= 0.0f)
		{
			return Index;
		}
	}
	return FMath::Max(0, Pool.Num() - 1);
}

void UFF_ClawMachineRuntime::StartPhase(
	const EFF_ClawMachinePhase NewPhase,
	const float DurationSeconds)
{
	ClawSnapshot.Phase = NewPhase;
	ClawSnapshot.PhaseAlpha = 0.0f;
	PhaseElapsedSeconds = 0.0f;
	PhaseDurationSeconds = FMath::Max(0.05f, DurationSeconds);
}

void UFF_ClawMachineRuntime::CompleteCurrentPhase()
{
	switch (ClawSnapshot.Phase)
	{
	case EFF_ClawMachinePhase::Dropping:
		StartPhase(EFF_ClawMachinePhase::Closing, ResolvedRules.CloseDurationSeconds);
		break;
	case EFF_ClawMachinePhase::Closing:
		ResolveGrip();
		StartPhase(EFF_ClawMachinePhase::Lifting, ResolvedRules.LiftDurationSeconds);
		break;
	case EFF_ClawMachinePhase::Lifting:
		ReturnStartPosition = ClawSnapshot.ClawPositionNormalized;
		StartPhase(EFF_ClawMachinePhase::Returning, ResolvedRules.ReturnDurationSeconds);
		break;
	case EFF_ClawMachinePhase::Returning:
		ClawSnapshot.ClawPositionNormalized = ResolvedRules.HomePositionNormalized;
		StartPhase(EFF_ClawMachinePhase::Releasing, ResolvedRules.ReleaseDurationSeconds);
		break;
	case EFF_ClawMachinePhase::Releasing:
		ResolveRelease();
		if (ClawSnapshot.AttemptsUsed >= ClawSnapshot.MaxAttempts)
		{
			Finish(EFF_MinigameSessionState::Completed, EFF_MinigameEndReason::AttemptsExhausted);
		}
		else
		{
			StartPhase(EFF_ClawMachinePhase::Aiming, 0.05f);
			ClawSnapshot.PhaseAlpha = 0.0f;
		}
		break;
	default:
		break;
	}
}

void UFF_ClawMachineRuntime::ResolveGrip()
{
	ClawSnapshot.LastAlignmentQuality = CandidateAlignment;
	ClawSnapshot.LastGripQuality = 0.0f;
	ClawSnapshot.bCarryingPrize = false;
	ClawSnapshot.HeldPrizeSerial = 0;

	if (!ClawSnapshot.Prizes.IsValidIndex(CandidatePrizeIndex))
	{
		return;
	}
	const FFF_ClawMachinePrizeSnapshot& Prize = ClawSnapshot.Prizes[CandidatePrizeIndex];
	const FFF_ClawMachinePrizeTuning* Tuning = ResolvedRules.FindPrize(Prize.PrizeId);
	if (!Tuning || !Prize.bAvailable)
	{
		return;
	}

	ClawSnapshot.LastGripQuality = FFClawMachineRules::CalculatePhysicalGripQuality(
		CandidateAlignment,
		ResolvedRules.GripPower,
		*Tuning,
		CandidateGripVariance);
	if (ClawSnapshot.LastGripQuality >= ResolvedRules.RequiredGripQuality)
	{
		ClawSnapshot.bCarryingPrize = true;
		ClawSnapshot.HeldPrizeSerial = Prize.PrizeSerial;
	}
}

void UFF_ClawMachineRuntime::ResolveRelease()
{
	int32 ScoreDelta = 0;
	FName PrizeId = NAME_None;
	EFF_ClawMachineFeedbackType Feedback = EFF_ClawMachineFeedbackType::EmptyDrop;

	if (ClawSnapshot.bCarryingPrize && ClawSnapshot.Prizes.IsValidIndex(CandidatePrizeIndex))
	{
		FFF_ClawMachinePrizeSnapshot& Prize = ClawSnapshot.Prizes[CandidatePrizeIndex];
		const FFF_ClawMachinePrizeTuning* Tuning = ResolvedRules.FindPrize(Prize.PrizeId);
		if (Prize.bAvailable && Tuning)
		{
			Prize.bAvailable = false;
			PrizeId = Prize.PrizeId;
			ScoreDelta = Tuning->ScoreValue;
			const bool bPerfect = CandidateAlignment >= ResolvedRules.PerfectAlignmentThreshold;
			if (bPerfect)
			{
				ScoreDelta = FFClawMachineRules::AddScoreSaturated(
					ScoreDelta,
					ResolvedRules.PerfectGrabBonusScore);
				Feedback = EFF_ClawMachineFeedbackType::PerfectGrab;
			}
			else
			{
				Feedback = EFF_ClawMachineFeedbackType::Grabbed;
			}
			SetScore(FFClawMachineRules::AddScoreSaturated(GetSnapshot().Score, ScoreDelta));
			++ClawSnapshot.CapturedCount;
		}
	}
	else if (CandidatePrizeIndex != INDEX_NONE)
	{
		Feedback = EFF_ClawMachineFeedbackType::Slipped;
		PrizeId = ClawSnapshot.Prizes.IsValidIndex(CandidatePrizeIndex)
			? ClawSnapshot.Prizes[CandidatePrizeIndex].PrizeId
			: NAME_None;
		++ClawSnapshot.MissedAttempts;
	}
	else
	{
		++ClawSnapshot.MissedAttempts;
	}

	RecordFeedback(Feedback, PrizeId, ScoreDelta);
	ClawSnapshot.bCarryingPrize = false;
	ClawSnapshot.HeldPrizeSerial = 0;
	CandidatePrizeIndex = INDEX_NONE;
	CandidateAlignment = 0.0f;
	CandidateGripVariance = 0.0f;
}

void UFF_ClawMachineRuntime::RecordFeedback(
	const EFF_ClawMachineFeedbackType Type,
	const FName PrizeId,
	const int32 ScoreDelta)
{
	ClawSnapshot.LatestFeedbackSerial = ClawSnapshot.LatestFeedbackSerial == MAX_int32
		? 1
		: ClawSnapshot.LatestFeedbackSerial + 1;
	ClawSnapshot.LatestFeedbackType = Type;
	ClawSnapshot.LatestPrizeId = PrizeId;
	ClawSnapshot.LatestScoreDelta = ScoreDelta;
}

int32 UFF_ClawMachineRuntime::FindBestPrizeIndex(float& OutAlignment) const
{
	OutAlignment = 0.0f;
	int32 BestIndex = INDEX_NONE;
	for (int32 Index = 0; Index < ClawSnapshot.Prizes.Num(); ++Index)
	{
		const FFF_ClawMachinePrizeSnapshot& Prize = ClawSnapshot.Prizes[Index];
		if (!Prize.bAvailable)
		{
			continue;
		}
		const FFF_ClawMachinePrizeTuning* Tuning = ResolvedRules.FindPrize(Prize.PrizeId);
		if (!Tuning)
		{
			continue;
		}
		const float Alignment = FFClawMachineRules::CalculateAlignment(
			ClawSnapshot.DropPositionNormalized,
			Prize,
			*Tuning,
			ResolvedRules.GripRadiusNormalized);
		if (Alignment > OutAlignment)
		{
			OutAlignment = Alignment;
			BestIndex = Index;
		}
	}
	return BestIndex;
}

float UFF_ClawMachineRuntime::GetPhaseDuration(const EFF_ClawMachinePhase Phase) const
{
	if (!ClawDefinition)
	{
		return 0.0f;
	}
	switch (Phase)
	{
	case EFF_ClawMachinePhase::Dropping: return ResolvedRules.DropDurationSeconds;
	case EFF_ClawMachinePhase::Closing: return ResolvedRules.CloseDurationSeconds;
	case EFF_ClawMachinePhase::Lifting: return ResolvedRules.LiftDurationSeconds;
	case EFF_ClawMachinePhase::Returning: return ResolvedRules.ReturnDurationSeconds;
	case EFF_ClawMachinePhase::Releasing: return ResolvedRules.ReleaseDurationSeconds;
	default: return 0.0f;
	}
}
