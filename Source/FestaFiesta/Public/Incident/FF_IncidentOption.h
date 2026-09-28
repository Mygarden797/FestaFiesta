// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FF_IncidentOption.generated.h"

class UFF_IncidentEffect;

// 사건 하나가 주는 선택지 한 줄. Effect 를 Instanced 로 붙이려고 struct 가 아니라 UObject 다.
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, CollapseCategories)
class FESTAFIESTA_API UFF_IncidentOption : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (ClampMin = "0"))
	int32 CostGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option", meta = (ClampMin = "0"))
	int32 CostStaff = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Option")
	TObjectPtr<UFF_IncidentEffect> Effect;
};
