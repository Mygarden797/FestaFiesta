// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "FF_IncidentOption.generated.h"

class UFF_IncidentEffect;

// 사건 하나가 주는 선택지 한 줄. Effect 를 Instanced 로 붙이려고 struct 가 아니라 UObject 다.
// 비용은 골드와 작업 인원, 보상은 Effect 로 표현한다. WorkAmount 단위는 FFF_FestivalEventCard 와 같은 사람-분이다.
// 보낼 인원은 플레이어가 MinStaff 이상에서 고르고, 상한은 그때 출동 가능한 스태프 수다.
// 현장 작업 시간(축제 분) = WorkAmount ÷ 보낸 인원.
UCLASS(EditInlineNew, DefaultToInstanced, BlueprintType, CollapseCategories)
class FESTAFIESTA_API UFF_IncidentOption : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option|Cost", meta = (ClampMin = "0"))
	int32 CostGold = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option|Cost", meta = (ClampMin = "0"))
	int32 CostStaff = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option|Staff", meta = (ClampMin = "1"))
	int32 MinStaff = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Option|Staff", meta = (ClampMin = "0.0"))
	float WorkAmount = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Option|Reward")
	TObjectPtr<UFF_IncidentEffect> Effect;
};
