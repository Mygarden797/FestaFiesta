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

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	float MinuteUntileLapse = 60.f;
	
	//심화 조건
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category = "Incident|Identity")
	FGameplayTag EscalateTo;

	// 클릭 시 선택지. 비어 있으면 RequiredStaff 인원을 보내고, 0명이면 즉시 확인한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Incident|Options")
	TArray<TObjectPtr<UFF_IncidentOption>> Options;

	// 방치돼 수명이 다했을 때(Ignored) 적용한다. 플레이어가 고른 선택지가 없으므로 선택지가 아니라 사건에 붙는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Incident|Penalty")
	TArray<TObjectPtr<UFF_IncidentEffect>> OnFailedEffect;

	// 선택지 없이 마커를 눌렀을 때 출동할 인원. 단순 알림은 0으로 설정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Staff", meta = (ClampMin = "0"))
	int32 RequiredStaff = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Options")
	int32 MaxActive = INDEX_NONE;
	
	// 하루에 발생할 수 있는 같은 종류 사건 수. 자연 해소·운영 정리로 사라진 사건은 세지 않는다. INDEX_NONE이면 제한 없음.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Incident|Options")
	int32 MaxPerDay = INDEX_NONE;
	
	
	
	const UFF_IncidentOption* GetOption(int32 OptionIndex) const
	{
		return Options.IsValidIndex(OptionIndex) ? Options[OptionIndex].Get() : nullptr;
	}

};
