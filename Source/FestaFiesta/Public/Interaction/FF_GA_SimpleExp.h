// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "FF_GA_SimpleExp.generated.h"

// 소원등, 조개줍기 등 짧은 체험형 상호작용의 공통 기능
UCLASS()
class FESTAFIESTA_API UFF_GA_SimpleExp : public UGameplayAbility
{
	GENERATED_BODY()

public:

	UFF_GA_SimpleExp();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	float ExperienceDurationSeconds = 2.0f;
	// 체험이 진행되는 시간(초)

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	// 시작할 때 실행되는 로직

	UFUNCTION(BlueprintImplementableEvent, Category = "Trigger")
	void PlayExperienceEffect();
	// 체험 중 연출 (사운드 등, 블루프린트에서 구현)

	UFUNCTION(BlueprintNativeEvent, Category = "Trigger")
	void PlayCompletionEffect(const FText& Message);
	virtual void PlayCompletionEffect_Implementation(const FText& Message);
	//  BP에서 안 바꾸면 C++이 문구를 화면에 출력

private:

	void HandleExpComplete();
	// 시간 다 되면 실행되는 함수

	FTimerHandle ExpTimerHandle;
	// 시간 재는 타이머

	FGameplayAbilitySpecHandle CachedHandle;
	const FGameplayAbilityActorInfo* CachedActorInfo = nullptr;
	FGameplayAbilityActivationInfo CachedActivationInfo;
	// 나중에 EndAbility 부를 때 쓰려고 저장해두는 값들

	FText CachedCompletionMessage;
	// NEW 데이터 에셋에서 읽어온 완료 문구
};