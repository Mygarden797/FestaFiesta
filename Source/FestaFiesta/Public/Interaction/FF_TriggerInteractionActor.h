// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "FF_TriggerInteractionActor.generated.h"

class UAbilitySystemComponent;
class UFF_TriggerInteractionDataAsset;

UCLASS()
class FESTAFIESTA_API AFF_TriggerInteractionActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AFF_TriggerInteractionActor();
	// 생성자 : 이 액터가 만들어질 때 컴포넌트들을 준비하는 함수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	// 이 액터의 가방 - GAS 기능을 쓰려면 반드시 필요한 요소

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UFF_TriggerInteractionDataAsset> InteractionData;
	// 콘텐츠 담당자가 여기에 DataAsset을 끼워 넣으면, 이 부스의 설정값이 결정됨

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// IAbilitySystemInterface가 요구하는 함수 
};
