#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildFeatureGuideButton.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildingFeatureDiscoverySubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolbarFeaturePopupTest,
	"FestaFiesta.Building.FeatureGuide.ToolbarPopup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFBuildToolbarFeaturePopupTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Profile configuration"), GConfig)) return false;
	const FString PreviousProfile = GGameUserSettingsIni;
	const FString Profile = FPaths::ConvertRelativePathToFull(
		FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("BuildToolbarPopup"), TEXT(".ini")));
	GConfig->Add(Profile, FConfigFile());
	GGameUserSettingsIni = Profile;
	ON_SCOPE_EXIT { GGameUserSettingsIni = PreviousProfile; GConfig->UnloadFile(Profile); IFileManager::Get().Delete(*Profile); };

	UFFZSDeveloperSettings* ZoneSettings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> GroundChecks(ZoneSettings->bEnableGroundChecks, false);
	TGuardValue<bool> CollisionChecks(ZoneSettings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> NavChecks(ZoneSettings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(ZoneSettings->bRequireProductionPolicyDomains, false);
	TGuardValue<EFFZSChangePhase> InitialPhase(ZoneSettings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { Fixture.ForwardErrorMessages(this); return false; }
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	auto* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	auto* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	auto* Discovery = Game ? Game->GetSubsystem<UFF_BuildingFeatureDiscoverySubsystem>() : nullptr;
	auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr, FFBuildUIAssetPaths::BuildToolsClass));
	if (!TestNotNull(TEXT("Build subsystem"), Build) || !TestNotNull(TEXT("Zone authority"), Zones) ||
		!TestNotNull(TEXT("Discovery subsystem"), Discovery) || !TestNotNull(TEXT("Build tools WBP"), Class)) return false;
	const bool bPreviousPlayerless = Class->bCanCallInitializedWithoutPlayerContext;
	Class->bCanCallInitializedWithoutPlayerContext = true;
	ON_SCOPE_EXIT { Class->bCanCallInitializedWithoutPlayerContext = bPreviousPlayerless; };
	TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
	if (!TestNotNull(TEXT("Build tools instance"), Widget.Get())) return false;
	const TSharedRef<SWidget> Slate = Widget->TakeWidget();
	Build->BuildToolsWidget = Widget.Get();
	Build->State = EFF_BuildModeState::BuildMode;
	Build->bBuildSessionActive = true;
	Widget->RefreshTools();
	ON_SCOPE_EXIT { Build->BuildToolsWidget = nullptr; Build->State = EFF_BuildModeState::Closed; Build->bBuildSessionActive = false; };

	UWidget* Anchor = Widget->GetWidgetFromName(TEXT("Widget_ToolbarFeatureIcons"));
	auto* Action = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ToolbarFeatureAction")));
	auto* Close = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ToolbarFeatureClose")));
	auto* AllGuide = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ToolbarAllGuide")));
	auto* Status = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_ToolbarFeatureStatus")));
	if (!TestNotNull(TEXT("Toolbar icon anchor"), Anchor) || !TestNotNull(TEXT("Popup action"), Action) ||
		!TestNotNull(TEXT("Popup close"), Close) || !TestNotNull(TEXT("Full guide action"), AllGuide) ||
		!TestNotNull(TEXT("Popup status"), Status)) return false;
	auto* Toolbar = Cast<UUserWidget>(Anchor);
	if (!TestNotNull(TEXT("Authored icon toolbar"), Toolbar)) return false;
	auto* GridIcon = Cast<UFF_BuildFeatureGuideButton>(Toolbar->GetWidgetFromName(TEXT("Guide_grid")));
	if (!TestNotNull(TEXT("Authored grid icon"), GridIcon)) return false;
	const TSharedRef<SWidget> GridIconSlate = GridIcon->TakeWidget();
	auto* GridIconButton = Cast<UButton>(GridIcon->GetWidgetFromName(TEXT("Button_Guide")));
	if (!TestNotNull(TEXT("Grid icon button"), GridIconButton)) return false;
	GridIcon->RefreshGuidePresentation();
	TestEqual(TEXT("The inner icon button exposes its topic as tooltip"), GridIconButton->GetToolTipText().ToString(), FString(TEXT("격자")));
#if WITH_ACCESSIBILITY
	if (const TSharedPtr<SWidget> Accessible = GridIconButton->GetCachedWidget())
		TestEqual(TEXT("The inner icon button exposes its topic to accessibility"), Accessible->GetAccessibleText().ToString(), FString(TEXT("격자")));
	else AddError(TEXT("The icon button has no Slate widget to verify accessibility."));
#endif
	TestTrue(TEXT("Action uses native click routing"), Action->OnClicked.IsBound());
	TestTrue(TEXT("Close uses native click routing"), Close->OnClicked.IsBound());
	TestFalse(TEXT("Unknown IDs do not open a popup"), Widget->OpenToolbarFeatureTopic(TEXT("Unknown"), Anchor));
	for (const FName Topic : {FName(TEXT("MultiSelection")), FName(TEXT("BuildGrid")), FName(TEXT("BuildSnap")),
		FName(TEXT("FacilityRanges")), FName(TEXT("BuildSettings")), FName(TEXT("BuildCatalog")), FName(TEXT("BuildControls"))})
	{
		TestFalse(TEXT("Fresh topic starts unseen"), Discovery->HasSeenFeature(Topic));
		TestTrue(TEXT("Seven existing topics open"), Widget->OpenToolbarFeatureTopic(Topic, Anchor));
		TestTrue(TEXT("Seven existing actions remain available"), Action->IsVisible());
		TestFalse(TEXT("Merely reading does not acknowledge a feature"), Discovery->HasSeenFeature(Topic));
		Close->OnClicked.Broadcast();
		TestFalse(TEXT("Close dismisses the popup"), Widget->IsToolbarFeaturePopupOpen());
		TestFalse(TEXT("Closing does not record first use"), Discovery->HasSeenFeature(Topic));
	}
	TestTrue(TEXT("Information-only topic opens"), Widget->OpenToolbarFeatureTopic(TEXT("FacilityQueue"), Anchor));
	TestEqual(TEXT("Information-only topic exposes no fabricated action"), Action->GetVisibility(), ESlateVisibility::Collapsed);
	Action->OnClicked.Broadcast();
	TestFalse(TEXT("A forced hidden action cannot acknowledge that topic"), Discovery->HasSeenFeature(TEXT("FacilityQueue")));
	Close->OnClicked.Broadcast();
	for (const FName Topic : {FName(TEXT("BuildUndo")), FName(TEXT("BuildRedo"))})
	{
		TestTrue(TEXT("History topic opens"), Widget->OpenToolbarFeatureTopic(Topic, Anchor));
		TestEqual(TEXT("History action remains visible"), Action->GetVisibility(), ESlateVisibility::Visible);
		TestFalse(TEXT("Empty construction history disables the action"), Action->GetIsEnabled());
		TestTrue(TEXT("Empty history explains why the action is disabled"), !Status->GetText().IsEmpty() && Status->IsVisible());
		Action->OnClicked.Broadcast();
		TestFalse(TEXT("A disabled history action does not record first use"), Discovery->HasSeenFeature(Topic));
		TestTrue(TEXT("A forced disabled click leaves the popup open"), Widget->IsToolbarFeaturePopupOpen());
		Close->OnClicked.Broadcast();
	}

	// Exercise the popup against actual construction history and authority.
	auto* Economy = Game->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("History economy"), Economy)) return false;
	TStrongObjectPtr<UFF_FestivalDefinition> Festival(NewObject<UFF_FestivalDefinition>(Game));
	TStrongObjectPtr<UDataTable> Costs(NewObject<UDataTable>(Game));
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
	FFF_FacilityCostRow Cost; Cost.InstallCost = 100;
	Costs->AddRow(Spec.InstallCostKey, Cost);
	Festival->FestivalId = TEXT("ToolbarHistoryFixture");
	Festival->FacilityCostTable = Costs.Get();
	Festival->InitialGold = 10000;
	Economy->StartFestival(Festival.Get());
	TestTrue(TEXT("Preparation phase is available"), Zones->SetChangePhase(EFFZSChangePhase::InitialSetup));
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!FFProjectZoneTest::RegisterZone(*this, Zone, Zones)) return false;
	FFFZSGridPlacement HistoryPlacement; HistoryPlacement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, Zone->ZoneId, HistoryPlacement, -1);
	const FFFZSCommandResult Installed = Zones->ExecuteCommand(Install);
	if (!TestTrue(*FString::Printf(TEXT("History fixture installs: %s"), *Installed.Message.ToString()), Installed.bSucceeded)) return false;
	TestTrue(TEXT("Undo history is available in preparation"), Build->CanUndoBuildEdit() && Build->CanExecuteBuildHistory());
	TestTrue(TEXT("Undo popup opens with history"), Widget->OpenToolbarFeatureTopic(TEXT("BuildUndo"), Anchor));
	TestTrue(TEXT("Undo action is enabled"), Action->GetIsEnabled());
	Action->OnClicked.Broadcast();
	TestNull(TEXT("Popup undo removes the installed facility"), Zones->FindFacility(Install.FacilityInstanceId));
	TestTrue(TEXT("Undo creates redo history"), Build->CanRedoBuildEdit());
	TestTrue(TEXT("Redo popup opens with history"), Widget->OpenToolbarFeatureTopic(TEXT("BuildRedo"), Anchor));
	TestTrue(TEXT("Redo action is enabled"), Action->GetIsEnabled());
	Action->OnClicked.Broadcast();
	TestNotNull(TEXT("Popup redo restores the facility"), Zones->FindFacility(Install.FacilityInstanceId));
	TestFalse(TEXT("Undo does not acknowledge a guide"), Discovery->HasSeenFeature(TEXT("BuildUndo")));
	TestFalse(TEXT("Redo does not acknowledge a guide"), Discovery->HasSeenFeature(TEXT("BuildRedo")));
	TestTrue(TEXT("Phase may change to live"), Zones->SetChangePhase(EFFZSChangePhase::LiveOperation));
	TestFalse(TEXT("Live phase disallows history execution"), Build->CanExecuteBuildHistory());
	TestTrue(TEXT("Live phase history popup remains informative"), Widget->OpenToolbarFeatureTopic(TEXT("BuildUndo"), Anchor));
	TestFalse(TEXT("Live phase disables undo"), Action->GetIsEnabled());
	TestTrue(TEXT("Live phase gives a reason"), Status->IsVisible() && !Status->GetText().IsEmpty());
	const int64 LiveRevision = Zones->GetWorldRevision();
	Action->OnClicked.Broadcast();
	TestEqual(TEXT("Forced live-phase click preserves world"), Zones->GetWorldRevision(), LiveRevision);
	TestNotNull(TEXT("Forced live-phase click preserves facility"), Zones->FindFacility(Install.FacilityInstanceId));
	Close->OnClicked.Broadcast();
	TestTrue(TEXT("Return to preparation"), Zones->SetChangePhase(EFFZSChangePhase::InitialSetup));

	const bool bGridBefore = Build->IsPlacementGridVisible();
	TestTrue(TEXT("Grid popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("BuildGrid"), GridIcon));
	TestTrue(TEXT("Popup remembers the originating icon"), Widget->ToolbarFeatureAnchor.Get() == GridIcon);
	TestEqual(TEXT("Opening information does not execute grid"), Build->IsPlacementGridVisible(), bGridBefore);
	Action->OnClicked.Broadcast();
	TestEqual(TEXT("Explicit popup action executes the existing grid operation"), Build->IsPlacementGridVisible(), !bGridBefore);
	TestTrue(TEXT("Explicit action records the acknowledged feature"), Discovery->HasSeenFeature(TEXT("BuildGrid")));
	TestFalse(TEXT("Action dismisses the popup"), Widget->IsToolbarFeaturePopupOpen());
	const int32 SnapBefore = Build->GetPreferredPositionSnapCm();
	TestTrue(TEXT("Snap popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("BuildSnap"), Anchor));
	Action->OnClicked.Broadcast();
	TestEqual(TEXT("Snap action reaches existing step cycle"), Build->GetPreferredPositionSnapCm(),
		SnapBefore == 100 ? 25 : SnapBefore == 25 ? 1 : 100);
	TestTrue(TEXT("Snap action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("BuildSnap")));
	const bool bRangesBefore = Build->AreFacilityRangesVisible();
	TestTrue(TEXT("Ranges popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("FacilityRanges"), Anchor));
	Action->OnClicked.Broadcast();
	TestEqual(TEXT("Ranges action reaches existing toggle"), Build->AreFacilityRangesVisible(), !bRangesBefore);
	TestTrue(TEXT("Ranges action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("FacilityRanges")));
	const bool bHelpBefore = Widget->bHelpExpanded;
	TestTrue(TEXT("Help popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("BuildControls"), Anchor));
	TestTrue(TEXT("Help popup retains access to the full guide"), AllGuide->IsVisible());
	Action->OnClicked.Broadcast();
	TestEqual(TEXT("Help action reaches existing toggle"), Widget->bHelpExpanded, !bHelpBefore);
	TestTrue(TEXT("Help action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("BuildControls")));
	TestTrue(TEXT("Settings popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("BuildSettings"), Anchor));
	Action->OnClicked.Broadcast();
	TestTrue(TEXT("Settings action opens the existing drawer"), Widget->IsBuildSettingsOpen());
	TestTrue(TEXT("Settings action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("BuildSettings")));
	Widget->ToggleBuildSettings();
	TestTrue(TEXT("Multi-selection popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("MultiSelection"), Anchor));
	Action->OnClicked.Broadcast();
	TestTrue(TEXT("Multi-selection action starts the existing mode"), Build->IsMultiSelectionActive());
	TestTrue(TEXT("Multi-selection action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("MultiSelection")));
	Build->ToggleMultiSelection();

	TestTrue(TEXT("Popup can reopen"), Widget->OpenToolbarFeatureTopic(TEXT("BuildSnap"), Anchor));
	const FKeyEvent Tab(EKeys::Tab, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("Tab is handled by popup navigation"), Widget->NativeOnPreviewKeyDown(FGeometry(), Tab).IsEventHandled());
	const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
	TestTrue(TEXT("Escape is handled"), Widget->NativeOnPreviewKeyDown(FGeometry(), Escape).IsEventHandled());
	TestFalse(TEXT("Escape closes the popup"), Widget->IsToolbarFeaturePopupOpen());
	TestTrue(TEXT("Popup can reopen for back navigation"), Widget->OpenToolbarFeatureTopic(TEXT("BuildSnap"), Anchor));
	Widget->HandleBuildBack();
	TestFalse(TEXT("Back closes the popup before leaving build mode"), Widget->IsToolbarFeaturePopupOpen());
	TestEqual(TEXT("Back preserves the build session"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);

	auto* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Input controller"), Controller)) return false;
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	TestTrue(TEXT("Popup opens before world input"), Widget->OpenToolbarFeatureTopic(TEXT("BuildGrid"), Anchor));
	const bool bGridDuringPopup = Build->IsPlacementGridVisible();
	const int64 Revision = Zones->GetWorldRevision();
	constexpr float Delta = 1.f / 60.f;
	TArray<UInputComponent*> InputStack;
	Controller->PlayerInput->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::G, IE_Pressed, 1.f));
	Controller->PlayerInput->ProcessInputStack(InputStack, Delta, false);
	Build->HandleInput(*Controller, Delta);
	TestEqual(TEXT("Popup blocks raw world shortcuts"), Build->IsPlacementGridVisible(), bGridDuringPopup);
	TestEqual(TEXT("Popup does not alter world construction"), Zones->GetWorldRevision(), Revision);
	TestTrue(TEXT("Closing-frame pointer is suppressed"), Build->bSuppressWorldClickUntilReleased);
	// A collapsed ancestor does not clear its child's cached Slate geometry.
	// Render once, then keep the cursor at the exact same point while closing.
	if (!FParse::Param(FCommandLine::Get(), TEXT("nullrhi")) && FSlateApplication::IsInitialized())
	{
		const FVector2D ViewSize(1920.f, 1080.f);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(ViewSize, TF_Bilinear, true));
		if (!TestNotNull(TEXT("Popup render target"), Target.Get())) return false;
		Renderer.DrawWidget(Target.Get(), Slate, ViewSize, 0.f, false);
		FlushRenderingCommands();
		UWidget* Surface = Widget->GetWidgetFromName(TEXT("Border_ToolbarFeaturePopup"));
		if (!TestNotNull(TEXT("Popup input surface"), Surface)) return false;
		const FGeometry& SurfaceGeometry = Surface->GetCachedGeometry();
		if (!TestTrue(TEXT("Popup has realized geometry"), SurfaceGeometry.GetLocalSize().X > 1.f && SurfaceGeometry.GetLocalSize().Y > 1.f)) return false;
		const FVector2D PreviousCursor = FSlateApplication::Get().GetCursorPos();
		ON_SCOPE_EXIT { FSlateApplication::Get().SetCursorPos(PreviousCursor); };
		FSlateApplication::Get().SetCursorPos(SurfaceGeometry.LocalToAbsolute(SurfaceGeometry.GetLocalSize() * .5f));
		TestTrue(TEXT("Open popup blocks its surface"), Widget->IsPointerOverTools());
		Widget->CloseToolbarFeaturePopup();
		TestFalse(TEXT("Closed popup's stale geometry no longer blocks the same world point"), Widget->IsPointerOverTools());
	}
	else
	{
		AddWarning(TEXT("Popup pointer-geometry regression needs a rendering RHI and Slate."));
		Widget->CloseToolbarFeaturePopup();
	}
	if (FSlateApplication::IsInitialized() && GEngine)
	{
		FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
		if (!TestNotNull(TEXT("Focus world context"), Context)) return false;
		auto* Viewport = NewObject<UGameViewportClient>(GEngine);
		Context->GameViewport = Viewport;
		Viewport->Init(*Context, Game, false);
		FString PlayerError;
		ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
		auto* FocusController = World->SpawnActor<APlayerController>();
		if (!TestNotNull(TEXT("Focus local player"), Player) || !TestNotNull(TEXT("Focus player controller"), FocusController)) return false;
		FocusController->SetPlayer(Player);
		World->AddController(FocusController);
		TStrongObjectPtr<UFF_BuildToolsWidget> FocusWidget(CreateWidget<UFF_BuildToolsWidget>(FocusController, Class));
		if (!TestNotNull(TEXT("Player-owned toolbar"), FocusWidget.Get())) return false;
		const TSharedRef<SWidget> FocusSlate = FocusWidget->TakeWidget();
		FocusWidget->RefreshTools();
		auto* FocusToolbar = Cast<UUserWidget>(FocusWidget->GetWidgetFromName(TEXT("Widget_ToolbarFeatureIcons")));
		if (!TestNotNull(TEXT("Player-owned icon toolbar"), FocusToolbar)) return false;
		FocusToolbar->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* FocusIcon = Cast<UFF_BuildFeatureGuideButton>(FocusToolbar->GetWidgetFromName(TEXT("Guide_grid")));
		if (!TestNotNull(TEXT("Player-owned grid icon"), FocusIcon)) return false;
		auto* FocusButton = Cast<UButton>(FocusIcon->GetWidgetFromName(TEXT("Button_Guide")));
		if (!TestNotNull(TEXT("Player-owned inner button"), FocusButton)) return false;
		auto* SettingsIcon = Cast<UFF_BuildFeatureGuideButton>(FocusToolbar->GetWidgetFromName(TEXT("Guide_settings")));
		if (!TestNotNull(TEXT("Player-owned settings icon"), SettingsIcon)) return false;
		FocusWidget->ToggleBuildSettings();
		TestTrue(TEXT("Settings drawer opens"), FocusWidget->IsBuildSettingsOpen());
		TestFalse(TEXT("Other icon stays blocked while settings is open"), FocusIcon->TryOpenGuide());
		TestTrue(TEXT("Settings icon closes its own open drawer"), SettingsIcon->TryOpenGuide());
		TestFalse(TEXT("Settings drawer is closed by its icon"), FocusWidget->IsBuildSettingsOpen());
		const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
		const TSharedRef<SVirtualWindow> FocusWindow = SNew(SVirtualWindow).Size(FVector2D(1920, 1080));
		FocusWindow->SetContent(FocusSlate);
		FSlateApplication::Get().RegisterVirtualWindow(FocusWindow);
		FocusWindow->SlatePrepass();
		ON_SCOPE_EXIT
		{
			FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
			FSlateApplication::Get().UnregisterVirtualWindow(FocusWindow);
			if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
		};
		TestTrue(TEXT("A player-owned icon opens its popup"), FocusWidget->OpenToolbarFeatureTopic(TEXT("BuildGrid"), FocusIcon));
		FocusWidget->CloseToolbarFeaturePopup();
		TestTrue(TEXT("Closing restores the owning user's focus to the originating icon"), FocusButton->HasUserFocus(FocusController));
		TestTrue(TEXT("The icon can reopen its popup"), FocusWidget->OpenToolbarFeatureTopic(TEXT("BuildGrid"), FocusIcon));
		const FKeyEvent FocusEscape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
		TestTrue(TEXT("Escape closes the player-owned popup"), FocusWidget->NativeOnPreviewKeyDown(FGeometry(), FocusEscape).IsEventHandled());
		TestTrue(TEXT("Escape restores focus to the originating icon"), FocusButton->HasUserFocus(FocusController));
	}
	else AddWarning(TEXT("Focus restoration needs Slate and an engine context."));
	// The isolated world has no festival stage; the real catalog action must
	// therefore respect the existing denial path instead of inventing a session.
	if (!Build->IsBuildEntryAllowed())
	{
		TestTrue(TEXT("Catalog popup opens"), Widget->OpenToolbarFeatureTopic(TEXT("BuildCatalog"), Anchor));
		Action->OnClicked.Broadcast();
		TestTrue(TEXT("Catalog action records its first-use confirmation"), Discovery->HasSeenFeature(TEXT("BuildCatalog")));
		TestEqual(TEXT("Catalog action reaches the existing stage denial path"), Build->GetBuildModeState(), EFF_BuildModeState::Closed);
	}
	else AddWarning(TEXT("Catalog dispatch denial assertion skipped because this isolated world allows building."));
	return true;
}

#endif
