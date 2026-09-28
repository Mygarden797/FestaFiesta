// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Incident/Effects/FF_IncidentEffect.h"
#include "FF_IncidentEffect_Info.generated.h"

// 정보성 효과. 월드를 바꾸지 않고 기록만 남긴다. 파이프라인 관통 확인용 첫 효과다.
UCLASS(meta = (DisplayName = "Info (기록만)"))
class FESTAFIESTA_API UFF_IncidentEffect_Info : public UFF_IncidentEffect
{
	GENERATED_BODY()

public:
	virtual void Apply(const FFF_IncidentContext& Context) override;
	virtual FText GetDescription() const override { return Note; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FText Note;
};
