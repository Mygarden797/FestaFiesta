// Fill out your copyright notice in the Description page of Project Settings.


#include "Interaction/UFF_GE_TriggerCooldown.h"

UFF_GE_TriggerCooldown::UFF_GE_TriggerCooldown()
{
	// 효과 지속 방식 : 정해진 시간 동안만 유지
	DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// 지속 시간 기본값 설정 (10초)
	DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(10.f));
}
