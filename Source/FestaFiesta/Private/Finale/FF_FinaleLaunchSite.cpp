// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleLaunchSite.h"

#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AFF_FinaleLaunchSite::AFF_FinaleLaunchSite()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AFF_FinaleLaunchSite::CollectAll(const UWorld* World, TArray<AFF_FinaleLaunchSite*>& OutSites)
{
	OutSites.Reset();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AFF_FinaleLaunchSite> It(const_cast<UWorld*>(World)); It; ++It)
	{
		if (IsValid(*It))
		{
			OutSites.Add(*It);
		}
	}

	// Order 가 같으면 이름으로 가른다. 순서가 실행마다 달라지면
	// 같은 시드로 돌려도 다른 그림이 나온다.
	OutSites.Sort([](const AFF_FinaleLaunchSite& A, const AFF_FinaleLaunchSite& B)
	{
		if (A.Order != B.Order)
		{
			return A.Order < B.Order;
		}
		return A.GetName() < B.GetName();
	});
}
