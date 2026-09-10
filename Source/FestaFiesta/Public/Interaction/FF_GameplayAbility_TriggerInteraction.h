// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FF_GameplayAbility_TriggerInteraction.generated.h"

class UFF_GE_TriggerCooldown;

// 축제 부스/포토존 같은 트리거 상호작용의 공통 베이스 기능
UCLASS()
class FESTAFIESTA_API UFF_GameplayAbility_TriggerInteraction : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UFF_GameplayAbility_TriggerInteraction();
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TSubclassOf<class UFF_GE_TriggerCooldown> CooldownEffectClass;
	// 상호작용 끝나고 나타나는 쿨다운 효과

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	float PhotoFocusDelaySeconds = 1.0f;
	// 카메라 프레임 뜬 뒤 실제 촬영까지 기다리는 시간 (초)

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	// 기능 발동 시 실행되는 메인 로직 - 실제 상호작용 시작 로직을 여기 적음

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;
	// 기능 종료 시 호출되는 정리 로직

	UFUNCTION(BlueprintImplementableEvent, Category = "Trigger")
	void PlayPhotoCaptureEffect();
	// 화면 플래시 연출

private:

	void HandlePhotoFocusComplete();
	// 타이머가 끝났을 때 실제 촬영을 실행하는 함수

	FTimerHandle PhotoFocusTimerHandle;
	// 초점 대기시간을 재는 타이머

	FGameplayAbilitySpecHandle CachedHandle;
	const FGameplayAbilityActorInfo* CachedActorInfo = nullptr;
	FGameplayAbilityActivationInfo CachedActivationInfo;
	// 타이머 콜백에서 EndAbility를 호출하기 위해 값들을 잠깐 저장해두는 변수들

};
