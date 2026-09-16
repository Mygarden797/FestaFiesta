#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilityCodexPresenter, Log, All);

bool UFF_FacilityCodexPresenterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

FSoftClassPath UFF_FacilityCodexPresenterSubsystem::GetDefaultScreenClassPath()
{
	return FSoftClassPath(TEXT(
		"/Game/FestaFiesta/UI/FacilityCodex/WBP_FF_FacilityCodexScreen.WBP_FF_FacilityCodexScreen_C"));
}

bool UFF_FacilityCodexPresenterSubsystem::IsCodexOpen() const
{
	return IsValid(Screen) && Screen->IsInViewport();
}

bool UFF_FacilityCodexPresenterSubsystem::OpenCodex(
	APlayerController* RequestingController, const FName FocusFacilityId)
{
	UWorld* World = GetWorld();
	AFF_FestivalPlayerController* Controller = Cast<AFF_FestivalPlayerController>(RequestingController);
	if (bDeinitializing || bClosing || !World || !IsValid(Controller) ||
		Controller->GetWorld() != World || !Controller->IsLocalPlayerController() ||
		!Controller->GetLocalPlayer())
	{
		return false;
	}
	if (IsCodexOpen())
	{
		if (OwningController.Get() != Controller) return false;
		Screen->SetFacilityFilter(FocusFacilityId);
		return true;
	}
	// Recover an externally detached screen before acquiring a new session.
	if (Screen || bOwnsModalInput) CloseSession(true);

	UFF_BuildModeSubsystem* BuildMode = World->GetSubsystem<UFF_BuildModeSubsystem>();
	UFF_FacilityInfoSubsystem* FacilityInfo = World->GetSubsystem<UFF_FacilityInfoSubsystem>();
	const EFF_CameraInputBlock BlockReason = Controller->GetInputBlockReason();
	// The existing APIs expose exclusive boolean leases, not nesting counters.
	// Never borrow or later release another modal/pause screen's ownership.
	if (BlockReason == EFF_CameraInputBlock::ModalUi ||
		BlockReason == EFF_CameraInputBlock::Scripted || UGameplayStatics::IsGamePaused(World) ||
		(BuildMode && BuildMode->IsInputSuspendedForPauseMenu()) ||
		(FacilityInfo && FacilityInfo->IsPauseMenuPresentationSuppressed()))
	{
		return false;
	}

	UClass* ScreenClass = GetDefaultScreenClassPath().TryLoadClass<UFF_FacilityCodexScreen>();
	if (!ScreenClass)
	{
		UE_LOG(LogFFFacilityCodexPresenter, Error,
			TEXT("Facility codex WBP is missing: %s. Run FF.BuildFacilityCodexUMG in the editor."),
			*GetDefaultScreenClassPath().ToString());
		return false;
	}
	UFF_FacilityCodexScreen* Candidate = CreateWidget<UFF_FacilityCodexScreen>(Controller, ScreenClass);
	if (!Candidate) return false;

	Screen = Candidate;
	OwningController = Controller;
	bPreviousCursorVisible = Controller->bShowMouseCursor;
	InputViewport = Controller->GetLocalPlayer()->ViewportClient;
	bPreviousViewportIgnoredInput = InputViewport.IsValid() && InputViewport->IgnoreInput();
	if (FSlateApplication::IsInitialized())
	{
		PreviousFocusedWidget = FSlateApplication::Get().GetKeyboardFocusedWidget();
	}
	if (BuildMode && BuildMode->SuspendInputForPauseMenu()) SuspendedBuildMode = BuildMode;
	if (FacilityInfo)
	{
		SuppressedFacilityInfo = FacilityInfo;
		FacilityInfo->SetPauseMenuPresentationSuppressed(true);
	}
	Controller->SetModalUiActive(true);
	bOwnsModalInput = true;
	Controller->OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleControllerEndPlay);
	if (UGameInstance* GameInstance = World->GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			BoundStage = Stage;
			Stage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
		}
	}
	if (UFFZSWorldSubsystem* ZoneSystem = World->GetSubsystem<UFFZSWorldSubsystem>())
	{
		BoundZoneSystem = ZoneSystem;
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
	Candidate->OnCloseRequested.AddUObject(this, &ThisClass::CloseCodex);
	Candidate->SetFacilityFilter(FocusFacilityId);
	// The facility inspector uses global Viewport Z=95. PlayerScreen Z is a
	// different layer, so even a large player Z cannot reliably cover it.
	Candidate->AddToViewport(100);
	if (!Candidate->IsInViewport())
	{
		CloseSession(true);
		return false;
	}
	Controller->bShowMouseCursor = true;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(Candidate->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Controller->SetInputMode(InputMode);
	Candidate->SetKeyboardFocus();
	return true;
}

void UFF_FacilityCodexPresenterSubsystem::CloseCodex()
{
	CloseSession(true);
}

void UFF_FacilityCodexPresenterSubsystem::CloseSession(const bool bRestoreFocus)
{
	if (bClosing) return;
	TGuardValue<bool> ClosingGuard(bClosing, true);
	if (UFF_FestivalStageSubsystem* Stage = BoundStage.Get())
	{
		Stage->OnStageChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
	}
	if (UFFZSWorldSubsystem* ZoneSystem = BoundZoneSystem.Get())
	{
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
	BoundStage.Reset();
	BoundZoneSystem.Reset();
	if (Screen)
	{
		// NativeDestruct also requests close if somebody removes this widget.
		// Unbind first so normal teardown cannot recurse through that signal.
		Screen->OnCloseRequested.RemoveAll(this);
		Screen->RemoveFromParent();
		Screen = nullptr;
	}
	AFF_FestivalPlayerController* Controller = OwningController.Get();
	if (Controller)
	{
		Controller->OnEndPlay.RemoveDynamic(this, &ThisClass::HandleControllerEndPlay);
		if (bOwnsModalInput)
		{
			Controller->SetModalUiActive(false);
			Controller->bShowMouseCursor = bPreviousCursorVisible;
		}
	}
	if (UFF_FacilityInfoSubsystem* FacilityInfo = SuppressedFacilityInfo.Get())
	{
		// The inspector itself verifies the selected identity before restoring.
		FacilityInfo->SetPauseMenuPresentationSuppressed(false);
	}
	SuppressedFacilityInfo.Reset();
	if (Controller && bOwnsModalInput && bRestoreFocus) RestoreInput(*Controller);
	// A viewport can survive controller/world replacement. Clear our UIOnly block
	// even if that controller can no longer restore focus during EndPlay.
	if (bOwnsModalInput && InputViewport.IsValid())
	{
		InputViewport->SetIgnoreInput(bPreviousViewportIgnoredInput);
	}
	if (UFF_BuildModeSubsystem* BuildMode = SuspendedBuildMode.Get())
	{
		// Restores its own state and focus next tick, including click-through guards.
		BuildMode->ResumeInputAfterPauseMenu();
	}
	SuspendedBuildMode.Reset();
	OwningController.Reset();
	InputViewport.Reset();
	PreviousFocusedWidget.Reset();
	bOwnsModalInput = false;
	bPreviousCursorVisible = false;
	bPreviousViewportIgnoredInput = false;
}

void UFF_FacilityCodexPresenterSubsystem::RestoreInput(AFF_FestivalPlayerController& Controller)
{
	if (!bPreviousCursorVisible)
	{
		Controller.SetInputMode(FInputModeGameOnly());
		return;
	}
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	TSharedPtr<SWidget> Focus = PreviousFocusedWidget.Pin();
	FWidgetPath VisiblePath;
	if (Focus && Focus->SupportsKeyboardFocus() && FSlateApplication::IsInitialized() &&
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(
			Focus.ToSharedRef(), VisiblePath, EVisibility::Visible))
	{
		InputMode.SetWidgetToFocus(Focus);
	}
	Controller.SetInputMode(InputMode);
}

void UFF_FacilityCodexPresenterSubsystem::HandleStageChanged(
	const EFF_FestivalStage PreviousStage, const EFF_FestivalStage NewStage)
{
	(void)PreviousStage;
	(void)NewStage;
	CloseCodex();
}

void UFF_FacilityCodexPresenterSubsystem::HandleSnapshotRestored()
{
	CloseCodex();
}

void UFF_FacilityCodexPresenterSubsystem::HandleControllerEndPlay(
	AActor* Actor, const EEndPlayReason::Type EndPlayReason)
{
	(void)Actor;
	(void)EndPlayReason;
	CloseSession(false);
}

void UFF_FacilityCodexPresenterSubsystem::Deinitialize()
{
	bDeinitializing = true;
	CloseSession(false);
	Super::Deinitialize();
}
