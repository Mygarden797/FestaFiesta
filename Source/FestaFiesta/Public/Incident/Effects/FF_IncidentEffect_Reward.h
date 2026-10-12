// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/Effects/FF_IncidentEffect.h"
#include "FF_IncidentEffect_Reward.generated.h"

// 사건 성공 효과의 금화 보상. 경제 원장으로 지급하고 실제 수용액을 결과로 돌려준다.
UCLASS(meta = (DisplayName = "Reward (골드 보상)"))
class FESTAFIESTA_API UFF_IncidentEffect_Reward : public UFF_IncidentEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FFF_IncidentContext& Context) override;
	virtual FFF_IncidentEffectResult ApplyWithResult(const FFF_IncidentContext& Context) override;
	virtual FText GetDescription() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 RewardGold = 0;
};
