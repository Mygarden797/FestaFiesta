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
	if (IsBuildSessionActive())
	{
		return EFF_CameraInputBlock::BuildSession;
	}
	return EFF_CameraInputBlock::None;
}

bool AFF_FestivalPlayerController::IsCameraMovementAllowed() const
{
	// 건축 중에도 화면은 움직여야 한다. 부스를 놓을 자리를 봐야 하니까.
	// 모달 UI 만 전체를 막는다.
	return GetInputBlockReason() != EFF_CameraInputBlock::ModalUi;
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
	TickEdgeScroll(DeltaTime);
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
