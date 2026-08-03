// Fill out your copyright notice in the Description page of Project Settings.


#include "FestaFiesta/Public/Crowd/FF_SpawnActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "FestaFiesta/Public/Systems/FF_CrowdSimulationSubsystem.h"


// Sets default values
AFF_SpawnActor::AFF_SpawnActor()
{
	
	PrimaryActorTick.bCanEverTick = false;
	ISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>("ISMComponent");
	
	
}

void AFF_SpawnActor::BeginPlay()
{
	Super::BeginPlay();
	
	GetWorld()->GetSubsystem<UFF_CrowdSimulationSubsystem>()->SpawnUnits(UnitCount,ISMComponent);
	
}

