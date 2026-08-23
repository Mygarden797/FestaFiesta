// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/FF_TriggerInteractionActor.h"

#include "AbilitySystemComponent.h"

AFF_TriggerInteractionActor::AFF_TriggerInteractionActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	// 이 액터가 만들어질 때, "능력 가방"을 하나 자동으로 만들어서 붙여줌
}

UAbilitySystemComponent* AFF_TriggerInteractionActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
	// 누가 "네 능력 가방 뭐야?"라고 물으면 우리가 만든 AbilitySystemComponent를 알려줌
}

