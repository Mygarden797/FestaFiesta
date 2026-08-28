// Copyright Epic Games, Inc. All Rights Reserved.

#include "Minigame/FF_MinigameRuntime.h"

#include "Minigame/FF_MinigameDefinition.h"

#define LOCTEXT_NAMESPACE "FFMinigameRuntime"

bool UFF_MinigameRuntime::Initialize(
	UFF_MinigameDefinition* InDefinition,
	const FFF_MinigameSessionContext& InContext,
	FString* OutFailureReason)
{
	auto Fail = [OutFailureReason](const TCHAR* Reason)
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};

	if (bInitializationAttempted)
	{
		return Fail(TEXT("runtime objects cannot be initialized more than once"));
	}
	bInitializationAttempted = true;

	if (!InDefinition)
	{
		return Fail(TEXT("definition is null"));
	}

	TArray<FText> DefinitionErrors;
	if (!InDefinition->IsDefinitionValid(DefinitionErrors))
	{
		return Fail(TEXT("definition validation failed"));
	}

	if (!InDefinition->RuntimeClass || !GetClass()->IsChildOf(InDefinition->RuntimeClass))
	{
		return Fail(TEXT("runtime object does not match the definition's RuntimeClass"));
	}

	if (!InContext.SessionId.IsValid()
		|| !InContext.FacilityInstanceId.IsValid()
		|| InContext.FacilityDefinitionId.IsNone()
		|| InContext.MinigameId.IsNone())
	{
		return Fail(TEXT("session context contains an invalid identity"));
	}

	if (InContext.MinigameId != InDefinition->MinigameId)
	{
		return Fail(TEXT("session MinigameId does not match the definition"));
	}

	if (!InDefinition->SupportsFacility(InContext.FacilityDefinitionId))
	{
		return Fail(TEXT("facility is not compatible with this minigame"));
	}

	Definition = InDefinition;
	Snapshot = FFF_MinigameSessionSnapshot();
	Snapshot.Context = InContext;
	Snapshot.State = EFF_MinigameSessionState::Playing;
	Snapshot.RemainingSeconds = InDefinition->DurationSeconds;

	FString ConcreteFailureReason;
	if (!OnInitialize(ConcreteFailureReason))
	{
		Definition = nullptr;
		Snapshot = FFF_MinigameSessionSnapshot();
		if (OutFailureReason)
		{
			*OutFailureReason = ConcreteFailureReason.IsEmpty()
				? TEXT("concrete runtime initialization failed")
				: ConcreteFailureReason;
		}
		return false;
	}

	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}
	return true;
}

void UFF_MinigameRuntime::Advance(const float DeltaSeconds)
{
	if (!IsPlaying() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f || !Definition)
	{
		return;
	}

	const float DurationSeconds = Definition->DurationSeconds;
	const float StartingElapsedSeconds = Snapshot.ElapsedSeconds;
	const float TimeUntilEnd = FMath::Max(0.0f, DurationSeconds - StartingElapsedSeconds);
	const float ConsumedSeconds = FMath::Min(DeltaSeconds, TimeUntilEnd);

	if (ConsumedSeconds > 0.0f)
	{
		OnAdvancePlaying(ConsumedSeconds, StartingElapsedSeconds);
	}

	if (!IsPlaying())
	{
		return;
	}

	Snapshot.ElapsedSeconds = FMath::Min(DurationSeconds, StartingElapsedSeconds + ConsumedSeconds);
	Snapshot.RemainingSeconds = FMath::Max(0.0f, DurationSeconds - Snapshot.ElapsedSeconds);

	if (Snapshot.ElapsedSeconds >= DurationSeconds)
	{
		Finish(EFF_MinigameSessionState::Completed, EFF_MinigameEndReason::TimeExpired);
	}
}

bool UFF_MinigameRuntime::Cancel(EFF_MinigameEndReason Reason)
{
	if (!IsPlaying())
	{
		return false;
	}

	if (Reason == EFF_MinigameEndReason::None || Reason == EFF_MinigameEndReason::TimeExpired)
	{
		Reason = EFF_MinigameEndReason::PlayerCancelled;
	}

	Finish(EFF_MinigameSessionState::Cancelled, Reason);
	return true;
}

int32 UFF_MinigameRuntime::CalculateRewardGold(const int32 MaxRewardGold) const
{
	if (Snapshot.State != EFF_MinigameSessionState::Completed || !Definition)
	{
		return 0;
	}

	if (Snapshot.Score <= 0 || Definition->ScoreForMaxReward <= 0 || MaxRewardGold <= 0)
	{
		return 0;
	}

	// Reward scaling is a contract shared by every runtime strategy. Keeping it
	// here prevents a second minigame from depending on target-shooting rules.
	const double Ratio = FMath::Clamp(
		static_cast<double>(Snapshot.Score)
			/ static_cast<double>(Definition->ScoreForMaxReward),
		0.0,
		1.0);
	return FMath::Clamp(
		FMath::RoundToInt(static_cast<double>(MaxRewardGold) * Ratio),
		0,
		MaxRewardGold);
}

FFF_MinigameResult UFF_MinigameRuntime::BuildResult(const int32 MaxRewardGold) const
{
	FFF_MinigameResult Result;
	Result.SessionId = Snapshot.Context.SessionId;
	Result.MinigameId = Snapshot.Context.MinigameId;
	Result.Score = Snapshot.Score;
	Result.RewardGold = CalculateRewardGold(MaxRewardGold);
	Result.EndReason = Snapshot.EndReason;

	if (Snapshot.State == EFF_MinigameSessionState::Completed && Definition)
	{
		const double Ratio = FMath::Clamp(
			static_cast<double>(Snapshot.Score) / static_cast<double>(Definition->ScoreForMaxReward),
			0.0,
			1.0);
		if (Ratio >= 1.0)
		{
			Result.GradeText = LOCTEXT("GradeS", "S");
		}
		else if (Ratio >= 0.75)
		{
			Result.GradeText = LOCTEXT("GradeA", "A");
		}
		else if (Ratio >= 0.5)
		{
			Result.GradeText = LOCTEXT("GradeB", "B");
		}
		else
		{
			Result.GradeText = LOCTEXT("GradeC", "C");
		}
	}

	return Result;
}

void UFF_MinigameRuntime::Finish(
	const EFF_MinigameSessionState FinalState,
	const EFF_MinigameEndReason EndReason)
{
	if (!IsPlaying()
		|| (FinalState != EFF_MinigameSessionState::Completed
			&& FinalState != EFF_MinigameSessionState::Cancelled))
	{
		return;
	}

	Snapshot.State = FinalState;
	Snapshot.EndReason = EndReason;
	if (FinalState == EFF_MinigameSessionState::Completed)
	{
		Snapshot.RemainingSeconds = 0.0f;
	}

	OnFinished(FinalState, EndReason);
	OnEnded.Broadcast(this);
}

#undef LOCTEXT_NAMESPACE
