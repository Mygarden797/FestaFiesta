// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "FF_StaffActor.generated.h"

class UAbilitySystemComponent;
class UFF_StaffAttributeSet;
class UGameplayEffect;
class USkeletalMeshComponent;

// 스태프 한 명의 몸. 충돌은 끄고, 이동은 UFF_StaffSubsystem 이 경로로 옮겨 준다
UCLASS()
class FESTAFIESTA_API AFF_StaffActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFF_StaffActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void SetStaffId(int32 InStaffId);
	int32 GetStaffId() const { return StaffId; }

	void InitFromLevel(int32 TrainingLevel);
	float GetMoveSpeed() const;
	float GetWorkPower() const;

	bool ApplyEffect(TSubclassOf<UGameplayEffect> Effect);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Staff")
	TObjectPtr<USceneComponent> RootSceneComponent;

	UPROPERTY(VisibleAnywhere, Category = "Staff")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, Category = "Staff|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UFF_StaffAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Staff|GAS")
	TSubclassOf<UGameplayEffect> InitEffect;

	int32 StaffId = INDEX_NONE;
};
