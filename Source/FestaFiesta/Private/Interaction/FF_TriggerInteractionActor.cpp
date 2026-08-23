// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/FF_TriggerInteractionActor.h"

#include "AbilitySystemComponent.h"

#include "Interaction/FF_GameplayAbility_TriggerInteraction.h"

AFF_TriggerInteractionActor::AFF_TriggerInteractionActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 이 액터가 만들어질 때 상호작용 기능(GAS) 부품을 자동으로 붙임
}

UAbilitySystemComponent* AFF_TriggerInteractionActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
	// 외부/시스템 요청 시 소유한 GAS 컴포넌트 반환
}

void AFF_TriggerInteractionActor::TryStartInteraction()
{
	if (!AbilitySystemComponent || !InteractionAbilityClass)
		
	{
		UE_LOG(LogTemp, Warning, TEXT("[TriggerInteraction] AbilitySystemComponent 또는 InteractionAbilityClass가 비어있습니다."));
		return;
		// 실행할 상호작용 기능이 비어있으면 경고 및 종료
	}

	if (UGameInstance* GameInstance = GetGameInstance())
		
	{
		if (UFF_FestivalStageSubsystem* StageSubsystem = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
			
		{
			if (!StageSubsystem->IsLiveStage())
				
			{
				UE_LOG(LogTemp, Log, TEXT("[TriggerInteraction] Live 단계가 아니라 상호작용 불가"));
				return;
				
			}
		}
	}
	//live단계인지 확인

	if (!AbilitySystemComponent->FindAbilitySpecFromClass(InteractionAbilityClass))
		
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(InteractionAbilityClass, 1));
		
	}

	AbilitySystemComponent->TryActivateAbilityByClass(InteractionAbilityClass);
	// 준비된 상호작용 기능 실행
}
