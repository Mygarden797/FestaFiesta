// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_FestivalCrowdTypes.generated.h"



USTRUCT(BlueprintType)
struct FUnitData
{
	GENERATED_BODY()
	
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	float Speed = 150.f;
	
	int32 InstanceIndex = INDEX_NONE;
};