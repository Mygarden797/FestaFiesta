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
};
