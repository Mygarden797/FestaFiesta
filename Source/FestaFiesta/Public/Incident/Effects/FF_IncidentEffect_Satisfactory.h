// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_IncidentEffect.h"
#include "FF_IncidentEffect_Satisfactory.generated.h"

/**
 * 
 */
UCLASS()
class FESTAFIESTA_API UFF_IncidentEffect_Satisfactory : public UFF_IncidentEffect
{
	GENERATED_BODY()
	
public:
	virtual void Apply(const FFF_IncidentContext& Context) override;
	virtual FText GetDescription() const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 SatisfactoryAmount = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 Radius = 0;
};
