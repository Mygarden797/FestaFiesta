// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_IncidentCatalog.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "FF_IncidentSettings.generated.h"

class AFF_StaffActor;

/**
 * 
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "FestaFiesta Incident"))
class FESTAFIESTA_API UFF_IncidentSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	virtual FName GetCategoryName() const override {return TEXT("Game");}
	
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Incident")
	static const UFF_IncidentSettings* Get();
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Incident")
	TSoftObjectPtr<UFF_IncidentCatalog> Catalog;
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Incident", meta = (ClampMin = "1"))
	int32 MaxConcurrentIncidents = 64;

	// 출동 때 스폰할 스태프 몸. 비우면 AFF_StaffActor 기본형(주황 원기둥)을 쓴다. 직원 외형 BP를 지정할 수 있다.
	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Staff Response")
	TSoftClassPtr<AFF_StaffActor> StaffActorClass;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Staff Response", meta = (ClampMin = "1.0", Units = "cm/s"))
	float StaffTravelSpeed = 300.f;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Crowd Bridge")
	FGameplayTag CrowdCongestionTag;

	UPROPERTY(EditAnywhere, Config, BlueprintReadOnly, Category = "Crowd Bridge")
	FGameplayTag CrowdMemberMissingTag;
};
