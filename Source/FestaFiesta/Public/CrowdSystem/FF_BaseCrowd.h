// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Types/FF_FestivalCrowdTypes.h"
#include "FF_BaseCrowd.generated.h"

class UFF_CrowdAnimInstance;
class UFF_CrowdMovementComponent;
class UFF_CrowdVisualComponent;


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
	
protected:



public:
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Crowd|Component")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category = "Crowd|Animation")
	TObjectPtr<UFF_CrowdAnimInstance> CrowdAnimInstance;
	
private:
	int32 AgentIndex = INDEX_NONE;
	
	
};
