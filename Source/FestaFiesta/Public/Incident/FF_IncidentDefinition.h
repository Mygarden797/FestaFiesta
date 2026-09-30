// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FF_IncidentTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "Incident/FF_IncidentOption.h"
#include "FF_IncidentDefinition.generated.h"

/**
 * 
 */
UCLASS()
class FESTAFIESTA_API UFF_IncidentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	FGameplayTag IncidentTag;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	EFF_IncidentTone Tone = EFF_IncidentTone::Problem;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	EFF_IncidentUrgency Urgency= EFF_IncidentUrgency::Medium;
	
	//사건 디스플레이
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	FText Title;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	FText Body;
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Display")
	TSoftObjectPtr<UTexture2D> MarkerIcon;
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	float MinuteUntileLapse = 60.f;
	
	//심화 조건
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	FGameplayTag EscalateTo;

	//클릭 시 선택지. 비어 있으면 확인만 하는 사건이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Incident|Options")
	TArray<TObjectPtr<UFF_IncidentOption>> Options;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Options")
	int32 MaxActive = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Options")
	int32 MaxPerDay = INDEX_NONE;
	
	
	
	const UFF_IncidentOption* GetOption(int32 OptionIndex) const
	{
		return Options.IsValidIndex(OptionIndex) ? Options[OptionIndex].Get() : nullptr;
	}

};
