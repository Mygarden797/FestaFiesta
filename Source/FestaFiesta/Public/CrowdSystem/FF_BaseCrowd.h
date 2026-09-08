// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Types/FF_FestivalCrowdTypes.h"
#include "FF_BaseCrowd.generated.h"

class UAnimInstance;
class UFF_CrowdAnimInstance;
class UFF_CrowdMovementComponent;
class UFF_CrowdVisualComponent;
class UStaticMeshComponent;


//BaseCrowd 비용을 아끼기 위해서 모든 이동 정보는 Subsystem에서 담당하고, Actor는 오직 후보정이 끝난 값만 받아서, 
//Tick 당 이동하기 위한 함수만 처리한다.



UCLASS()
class FESTAFIESTA_API AFF_BaseCrowd : public APawn
{
	GENERATED_BODY()

public:

	AFF_BaseCrowd();
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	//--- Agent와의 연결/연결 해제를 위한 함수 ---/
	virtual void BindToAgent(int32 NewAgentIndex, FVector SpawnLocation, FRotator SpawnRotation);
	virtual void ReleaseFromAgent();
	// --- 후 보정이 끝난 이동 값을 그대로 적용하기 위한 함수 ---/
	virtual void PushPresentation(FVector NewPosition, FRotator NewRotator, float NewSpeed);
	// --- 알맞는 외형을 선택하기 위한 장치 --- /
	virtual void ApplyAppearance(USkeletalMesh* Mesh,UClass* AnimClass, float YawOffset, float UniformScale);
	
	virtual FVector GetVelocity() const override { return CachedVelocity; }
	
protected:



public:

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// BP에서 상대 회전/위치를 자유롭게 줄 수 있다. 메시 정면이 +X가 아니면 여기서 Yaw를 보정한다
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Crowd|Animation")
	TObjectPtr<UFF_CrowdAnimInstance> CrowdAnimInstance;

	//SkeletalMesh가 없을 때 대체용으로 켜지는 메시
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<UStaticMeshComponent> ProxyMeshComponent;


	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Crowd|Visual")
	float MeshYawOffset = 0.f;

private:
	// BP(BP_Crowd)의 SkeletalMeshComponent에 지정된 AnimClass. BeginPlay에서 한 번 받아 둔다.
	// 아키타입 행이 AnimClass를 비워 뒀을 때 되돌릴 자리가 있어야 한다 —
	// 없으면 풀 슬롯을 물려받는 순간 이전 아키타입의 ABP가 그대로 남는다.
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> DefaultAnimClass;

	int32 AgentIndex = INDEX_NONE;

	// GetVelocity()가 돌려줄 값. PushPresentation에서 갱신한다
	FVector CachedVelocity = FVector::ZeroVector;
	
	
};
