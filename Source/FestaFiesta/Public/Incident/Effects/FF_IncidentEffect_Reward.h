// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/Effects/FF_IncidentEffect.h"
#include "FF_IncidentEffect_Reward.generated.h"

// 골드 보상. 지금은 상세 패널에 문구를 보여 주는 데까지만 쓴다.
// Economy 에 사건 보상 입구(EFF_EarnReason 값)가 정해지기 전이라 Apply 는 기록만 남기고 지급하지 않는다.
UCLASS(meta = (DisplayName = "Reward (골드 보상)"))
class FESTAFIESTA_API UFF_IncidentEffect_Reward : public UFF_IncidentEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FFF_IncidentContext& Context) override;
	virtual FText GetDescription() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (ClampMin = "0"))
	int32 RewardGold = 0;
};
