// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/FF_GameplayAbility_TriggerInteraction.h"
#include "Interaction/UFF_GE_TriggerCooldown.h"
#include "AbilitySystemComponent.h"


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

	// 취소되지 않고 정상적으로 끝났을 때만 쿨다운 적용
	if (!bWasCancelled && CooldownEffectClass && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

		// 쿨다운 효과를 적용하기 위한 세팅 데이터 만들기
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(CooldownEffectClass, 1.f, EffectContext);

		// 데이터 생성이 성공했다면 액터 본인에게 쿨다운 적용
		if (SpecHandle.IsValid())
		{
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
			UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 쿨다운 효과 적용 완료"));
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	// 부모 클래스의 마무리 실행
}

