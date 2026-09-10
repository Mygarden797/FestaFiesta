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
class USphereComponent;

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
	// 상호작용 키 입력 시 기능를 호출하는 함수

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TObjectPtr<class USphereComponent> InteractionRange;
	// 플레이어가 접근을 감지하는 범위

	UPROPERTY(BlueprintReadOnly, Category = "Trigger")
	bool bPlayerInRange = false;
	// 지금 플레이어가 범위 안에 있는지 여부

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Trigger")
	void ShowInteractionPrompt();
	virtual void ShowInteractionPrompt_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Trigger")
	void HideInteractionPrompt();
	virtual void HideInteractionPrompt_Implementation();

	UFUNCTION(BlueprintImplementableEvent, Category = "Trigger")
	void ShowCameraFrame();
	// M키로 사진 모드 진입 시 카메라 프레임 UI를 띄우는 함수

	UFUNCTION(BlueprintImplementableEvent, Category = "Trigger")
	void HideCameraFrame();
	// 촬영 끝나면 카메라 프레임 UI를 없애는 함수


	UFUNCTION()
	void OnRangeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	// 플레이어가 감지 범위 안으로 들어왔을 때 실행되는 이벤트

	UFUNCTION()
	void OnRangeEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	// 플레이어가 감지 범위 밖으로 벗어났을 때 실행되는 이벤트

};
