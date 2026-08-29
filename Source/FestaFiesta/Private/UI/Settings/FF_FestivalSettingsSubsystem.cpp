// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Settings/FF_FestivalSettingsSubsystem.h"

#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/FF_UILog.h"
#include "UI/Settings/FF_FestivalSettingsScreen.h"
#include "UI/Settings/FF_RuntimeSettingsSubsystem.h"

const UFF_FestivalSettingsDeveloperSettings* UFF_FestivalSettingsDeveloperSettings::Get()
{
	return GetDefault<UFF_FestivalSettingsDeveloperSettings>();
}

void UFF_FestivalSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
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
	CloseFestivalSettings();
	UnbindOpenCloseInput();

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

	// Travel 뒤에는 이전 월드의 약한 위젯 참조와 Pause 소유권을 들고 가지 않는다.
	if (!ActiveScreen.IsValid())
	{
		ActiveController.Reset();
		bPausedGameForSettings = false;
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
			TEXT("UFF_FestivalSettingsSubsystem: Festival Controller의 InputComponent가 없어 ESC/F5를 바인딩하지 못했다."));
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

	// UE BaseInput.ini의 개발용 F5 → Shader Complexity만 로컬 인스턴스에서 제거한다.
	// Project DefaultInput.ini를 수정하지 않으므로 팀 입력 설정과 다른 디버그 단축키는 그대로다.
	const int32 RemovedCount = PlayerInput->DebugExecBindings.RemoveAll([](const FKeyBind& Binding)
	{
		return Binding.Key == EKeys::F5 &&
			Binding.Command.Contains(TEXT("viewmode shadercomplexity"), ESearchCase::IgnoreCase);
	});

	if (RemovedCount > 0)
	{
		UE_LOG(LogFestaFiestaUI, Log,
			TEXT("UFF_FestivalSettingsSubsystem: F5 Shader Complexity 디버그 바인딩을 %d개 제거했다."),
			RemovedCount);
	}
}

void UFF_FestivalSettingsSubsystem::RestoreLitViewModeIfNeeded(const FKey& PressedKey) const
{
	if (PressedKey != EKeys::F5 || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	const int32 CurrentViewMode = GEngine->GameViewport->ViewModeIndex;
	if (CurrentViewMode == VMI_ShaderComplexity || CurrentViewMode == VMI_QuadOverdraw ||
		CurrentViewMode == VMI_ShaderComplexityWithQuadOverdraw)
	{
		GEngine->GameViewport->SetViewMode(VMI_Lit);
	}
}

void UFF_FestivalSettingsSubsystem::HandleOpenCloseInput(FKey PressedKey)
{
	RestoreLitViewModeIfNeeded(PressedKey);
	ToggleFestivalSettings();
}

bool UFF_FestivalSettingsSubsystem::ToggleFestivalSettings(APlayerController* RequestingController)
{
	return IsFestivalSettingsOpen()
		? CloseFestivalSettings()
		: OpenFestivalSettings(RequestingController);
}

bool UFF_FestivalSettingsSubsystem::OpenFestivalSettings(APlayerController* RequestingController)
{
	if (IsFestivalSettingsOpen())
	{
		return true;
	}

	APlayerController* Controller = RequestingController;
	if (!Controller && GetGameInstance())
	{
		Controller = GetGameInstance()->GetFirstLocalPlayerController();
	}
	AFF_FestivalPlayerController* FestivalController = Cast<AFF_FestivalPlayerController>(Controller);
	if (!FestivalController || !FestivalController->IsLocalController())
	{
		return false;
	}

	const UFF_FestivalSettingsDeveloperSettings* Settings = UFF_FestivalSettingsDeveloperSettings::Get();
	UClass* ScreenClass = Settings ? Settings->SettingsScreenClass.LoadSynchronous() : nullptr;
	if (!ScreenClass)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: 설정 WBP를 불러오지 못했다. Project Settings > Game > FestaFiesta Festival Settings를 확인한다."));
		return false;
	}

	UFF_FestivalSettingsScreen* Screen = CreateWidget<UFF_FestivalSettingsScreen>(FestivalController, ScreenClass);
	if (!Screen || !Screen->AddToPlayerScreen(Settings->ScreenZOrder))
	{
		if (Screen)
		{
			Screen->RemoveFromParent();
		}
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_FestivalSettingsSubsystem: 설정 WBP를 로컬 플레이어 화면에 추가하지 못했다 (%s)."),
			*Settings->SettingsScreenClass.ToString());
		return false;
	}

	ActiveScreen = Screen;
	ActiveController = FestivalController;
	Screen->OnCloseRequested.AddUniqueDynamic(
		this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);

	ApplyGameplaySettings(FestivalController);
	if (UFF_RuntimeSettingsSubsystem* RuntimeSettings = UFF_RuntimeSettingsSubsystem::Get(FestivalController))
	{
		RuntimeSettings->ApplyAudioSettings();
	}

	if (Settings->bPauseGameWhileOpen && !UGameplayStatics::IsGamePaused(FestivalController))
	{
		bPausedGameForSettings = UGameplayStatics::SetGamePaused(FestivalController, true);
	}

	Screen->ActivateWidget();
	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_FestivalSettingsSubsystem: 설정 화면을 열었다."));
	return true;
}

bool UFF_FestivalSettingsSubsystem::CloseFestivalSettings()
{
	UFF_FestivalSettingsScreen* Screen = ActiveScreen.Get();
	APlayerController* Controller = ActiveController.Get();
	if (!Screen && !bPausedGameForSettings)
	{
		return false;
	}

	if (Screen)
	{
		Screen->OnCloseRequested.RemoveDynamic(
			this, &UFF_FestivalSettingsSubsystem::HandleCloseRequested);
		Screen->DeactivateWidget();
		Screen->RemoveFromParent();
	}

	if (bPausedGameForSettings && Controller)
	{
		UGameplayStatics::SetGamePaused(Controller, false);
	}

	ActiveScreen.Reset();
	ActiveController.Reset();
	bPausedGameForSettings = false;

	UE_LOG(LogFestaFiestaUI, Log, TEXT("UFF_FestivalSettingsSubsystem: 설정 화면을 닫았다."));
	return true;
}

void UFF_FestivalSettingsSubsystem::HandleCloseRequested()
{
	CloseFestivalSettings();
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
