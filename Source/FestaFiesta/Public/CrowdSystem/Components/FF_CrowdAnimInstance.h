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
	/**
	 * 서브시스템이 매 프레임 밀어 넣는 현재 속도, cm/s.
	 *
	 * UPROPERTY여야 하는 이유: private 비-UPROPERTY 멤버는 AnimBP 그래프에서 변수로 못 읽는다.
	 * 스테이트 머신 트랜지션 조건("Speed > 10")에 쓰려면 이 노출이 필요하고,
	 * 그래야 스레드 세이프 업데이트(Fast Path)도 탈 수 있다 — 500명 규모에서는 이게 비용 차이를 만든다.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Crowd|Animation")
	float Speed = 0.f;
};
