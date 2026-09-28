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

	RefreshTickEnabled();
}

void AFF_FinaleViewpoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrifting)
	{
		DriftElapsed += DeltaSeconds;
		const float Alpha = DriftDuration > 0.f ? FMath::Clamp(DriftElapsed / DriftDuration, 0.f, 1.f) : 1.f;

		// 시작과 끝을 부드럽게 한다. 등속으로 움직이면 컷이 바뀌는 순간 덜컥 멈춘다.
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
		SetActorLocation(DriftHome + DriftDelta * Eased);

		if (Alpha >= 1.f)
		{
			bDrifting = false;
			RefreshTickEnabled();
		}
	}

	if (bTrackTargetEveryFrame && LookTarget.IsValid())
	{
		SetActorRotation(ResolveLookRotation());
	}
}

void AFF_FinaleViewpoint::BeginTourDrift(const float Seconds)
{
	if (!bHasDriftHome)
	{
		DriftHome = GetActorLocation();
		bHasDriftHome = true;
	}
	else
	{
		SetActorLocation(DriftHome);
	}

	// 제자리에서 다시 겨눈다. 대상을 추적하는 자리라면 옮긴 위치 기준으로 방향이 바뀐다.
	if (LookTarget.IsValid())
	{
		SetActorRotation(ResolveLookRotation());
	}

	DriftElapsed = 0.f;
	DriftDuration = FMath::Max(0.f, Seconds);
	bDrifting = !TourDriftOffset.IsNearlyZero() && DriftDuration > 0.f;

	// 방향은 여기서 한 번만 정한다. 대상을 매 프레임 겨누는 자리라도 이동 경로는 곧게 간다.
	DriftDelta = bDrifting ? GetActorRotation().RotateVector(TourDriftOffset) : FVector::ZeroVector;

	RefreshTickEnabled();
}

void AFF_FinaleViewpoint::RefreshTickEnabled()
{
	SetActorTickEnabled(bDrifting || bTrackTargetEveryFrame);
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

void AFF_FinaleViewpoint::CollectTour(const UWorld* World, TArray<AFF_FinaleViewpoint*>& OutViewpoints)
{
	OutViewpoints.Reset();

	if (!World)
	{
		return;
	}

	for (TActorIterator<AFF_FinaleViewpoint> It(const_cast<UWorld*>(World)); It; ++It)
	{
		AFF_FinaleViewpoint* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->bIncludeInTour)
		{
			continue;
		}
		OutViewpoints.Add(Candidate);
	}

	/* TourOrder 로 정렬한다. 같으면 이름으로 가른다.

	   같은 값일 때 기준이 없으면 액터를 훑는 순서를 타는데, 그건 레벨을 저장할
	   때마다 달라질 수 있다. 어제와 오늘의 연출이 달라지면 조율할 수가 없다.
	*/
	OutViewpoints.Sort([](const AFF_FinaleViewpoint& A, const AFF_FinaleViewpoint& B)
	{
		if (A.TourOrder != B.TourOrder)
		{
			return A.TourOrder < B.TourOrder;
		}
		return A.GetName() < B.GetName();
	});
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

		// 축제장 순회 자리는 땅을 본다. 불꽃을 보여줄 한 자리로는 쓰지 않는다.
		if (!IsValid(Candidate) || Candidate->TourAct == EFF_FinaleTourAct::SiteTour)
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
