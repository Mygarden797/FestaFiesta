// Copyright Epic Games, Inc. All Rights Reserved.

#include "Character/FF_RiverRespawnPlane.h"

#include "Character/FF_FestivalCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

AFF_RiverRespawnPlane::AFF_RiverRespawnPlane()
{
	PrimaryActorTick.bCanEverTick = false;

	Region = CreateDefaultSubobject<UBoxComponent>(TEXT("Region"));
	SetRootComponent(Region);
	Region->SetBoxExtent(FVector(5000.f, 5000.f, 10.f));
	Region->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Region->SetGenerateOverlapEvents(false);
	Region->ShapeColor = FColor::Cyan;
}

void AFF_RiverRespawnPlane::BeginPlay()
{
	Super::BeginPlay();

	GetWorldTimerManager().SetTimer(
		CheckTimer, this, &AFF_RiverRespawnPlane::CheckCharacter, CheckIntervalSeconds, true);
}

void AFF_RiverRespawnPlane::CheckCharacter()
{
	// 조종 중인 캐릭터만 본다. 관찰 시점에서는 폰이 카메라라서 캐스트가 실패해 그냥 넘어간다.
	const APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	AFF_FestivalCharacter* Character = Controller ? Cast<AFF_FestivalCharacter>(Controller->GetPawn()) : nullptr;
	if (!IsValid(Character))
	{
		return;
	}

	const FVector Location = Character->GetActorLocation();
	const float FeetZ = Location.Z - Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	if (FeetZ < GetActorLocation().Z - TriggerDepthCm && IsInsideRegion(Location))
	{
		Character->RestoreToLastSafeLocation();
	}
}

bool AFF_RiverRespawnPlane::IsInsideRegion(const FVector& WorldLocation) const
{
	const FVector Local = Region->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Region->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y;
}
