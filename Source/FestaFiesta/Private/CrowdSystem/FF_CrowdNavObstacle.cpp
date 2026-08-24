// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/FF_CrowdNavObstacle.h"

#include "Components/BoxComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"

AFF_CrowdNavObstacle::AFF_CrowdNavObstacle()
{
	PrimaryActorTick.bCanEverTick = false;

	ObstacleBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ObstacleBox"));
	SetRootComponent(ObstacleBox);

	ObstacleBox->SetBoxExtent(FVector::ZeroVector);
	ObstacleBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ObstacleBox->SetGenerateOverlapEvents(false);
	ObstacleBox->SetSimulatePhysics(false);
	ObstacleBox->SetEnableGravity(false);
	ObstacleBox->SetHiddenInGame(true);

	ObstacleBox->SetCanEverAffectNavigation(true);
	ObstacleBox->bDynamicObstacle = true;
	ObstacleBox->SetAreaClassOverride(UNavArea_Null::StaticClass());
}

void AFF_CrowdNavObstacle::ConfigureForFacility(const FVector& CentreCm, float YawDegrees, const FVector& HalfExtentCm)
{
	if (!ObstacleBox)
	{
		return;
	}

	SetActorLocationAndRotation(
		CentreCm + FVector(0.0, 0.0, HalfExtentCm.Z),
		FRotator(0.0, YawDegrees, 0.0));

	ObstacleBox->SetBoxExtent(HalfExtentCm);

	FNavigationSystem::UpdateComponentData(*ObstacleBox);
}
