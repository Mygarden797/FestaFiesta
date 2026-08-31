// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/ClawMachine/FF_ClawMachineTypes.h"
#include "Minigame/FF_MinigameRuntime.h"
#include "FF_ClawMachineRuntime.generated.h"

class UFF_ClawMachineDefinition;

/** Deterministic, frame-rate-independent claw-machine session strategy. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_ClawMachineRuntime final : public UFF_MinigameRuntime
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine")
	EFF_ClawMachineInputResult SetMovementInput(
		const FGuid& SessionId,
		const FVector2D& InputAxis);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame|Claw Machine")
	EFF_ClawMachineInputResult SubmitDrop(const FGuid& SessionId);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame|Claw Machine")
	FFF_ClawMachineSnapshot GetClawSnapshot() const;

protected:
	virtual bool OnInitialize(FString& OutFailureReason) override;
	virtual void OnAdvancePlaying(float DeltaSeconds, float StartingElapsedSeconds) override;
	virtual void OnFinished(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason) override;

private:
	void PopulatePrizes();
	int32 ChoosePrizePoolIndex();
	void StartPhase(EFF_ClawMachinePhase NewPhase, float DurationSeconds);
	void CompleteCurrentPhase();
	void ResolveGrip();
	void ResolveRelease();
	void RecordFeedback(EFF_ClawMachineFeedbackType Type, FName PrizeId, int32 ScoreDelta);
	int32 FindBestPrizeIndex(float& OutAlignment) const;
	float GetPhaseDuration(EFF_ClawMachinePhase Phase) const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_ClawMachineDefinition> ClawDefinition;

	UPROPERTY(Transient)
	FFF_ClawMachineSnapshot ClawSnapshot;

	/** Immutable session copy after the optional planner DataTable override is resolved. */
	FFF_ClawMachineRules ResolvedRules;

	FVector2D MovementInput = FVector2D::ZeroVector;
	FVector2D ReturnStartPosition = FVector2D::ZeroVector;
	FRandomStream RandomStream;
	float PhaseElapsedSeconds = 0.0f;
	float PhaseDurationSeconds = 0.0f;
	int32 CandidatePrizeIndex = INDEX_NONE;
	float CandidateAlignment = 0.0f;
	float CandidateGripVariance = 0.0f;
};
