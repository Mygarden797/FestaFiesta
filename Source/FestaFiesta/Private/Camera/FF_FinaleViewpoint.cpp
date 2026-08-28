// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_FinaleViewpoint.h"

#include "Camera/CameraComponent.h"
#include "Camera/FF_CameraTypes.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"

AFF_FinaleViewpoint::AFF_FinaleViewpoint()
{
	// 대상을 추적할 때만 돈다. 고정 연출에서는 BeginPlay 에서 한 번 겨누고 끝이다.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
	ViewCamera->SetupAttachment(SceneRoot);
	ViewCamera->bUsePawnControlRotation = false;
}

void AFF_FinaleViewpoint::BeginPlay()
{
	Super::BeginPlay();

	if (LookTargetActor)
	{
		SetLookTarget(LookTargetActor);
	}

	// 대상이 있으면 시작할 때 한 번 겨눈다. 레벨에서 회전을 대충 맞춰놔도 구도가 잡힌다.
	if (LookTarget.IsValid())
	{
		SetActorRotation(ResolveLookRotation());
	}

	SetActorTickEnabled(bTrackTargetEveryFrame);
}

void AFF_FinaleViewpoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bTrackTargetEveryFrame || !LookTarget.IsValid())
	{
		return;
	}
	SetActorRotation(ResolveLookRotation());
}

void AFF_FinaleViewpoint::SetLookTarget(AActor* NewTarget)
{
	LookTarget = NewTarget;

	// 대상이 바뀌면 즉시 겨눈다. 다음 틱까지 기다리면 한 프레임 엉뚱한 데를 본다.
	if (LookTarget.IsValid())
	{
		SetActorRotation(ResolveLookRotation());
	}
}

FRotator AFF_FinaleViewpoint::ResolveLookRotation() const
{
	const AActor* Target = LookTarget.Get();
	if (!Target)
	{
		// 대상이 없으면 레벨에서 잡아둔 회전을 존중한다.
		return GetActorRotation();
	}

	// 발사대 발밑을 겨누면 화면 아래를 보게 된다. 불꽃이 터지는 높이를 겨눈다.
	const FVector AimPoint = Target->GetActorLocation() + FVector(0.f, 0.f, LookAtHeightOffset);
	return UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), AimPoint);
}

AFF_FinaleViewpoint* AFF_FinaleViewpoint::FindBest(const UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	AFF_FinaleViewpoint* Best = nullptr;
	for (TActorIterator<AFF_FinaleViewpoint> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AFF_FinaleViewpoint* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}
		if (!Best || Candidate->Priority > Best->Priority)
		{
			Best = Candidate;
		}
	}

	if (!Best)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[피날레] 레벨에 AFF_FinaleViewpoint 가 없다. 연출 시점 전환을 건너뛴다."));
	}
	return Best;
}
