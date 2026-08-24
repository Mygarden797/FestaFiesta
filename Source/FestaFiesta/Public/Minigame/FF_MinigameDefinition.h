// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Minigame/FF_MinigameTypes.h"
#include "Minigame/Surfing/FF_SurfingTypes.h"
#include "FF_MinigameDefinition.generated.h"

class UFF_MinigameRuntime;
class UFF_MinigameScreenBase;
class UDataTable;

/**
 * Data-authored contract shared by every facility minigame.
 *
 * A definition contains identity, facility routing, duration and reward scaling,
 * while RuntimeClass contains only the session rules. This keeps balance changes
 * out of the coordinator and lets later minigames plug in without editing it.
 */
UCLASS(Abstract, BlueprintType)
class FESTAFIESTA_API UFF_MinigameDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFF_MinigameDefinition();

	static const FPrimaryAssetType& GetMinigameDefinitionAssetType();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame")
	FName MinigameId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame")
	FText DisplayName;

	/** Stable FFZS FacilityDefinitionIds that may launch this minigame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Facilities")
	TArray<FName> CompatibleFacilityDefinitionIds;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Session", meta = (ClampMin = "0.1"))
	float DurationSeconds = 30.0f;

	/** A score at or above this value receives the coordinator's full reward cap. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Reward", meta = (ClampMin = "1"))
	int32 ScoreForMaxReward = 1000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Runtime")
	TSubclassOf<UFF_MinigameRuntime> RuntimeClass;

	/** Soft UI dependency: catalog/runtime tests never need to load a widget class. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|UI")
	TSoftClassPtr<UFF_MinigameScreenBase> ScreenClass;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool SupportsFacility(FName FacilityDefinitionId) const;

	/** Appends errors and returns true when this definition can create a session. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Minigame")
	bool IsDefinitionValid(TArray<FText>& OutErrors) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const;
};

/** First MVP: a deterministic thirty-second target-shooting session. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_TargetShootingDefinition final : public UFF_MinigameDefinition
{
	GENERATED_BODY()

public:
	UFF_TargetShootingDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Target Shooting")
	FFF_TargetShootingRules Rules;

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const override;
};

/** Deterministic thirty-second free-ride session in a fixed two-dimensional arena. */
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_SurfingDefinition final : public UFF_MinigameDefinition
{
	GENERATED_BODY()

public:
	UFF_SurfingDefinition();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Surfing")
	FFF_SurfingRules Rules;

	/** Fixed Rock/Log/Buoy and pickup placement authored as FFF_SurfingCourseRow rows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Minigame|Surfing")
	TSoftObjectPtr<UDataTable> CourseLayoutTable;

protected:
	virtual void AppendValidationErrors(TArray<FText>& OutErrors) const override;
};
