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
	UFUNCTION(BlueprintCallable)
	void SetSpeed(float NewSpeed) { Speed = NewSpeed;}

	UFUNCTION(BlueprintCallable)
	float GetSpeed() { return Speed;}

protected:
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Animation")
	float Speed = 0.f;
};
