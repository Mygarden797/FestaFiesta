// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/UFF_GE_TriggerCooldown.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UFF_GE_TriggerCooldown::UFF_GE_TriggerCooldown()
{
	// 효과 지속 방식 : 정해진 시간 동안만 유지
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 지속 시간 기본값 설정 (10초)
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));

	UTargetTagsGameplayEffectComponent* TargetTagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(FGameplayTag::RequestGameplayTag(FName("Trigger.State.Cooldown")));
	TargetTagsComponent->SetAndApplyTargetTagChanges(TagContainer);

	GEComponents.Add(TargetTagsComponent);
	// 이 효과가 붙어있는 동안 액터에게 쿨다운 태그를 부여함
	// 이 효과가 붙어있는 동안 액터에게 쿨다운 중이라는 태그를 부여함
}
