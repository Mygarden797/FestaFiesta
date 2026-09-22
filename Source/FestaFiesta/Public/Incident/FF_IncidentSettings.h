// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_IncidentCatalog.h"
#include "Engine/DeveloperSettings.h"
#include "FF_IncidentSettings.generated.h"

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Incident"))
class FESTAFIESTA_API UFF_IncidentSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	virtual FName GetCategoryName() const override {return TEXT("Game");}
	
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Incident")
	static const UFF_IncidentSettings* Get();
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Incident")
	TSoftObjectPtr<UFF_IncidentCatalog> Catalog;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Incident", meta = (ClampMin = "1"))
	int32 MaxConcurrentIncidents = 64;
};
