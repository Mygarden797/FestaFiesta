// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_BaseCrowd.h"

#include "CrowdSystem/Components/FF_CrowdMovementComponent.h"
#include "CrowdSystem/Components/FF_CrowdVisualComponent.h"



AFF_BaseCrowd::AFF_BaseCrowd()
{
	
	PrimaryActorTick.bCanEverTick = false;
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SetRootComponent(SkeletalMeshComponent);

	
}


void AFF_BaseCrowd::BeginPlay()
{
	Super::BeginPlay();
	
}




void AFF_BaseCrowd::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AFF_BaseCrowd::BindToAgent(int32 NewAgentIndex)
{
	AgentIndex = NewAgentIndex;
}

void AFF_BaseCrowd::ReleaseFromAgent()
{
	AgentIndex = INDEX_NONE;
}

void AFF_BaseCrowd::PushPresentation(FVector NewPosition, FRotator NewRotator, UFF_CrowdAnimInstance* AnimInstance)
{
	UnitData.Position = NewPosition;
	UnitData.Rotation = NewRotator;
	
	
	
	SetActorLocation(NewPosition);
	SetActorRotation(NewRotator);
}

