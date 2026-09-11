// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/FF_GameplayAbility_TriggerInteraction.h"
#include "Interaction/UFF_GE_TriggerCooldown.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Interaction/FF_TriggerInteractionActor.h"

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
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayTag CooldownTag = FGameplayTag::RequestGameplayTag(FName("Trigger.State.Cooldown"));
		if (ActorInfo->AbilitySystemComponent->HasMatchingGameplayTag(CooldownTag))
		{
			UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 쿨다운 중이라 상호작용 거부됨"));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			// bWasCancelled=true로 종료 -> 쿨다운 재적용 안 되게 함
			return;
		}
	}
	// 쿨다운 중이면 아예 시작하지 않음

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	// 부모 클래스의 기본 처리를 먼저 실행

	UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 상호작용 시작됨!"));
	// 지금은 시작이 잘 되는지만 확인 (나중에 여기에 UI 표시, 카메라 연출 등 추가 예정)

	CachedHandle = Handle;
	CachedActorInfo = ActorInfo;
	CachedActivationInfo = ActivationInfo;

	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (AFF_TriggerInteractionActor* TriggerActor = Cast<AFF_TriggerInteractionActor>(ActorInfo->AvatarActor.Get()))
		{

			TriggerActor->HideInteractionPrompt();
			// 촬영 시작하면 "M 사진 촬영" 안내는 숨김

			TriggerActor->ShowCameraFrame();
			// 카메라 프레임 UI 표시

			TriggerActor->GetWorldTimerManager().SetTimer(
				PhotoFocusTimerHandle,
				this,
				&UFF_GameplayAbility_TriggerInteraction::HandlePhotoFocusComplete,
				PhotoFocusDelaySeconds,
				false
			);
			// PhotoFocusDelaySeconds(기본 1초) 뒤에 HandlePhotoFocusComplete() 자동 호출
		}
	}
}

// ⭐ NEW - 타이머 끝나면 실행되는 함수 (실제 촬영)
void UFF_GameplayAbility_TriggerInteraction::HandlePhotoFocusComplete()
{
	UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] 초점 완료, 촬영 실행"));

	if (CachedActorInfo && CachedActorInfo->AvatarActor.IsValid())
	{
		if (AFF_TriggerInteractionActor* TriggerActor = Cast<AFF_TriggerInteractionActor>(CachedActorInfo->AvatarActor.Get()))
		{
			TriggerActor->HideCameraFrame();
			// 카메라 프레임 UI 숨기기

			TriggerActor->CaptureAndShowPhoto();
			// 화면 캡처 + 결과 사진 UI 표시

		}
	}

	PlayPhotoCaptureEffect();
	// 화면 플래시 연출 실행

	// 상호작용 종료 (쿨다운 적용됨)
	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);

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

