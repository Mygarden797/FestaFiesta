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

void AFF_FinaleLaunchSite::ComputeBargeOffsets(const int32 Count, const float SpacingCm, TArray<float>& OutOffsets)
{
	OutOffsets.Reset();
	const int32 SafeCount = FMath::Max(1, Count);
	const float Spacing = FMath::Max(0.f, SpacingCm);

	// 가운데를 0 에 둔다. 짝수 척이면 가운데 배가 없고 양쪽으로 반 칸씩 비킨다.
	const float Half = static_cast<float>(SafeCount - 1) * 0.5f;
	for (int32 Index = 0; Index < SafeCount; ++Index)
	{
		OutOffsets.Add((static_cast<float>(Index) - Half) * Spacing);
	}
}

void AFF_FinaleLaunchSite::GetLaunchPoints(TArray<FVector>& OutPoints) const
{
	TArray<float> Offsets;
	ComputeBargeOffsets(BargeCount, BargeSpacingCm, Offsets);

	const FVector Origin = GetActorLocation();
	// 수평으로만 편다. 액터가 기울어 있어도 배가 공중이나 물속으로 가지 않게 한다.
	const FVector Along = GetActorRightVector().GetSafeNormal2D();
	for (const float Offset : Offsets)
	{
		OutPoints.Add(Origin + Along * Offset);
	}
}

void AFF_FinaleLaunchSite::CollectLaunchPoints(const UWorld* World, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();

	TArray<AFF_FinaleLaunchSite*> Sites;
	CollectAll(World, Sites);
	for (const AFF_FinaleLaunchSite* Site : Sites)
	{
		Site->GetLaunchPoints(OutPoints);
	}
}
