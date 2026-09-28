// FF_FacilityCodexPresenterSubsystem.cpp
// 역할: 도감 열기·닫기와 선택 시설·카탈로그 이동을 연결한다.

#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"

#include "Building/FF_BuildModeSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/PanelWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/AssetManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSFacilityDefinition.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Layout/WidgetPath.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "Widgets/SWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogFFFacilityCodexPresenter, Log, All);

bool UFF_FacilityCodexPresenterSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

FSoftClassPath UFF_FacilityCodexPresenterSubsystem::GetDefaultScreenClassPath()
{
	return FSoftClassPath(FFBuildUIAssetPaths::FacilityCodexScreenClass);
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
	if (Screen || bOwnsModalInput || CatalogNavigationTimer.IsValid()) CloseSession(true);

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
	SuppressCatalogVisibility(*Controller);
	if (FacilityInfo)
	{
		SuppressedFacilityInfo = FacilityInfo;
		FacilityInfo->SetPauseMenuPresentationSuppressed(true);
	}
	Controller->SetModalUiActive(true);
	bOwnsModalInput = true;
	BindSessionLifecycle(*Controller);
	Candidate->OnCloseRequested.AddUObject(this, &ThisClass::CloseCodex);
	Candidate->OnFacilityCatalogRequested.AddUObject(this, &ThisClass::HandleFacilityCatalogRequested);
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

void UFF_FacilityCodexPresenterSubsystem::BindSessionLifecycle(AFF_FestivalPlayerController& Controller)
{
	Controller.OnEndPlay.AddUniqueDynamic(this, &ThisClass::HandleControllerEndPlay);
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			BoundStage = Stage;
			Stage->OnStageChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
		}
	}
	if (UFFZSWorldSubsystem* ZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>())
	{
		BoundZoneSystem = ZoneSystem;
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleSnapshotRestored);
	}
}

void UFF_FacilityCodexPresenterSubsystem::CloseCodex()
{
	CloseSession(true);
}

void UFF_FacilityCodexPresenterSubsystem::HandleFacilityCatalogRequested(const FName FacilityDefinitionId)
{
	UWorld* World = GetWorld();
	AFF_FestivalPlayerController* Controller = OwningController.Get();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_BuildCatalogSubsystem* Catalog = Game ? Game->GetSubsystem<UFF_BuildCatalogSubsystem>() : nullptr;
	if (bClosing || bDeinitializing || !IsCodexOpen() || !Controller || !Catalog ||
		FacilityDefinitionId.IsNone() || !World->GetSubsystem<UFF_BuildModeSubsystem>()) return;
	FFF_BuildCatalogEntry Entry;
	if (Catalog->IsCatalogReady())
	{
		if (!Catalog->FindEntry(FacilityDefinitionId, Entry)) return;
	}
	else
	{
		// The menu can still be loading, but a stale/unknown facility must not close
		// the codex. The catalog owns final validation once its async load completes.
		const FPrimaryAssetId AssetId(UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(), FacilityDefinitionId);
		if (!UAssetManager::Get().GetPrimaryAssetPath(AssetId).IsValid()) return;
	}

	CloseSession(true);
	// Closing releases the modal now and the suspended placement input next tick.
	// Keep lifecycle guards during that boundary so transitions cannot reopen UI.
	OwningController = Controller;
	BindSessionLifecycle(*Controller);
	CatalogNavigationTimer = World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::CompleteCatalogNavigation, FacilityDefinitionId));
}

void UFF_FacilityCodexPresenterSubsystem::CompleteCatalogNavigation(const FName FacilityDefinitionId)
{
	CatalogNavigationTimer.Invalidate();
	UWorld* World = GetWorld();
	AFF_FestivalPlayerController* Controller = OwningController.Get();
	UFF_BuildModeSubsystem* BuildMode = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	const bool bCanNavigate = !bDeinitializing && !IsCodexOpen() && Controller &&
		Controller->GetWorld() == World && Controller->IsLocalPlayerController() && BuildMode &&
		!BuildMode->IsInputSuspendedForPauseMenu() && !UGameplayStatics::IsGamePaused(World) &&
		Controller->GetInputBlockReason() != EFF_CameraInputBlock::ModalUi &&
		Controller->GetInputBlockReason() != EFF_CameraInputBlock::Scripted;
	CloseSession(false);
	// This route resets any old preview, reveals the requested category/page and
	// focuses its card. It never starts placement or spends construction funds.
	if (bCanNavigate) BuildMode->OpenCatalogForFacility(FacilityDefinitionId);
}

void UFF_FacilityCodexPresenterSubsystem::SuppressCatalogVisibility(AFF_FestivalPlayerController& Controller)
{
	const UFF_BuildModeSubsystem* BuildMode = GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>();
	if (!BuildMode || BuildMode->GetBuildModeState() != EFF_BuildModeState::CatalogOpen) return;
	TArray<UUserWidget*> Panels;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Panels, UFF_PrepPanelBuild::StaticClass(), false);
	for (UUserWidget* Widget : Panels)
	{
		UFF_PrepPanelBuild* Panel = Cast<UFF_PrepPanelBuild>(Widget);
		if (!Panel || Panel->GetWorld() != GetWorld() || Panel->GetOwningPlayer() != &Controller || !Panel->IsVisible()) continue;
		UFF_PrepHudBar* Bar = Panel->GetOwningBar();
		const bool bMounted = Panel->IsInViewport() || (Panel->GetParent() && Bar && Bar->GetSelection().IsSelected(EFF_PrepHudTab::Build));
		if (!bMounted) continue;
		SuppressedCatalog = Panel;
		SuppressedCatalogParent = Panel->GetParent();
		SuppressedCatalogBar = Bar;
		bCatalogWasInViewport = Panel->IsInViewport();
		bCatalogHadOwningBar = Bar != nullptr;
		PreviousCatalogVisibility = Panel->GetVisibility();
		// This is a temporary modal presentation lease, not a close/open of the
		// bar-owned browsing session. Filters, page, selection and subscriptions survive.
		Panel->SetVisibility(ESlateVisibility::Hidden);
		break;
	}
}

void UFF_FacilityCodexPresenterSubsystem::RestoreCatalogVisibility()
{
	UFF_PrepPanelBuild* Panel = SuppressedCatalog.Get();
	const UFF_BuildModeSubsystem* BuildMode = GetWorld() ? GetWorld()->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFF_PrepHudBar* Bar = SuppressedCatalogBar.Get();
	const bool bSameOwner = !bCatalogHadOwningBar || (Bar && Panel && Panel->GetOwningBar() == Bar &&
		Bar->GetSelection().IsSelected(EFF_PrepHudTab::Build));
	const bool bSameAttachment = Panel && Panel->GetParent() == SuppressedCatalogParent.Get() &&
		(bCatalogWasInViewport ? Panel->IsInViewport() : Panel->GetParent() != nullptr);
	if (Panel && Panel->GetOwningPlayer() == OwningController.Get() && bSameOwner && bSameAttachment &&
		BuildMode && BuildMode->GetBuildModeState() == EFF_BuildModeState::CatalogOpen &&
		Panel->GetVisibility() == ESlateVisibility::Hidden)
	{
		// If its owner closed, replaced or removed the panel while the codex was up,
		// do not resurrect it or overwrite that owner's newer visibility decision.
		Panel->SetVisibility(PreviousCatalogVisibility);
	}
	SuppressedCatalog.Reset();
	SuppressedCatalogParent.Reset();
	SuppressedCatalogBar.Reset();
	PreviousCatalogVisibility = ESlateVisibility::Collapsed;
	bCatalogWasInViewport = false;
	bCatalogHadOwningBar = false;
}

void UFF_FacilityCodexPresenterSubsystem::CloseSession(const bool bRestoreFocus)
{
	if (bClosing) return;
	TGuardValue<bool> ClosingGuard(bClosing, true);
	if (UWorld* World = GetWorld(); World && CatalogNavigationTimer.IsValid())
	{
		World->GetTimerManager().ClearTimer(CatalogNavigationTimer);
	}
	CatalogNavigationTimer.Invalidate();
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
		Screen->OnFacilityCatalogRequested.RemoveAll(this);
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
	RestoreCatalogVisibility();
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
