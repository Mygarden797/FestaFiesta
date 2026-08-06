// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_BaseCrowd.h"

#include "CrowdSystem/Components/FF_CrowdMovementComponent.h"
#include "CrowdSystem/Components/FF_CrowdVisualComponent.h"


// Sets default values
AFF_BaseCrowd::AFF_BaseCrowd()
{
	
	PrimaryActorTick.bCanEverTick = false;
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SetRootComponent(SceneComponent);
	
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
	SkeletalMeshComponent->SetupAttachment(SceneComponent);
	
	CrowdMovementComponent = CreateDefaultSubobject<UFF_CrowdMovementComponent>("CrowdMovementComponent");
	CrowdVisualComponent = CreateDefaultSubobject<UFF_CrowdVisualComponent>("CrowdVisualComponent");
	
	

	
}

// Called when the game starts or when spawned
void AFF_BaseCrowd::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AFF_BaseCrowd::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AFF_BaseCrowd::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

