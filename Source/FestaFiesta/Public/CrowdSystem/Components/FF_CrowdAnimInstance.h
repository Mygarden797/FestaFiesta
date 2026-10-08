// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
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

	// AFF_BaseCrowd 가 자기 CurrentState 를 여기에 복사해 준다. ABP 는 읽기만 한다
	void SetState(EFF_CrowdRunState NewState) { State = NewState; }

protected:
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Animation")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Animation")
	EFF_CrowdRunState State = EFF_CrowdRunState::Running;
};
