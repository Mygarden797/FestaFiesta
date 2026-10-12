// Fill out your copyright notice in the Description page of Project Settings.

#include "Interaction/FF_GA_SimpleExp.h"
#include "Interaction/FF_TriggerInteractionActor.h" 
#include "Interaction/FF_TriggerInteractionDataAsset.h"

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

	float Duration = ExperienceDurationSeconds;
	// ⭐ NEW 일단 Ability 기본 시간으로 시작

	CachedCompletionMessage = FText::GetEmpty();
	// ⭐ NEW 이전 체험 문구 비우기

	if (ActorInfo && ActorInfo->AvatarActor.IsValid())
	{
		if (AFF_TriggerInteractionActor* TriggerActor = Cast<AFF_TriggerInteractionActor>(ActorInfo->AvatarActor.Get()))
		{
			TriggerActor->HideInteractionPrompt();
			// ⭐ NEW 체험 중에는 안내 글자 숨기기

			if (TriggerActor->InteractionData)
			{
				if (TriggerActor->InteractionData->ExperienceDurationSeconds > 0.f)
				{
					Duration = TriggerActor->InteractionData->ExperienceDurationSeconds;
					// ⭐ NEW 데이터 에셋에 시간 있으면 그걸 사용
				}
				CachedCompletionMessage = TriggerActor->InteractionData->CompletionMessage;
				// ⭐ NEW 완료 문구 저장
			}
		}

		ActorInfo->AvatarActor->GetWorldTimerManager().SetTimer(
			ExpTimerHandle,
			this,
			&UFF_GA_SimpleExp::HandleExpComplete,
			Duration,
			false
		);
	}
}

void UFF_GA_SimpleExp::HandleExpComplete()
{
	UE_LOG(LogTemp, Log, TEXT("[SimpleExp] 체험 완료!"));

	PlayCompletionEffect(CachedCompletionMessage);
	// 완료 연출 실행

	EndAbility(CachedHandle, CachedActorInfo, CachedActivationInfo, true, false);
	// 상호작용 종료
}

void UFF_GA_SimpleExp::PlayCompletionEffect_Implementation(const FText& Message)
{
	UE_LOG(LogTemp, Log, TEXT("[SimpleExp] 완료 연출 호출, 문구: %s"), *Message.ToString());
	// ⭐ NEW 확인용 로그: 문구가 제대로 왔는지

	if (CachedActorInfo && CachedActorInfo->AvatarActor.IsValid())
	{
		if (AFF_TriggerInteractionActor* TriggerActor = Cast<AFF_TriggerInteractionActor>(CachedActorInfo->AvatarActor.Get()))
		{
			UE_LOG(LogTemp, Log, TEXT("[SimpleExp] BP로 완료 신호 보냄: %s"), *TriggerActor->GetName());
			// ⭐ NEW 확인용 로그: 어느 액터에게 보냈는지

			TriggerActor->OnExperienceCompleted(Message);
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[SimpleExp] 액터를 못 찾아서 완료 신호를 못 보냄"));
	// ⭐ NEW 확인용 로그: 실패한 경우
}