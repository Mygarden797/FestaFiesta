// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "FF_TriggerInteractionDataAsset.generated.h"

UENUM(BlueprintType)
enum class EFF_TriggerInputType : uint8
{
	Press,
	// 짧게 한 번 누르면 바로 실행

	Hold
	// 일정 시간 동안 눌러야 실행
};

// 부스/포토존 등 Ground Trigger 상호작용 하나하나의 설정값을 담는 DataAsset
UCLASS(BlueprintType)
class FESTAFIESTA_API UFF_TriggerInteractionDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Display")
	FText InteractionDisplayName;
	// 화면에 표시할 상호작용 이름 (예: "사진 찍기")

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Input")
	EFF_TriggerInputType InputType = EFF_TriggerInputType::Press;
	// 짧게 누르기(Press)인지, 꾹 누르기(Hold)인지

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Input", meta = (EditCondition = "InputType == EFF_TriggerInputType::Hold"))
	float HoldDurationSeconds = 2.0f;
	// Hold일 때 몇 초 눌러야 완료되는지 (Press면 이 값 안 씀)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Cooldown")
	float CooldownSeconds = 10.0f;
	// 완료 후 다시 사용 가능해지기까지 걸리는 시간 (기획서 기준 5~15초 범위)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger|Tag")
	FGameplayTag InteractionTag;
	// 이 상호작용을 식별하는 태그 (예: Interaction.PhotoZone)
};