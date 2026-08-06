// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FF_CrowdSimulationSubsystem.generated.h"

struct FUnitData;
/**
 * 
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdSimulationSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	

	void SpawnNPC(int32 Count,UWorld* SpawnWorld);

	
private:

	
	
	
};
