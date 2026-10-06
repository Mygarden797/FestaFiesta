// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"
#include "Staff/FF_StaffTypes.h"
#include "FF_StaffAnimInstance.generated.h"

// 스태프 몸 전용 AnimInstance. Crowd 의 Speed 위에 지금 상태(Moving/Working/Returning)를 얹어, ABP 스테이트 머신이 현장 작업 동작으로 갈 수 있게 한다.
UCLASS()
class FESTAFIESTA_API UFF_StaffAnimInstance : public UFF_CrowdAnimInstance
{
	GENERATED_BODY()

public:
	void SetStaffState(EFF_StaffState NewState) { StaffState = NewState; }

	UFUNCTION(BlueprintPure, Category = "Staff|Animation", meta = (BlueprintThreadSafe))
	EFF_StaffState GetStaffState() const { return StaffState; }

	UFUNCTION(BlueprintPure, Category = "Staff|Animation", meta = (BlueprintThreadSafe))
	bool IsWorking() const { return StaffState == EFF_StaffState::Working; }

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Staff|Animation")
	EFF_StaffState StaffState = EFF_StaffState::Idle;
};
