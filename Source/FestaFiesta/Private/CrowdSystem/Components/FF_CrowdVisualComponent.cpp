// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Components/FF_CrowdVisualComponent.h"


// Sets default values for this component's properties
UFF_CrowdVisualComponent::UFF_CrowdVisualComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UFF_CrowdVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UFF_CrowdVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

