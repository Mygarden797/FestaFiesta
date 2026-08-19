// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FF_SimulationSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Simulation"))
class FESTAFIESTA_API UFF_SimulationSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	static const UFF_SimulationSettings* Get();
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Determinism")
	int32 GlobalSimulationSeed = 20260812;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Variation")
	TSoftObjectPtr<UDataTable> IndividualVariationTable;
	
	// — 어긋나면 같은 에이전트가 두 계층에서 다른 사람이 되고, 아무 경고도 안 난다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Variation")
	float VariationCorrelationStrength = 0.3f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Utility")
	TSoftObjectPtr<UDataTable> UtilityCoefficientTable;
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	TSoftObjectPtr<UDataTable> ArchetypeTable;
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	FName ArchetypeScenarioId = TEXT("resident_neutral_v0");
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	TSoftObjectPtr<UDataTable> CompanionModifierTable;
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	TSoftObjectPtr<UDataTable> GroupSizeTable;
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	TSoftObjectPtr<UDataTable> ChildCompositionTable;
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Population")
	FName GroupScopeId = TEXT("tourism_daytrip");
};
