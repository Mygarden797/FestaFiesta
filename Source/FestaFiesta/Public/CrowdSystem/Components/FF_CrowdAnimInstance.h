// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "FF_CrowdAnimInstance.generated.h"

/**
 * 
 */
UCLASS()
class FESTAFIESTA_API UFF_CrowdAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Crowd|Animation")
	bool IsWalk = false;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Crowd|Animation")
	bool IsRun = false;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly, Category = "Crowd|Animation")
	bool IsIdle = true;
};
