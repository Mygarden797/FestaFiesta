// Fill out your copyright notice in the Description page of Project Settings.



#include "CrowdSystem/FF_SpawnActor.h"
#include "Components/InstancedStaticMeshComponent.h"


class UFF_CrowdSimulationSubsystem;
// Sets default values
AFF_SpawnActor::AFF_SpawnActor()
{
	
	PrimaryActorTick.bCanEverTick = false;
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>("ISMComponent");
	
	
}

void AFF_SpawnActor::BeginPlay()
{
	Super::BeginPlay();
	
	
	
}

