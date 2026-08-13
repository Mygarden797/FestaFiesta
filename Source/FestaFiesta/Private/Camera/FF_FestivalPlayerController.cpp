// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_FestivalPlayerController.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Camera/FF_OverviewCameraRig.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AFF_FestivalPlayerController::AFF_FestivalPlayerController()
{
	// 관찰 시점이므로 커서를 기본으로 띄운다.
	// 단, 건축 세션이 열린 뒤에는 이 값을 건드리지 않는다. 건축 모드가 저장하고 되돌린다.
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AFF_FestivalPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			if (CameraMappingContext)
			{
				InputSubsystem->AddMappingContext(CameraMappingContext, CameraMappingPriority);
			}
			else
			{
				UE_LOG(LogFestaFiestaCamera, Warning,
					TEXT("[카메라] CameraMappingContext 가 비어 있어 카메라 입력이 동작하지 않는다. ")
					TEXT("PlayerController 블루프린트에서 IMC 와 InputAction 을 지정할 것."));
			}
		}
	}
}

void AFF_FestivalPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] EnhancedInputComponent 가 아니다. 프로젝트 입력 설정을 확인할 것."));
		return;
	}

	// 자산이 비어 있으면 그 액션만 건너뛴다. 하나 빠졌다고 나머지까지 죽으면 곤란하다.
	if (PanAction)
	{
		EnhancedInput->BindAction(PanAction, ETriggerEvent::Triggered, this, &AFF_FestivalPlayerController::HandlePan);
	}
	if (ZoomAction)
	{
		EnhancedInput->BindAction(ZoomAction, ETriggerEvent::Triggered, this, &AFF_FestivalPlayerController::HandleZoom);
	}
	if (RotateAction)
	{
		EnhancedInput->BindAction(RotateAction, ETriggerEvent::Triggered, this, &AFF_FestivalPlayerController::HandleRotate);
	}
	if (SnapRotateAction)
	{
		EnhancedInput->BindAction(SnapRotateAction, ETriggerEvent::Started, this, &AFF_FestivalPlayerController::HandleSnapRotate);
	}
	if (PickAction)
	{
		EnhancedInput->BindAction(PickAction, ETriggerEvent::Started, this, &AFF_FestivalPlayerController::HandlePick);
	}
	if (ResetViewAction)
	{
		EnhancedInput->BindAction(ResetViewAction, ETriggerEvent::Started, this, &AFF_FestivalPlayerController::HandleResetView);
	}
	if (ToggleViewAction)
	{
		EnhancedInput->BindAction(ToggleViewAction, ETriggerEvent::Started, this, &AFF_FestivalPlayerController::HandleToggleView);
	}
}

AFF_OverviewCameraPawn* AFF_FestivalPlayerController::GetOverviewCameraPawn() const
{
	return Cast<AFF_OverviewCameraPawn>(GetPawn());
}

bool AFF_FestivalPlayerController::IsBuildSessionActive() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 읽기 전용 조회다. 건축 모드 쪽 코드는 건드리지 않는다.
	// 매 입력마다 부르지만 enum 하나 읽는 것이라 비용이 없다.
	const UFF_BuildModeSubsystem* BuildMode = World->GetSubsystem<UFF_BuildModeSubsystem>();
	return BuildMode ? FFBuildModeModel::IsBuildSessionActive(BuildMode->GetBuildModeState()) : false;
}

EFF_CameraInputBlock AFF_FestivalPlayerController::GetInputBlockReason() const
{
	if (bModalUiActive)
	{
		return EFF_CameraInputBlock::ModalUi;
	}
	// 관찰 카메라가 화면에 안 보이는 동안 조작을 받으면, 돌아왔을 때 엉뚱한 데를 보고 있다.
	if (ViewMode == EFF_CameraViewMode::Actor)
	{
		return EFF_CameraInputBlock::ActorView;
	}
	if (IsBuildSessionActive())
	{
		return EFF_CameraInputBlock::BuildSession;
	}
	return EFF_CameraInputBlock::None;
}

bool AFF_FestivalPlayerController::IsCameraMovementAllowed() const
{
	// 건축 중에도 화면은 움직여야 한다. 부스를 놓을 자리를 봐야 하니까.
	// 모달 UI 와 액터 시점만 전체를 막는다.
	const EFF_CameraInputBlock Reason = GetInputBlockReason();
	return Reason != EFF_CameraInputBlock::ModalUi
		&& Reason != EFF_CameraInputBlock::ActorView;
}

bool AFF_FestivalPlayerController::IsPickingAllowed() const
{
	// 건축 중 클릭은 배치 입력이지 선택 입력이 아니다.
	return GetInputBlockReason() == EFF_CameraInputBlock::None;
}

void AFF_FestivalPlayerController::SetModalUiActive(bool bActive)
{
	bModalUiActive = bActive;
}

void AFF_FestivalPlayerController::HandlePan(const FInputActionValue& Value)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}
	if (AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn())
	{
		CameraPawn->AddPanInput(Value.Get<FVector2D>());
	}
}

void AFF_FestivalPlayerController::HandleZoom(const FInputActionValue& Value)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}

	AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	if (!CameraPawn)
	{
		return;
	}

	const float Steps = Value.Get<float>();

	// 커서 아래 지점을 못 구하면 중앙 기준으로 내려간다.
	// 커서가 창 밖에 있거나 하늘을 향하고 있을 때가 그렇다. 줌이 아예 안 먹는 것보다는 낫다.
	FVector Anchor = FVector::ZeroVector;
	if (ResolveCursorWorldAnchor(Anchor))
	{
		CameraPawn->AddZoomInputAtAnchor(Steps, Anchor);
	}
	else
	{
		CameraPawn->AddZoomInput(Steps);
	}
}

bool AFF_FestivalPlayerController::ResolveCursorWorldAnchor(FVector& OutAnchor) const
{
	const AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	if (!CameraPawn)
	{
		return false;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return false;
	}

	return FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(
		WorldOrigin, WorldDirection, CameraPawn->GetFocusPlaneZ(), OutAnchor);
}

void AFF_FestivalPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	EnsureViewTargetAlive();
	TickEdgeScroll(DeltaTime);
}

bool AFF_FestivalPlayerController::SwitchToActorView(AActor* TargetActor, float BlendTime)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogFestaFiestaCamera, Warning, TEXT("[카메라] 유효하지 않은 대상으로 시점 전환을 시도했다."));
		return false;
	}

	// 관찰 카메라를 그대로 두고 뷰타깃만 옮긴다. Possess 를 쓰면 돌아왔을 때 구도를 잃는다.
	SetViewTargetWithBlend(TargetActor, FMath::Max(0.f, BlendTime));

	ViewMode = EFF_CameraViewMode::Actor;
	ViewedActor = TargetActor;
	LastViewedActor = TargetActor;

	OnViewModeChanged.Broadcast(ViewMode, TargetActor);
	return true;
}

void AFF_FestivalPlayerController::SwitchToOverview(float BlendTime)
{
	AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	if (!CameraPawn)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 관찰 카메라 Pawn 이 없어 시점을 되돌리지 못했다."));
		return;
	}

	// 떠날 때 구도가 그대로 남아 있다. 리그 상태를 건드리지 않았기 때문이다.
	SetViewTargetWithBlend(CameraPawn, FMath::Max(0.f, BlendTime));

	ViewMode = EFF_CameraViewMode::Overview;
	ViewedActor = nullptr;

	OnViewModeChanged.Broadcast(ViewMode, nullptr);
}

void AFF_FestivalPlayerController::ToggleView(float BlendTime)
{
	if (ViewMode == EFF_CameraViewMode::Actor)
	{
		SwitchToOverview(BlendTime);
		return;
	}

	// 되돌아갈 대상이 이미 사라졌으면 관찰 시점에 머문다.
	if (AActor* Target = LastViewedActor.Get())
	{
		SwitchToActorView(Target, BlendTime);
	}
}

void AFF_FestivalPlayerController::EnsureViewTargetAlive()
{
	if (ViewMode != EFF_CameraViewMode::Actor)
	{
		return;
	}

	// 보던 NPC 가 퇴장하거나 시설이 철거되면 뷰타깃이 사라진다.
	// 그대로 두면 화면이 원점을 비추고 입력도 계속 막혀 있어서 조작 불능처럼 보인다.
	if (!ViewedActor.IsValid())
	{
		UE_LOG(LogFestaFiestaCamera, Log,
			TEXT("[카메라] 보고 있던 대상이 사라져 관찰 시점으로 되돌린다."));
		SwitchToOverview(0.f);
	}
}

void AFF_FestivalPlayerController::TickEdgeScroll(float DeltaTime)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}

	AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	if (!CameraPawn || !CameraPawn->CameraSettings.bEnableEdgeScroll)
	{
		return;
	}

	// 커서가 숨겨져 있으면 가장자리 개념이 없다.
	if (!bShowMouseCursor)
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!GetMousePosition(MouseX, MouseY))
	{
		return;
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	GetViewportSize(ViewportX, ViewportY);

	const FVector2D EdgeInput = FFF_OverviewCameraRig::ComputeEdgeScrollInput(
		FVector2D(MouseX, MouseY),
		FVector2D(static_cast<float>(ViewportX), static_cast<float>(ViewportY)),
		CameraPawn->CameraSettings.EdgeScrollMarginPixels);

	if (EdgeInput.IsNearlyZero())
	{
		return;
	}

	// 배율을 여기서 곱하지 않는다. 리그가 EdgeScroll 전용 속도로 처리한다.
	CameraPawn->AddEdgeScrollInput(EdgeInput);
}

void AFF_FestivalPlayerController::HandleRotate(const FInputActionValue& Value)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}
	if (AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn())
	{
		CameraPawn->AddRotateInput(Value.Get<float>());
	}
}

void AFF_FestivalPlayerController::HandleSnapRotate(const FInputActionValue& Value)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}
	const float Direction = Value.Get<float>();
	if (FMath::IsNearlyZero(Direction))
	{
		return;
	}
	if (AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn())
	{
		CameraPawn->SnapRotate(Direction > 0.f ? 1 : -1);
	}
}

void AFF_FestivalPlayerController::HandleResetView(const FInputActionValue& Value)
{
	if (!IsCameraMovementAllowed())
	{
		return;
	}
	if (AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn())
	{
		CameraPawn->ResetView();
	}
}

void AFF_FestivalPlayerController::HandleToggleView(const FInputActionValue& Value)
{
	// 여기서는 IsCameraMovementAllowed 를 보지 않는다.
	// 액터 시점일 때 관찰 카메라 조작이 막혀 있는데, 그 판정을 그대로 쓰면
	// 액터 시점으로 들어간 뒤 되돌아 나올 수가 없다.
	// 다만 모달 UI 가 떠 있는 동안에는 화면이 바뀌면 안 된다.
	if (GetInputBlockReason() == EFF_CameraInputBlock::ModalUi)
	{
		return;
	}

	ToggleView(ViewSwitchBlendTime);
}

void AFF_FestivalPlayerController::HandlePick(const FInputActionValue& Value)
{
	if (!IsPickingAllowed())
	{
		return;
	}

	FVector WorldOrigin = FVector::ZeroVector;
	FVector WorldDirection = FVector::ZeroVector;
	if (!DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
	{
		return;
	}

	// 단순 충돌만 본다. 복합 충돌은 700 에이전트 환경에서 비싸다.
	constexpr bool bTraceComplex = false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FF_CameraPick), bTraceComplex);
	Params.AddIgnoredActor(GetPawn());

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		Hit, WorldOrigin, WorldOrigin + WorldDirection * PickTraceDistance, ECC_Visibility, Params);

	// 빈 곳을 클릭하면 선택 해제로 본다. nullptr 을 그대로 흘려보낸다.
	OnActorPicked.Broadcast(bHit ? Hit.GetActor() : nullptr);
}
