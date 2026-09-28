// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/FF_GA_SimpleExp.h"

UFF_GA_SimpleExp::UFF_GA_SimpleExp()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	// 부스 하나당 하나만 만들어서 재사용
}

void UFF_GA_SimpleExp::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 부모 기본 처리 먼저 실행

	UE_LOG(LogTemp, Log, TEXT("[SimpleExp] 체험 시작됨!"));

	CachedHandle = Handle;
	CachedActorInfo = ActorInfo;
	CachedActivationInfo = ActivationInfo;
	// 타이머 콜백에서 쓰려고 저장

	PlayExperienceEffect();
	// 체험 연출 시작

	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			ExpTimerHandle,
			this,
			&UFF_GA_SimpleExp::HandleExpComplete,
			ExperienceDurationSeconds,
			false
		);
		// 정해진 시간 뒤에 HandleExpComplete 자동 호출
	}
}

void UFF_GA_SimpleExp::HandleExpComplete()
{
	UE_LOG(LogTemp, Log, TEXT("[SimpleExp] 체험 완료!"));

	PlayCompletionEffect();
	// 완료 연출 실행

	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);
	// 상호작용 종료
}