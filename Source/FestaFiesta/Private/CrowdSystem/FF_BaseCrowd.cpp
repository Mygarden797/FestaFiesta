// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_BaseCrowd.h"

#include "CrowdSystem/Components/FF_CrowdAnimInstance.h"




AFF_BaseCrowd::AFF_BaseCrowd()
{
	
	PrimaryActorTick.bCanEverTick = false;
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SetRootComponent(SkeletalMeshComponent);
	CrowdAnimInstance = Cast<UFF_CrowdAnimInstance>(SkeletalMeshComponent->GetAnimInstance());
	if (!IsValid(CrowdAnimInstance))
	{
		UE_LOG(LogTemp,Warning,TEXT("[AFF_BaseCrowd] SkeletalMeshComponent's AnimInstance is not CrowdAnimInstance"));
		return;
	}
}


void AFF_BaseCrowd::BeginPlay()
{
	Super::BeginPlay();
	
}




void AFF_BaseCrowd::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AFF_BaseCrowd::BindToAgent(int32 NewAgentIndex, FVector SpawnLocation, FRotator SpawnRotation)
{
	AgentIndex = NewAgentIndex;
	SetActorLocationAndRotation(SpawnLocation,SpawnRotation);
}

void AFF_BaseCrowd::ReleaseFromAgent()
{
	AgentIndex = INDEX_NONE;
}

void AFF_BaseCrowd::PushPresentation(FVector NewPosition, FRotator NewRotator, float NewSpeed)
{
	SetActorLocationAndRotation(NewPosition,NewRotator,false);
	
	if (!CrowdAnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("CrowdAnimInstance is inValid"));
		return;
		
	}
	
	CrowdAnimInstance->SetSpeed(NewSpeed);
}


