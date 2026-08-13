// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_OverviewCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
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

	Rig.AdvanceInterpolation(DeltaSeconds);
	ApplyRigToComponents();
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
