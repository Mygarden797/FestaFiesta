// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Components/FF_CrowdMovementComponent.h"


// Sets default values for this component's properties
UFF_CrowdMovementComponent::UFF_CrowdMovementComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UFF_CrowdMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UFF_CrowdMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

