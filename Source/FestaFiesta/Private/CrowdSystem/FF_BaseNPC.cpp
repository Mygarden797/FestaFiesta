// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_BaseNPC.h"

#include "Elements/Framework/TypedElementSorter.h"


// Sets default values
AFF_BaseNPC::AFF_BaseNPC()
{
	
	PrimaryActorTick.bCanEverTick = true;
	
	SceneComponent = CreateDefaultSubobject<USceneComponent>("SceneComponent");
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>("SkeletalMeshComponent");
}


void AFF_BaseNPC::BeginPlay()
{
	Super::BeginPlay();
	
}


void AFF_BaseNPC::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AFF_BaseNPC::MoveToTargetLocation(FVector TargetLocation, float DeltaTime)
{
	FVector ToTarget = TargetLocation - GetActorLocation();
	const float Distance = ToTarget.Size();
	
	const float Speed = FMath::Clamp(Distance, 100, 500);
	
	const FVector NewLocation = GetActorLocation() + DeltaTime* Speed;
	
	SetActorLocation(NewLocation);
	
}



