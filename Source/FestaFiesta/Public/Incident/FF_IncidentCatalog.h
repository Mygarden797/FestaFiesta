// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_IncidentDefinition.h"
#include "Engine/DataAsset.h"
#include "FF_IncidentCatalog.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_IncidentCatalogEntry
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Incident|Category")
	bool bEnabled = true;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Incident|Category")
	TSoftObjectPtr<UFF_IncidentDefinition> Definition;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Incident|Category")
	float WeightOverride = -1.f;
};

UCLASS()
class FESTAFIESTA_API UFF_IncidentCatalog : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Incident|Category")
	TArray<FFF_IncidentCatalogEntry> Entries;
};
