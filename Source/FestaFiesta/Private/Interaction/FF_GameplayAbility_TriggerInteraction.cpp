// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/FF_GameplayAbility_TriggerInteraction.h"


UFF_GameplayAbility_TriggerInteraction::UFF_GameplayAbility_TriggerInteraction()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// InstancedPerActor : 부스 하나당 기능 인스턴스를 하나만 만들어서 재사용
}

void UFF_GameplayAbility_TriggerInteraction::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 부모 클래스의 기본 처리를 먼저 실행

	UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 상호작용 시작됨!"));
	// 지금은 시작이 잘 되는지만 확인 (나중에 여기에 UI 표시, 카메라 연출 등 추가 예정)

	// TODO: 추후 Hold 타이머 로직 적용 예정 (현재는 테스트용으로 즉시 종료)
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
	
}

void UFF_GameplayAbility_TriggerInteraction::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 상호작용 종료됨! 취소여부=%s"), bWasCancelled ? TEXT("true") : TEXT("false"));
	
	// TODO: 추후 쿨다운 GameplayEffect 적용 위치

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// 부모 클래스의 마무리 실행
}

