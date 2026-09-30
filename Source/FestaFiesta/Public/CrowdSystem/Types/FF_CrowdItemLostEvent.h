// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdItemLostEvent.generated.h"

// 에이전트가 물건을 떨어뜨린 순간 한 번 쌓인다. 물건은 그 자리에 남으므로 위치만 싣고 Actor는 싣지 않는다
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdItemLostEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|ItemLost")
	int32 StableAgentID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|ItemLost")
	int32 PartyID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|ItemLost")
	FVector LocationUU = FVector::ZeroVector;
};
