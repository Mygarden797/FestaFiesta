// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "UFF_GE_TriggerCooldown.generated.h"


// 상호작용 완료 후 부여하는 쿨다운 효과 (재상호작용 방지용)
UCLASS()
class FESTAFIESTA_API UFF_GE_TriggerCooldown : public UGameplayEffect
{
	GENERATED_BODY()

public:
	// 생성자 : 쿨다운 지속시간 및 기본 효과 설정
	UFF_GE_TriggerCooldown();
};