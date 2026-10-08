// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/FF_FestivalPlayerController.h"

#include "Character/FF_FestivalCharacter.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildToolbarInput.h"
#include "InputKeyEventArgs.h"
#include "Building/Testing/FF_ZoneLifecycleTestPanelSubsystem.h"
#include "Camera/FF_FinaleViewpoint.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Camera/FF_OverviewCameraRig.h"
#include "Finale/FF_FinaleShowSubsystem.h"
#include "CollisionQueryParams.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"

namespace
{
	bool IsFacilityLightingInputCaptured(const UWorld* World)
	{
		const UFF_FacilityInfoSubsystem* Info=World?World->GetSubsystem<UFF_FacilityInfoSubsystem>():nullptr;
		if(!Info) return false;
		return Info->IsLightingTextInputFocused() || (Info->IsPointerOverPanel() &&
			FSlateApplication::IsInitialized() && FSlateApplication::Get().HasAnyMouseCaptor());
	}
	bool IsPointerOverFacilityUI(const UWorld* World)
	{
		const UFF_FacilityInfoSubsystem* Info=World?World->GetSubsystem<UFF_FacilityInfoSubsystem>():nullptr;
		return Info && Info->IsPointerOverPanel();
	}
}

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

	// 시작은 관찰 카메라다. 생성자에서 커서만 켜두면 입력 모드가 안 걸린다.
	ApplyViewInputMode(ViewMode);

	// 시작 시점의 빙의 대상이 관찰 카메라다. 이후 캐릭터로 빙의가 옮겨가도 이 참조는 유지된다.
	OverviewPawn = ResolveOverviewPawn();
	if (!OverviewPawn.IsValid())
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 시작 Pawn 이 관찰 카메라가 아니다. GameMode 의 DefaultPawnClass 를 확인할 것."));
	}

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

bool AFF_FestivalPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const auto* Build = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	if (Build && Build->GetBuildModeState() != EFF_BuildModeState::Closed &&
		FFBuildToolbarInput::IsRetiredShortcut(Params.Key))
	{
		// Consume before PlayerInput, including the engine's F-key debug bindings.
		return true;
	}
	return Super::InputKey(Params);
}

void AFF_FestivalPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();
	// 창 포커스를 잃거나 입력 모드를 바꿀 때 키만 비우면 이미 쌓인 카메라 입력이 남는다.
	StopOverviewCameraMotion();
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
	// GetPawn() 을 쓰면 캐릭터에 빙의한 순간 null 이 되어 관찰 카메라를 잃는다.
	if (AFF_OverviewCameraPawn* Cached = OverviewPawn.Get())
	{
		return Cached;
	}
	return ResolveOverviewPawn();
}

AFF_OverviewCameraPawn* AFF_FestivalPlayerController::ResolveOverviewPawn() const
{
	// 지금 빙의 중이면 그게 맞다.
	if (AFF_OverviewCameraPawn* Current = Cast<AFF_OverviewCameraPawn>(GetPawn()))
	{
		return Current;
	}

	// 캐시가 낡은 경우. Zone 플레이테스트처럼 다른 시스템이 컨트롤러를 자기 Pawn 으로
	// 빙의시키면 BeginPlay 에서 잡은 참조가 파괴되거나 무의미해질 수 있다.
	// 그 상태로 두면 관찰 시점으로 돌아갈 방법이 없어져 조작 불능처럼 보인다.
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AFF_OverviewCameraPawn> It(const_cast<UWorld*>(World)); It; ++It)
		{
			if (IsValid(*It))
			{
				return *It;
			}
		}
	}
	return nullptr;
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

bool AFF_FestivalPlayerController::IsBuildPreviewActive() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const UFF_BuildModeSubsystem* BuildMode = World->GetSubsystem<UFF_BuildModeSubsystem>();
	return BuildMode ? FFBuildModeModel::IsPreviewActive(BuildMode->GetBuildModeState()) : false;
}

EFF_CameraInputBlock AFF_FestivalPlayerController::GetInputBlockReason() const
{
	// 연출이 모달 UI 보다 먼저다. 연출 중에 창이 떠도 화면은 연출이 소유한다.
	if (bScriptedViewActive)
	{
		return EFF_CameraInputBlock::Scripted;
	}
	if (bModalUiActive)
	{
		return EFF_CameraInputBlock::ModalUi;
	}
	if (!IsCameraViewportFocused())
	{
		return EFF_CameraInputBlock::InactiveViewport;
	}
	// 관찰 카메라가 화면에 안 보이는 동안 조작을 받으면, 돌아왔을 때 엉뚱한 데를 보고 있다.
	if (ViewMode == EFF_CameraViewMode::Actor)
	{
		return EFF_CameraInputBlock::ActorView;
	}
	// 캐릭터를 조종하는 동안에도 같다. 다만 빙의가 옮겨가 있어서 입력 자체가 캐릭터로 가므로
	// 실제로 겹칠 일은 적다. 그래도 명시적으로 막아둔다.
	if (ViewMode == EFF_CameraViewMode::Character)
	{
		return EFF_CameraInputBlock::CharacterControl;
	}
	if (IsBuildSessionActive())
	{
		return EFF_CameraInputBlock::BuildSession;
	}
	return EFF_CameraInputBlock::None;
}

bool AFF_FestivalPlayerController::IsCameraViewportFocused() const
{
	if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive())
	{
		return false;
	}

	const ULocalPlayer* LocalPlayer = GetLocalPlayer();
	const FViewport* Viewport = LocalPlayer && LocalPlayer->ViewportClient
		? LocalPlayer->ViewportClient->Viewport : nullptr;
	// 에디터 자체가 활성 상태여도 다른 패널/창을 조작 중이면 게임 카메라는 멈춘다.
	return Viewport && Viewport->HasFocus() && Viewport->IsForegroundWindow();
}

void AFF_FestivalPlayerController::StopOverviewCameraMotion()
{
	if (AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn())
	{
		CameraPawn->StopCameraMotion();
	}
}

bool AFF_FestivalPlayerController::IsCameraMovementAllowed() const
{
	// 건축 중에도 화면은 움직여야 한다. 부스를 놓을 자리를 봐야 하니까.
	return !IsFacilityLightingInputCaptured(GetWorld()) && FFCameraInputModel::IsMovementAllowed(GetInputBlockReason());
}

bool AFF_FestivalPlayerController::IsCameraRotationAllowed() const
{
	return !IsFacilityLightingInputCaptured(GetWorld()) && FFCameraInputModel::IsRotationAllowed(
		GetInputBlockReason(),
		IsBuildPreviewActive());
}

bool AFF_FestivalPlayerController::IsPickingAllowed() const
{
	// 캐릭터 조종은 관찰 카메라 이동만 막는다. 좌클릭 시설 선택까지 막으면
	// 그라운드 모드에서 시설 정보와 미니게임 진입 경로를 사용할 수 없다.
	// 건축 세션은 CharacterControl보다 뒤에서 판정되므로 별도 인자로 반드시 보존한다.
	return FFCameraInputModel::IsPickingAllowed(
		GetInputBlockReason(),
		IsBuildSessionActive());
}

bool AFF_FestivalPlayerController::EnterScriptedView(AFF_FinaleViewpoint* Viewpoint, float BlendTime)
{
	// 지정이 없으면 레벨에서 찾는다. 호출부가 시점 액터를 몰라도 되게 한다.
	if (!IsValid(Viewpoint))
	{
		Viewpoint = AFF_FinaleViewpoint::FindBest(GetWorld());
	}
	if (!IsValid(Viewpoint))
	{
		return false;
	}

	// 빙의는 옮기지 않는다. 관찰 카메라가 계속 조종 대상으로 남아 있어야
	// 연출이 끝났을 때 리그 상태를 그대로 이어받는다.
	StopOverviewCameraMotion();
	SetViewTargetWithBlend(Viewpoint, FMath::Max(0.f, BlendTime));
	bScriptedViewActive = true;

	OnViewModeChanged.Broadcast(ViewMode, Viewpoint);
	return true;
}

void AFF_FestivalPlayerController::ExitScriptedView(float BlendTime)
{
	if (!bScriptedViewActive)
	{
		return;
	}

	bScriptedViewActive = false;
	StopOverviewCameraMotion();

	// 연출 전에 무엇을 보고 있었는지에 따라 돌아갈 곳이 다르다.
	if (ViewMode == EFF_CameraViewMode::Overview)
	{
		SwitchToOverview(BlendTime);
		return;
	}

	if (AActor* Previous = ViewedActor.Get())
	{
		SetViewTargetWithBlend(Previous, FMath::Max(0.f, BlendTime));
		OnViewModeChanged.Broadcast(ViewMode, Previous);
		return;
	}

	// 보던 대상이 사라졌으면 관찰 시점이 안전하다.
	SwitchToOverview(BlendTime);
}

void AFF_FestivalPlayerController::SetModalUiActive(bool bActive)
{
	bModalUiActive = bActive;
	if (bActive)
	{
		StopOverviewCameraMotion();
	}
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
	if (!IsCameraMovementAllowed() || IsPointerOverFacilityUI(GetWorld()))
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
	bPickRequestHandledThisTick = false;
	Super::PlayerTick(DeltaTime);

	// CommonUI input mode나 동적으로 교체된 IMC가 IA_Pick을 소비해도 기본 포인터 클릭은
	// 살아 있어야 한다. BuildMode가 이미 사용하는 raw key edge를 보조 경로로 읽되,
	// Enhanced Input action이 정상 실행된 프레임에는 ProcessPickRequest가 중복을 막는다.
	if (FFCameraInputModel::ShouldUsePrimaryPointerPickFallback(
			bPickRequestHandledThisTick,
			WasInputKeyJustPressed(EKeys::LeftMouseButton)))
	{
		ProcessPickRequest();
	}
	EnsureViewTargetAlive();
	if (IsCameraMovementAllowed())
	{
		TickEdgeScroll(DeltaTime);
	}
	else
	{
		// 차단되기 직전의 팬 입력, 줌 전환, 명시적 추적도 숨은 상태로 진행시키지 않는다.
		StopOverviewCameraMotion();
	}

	// 끌기 이동은 막힘을 스스로 본다(위와 같은 IsCameraMovementAllowed). 멈춘 카메라를 다시 움직이지 않는다.
	TickDragPan();
}

bool AFF_FestivalPlayerController::SwitchToActorView(AActor* TargetActor, float BlendTime)
{
	if (!IsValid(TargetActor))
	{
		UE_LOG(LogFestaFiestaCamera, Warning, TEXT("[카메라] 유효하지 않은 대상으로 시점 전환을 시도했다."));
		return false;
	}

	// 관찰 카메라를 그대로 두고 뷰타깃만 옮긴다. Possess 를 쓰면 돌아왔을 때 구도를 잃는다.
	StopOverviewCameraMotion();
	SetViewTargetWithBlend(TargetActor, FMath::Max(0.f, BlendTime));

	ViewMode = EFF_CameraViewMode::Actor;
	ApplyViewInputMode(ViewMode);
	ViewedActor = TargetActor;
	LastViewedActor = TargetActor;

	OnViewModeChanged.Broadcast(ViewMode, TargetActor);
	return true;
}

bool AFF_FestivalPlayerController::PossessCharacter(APawn* TargetCharacter, float BlendTime)
{
	if (!IsValid(TargetCharacter))
	{
		UE_LOG(LogFestaFiestaCamera, Warning, TEXT("[카메라] 유효하지 않은 캐릭터로 빙의를 시도했다."));
		return false;
	}

	// 관람과 달리 빙의를 옮긴다. 그래야 입력이 캐릭터로 가고 WASD 가 카메라 팬으로 새지 않는다.
	// 관찰 카메라는 파괴되지 않고 월드에 남아 있어서, 돌아올 때 리그 상태가 그대로다.
	StopOverviewCameraMotion();
	Possess(TargetCharacter);
	SetViewTargetWithBlend(TargetCharacter, FMath::Max(0.f, BlendTime));

	ViewMode = EFF_CameraViewMode::Character;
	ViewedActor = TargetCharacter;
	LastViewedActor = TargetCharacter;

	ApplyViewInputMode(ViewMode);

	OnViewModeChanged.Broadcast(ViewMode, TargetCharacter);
	return true;
}

void AFF_FestivalPlayerController::RefreshViewInputMode()
{
	ApplyViewInputMode(ViewMode);
}

void AFF_FestivalPlayerController::ApplyViewInputMode(const EFF_CameraViewMode Mode)
{
	if (Mode == EFF_CameraViewMode::Character)
	{
		// AController 에 이미 Character 멤버가 있어 다른 이름을 쓴다.
		const AFF_FestivalCharacter* Possessed = Cast<AFF_FestivalCharacter>(GetPawn());
		const bool bShiftLock = Possessed && Possessed->IsShiftLockEnabled();

		if (bShiftLock)
		{
			/* 커서를 치우고 마우스를 화면 안에 가둔다.

			   가두지 않으면 마우스가 창 밖으로 나가고, 그 순간부터 시점이 안 돌아간다.
			   시야 고정은 계속 돌릴 때 쓰는 기능이라 갇혀 있는 편이 맞다.
			*/
			FInputModeGameOnly InputMode;
			SetInputMode(InputMode);
			SetShowMouseCursor(false);
			return;
		}

		// 평소에는 커서를 둔다. 클릭해서 무언가 고를 수 있어야 한다.
		//
		// 대신 커서 아래 위젯이 마우스 움직임을 먼저 가져가므로,
		// 시점 회전은 마우스 버튼을 누른 채로만 되게 입력에서 묶어야 한다.
		FInputModeGameAndUI CharacterInputMode;
		CharacterInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		CharacterInputMode.SetHideCursorDuringCapture(false);
		SetInputMode(CharacterInputMode);
		SetShowMouseCursor(true);
		return;
	}

	/* 관찰과 연출은 커서를 쓴다.

	   GameAndUI 로 두고 캡처를 걸지 않는 이유는, 화면 가장자리로 마우스를 옮겨
	   카메라를 미는 조작이 있기 때문이다. 캡처를 걸면 커서가 화면 안에 갇힌다.
	*/
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
	SetShowMouseCursor(true);
}

void AFF_FestivalPlayerController::SetVisitorCharacter(APawn* InCharacter)
{
	VisitorCharacter = InCharacter;
}

void AFF_FestivalPlayerController::SwitchToOverview(float BlendTime)
{
	AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	if (!CameraPawn)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 관찰 카메라 Pawn 이 없어 시점을 되돌리지 못했다. ")
			TEXT("레벨에 AFF_OverviewCameraPawn 이 있는지, GameMode 의 DefaultPawnClass 를 확인할 것."));
		return;
	}

	// 탐색으로 찾았으면 캐시를 갱신해둔다. 다음부터는 순회하지 않는다.
	OverviewPawn = CameraPawn;
	StopOverviewCameraMotion();

	// 캐릭터를 조종하다 왔으면 빙의도 되돌려야 입력이 카메라로 온다.
	if (GetPawn() != CameraPawn)
	{
		Possess(CameraPawn);
	}

	// 떠날 때 구도가 그대로 남아 있다. 리그 상태를 건드리지 않았기 때문이다.
	SetViewTargetWithBlend(CameraPawn, FMath::Max(0.f, BlendTime));

	ViewMode = EFF_CameraViewMode::Overview;
	ApplyViewInputMode(ViewMode);
	ViewedActor = nullptr;

	OnViewModeChanged.Broadcast(ViewMode, nullptr);
}

void AFF_FestivalPlayerController::ToggleView(float BlendTime)
{
	if (ViewMode != EFF_CameraViewMode::Overview)
	{
		SwitchToOverview(BlendTime);
		return;
	}

	// 되돌아갈 대상이 이미 사라졌으면 관찰 시점에 머문다.
	AActor* Target = LastViewedActor.Get();

	// 아직 아무 데도 안 가봤으면 기본 캐릭터로 들어간다.
	// 이게 없으면 첫 전환 키가 아무 일도 하지 않아서 고장난 것처럼 보인다.
	if (!Target)
	{
		Target = VisitorCharacter.Get();
	}

	if (!Target)
	{
		UE_LOG(LogFestaFiestaCamera, Warning,
			TEXT("[카메라] 전환할 대상이 없다. GameMode 의 VisitorCharacterClass 를 확인하거나 ")
			TEXT("SetVisitorCharacter 로 대상을 지정할 것."));
		return;
	}

	// 마지막 대상이 조종 가능한 폰이었으면 빙의로, 아니면 관람으로 돌아간다.
	if (APawn* AsPawn = Cast<APawn>(Target))
	{
		PossessCharacter(AsPawn, BlendTime);
	}
	else
	{
		SwitchToActorView(Target, BlendTime);
	}
}

void AFF_FestivalPlayerController::EnsureViewTargetAlive()
{
	if (ViewMode == EFF_CameraViewMode::Overview)
	{
		return;
	}

	/* 연출 중이면 건드리지 않는다.

	   지금 화면은 연출 시점을 비추고 있어서 보던 대상이 사라져도 화면이 원점으로
	   가지 않는다. 여기서 관찰 시점으로 돌리면 연출 카메라를 빼앗는다.
	   연출이 끝날 때 ExitScriptedView 가 대상이 사라진 경우를 따로 처리한다.
	*/
	if (bScriptedViewActive)
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
	if (!IsCameraMovementAllowed() || IsPointerOverFacilityUI(GetWorld()) ||
		!EdgeScrollActivationKey.IsValid() || !IsInputKeyDown(EdgeScrollActivationKey))
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

	/* 화면 아래쪽은 가장자리로 치지 않는다.

	   하단에 탭 바와 패널이 있어서, 그것들을 누르려고 마우스를 내릴 때마다
	   카메라가 밀린다. UI 조작과 화면 밀기가 같은 자리에서 일어나면 둘 중
	   하나는 늘 방해가 된다.

	   위와 좌우는 가릴 것이 없으므로 그대로 둔다.
	*/
	const float BottomDeadZone = FMath::Max(0.f,
		CameraPawn->CameraSettings.EdgeScrollBottomDeadZonePixels);

	if (MouseY >= static_cast<float>(ViewportY) - BottomDeadZone)
	{
		return;
	}

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

void AFF_FestivalPlayerController::TickDragPan()
{
	/* 땅을 잡아 끄는 이동(2026-10-07 피드백: 탑뷰를 마우스 드래그로 움직이고 싶다).

	   누른 순간 커서 아래 땅 지점을 잡고, 누르는 동안 그 지점이 커서 아래에 남도록 초점을 옮긴다.
	   커서가 움직인 화면 거리로 속도를 정하지 않고 땅 위치끼리 맞추므로, 확대 정도와 회전에 관계없이
	   손에 붙어 움직인다. 초점은 보간되므로 땅이 커서를 살짝 늦게 따라와 맞춰진다.

	   건축 중에는 끈다. 길을 끌어서 까는 조작 · 오른쪽 클릭 취소와 겹친다(사용자 지시).
	   누를 때만 보지 않고 끄는 도중에도 본다. 건축이 열리거나 시점이 바뀌면 그 자리에서 놓는다.
	*/
	const bool bKeyDown = DragPanKey.IsValid() && IsInputKeyDown(DragPanKey);
	if (!bKeyDown || !bShowMouseCursor || !IsCameraMovementAllowed() || IsBuildSessionActive())
	{
		bDragPanActive = false;
		return;
	}

	AFF_OverviewCameraPawn* CameraPawn = GetOverviewCameraPawn();
	FVector Cursor = FVector::ZeroVector;
	if (!CameraPawn || !ResolveCursorWorldAnchor(Cursor))
	{
		// 커서가 창 밖이거나 하늘을 가리킨다. 다시 땅을 가리키면 새로 눌러야 이어진다.
		bDragPanActive = false;
		return;
	}

	if (!bDragPanActive)
	{
		// 시작은 막 누른 프레임에만. 정보창 위에서 누른 것은 끌기가 아니다.
		// 끄는 도중 커서가 정보창 위를 지나가는 것은 막지 않는다.
		if (!WasInputKeyJustPressed(DragPanKey) || IsPointerOverFacilityUI(GetWorld()))
		{
			return;
		}
		DragPanAnchor = Cursor;
		bDragPanActive = true;
		return;
	}

	// 잡은 지점과 지금 커서 아래 지점의 차이만큼 초점을 민다. 높이는 지면 추종이 정한다.
	const FVector Offset = DragPanAnchor - Cursor;
	if (FVector2D(Offset.X, Offset.Y).IsNearlyZero(1.f))
	{
		return;
	}
	const FVector Focus = CameraPawn->GetCameraState().FocusLocation;
	CameraPawn->FocusOnLocation(FVector(Focus.X + Offset.X, Focus.Y + Offset.Y, Focus.Z));
}

void AFF_FestivalPlayerController::HandleRotate(const FInputActionValue& Value)
{
	if (!IsCameraRotationAllowed())
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
	if (!IsCameraRotationAllowed())
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
	const EFF_CameraInputBlock BlockReason = GetInputBlockReason();
	if (BlockReason == EFF_CameraInputBlock::ModalUi ||
		BlockReason == EFF_CameraInputBlock::InactiveViewport)
	{
		return;
	}

	UFF_FinaleShowSubsystem* Finale = GetWorld() ? GetWorld()->GetSubsystem<UFF_FinaleShowSubsystem>() : nullptr;

	/* 연출이 카메라를 쥐고 있을 때.

	   피날레면 연출 카메라만 놓아 주고(SkipCinematicCamera) 그대로 전환한다. 불꽃은 곡 끝까지 이어진다
	   (2026-10-06 피드백: Tab 으로 1인칭이 돼도 불꽃놀이가 끝나지 않게). 연출 표시를 먼저 풀어야
	   다음 순회 시점이 카메라를 다시 끌고 가지 않는다. 피날레가 아닌 연출이면 전처럼 막는다.
	*/
	if (bScriptedViewActive)
	{
		if (!Finale || !Finale->IsShowInProgress())
		{
			return;
		}

		Finale->SkipCinematicCamera();
		if (bScriptedViewActive)
		{
			return;
		}

		// 연출 전에 캐릭터 시점이었으면 방금 거기로 돌아왔다. 그게 1인칭이라 한 번 더 바꾸지 않는다.
		if (ViewMode != EFF_CameraViewMode::Overview)
		{
			return;
		}
	}
	/* 피날레 중 연출 카메라를 놓아 둔 상태(위에서 Tab 으로 캐릭터 시점에 왔다).

	   기본 카메라로 가지 않고 연출 카메라를 다시 잡는다. 불꽃이 끝날 때까지 Tab 은 연출 카메라와 캐릭터
	   시점만 오간다(2026-10-07 피드백: 두 번째 Tab 이 기본 카메라로 가 불꽃이 화면에서 사라졌다).
	   캐릭터 빙의는 그대로 두므로 다음 Tab 은 위 분기에서 캐릭터 시점으로 돌아온다.
	*/
	else if (Finale && Finale->CanResumeCinematicCamera())
	{
		Finale->ResumeCinematicCamera();
		if (bScriptedViewActive)
		{
			return;
		}
	}

	ToggleView(ViewSwitchBlendTime);
}

void AFF_FestivalPlayerController::HandlePick(const FInputActionValue& Value)
{
	(void)Value;
	ProcessPickRequest();
}

void AFF_FestivalPlayerController::ProcessPickRequest()
{
	if (bPickRequestHandledThisTick)
	{
		return;
	}
	bPickRequestHandledThisTick = true;

	// Debug WBP 버튼은 Slate 입력과 별개인 raw 좌클릭 폴백까지 막아야 한다.
	// 패널이 숨겨졌거나 포인터가 카드 밖이면 서브시스템 조회는 즉시 false다.
	const UWorld* World = GetWorld();
	const UFF_ZoneLifecycleTestPanelSubsystem* DebugPanel = World
		? World->GetSubsystem<UFF_ZoneLifecycleTestPanelSubsystem>()
		: nullptr;
	if (DebugPanel && DebugPanel->IsPointerOverPanel())
	{
		return;
	}

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
