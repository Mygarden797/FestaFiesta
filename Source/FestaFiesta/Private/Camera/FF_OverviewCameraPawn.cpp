// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_OverviewCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/SpringArmComponent.h"

AFF_OverviewCameraPawn::AFF_OverviewCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	// 카메라가 시설이나 지형에 걸리면 배치 작업 자체가 불가능해진다.
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(SceneRoot);
	CameraBoom->TargetArmLength = 8000.f;
	CameraBoom->bDoCollisionTest = false;          // 지형에 카메라가 당겨지면 구도가 튄다
	CameraBoom->bUsePawnControlRotation = false;   // 회전은 리그가 소유한다
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = false;          // 보간은 리그가 하므로 이중으로 걸지 않는다
	CameraBoom->bEnableCameraRotationLag = false;

	OverviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
	OverviewCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	OverviewCamera->bUsePawnControlRotation = false;

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

void AFF_OverviewCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	// 레벨 크기를 아는 쪽이 InitializeOverview 를 불러주는 게 정상 경로다.
	// 안 불러도 현재 위치 기준으로 동작은 하게 둔다.
	if (!bInitialized)
	{
		InitializeOverview(GetActorLocation(), CameraBoom ? CameraBoom->TargetArmLength : 8000.f);
	}
}

void AFF_OverviewCameraPawn::InitializeOverview(const FVector& FocusLocation, float SuggestedArmLength)
{
	Rig.Initialize(CameraSettings, FocusLocation, SuggestedArmLength);
	bInitialized = true;
	ApplyRigToComponents();
}

void AFF_OverviewCameraPawn::AddPanInput(const FVector2D& ScreenInput)
{
	// 추적 중에 플레이어가 화면을 밀면 추적을 놓아준다.
	// 안 그러면 밀어도 카메라가 계속 끌려가서 입력이 씹히는 것처럼 느껴진다.
	if (!ScreenInput.IsNearlyZero() && FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}
	PendingPanInput += ScreenInput;
}

void AFF_OverviewCameraPawn::AddEdgeScrollInput(const FVector2D& ScreenInput)
{
	if (!ScreenInput.IsNearlyZero() && FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}
	PendingEdgeScrollInput += ScreenInput;
}

void AFF_OverviewCameraPawn::AddZoomInput(float Steps)
{
	Rig.Zoom(Steps);
}

void AFF_OverviewCameraPawn::AddZoomInputAtAnchor(float Steps, const FVector& WorldAnchor)
{
	if (!CameraSettings.bZoomToCursor)
	{
		Rig.Zoom(Steps);
		return;
	}

	// 추적 중에 커서 쪽으로 줌하면 초점이 대상에서 떨어져 나간다.
	// 그 상태로 두면 다음 프레임에 추적이 도로 끌어당겨서 화면이 떨린다. 추적을 놓아준다.
	if (FollowTarget.IsValid())
	{
		SetFollowTarget(nullptr);
	}

	Rig.ZoomTowards(Steps, WorldAnchor);
}

float AFF_OverviewCameraPawn::GetFocusPlaneZ() const
{
	return Rig.GetDisplayState().FocusLocation.Z;
}

void AFF_OverviewCameraPawn::AddRotateInput(float Direction)
{
	PendingRotateInput += Direction;
}

void AFF_OverviewCameraPawn::SnapRotate(int32 Direction)
{
	Rig.SnapRotate(Direction);
}

void AFF_OverviewCameraPawn::FocusOnLocation(const FVector& WorldLocation)
{
	Rig.FocusOn(WorldLocation);
}

void AFF_OverviewCameraPawn::SetFollowTarget(AActor* NewTarget)
{
	FollowTarget = NewTarget;
}

void AFF_OverviewCameraPawn::ResetView()
{
	SetFollowTarget(nullptr);
	Rig.ResetToInitial();
}

bool AFF_OverviewCameraPawn::HasActiveCamera() const
{
	return OverviewCamera != nullptr && OverviewCamera->IsActive();
}

void AFF_OverviewCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (PendingRotateInput != 0.f)
	{
		Rig.Rotate(PendingRotateInput, DeltaSeconds);
		PendingRotateInput = 0.f;
	}

	if (!PendingPanInput.IsNearlyZero())
	{
		Rig.Pan(PendingPanInput, DeltaSeconds);
		PendingPanInput = FVector2D::ZeroVector;
	}

	if (!PendingEdgeScrollInput.IsNearlyZero())
	{
		Rig.EdgeScroll(PendingEdgeScrollInput, DeltaSeconds);
		PendingEdgeScrollInput = FVector2D::ZeroVector;
	}

	// 추적은 팬보다 뒤에 둔다. 같은 프레임에 둘 다 들어오면 추적이 이긴다.
	if (const AActor* Target = FollowTarget.Get())
	{
		Rig.FocusOn(Target->GetActorLocation());
	}

	// 팬과 추적이 초점을 옮긴 뒤에 지면을 잡는다. 순서를 바꾸면 한 프레임 늦은 위치의
	// 지면을 따라가서 경사에서 카메라가 뒤처진다.
	UpdateGroundHeight(DeltaSeconds);

	Rig.AdvanceInterpolation(DeltaSeconds);
	ApplyRigToComponents();
}

bool AFF_OverviewCameraPawn::TraceGroundAt(const FVector2D& Where, float AroundZ, float& OutGroundZ) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float HalfHeight = FMath::Max(100.f, CameraSettings.GroundTraceHalfHeight);

	const FVector Start(Where.X, Where.Y, AroundZ + HalfHeight);
	const FVector End(Where.X, Where.Y, AroundZ - HalfHeight);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FF_CameraGround), false);
	QueryParams.AddIgnoredActor(this);

	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Start, End, CameraSettings.GroundTraceChannel, QueryParams))
	{
		return false;
	}

	OutGroundZ = static_cast<float>(Hit.ImpactPoint.Z);
	return true;
}

void AFF_OverviewCameraPawn::UpdateGroundHeight(const float DeltaSeconds)
{
	if (!CameraSettings.bFollowGroundHeight)
	{
		return;
	}

	// 초점의 수평 위치를 기준으로 위아래를 훑는다. 목표 위치를 쓰는 이유는
	// 표시 위치를 쓰면 보간 중인 값을 따라가면서 서로를 쫓는 진동이 생기기 때문이다.
	const FVector Focus = Rig.GetTargetState().FocusLocation;
	const FVector2D Center(Focus.X, Focus.Y);

	// 중심은 반드시 있어야 한다. 여기서 못 찾으면 주변을 재봐야 기준이 없다.
	float CenterZ = 0.f;
	if (!TraceGroundAt(Center, static_cast<float>(Focus.Z), CenterZ))
	{
		// 절벽, 물 위, 콜리전이 없는 구역. 마지막 높이를 유지한다.
		Rig.OnGroundHeightUnavailable();
		return;
	}

	double Sum = CenterZ;
	int32 Count = 1;

	// 주변을 같이 재서 평균을 낸다.
	//
	// 한 점만 보면 벽 끝이나 물가를 지날 때 높이가 한 프레임에 뚝 끊긴다.
	// 그 순간 카메라가 방지턱을 넘는 것처럼 출렁인다.
	// 주변을 섞으면 경계를 지나는 동안 값이 서서히 이어진다.
	const int32 SampleCount = FMath::Clamp(CameraSettings.GroundSampleCount, 0, 8);
	if (SampleCount > 0)
	{
		const float Radius = FMath::Max(10.f, CameraSettings.GroundSampleRadius);
		const float MaxDeviation = CameraSettings.GroundSampleMaxDeviation;

		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const float Angle = 2.f * PI * static_cast<float>(Index) / static_cast<float>(SampleCount);
			const FVector2D Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius);

			float SampleZ = 0.f;
			if (!TraceGroundAt(Center + Offset, static_cast<float>(Focus.Z), SampleZ))
			{
				// 이 방향은 허공이다. 평균에서 빼면 나머지가 그대로 반영된다.
				continue;
			}

			// 구멍이나 절벽 아래를 찍은 표본이 섞이면 평균이 끌려 내려간다.
			// 중심에서 너무 먼 값은 지형이 아니라 다른 층으로 본다.
			if (MaxDeviation > 0.f && FMath::Abs(SampleZ - CenterZ) > MaxDeviation)
			{
				continue;
			}

			Sum += SampleZ;
			++Count;
		}
	}

	Rig.ApplyGroundHeight(static_cast<float>(Sum / Count), DeltaSeconds);
}

void AFF_OverviewCameraPawn::ApplyRigToComponents()
{
	const FFF_OverviewCameraState& State = Rig.GetDisplayState();

	SetActorLocation(State.FocusLocation);

	if (CameraBoom)
	{
		CameraBoom->TargetArmLength = State.ArmLength;
		// 스프링암은 자기 회전의 반대편에 카메라를 매단다.
		// Pitch 가 음수면 카메라가 위에서 아래를 내려다본다.
		CameraBoom->SetRelativeRotation(FRotator(State.Pitch, State.Yaw, 0.f));
	}
}
