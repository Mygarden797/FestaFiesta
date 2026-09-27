// FF_FestivalSettingsSubsystem.cpp
// 역할: 일시정지·설정 화면의 열기·닫기, 입력 차단과 기존 건설 화면 복구를 관리한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/FF_UILog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Core/FF_UIRootLayout.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"

const UFF_FestivalSettingsDeveloperSettings* UFF_FestivalSettingsDeveloperSettings::Get()
{
	return GetDefault<UFF_FestivalSettingsDeveloperSettings>();
}

void UFF_FestivalSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFF_UIFlowSubsystem>();
	Collection.InitializeDependency<UFF_RuntimeSettingsSubsystem>();
	Super::Initialize(Collection);

	PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddUObject(
		this, &UFF_FestivalSettingsSubsystem::HandlePostLogin);

	if (UFF_RuntimeSettingsSubsystem* RuntimeSettings =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
	{
		RuntimeSettings->OnSettingsChanged.AddUniqueDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandleRuntimeSettingsChanged);
	}
}

void UFF_FestivalSettingsSubsystem::Deinitialize()
{
	ForceClosePauseSession();
	UnbindOpenCloseInput();
	if (UFF_UIRootLayout* Layout = StandaloneRootLayout.Get())
	{
		Layout->RemoveFromParent();
	}
	StandaloneRootLayout.Reset();

	if (PostLoginHandle.IsValid())
	{
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
	}
	PostLoginHandle.Reset();

	if (UFF_RuntimeSettingsSubsystem* RuntimeSettings =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UFF_RuntimeSettingsSubsystem>() : nullptr)
	{
		RuntimeSettings->OnSettingsChanged.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandleRuntimeSettingsChanged);
	}

	Super::Deinitialize();
}

UFF_FestivalSettingsSubsystem* UFF_FestivalSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}

	const UWorld* World = GEngine->GetWorldFromContextObject(
		WorldContextObject, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UFF_FestivalSettingsSubsystem>() : nullptr;
}

void UFF_FestivalSettingsSubsystem::HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
	if (!GameMode || GameMode->GetGameInstance() != GetGameInstance() ||
		!NewPlayer || !NewPlayer->IsLocalController() || !Cast<AFF_FestivalPlayerController>(NewPlayer))
	{
		return;
	}

	// Travel 뒤 old Controller weak가 먼저 무효화되어도 이전 월드의 위젯/lease를 새 Controller로 넘기지 않는다.
	if (HasPauseSession() && ActiveController.Get() != NewPlayer)
	{
		ForceClosePauseSession();
	}
	else if (!ActiveHub.IsValid() && !ActiveScreen.IsValid())
	{
		// 외부 화면 교체나 travel로 위젯이 먼저 사라져도 게임이 영구 정지되지 않는다.
		ReleasePauseSession();
		ActiveController.Reset();
		ActiveRootLayout.Reset();
	}

	BindOpenCloseInput(NewPlayer);

	// PostLogin 직후에는 Festival GameMode가 Overview Pawn을 아직 연결하는 중일 수 있다.
	if (UWorld* World = NewPlayer->GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UFF_FestivalSettingsSubsystem::ApplyToFirstLocalPlayer));
	}
}

void UFF_FestivalSettingsSubsystem::BindOpenCloseInput(APlayerController* Controller)
{
	if (!Controller)
	{
		return;
	}

	// UPlayerInput은 InputComponent보다 먼저 DebugExecBindings를 실행한다. 프로젝트가 F5를
	// 옵션 전용으로 소유하므로 이미 생성된 PlayerInput에서도 엔진 기본 셰이더 명령만 제거한다.
	RemoveConflictingDebugBindings(Controller);

	if (BoundControllers.Contains(Controller))
	{
		return;
	}

	UInputComponent* InputComponent = Controller->InputComponent;
	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	if (!InputComponent || !Settings)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_FestivalSettingsSubsystem: Festival Controller의 InputComponent가 없어 옵션 키를 바인딩하지 못했다."));
		return;
	}

	for (const FKey& Key : Settings->OpenCloseKeys)
	{
		if (!Key.IsValid())
		{
			continue;
		}

		FInputKeyBinding& Binding = InputComponent->BindKey(
			Key, IE_Pressed, this, &UFF_FestivalSettingsSubsystem::HandleOpenCloseInput);
		Binding.bExecuteWhenPaused = true;
		Binding.bConsumeInput = true;
	}

	BoundControllers.Add(Controller);
}

void UFF_FestivalSettingsSubsystem::UnbindOpenCloseInput()
{
	for (const TWeakObjectPtr<APlayerController>& ControllerPtr : BoundControllers)
	{
		if (APlayerController* Controller = ControllerPtr.Get())
		{
			if (UInputComponent* InputComponent = Controller->InputComponent)
			{
				InputComponent->KeyBindings.RemoveAll([this](const FInputKeyBinding& Binding)
				{
					return Binding.KeyDelegate.IsBoundToObject(this);
				});
			}
		}
	}
	BoundControllers.Reset();
}

void UFF_FestivalSettingsSubsystem::RemoveConflictingDebugBindings(APlayerController* Controller) const
{
	UPlayerInput* PlayerInput = Controller ? Controller->PlayerInput : nullptr;
	if (!PlayerInput)
	{
		return;
	}

	const int32 RemovedCount = PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Binding)
	{
		return Binding.Key == EKeys::F5
			&& Binding.Command.Contains(TEXT("viewmode shadercomplexity"), ESearchCase::IgnoreCase);
	});
	if (RemovedCount > 0)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_FestivalSettingsSubsystem: F5 Shader Complexity 디버그 바인딩을 %d개 제거했다."),
			RemovedCount);
	}
}

bool UFF_FestivalSettingsSubsystem::HasPauseSession() const
{
	return ActiveHub.IsValid() || ActiveScreen.IsValid() || ActiveController.IsValid() ||
		ActiveRootLayout.IsValid() || bPausedGameForPauseMenu || bOwnsModalUiBlock ||
		SuspendedBuildMode.IsValid() || SuppressedFacilityInfo.IsValid();
}

// 다른 모달 화면이 입력을 소유하고 있는 동안 설정 메뉴를 중복해서 열지 않도록 확인한다.
bool UFF_FestivalSettingsSubsystem::ShouldDeferOpenToActiveGameplayMode(
	const APlayerController& Controller) const
{
	const UWorld* World = Controller.GetWorld();
	if (!World)
	{
		return false;
	}
	if (const AFF_FestivalPlayerController* FestivalController =
		Cast<AFF_FestivalPlayerController>(&Controller);
		FestivalController && FestivalController->GetInputBlockReason() == EFF_CameraInputBlock::ModalUi)
	{
		return true;
	}

	// 건축 중에는 아래의 일시정지/복원 절차로 배치 상태를 유지한다.
	return false;
}

bool UFF_FestivalSettingsSubsystem::SuspendActiveBuildMode(APlayerController& Controller)
{
	UWorld* World = Controller.GetWorld();
	UFF_BuildModeSubsystem* BuildMode = World
		? World->GetSubsystem<UFF_BuildModeSubsystem>()
		: nullptr;
	if (!BuildMode || !BuildMode->SuspendInputForPauseMenu())
	{
		return false;
	}

	SuspendedBuildMode = BuildMode;
	return true;
}

void UFF_FestivalSettingsSubsystem::ResumeSuspendedBuildMode()
{
	if (UFF_BuildModeSubsystem* BuildMode = SuspendedBuildMode.Get())
	{
		BuildMode->ResumeInputAfterPauseMenu();
	}
	SuspendedBuildMode.Reset();
}

bool UFF_FestivalSettingsSubsystem::SuppressFacilityPresentation(APlayerController& Controller)
{
	UWorld* World = Controller.GetWorld();
	UFF_FacilityInfoSubsystem* FacilityInfo = World
		? World->GetSubsystem<UFF_FacilityInfoSubsystem>()
		: nullptr;
	if (!FacilityInfo)
	{
		return false;
	}

	FacilityInfo->SetPauseMenuPresentationSuppressed(true);
	SuppressedFacilityInfo = FacilityInfo;
	return true;
}

void UFF_FestivalSettingsSubsystem::RestoreSuppressedFacilityPresentation()
{
	if (UFF_FacilityInfoSubsystem* FacilityInfo = SuppressedFacilityInfo.Get())
	{
		FacilityInfo->SetPauseMenuPresentationSuppressed(false);
	}
	SuppressedFacilityInfo.Reset();
}

void UFF_FestivalSettingsSubsystem::HandleOpenCloseInput(FKey PressedKey)
{
	// The construction toolbar has no function-key shortcuts, including the
	// legacy global F5 pause binding. Escape and explicit UI buttons still work.
	if (PressedKey == EKeys::F5)
	{
		const auto* Build = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
		if (Build && Build->GetBuildModeState() != EFF_BuildModeState::Closed) return;
	}
	if (bTransitionInProgress || LastStateTransitionFrame == GFrameCounter)
	{
		return;
	}
	if (UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get())
	{
		Screen->RequestNavigateBack();
		return;
	}
	if (UFF_PauseHubScreen* Hub = ActiveHub.Get())
	{
		Hub->RequestAction(EFF_PauseHubAction::Resume);
		return;
	}
	OpenPauseMenuInternal(nullptr, PressedKey);
}

bool UFF_FestivalSettingsSubsystem::ToggleFestivalSettings(APlayerController* RequestingController)
{
	if (bTransitionInProgress || LastStateTransitionFrame == GFrameCounter)
	{
		return false;
	}
	if (UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get())
	{
		return Screen->RequestNavigateBack();
	}
	if (UFF_PauseHubScreen* Hub = ActiveHub.Get())
	{
		return Hub->RequestAction(EFF_PauseHubAction::Resume);
	}
	return OpenPauseMenuInternal(RequestingController, FKey());
}

bool UFF_FestivalSettingsSubsystem::OpenFestivalSettings(APlayerController* RequestingController)
{
	// 기존 Blueprint API도 새 1단계 IA를 건너뛰지 않는다.
	return OpenPauseMenu(RequestingController);
}

bool UFF_FestivalSettingsSubsystem::OpenPauseMenu(APlayerController* RequestingController)
{
	return OpenPauseMenuInternal(RequestingController, FKey());
}

APlayerController* UFF_FestivalSettingsSubsystem::ResolveLocalController(
	APlayerController* RequestedController) const
{
	APlayerController* Controller = RequestedController;
	if (!Controller && GetGameInstance())
	{
		Controller = GetGameInstance()->GetFirstLocalPlayerController();
	}
	return Controller && Controller->IsLocalController() ? Controller : nullptr;
}

UFF_UIRootLayout* UFF_FestivalSettingsSubsystem::ResolveRootLayout(
	APlayerController& Controller)
{
	UFF_UIFlowSubsystem* Flow = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_UIFlowSubsystem>() : nullptr;
	if (Flow)
	{
		if (UFF_UIRootLayout* Existing = Flow->GetRootLayout())
		{
			return Existing;
		}
	}

	if (UFF_UIRootLayout* Existing = StandaloneRootLayout.Get())
	{
		if (Existing->GetOwningPlayer() == &Controller)
		{
			return Existing;
		}
		Existing->RemoveFromParent();
		StandaloneRootLayout.Reset();
	}

	const UFF_UIFlowSettings* FlowSettings = UFF_UIFlowSettings::Get();
	UClass* LayoutClass = FlowSettings ? FlowSettings->RootLayoutClass.LoadSynchronous() : nullptr;
	if (!LayoutClass)
	{
		return nullptr;
	}

	UFF_UIRootLayout* Layout = CreateWidget<UFF_UIRootLayout>(&Controller, LayoutClass);
	if (!Layout)
	{
		return nullptr;
	}
	Layout->AddToPlayerScreen(0);
	StandaloneRootLayout = Layout;
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_FestivalSettingsSubsystem: UI Flow 비자동 맵에 독립 옵션 RootLayout을 생성했다."));
	return Layout;
}

bool UFF_FestivalSettingsSubsystem::OpenPauseMenuInternal(
	APlayerController* RequestingController, const FKey& TriggerKey)
{
	if (IsPauseMenuOpen())
	{
		return true;
	}
	if (bTransitionInProgress || LastStateTransitionFrame == GFrameCounter)
	{
		return false;
	}

	APlayerController* Controller = ResolveLocalController(RequestingController);
	AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(Controller);
	if (!FestivalController)
	{
		return false;
	}
	if (ShouldDeferOpenToActiveGameplayMode(*FestivalController))
	{
		return true;
	}

	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	UClass* HubClass = Settings ? Settings->PauseHubScreenClass.LoadSynchronous() : nullptr;
	if (!HubClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: 일시정지 허브 WBP를 불러오지 못했다 (%s)."),
			Settings ? *Settings->PauseHubScreenClass.ToString() : TEXT("설정 없음"));
		return false;
	}

	UFF_UIRootLayout* RootLayout = ResolveRootLayout(*FestivalController);
	if (!RootLayout)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: RootLayout이 없어 일시정지 허브를 열지 못했다."));
		return false;
	}

	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	const bool bSuspendedBuildMode = SuspendActiveBuildMode(*FestivalController);
	const bool bSuppressedFacilityPresentation = SuppressFacilityPresentation(*FestivalController);
	UFF_PauseHubScreen* Hub = Cast<UFF_PauseHubScreen>(RootLayout->PushScreen(HubClass));
	if (!Hub)
	{
		if (bSuppressedFacilityPresentation)
		{
			RestoreSuppressedFacilityPresentation();
		}
		if (bSuspendedBuildMode)
		{
			ResumeSuspendedBuildMode();
		}
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: 일시정지 허브를 ScreenStack에 Push하지 못했다 (%s)."),
			*Settings->PauseHubScreenClass.ToString());
		return false;
	}

	ActiveHub = Hub;
	ActiveRootLayout = RootLayout;
	ActiveController = FestivalController;
	Hub->OnActionRequested.AddUniqueDynamic(
		this, &UFF_FestivalSettingsSubsystem::HandlePauseHubAction);
	Hub->OnReleased.AddUniqueDynamic(
		this, &UFF_FestivalSettingsSubsystem::HandlePauseHubReleased);
	AcquirePauseSession(*FestivalController);

	ApplyGameplaySettings(FestivalController);
	if (UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(FestivalController))
	{
		RuntimeSettings->ApplyAudioSettings();
	}

	LastStateTransitionFrame = GFrameCounter;
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_FestivalSettingsSubsystem: %s 입력으로 일시정지 허브를 열었다."),
		TriggerKey.IsValid() ? *TriggerKey.GetDisplayName().ToString() : TEXT("API"));
	return true;
}

void UFF_FestivalSettingsSubsystem::AcquirePauseSession(APlayerController& Controller)
{
	AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(&Controller);
	bOwnsModalUiBlock = FestivalController
		&& FestivalController->GetInputBlockReason() != EFF_CameraInputBlock::ModalUi;
	if (bOwnsModalUiBlock)
	{
		FestivalController->SetModalUiActive(true);
	}

	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	if (Settings && Settings->bPauseGameWhileOpen && !UGameplayStatics::IsGamePaused(&Controller))
	{
		bPausedGameForPauseMenu = UGameplayStatics::SetGamePaused(&Controller, true);
	}
}

void UFF_FestivalSettingsSubsystem::ReleasePauseSession()
{
	APlayerController* OwnedController = ActiveController.Get();
	APlayerController* PauseController = OwnedController;
	if (bPausedGameForPauseMenu && !PauseController && GetGameInstance())
	{
		// seamless travel/controller 교체로 old weak가 먼저 끊겨도 같은 local session의 새 World를 멈춘 채 두지 않는다.
		PauseController = GetGameInstance()->GetFirstLocalPlayerController();
	}
	if (bPausedGameForPauseMenu && PauseController)
	{
		UGameplayStatics::SetGamePaused(PauseController, false);
	}
	if (bOwnsModalUiBlock)
	{
		if (AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(OwnedController))
		{
			FestivalController->SetModalUiActive(false);
		}
	}

	bPausedGameForPauseMenu = false;
	bOwnsModalUiBlock = false;
	// Hub가 사라진 뒤 전역 시설 표면을 먼저 되살리고, 다음 틱에 건축 상태별 포커스를 되찾는다.
	RestoreSuppressedFacilityPresentation();
	ResumeSuspendedBuildMode();
}

bool UFF_FestivalSettingsSubsystem::OpenSettingsLayer(
	const TOptional<EFF_FestivalSettingsCategory> Category)
{
	if (UFF_FestivalSettingsScreen* Existing = ActiveScreen.Get())
	{
		if (Category.IsSet())
		{
			Existing->ShowCategory(Category.GetValue());
		}
		return true;
	}
	if (!ActiveHub.IsValid() || bTransitionInProgress)
	{
		return false;
	}

	UFF_UIRootLayout* RootLayout = ActiveRootLayout.Get();
	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	UClass* ScreenClass = Settings ? Settings->SettingsScreenClass.LoadSynchronous() : nullptr;
	if (!RootLayout || !ScreenClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: 상세 설정 화면을 열 수 없다 (Root=%s, Class=%s)."),
			RootLayout ? TEXT("있음") : TEXT("없음"), ScreenClass ? TEXT("있음") : TEXT("없음"));
		return false;
	}

	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	UFF_FestivalSettingsScreen* Screen = Cast<UFF_FestivalSettingsScreen>(
		RootLayout->PushScreen(ScreenClass));
	if (!Screen)
	{
		return false;
	}

	ActiveScreen = Screen;
	Screen->OnCloseRequested.AddUniqueDynamic(
		this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);
	if (Category.IsSet())
	{
		Screen->ShowCategory(Category.GetValue());
	}
	LastStateTransitionFrame = GFrameCounter;
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_FestivalSettingsSubsystem: 일시정지 허브 위에 상세 설정을 열었다."));
	return true;
}

bool UFF_FestivalSettingsSubsystem::CloseSettingsLayer()
{
	UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get();
	if (!Screen || bTransitionInProgress)
	{
		return false;
	}

	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	Screen->OnCloseRequested.RemoveDynamic(
		this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);
	ActiveScreen.Reset();

	if (UFF_UIRootLayout* RootLayout = ActiveRootLayout.Get())
	{
		RootLayout->RemoveScreen(Screen);
	}
	else
	{
		Screen->RemoveFromParent();
	}

	LastStateTransitionFrame = GFrameCounter;
	UE_LOG(LogFestaFiestaUI, Log,
		TEXT("UFF_FestivalSettingsSubsystem: 상세 설정만 닫고 일시정지 허브로 돌아갔다."));
	return true;
}

bool UFF_FestivalSettingsSubsystem::ClosePauseSession()
{
	if (ActiveScreen.IsValid() || bTransitionInProgress)
	{
		return false;
	}
	UFF_PauseHubScreen* Hub = ActiveHub.Get();
	if (!Hub && !HasPauseSession())
	{
		return false;
	}

	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	if (Hub)
	{
		Hub->OnActionRequested.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandlePauseHubAction);
		Hub->OnReleased.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandlePauseHubReleased);
		ActiveHub.Reset();
		if (UFF_UIRootLayout* RootLayout = ActiveRootLayout.Get())
		{
			RootLayout->RemoveScreen(Hub);
		}
		else
		{
			Hub->RemoveFromParent();
		}
	}

	ReleasePauseSession();
	ActiveController.Reset();
	ActiveRootLayout.Reset();
	LastStateTransitionFrame = GFrameCounter;
	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_FestivalSettingsSubsystem: 게임으로 돌아갔다."));
	return true;
}

void UFF_FestivalSettingsSubsystem::ForceClosePauseSession()
{
	if (bTransitionInProgress)
	{
		return;
	}
	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	UFF_UIRootLayout* RootLayout = ActiveRootLayout.Get();

	if (UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get())
	{
		Screen->OnCloseRequested.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);
		if (RootLayout)
		{
			RootLayout->RemoveScreen(Screen);
		}
		else
		{
			Screen->RemoveFromParent();
		}
	}
	if (UFF_PauseHubScreen* Hub = ActiveHub.Get())
	{
		Hub->OnActionRequested.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandlePauseHubAction);
		Hub->OnReleased.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandlePauseHubReleased);
		if (RootLayout)
		{
			RootLayout->RemoveScreen(Hub);
		}
		else
		{
			Hub->RemoveFromParent();
		}
	}

	ActiveScreen.Reset();
	ActiveHub.Reset();
	ReleasePauseSession();
	ActiveController.Reset();
	ActiveRootLayout.Reset();
	LastStateTransitionFrame = GFrameCounter;
}

bool UFF_FestivalSettingsSubsystem::ClosePauseMenu()
{
	if (UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get())
	{
		return Screen->RequestNavigateBack();
	}
	return ClosePauseSession();
}

bool UFF_FestivalSettingsSubsystem::CloseFestivalSettings()
{
	return ClosePauseMenu();
}

void UFF_FestivalSettingsSubsystem::HandlePauseHubAction(const EFF_PauseHubAction Action)
{
	if (bTransitionInProgress || LastStateTransitionFrame == GFrameCounter)
	{
		return;
	}

	switch (Action)
	{
	case EFF_PauseHubAction::SystemSettings:
		OpenSettingsLayer();
		break;
	case EFF_PauseHubAction::KeyboardGuide:
		OpenSettingsLayer(EFF_FestivalSettingsCategory::Keyboard);
		break;
	case EFF_PauseHubAction::Account:
		OpenSettingsLayer(EFF_FestivalSettingsCategory::Account);
		break;
	case EFF_PauseHubAction::ExitGame:
		OpenSettingsLayer(EFF_FestivalSettingsCategory::ExitGame);
		break;
	case EFF_PauseHubAction::Resume:
	default:
		ClosePauseSession();
		break;
	}
}

void UFF_FestivalSettingsSubsystem::HandlePauseHubReleased()
{
	if (bTransitionInProgress)
	{
		return;
	}

	// ScreenStack/Travel이 허브를 먼저 제거해도 이 서브시스템이 소유한 Pause와 Modal을
	// 영구히 남기지 않는다. 비정상 경로이므로 상세 화면도 함께 정리한다.
	TGuardValue<bool> TransitionGuard(bTransitionInProgress, true);
	if (UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get())
	{
		Screen->OnCloseRequested.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);
		Screen->RemoveFromParent();
	}
	ActiveScreen.Reset();
	ActiveHub.Reset();
	ReleasePauseSession();
	ActiveController.Reset();
	ActiveRootLayout.Reset();
	LastStateTransitionFrame = GFrameCounter;
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("UFF_FestivalSettingsSubsystem: 외부에서 허브가 제거되어 Pause 세션을 회수했다."));
}

void UFF_FestivalSettingsSubsystem::HandleCloseRequested()
{
	CloseSettingsLayer();
}

void UFF_FestivalSettingsSubsystem::HandleRuntimeSettingsChanged()
{
	ApplyToFirstLocalPlayer();
}

void UFF_FestivalSettingsSubsystem::ApplyToFirstLocalPlayer()
{
	APlayerController* Controller = GetGameInstance()
		? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	BindOpenCloseInput(Controller);
	ApplyGameplaySettings(Controller);
}

void UFF_FestivalSettingsSubsystem::ApplyGameplaySettings(APlayerController* TargetController)
{
	APlayerController* Controller = TargetController;
	if (!Controller && GetGameInstance())
	{
		Controller = GetGameInstance()->GetFirstLocalPlayerController();
	}

	AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(Controller);
	AFF_OverviewCameraPawn* CameraPawn = FestivalController ? FestivalController->GetOverviewCameraPawn() : nullptr;
	const UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(Controller);
	if (!CameraPawn || !RuntimeSettings)
	{
		return;
	}

	// 이 두 값은 기존 카메라 코드가 매 입력 시 공개 CameraSettings에서 읽으므로 재초기화가 필요 없다.
	CameraPawn->CameraSettings.bZoomToCursor = RuntimeSettings->IsZoomToCursorEnabled();
	CameraPawn->CameraSettings.bEnableEdgeScroll = RuntimeSettings->IsEdgeScrollEnabled();
}
