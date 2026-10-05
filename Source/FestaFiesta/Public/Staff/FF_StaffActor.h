// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Actor.h"
#include "FF_StaffActor.generated.h"

class UAbilitySystemComponent;
class UFF_CrowdAnimInstance;
class UFF_StaffAttributeSet;
class UGameplayEffect;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// 스태프 한 명의 몸. 출동 때만 UFF_StaffSubsystem 이 스폰하고 복귀하면 지운다. 충돌은 끄고, 위치는 PushPresentation 으로만 받는다.
// SkeletalMesh 가 없으면 Crowd 와 같은 크기의 원기둥을 ProxyColor 로 칠해 군중과 구별되게 보여 준다.
// AnimInstance 가 UFF_CrowdAnimInstance 면 걷는 속도를 넘겨 Crowd 와 같은 이동 애니메이션을 쓴다.
UCLASS()
class FESTAFIESTA_API AFF_StaffActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFF_StaffActor();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	void SetStaffId(int32 InStaffId);
	int32 GetStaffId() const { return StaffId; }

	void PushPresentation(const FVector& NewPosition, FRotator NewRotation, float NewSpeed);

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

	UPROPERTY(VisibleAnywhere, Category = "Staff")
	TObjectPtr<UStaticMeshComponent> ProxyMeshComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Staff|Visual")
	FLinearColor ProxyColor = FLinearColor(1.f, 0.45f, 0.f);

	UPROPERTY(EditDefaultsOnly, Category = "Staff|Visual")
	float MeshYawOffset = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UFF_CrowdAnimInstance> AnimInstance;

	UPROPERTY(VisibleAnywhere, Category = "Staff|GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UFF_StaffAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, Category = "Staff|GAS")
	TSubclassOf<UGameplayEffect> InitEffect;

	int32 StaffId = INDEX_NONE;
};
