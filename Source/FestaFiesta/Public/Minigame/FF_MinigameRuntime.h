// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Minigame/FF_MinigameTypes.h"
#include "UObject/Object.h"
#include "FF_MinigameRuntime.generated.h"

class UFF_MinigameDefinition;
class UFF_MinigameRuntime;

/** Native lifecycle event consumed by the authoritative GameInstance coordinator. */
DECLARE_MULTICAST_DELEGATE_OneParam(FFF_OnMinigameRuntimeEnded, UFF_MinigameRuntime*);

/**
 * World-independent session strategy. It owns timing and score, but never opens
 * UI or writes economy state; those side effects stay in UFF_MinigameSubsystem.
 */
UCLASS(Abstract, BlueprintType)
class FESTAFIESTA_API UFF_MinigameRuntime : public UObject
{
	GENERATED_BODY()

public:
	/** Initializes a fresh runtime. A runtime object cannot be reused. */
	bool Initialize(
		UFF_MinigameDefinition* InDefinition,
		const FFF_MinigameSessionContext& InContext,
		FString* OutFailureReason = nullptr);

	/** Advances session time. Non-finite and non-positive deltas are ignored. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame")
	void Advance(float DeltaSeconds);

	/** Cancels a playing session. Cancelled sessions never produce a reward. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Minigame")
	bool Cancel(EFF_MinigameEndReason Reason = EFF_MinigameEndReason::PlayerCancelled);

	const FFF_MinigameSessionSnapshot& GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	FFF_MinigameSessionSnapshot GetSessionSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool IsPlaying() const { return Snapshot.State == EFF_MinigameSessionState::Playing; }

	UFF_MinigameDefinition* GetDefinition() const { return Definition; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	int32 CalculateRewardGold(int32 MaxRewardGold) const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	FFF_MinigameResult BuildResult(int32 MaxRewardGold) const;

	FFF_OnMinigameRuntimeEnded OnEnded;

protected:
	/** Concrete runtimes validate/copy their definition here. */
	virtual bool OnInitialize(FString& OutFailureReason) PURE_VIRTUAL(UFF_MinigameRuntime::OnInitialize, return false;);

	/** Called before the generic elapsed time is committed. */
	virtual void OnAdvancePlaying(float DeltaSeconds, float StartingElapsedSeconds)
		PURE_VIRTUAL(UFF_MinigameRuntime::OnAdvancePlaying, );

	virtual void OnFinished(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason) {}

	void Finish(EFF_MinigameSessionState FinalState, EFF_MinigameEndReason EndReason);
	void SetScore(int32 NewScore) { Snapshot.Score = FMath::Max(0, NewScore); }

	UPROPERTY(Transient)
	TObjectPtr<UFF_MinigameDefinition> Definition;

	UPROPERTY(Transient)
	FFF_MinigameSessionSnapshot Snapshot;

private:
	bool bInitializationAttempted = false;
};
