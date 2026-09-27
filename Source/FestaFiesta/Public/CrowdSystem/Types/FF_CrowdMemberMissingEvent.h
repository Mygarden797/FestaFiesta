// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdMemberMissingEvent.generated.h"

class AActor;

UENUM(BlueprintType)
enum class EFF_CrowdMissingChange : uint8
{
	Started      = 0, //미아가 되었을 때
	Returned     = 1, //미아 멤버가 복귀했을 때
	EnteredQueue = 2, //대기 Queue에 입장했을 때
	Despawned    = 3 //디스폰되었을 떄
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_CrowdMemberMissingEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Missing")
	EFF_CrowdMissingChange Change = EFF_CrowdMissingChange::Started;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Missing")
	int32 StableAgentID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Missing")
	int32 PartyID = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Missing")
	FVector LocationUU = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd|Missing")
	bool bFormationPathBlocked = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> Actor;
};
