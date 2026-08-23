// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "FF_TriggerInteractionActor.generated.h"


class UAbilitySystemComponent;
class UFF_TriggerInteractionDataAsset;
class UFF_GameplayAbility_TriggerInteraction;

UCLASS()
class FESTAFIESTA_API AFF_TriggerInteractionActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:

	AFF_TriggerInteractionActor();
	

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	// GAS 상호작용 처리를 위한 핵심 컴포넌트

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<UFF_TriggerInteractionDataAsset> InteractionData;
	// 상호작용 에셋 설정 값 (쿨타임, 텍스트 등 데이터 연결용)

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// GAS 인터페이스 필수 구현 함수 (외부에서 컴포넌트 접근용)

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TSubclassOf<class UFF_GameplayAbility_TriggerInteraction> InteractionAbilityClass;
	// 상호작용 시 실행할 기능 블루프린트 지정

	UFUNCTION(BlueprintCallable, Category = "Trigger")
	void TryStartInteraction();
	// 상호작용 키 입력 시 기능를 직접 발동시키는 함수

};
