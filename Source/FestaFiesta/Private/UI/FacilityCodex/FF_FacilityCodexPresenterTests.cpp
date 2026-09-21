#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/Codex/FF_FacilityCodexSubsystem.h"
#include "Building/FF_BuildCatalogSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Tickable.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UI/PrepHUD/Panels/FF_PrepPanelBuild.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexPresenterLifecycleTest,
	"FestaFiesta.UI.FacilityCodex.PresenterLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexPresenterLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// Exercise the production WBP, controller and subsystem APIs in an isolated
	// game world. The private viewport has no window and never touches the user's PIE.
	FTestWorldWrapper Fixture;
	const uint64 OriginalFrameCounter = GFrameCounter;
	ON_SCOPE_EXIT { GFrameCounter = OriginalFrameCounter; };
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	// This fixture intentionally does not BeginPlay (that would spawn the complete
	// festival). FTestWorldWrapper only advances GFrameCounter after BeginPlay, so
	// advance it explicitly or TimerManager ignores every tick after the first.
	const auto AdvanceFrame = [&Fixture]()
	{
		++GFrameCounter;
		return Fixture.TickTestWorld();
	};
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_FacilityCodexPresenterSubsystem* Presenter = World
		? World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>() : nullptr;
	UFF_FacilityInfoSubsystem* Info = World ? World->GetSubsystem<UFF_FacilityInfoSubsystem>() : nullptr;
	UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	if (!TestNotNull(TEXT("Game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
		!TestNotNull(TEXT("Presenter"), Presenter) || !TestNotNull(TEXT("Facility inspector"), Info) ||
		!TestNotNull(TEXT("Build subsystem"), Build) || !TestNotNull(TEXT("Zone subsystem"), Zones)) return false;
	FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
	if (!TestNotNull(TEXT("World context"), Context)) return false;
	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	const TSharedRef<SViewport> ViewportWidget = SNew(SViewport);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	TUniquePtr<FSceneViewport> SceneViewport = MakeUnique<FSceneViewport>(Viewport, ViewportWidget);
	Viewport->Viewport = SceneViewport.Get();
	ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	AFF_FestivalPlayerController* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Local player: %s"), *PlayerError), Player) ||
		!TestNotNull(TEXT("Festival controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	World->AddController(Controller);
	Controller->bShowMouseCursor = true;
	UFF_FestivalStageSubsystem* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("Festival stage"), Stage)) return false;
	Stage->CurrentStage = EFF_FestivalStage::Setup;
	Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
	const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	ON_SCOPE_EXIT
	{
		Presenter->CloseCodex();
		Build->ExitBuildMode();
		if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
		else FSlateApplication::Get().ClearKeyboardFocus();
	};

	TestFalse(TEXT("Null controller is rejected without acquiring input"), Presenter->OpenCodex(nullptr));
	Controller->SetModalUiActive(true);
	TestFalse(TEXT("Another modal keeps exclusive ownership"), Presenter->OpenCodex(Controller));
	Presenter->CloseCodex();
	TestEqual(TEXT("Closing an unopened codex preserves another modal"),
		Controller->GetInputBlockReason(), EFF_CameraInputBlock::ModalUi);
	Controller->SetModalUiActive(false);
	Info->SetPauseMenuPresentationSuppressed(true);
	TestFalse(TEXT("Another facility presentation lease blocks entry"), Presenter->OpenCodex(Controller));
	Presenter->CloseCodex();
	TestTrue(TEXT("An unowned inspector lease remains suppressed"), Info->IsPauseMenuPresentationSuppressed());
	Info->SetPauseMenuPresentationSuppressed(false);

	Build->EnterBuildMode();
	if (!TestEqual(TEXT("Fixture enters the normal build session"),
		Build->GetBuildModeState(), EFF_BuildModeState::BuildMode)) return false;
	// Regression: selecting a facility must not silently narrow the default launcher.
	// Use a real registered instance and the actual nested inspector WBP button.
	UFF_FacilityCodexSubsystem* Codex = World->GetSubsystem<UFF_FacilityCodexSubsystem>();
	if (!TestNotNull(TEXT("Production codex read model"), Codex)) return false;
	Codex->Refresh();
	const int32 AllRecipeCount = Codex->GetEntriesView().Num();
	FName SelectedDefinitionId;
	int32 SelectedRecipeCount = 0;
	for (const FFF_FacilityCodexEntry& Entry : Codex->GetEntriesView())
	{
		FFF_FacilityCodexFilter SelectedFilter;
		SelectedFilter.FacilityDefinitionId = Entry.TargetFacilityId;
		const int32 MatchCount = Codex->GetFilteredEntries(SelectedFilter).Num();
		if (MatchCount > 0 && MatchCount < AllRecipeCount)
		{
			SelectedDefinitionId = Entry.TargetFacilityId;
			SelectedRecipeCount = MatchCount;
			break;
		}
	}
	if (!TestFalse(TEXT("Authored data has a facility matching only part of the collection"), SelectedDefinitionId.IsNone())) return false;
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
	if (!TestNotNull(TEXT("Launcher fixture zone"), Zone) ||
		!TestTrue(TEXT("Launcher fixture zone preset"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
		!TestTrue(TEXT("Launcher fixture zone registers"), Zones->RegisterZone(Zone))) return false;
	FFFZSFacilitySpec FacilitySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	FacilitySpec.FacilityDefinitionId = SelectedDefinitionId;
	// Only the ID is borrowed for collection filtering; keep this synthetic FoodTruck's
	// price explicit so newly added recipe categories cannot change the routing fixture.
	FacilitySpec.InstallCostKey = TEXT("Cost.Booth.FoodStall.Burger");
	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec, ZoneId, Placement, Zones->GetWorldRevision());
	{
		// The fixture has no terrain/nav scene; UI routing is the behavior under test.
		UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
		TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
		TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
		TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
		TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
		const FFFZSCommandResult Installed = Zones->ExecuteCommand(Install);
		if (!TestTrue(*FString::Printf(TEXT("Launcher fixture installs: %s"), *Installed.Message.ToString()), Installed.bSucceeded)) return false;
	}
	AFFZSFacilityActor* SelectedFacility = Zones->FindFacility(Install.FacilityInstanceId);
	if (!TestNotNull(TEXT("Selected facility is registered"), SelectedFacility) ||
		!TestTrue(TEXT("Actual inspector selects the installed facility"),
			Info->SelectFacility(Controller, SelectedFacility, EFF_FacilityInfoContext::Build))) return false;
	TArray<UUserWidget*> Inspectors;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Inspectors, UFF_FacilityInfoPanel::StaticClass(), true);
	UFF_FacilityInfoPanel* Inspector = Inspectors.IsEmpty() ? nullptr : Cast<UFF_FacilityInfoPanel>(Inspectors[0]);
	if (!TestNotNull(TEXT("Authored inspector instance"), Inspector)) return false;
	Inspector->SetInfoPage(EFF_FacilityInfoPage::Synergy);
	UFF_FacilityCodexLauncher* Launcher = Cast<UFF_FacilityCodexLauncher>(
		Inspector->GetWidgetFromName(TEXT("Widget_FacilityCodexLauncher")));
	UButton* OpenButton = Launcher ? Cast<UButton>(Launcher->GetWidgetFromName(TEXT("Button_OpenCodex"))) : nullptr;
	if (!TestNotNull(TEXT("Actual nested launcher button"), OpenButton)) return false;
	OpenButton->OnClicked.Broadcast();
	if (!TestTrue(TEXT("Authored launcher opens the codex from the selected facility"), Presenter->IsCodexOpen())) return false;
	TArray<UUserWidget*> CodexWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
	UFF_FacilityCodexScreen* OpenScreen = CodexWidgets.IsEmpty() ? nullptr : Cast<UFF_FacilityCodexScreen>(CodexWidgets[0]);
	if (!TestNotNull(TEXT("Launcher-created codex screen"), OpenScreen)) return false;
	TestEqual(TEXT("Default launcher displays every configured recipe despite inspector selection"),
		OpenScreen->GetVisibleEntryCount(), AllRecipeCount);
	TestTrue(TEXT("Default launcher includes recipes unrelated to the selected facility"),
		OpenScreen->GetVisibleEntryCount() > SelectedRecipeCount);
	TestEqual(TEXT("Opening the full collection preserves inspector selection"), Info->GetSelectedFacility(), SelectedFacility);
	TestTrue(TEXT("Codex owns a viewport surface"), Presenter->IsCodexOpen());
	TestEqual(TEXT("Camera and world picking are blocked"),
		Controller->GetInputBlockReason(), EFF_CameraInputBlock::ModalUi);
	TestTrue(TEXT("UIOnly blocks game viewport input"), Viewport->IgnoreInput());
	TestTrue(TEXT("Raw build polling is suspended"), Build->IsInputSuspendedForPauseMenu());
	TestTrue(TEXT("Inspector does not remain clickable above the codex"), Info->IsPauseMenuPresentationSuppressed());
	TestFalse(TEXT("Reading the codex never pauses the festival clock"), UGameplayStatics::IsGamePaused(World));
	Build->ToggleBuildMode();
	TestEqual(TEXT("A build toggle cannot leak through the codex"),
		Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);
	TestTrue(TEXT("Repeated open updates the same session"), Presenter->OpenCodex(Controller, TEXT("Facility.Codex.Test")));
	CodexWidgets.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
	TestEqual(TEXT("Repeated open creates only one screen"), CodexWidgets.Num(), 1);
	Presenter->CloseCodex();
	Presenter->CloseCodex();
	TestFalse(TEXT("Repeated close removes the surface"), Presenter->IsCodexOpen());
	TestFalse(TEXT("Game input is restored"), Viewport->IgnoreInput());
	TestFalse(TEXT("Inspector presentation is released"), Info->IsPauseMenuPresentationSuppressed());
	TestTrue(TEXT("Advance past close input restoration"), AdvanceFrame());
	TestFalse(TEXT("Build resumes on its own next-tick boundary"), Build->IsInputSuspendedForPauseMenu());
	TestEqual(TEXT("Build state survives reading and closing"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode);

	// The second entry point lives inside an already-open catalog. Opening and
	// closing the codex must not close/recreate its browsing session or reset filters.
	{
		UClass* PrepClass = LoadClass<UFF_PrepPanelBuild>(nullptr,
			TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Catalog/WBP_FF_PrepPanelBuild.WBP_FF_PrepPanelBuild_C"));
		if (!TestNotNull(TEXT("Production prep catalog WBP"), PrepClass)) return false;
		// This fixture deliberately detaches the catalog and inspects it afterwards.
		// A preview world requests GC when it closes, so a raw local cannot own that
		// detached widget through the next frame and the scope-exit cleanup below.
		TStrongObjectPtr<UFF_PrepPanelBuild> PrepOwner(CreateWidget<UFF_PrepPanelBuild>(Controller, PrepClass));
		UFF_PrepPanelBuild* Prep = PrepOwner.Get();
		if (!TestNotNull(TEXT("Production prep catalog instance"), Prep)) return false;
		Prep->AddToViewport();
		Prep->HandlePanelOpened();
		ON_SCOPE_EXIT
		{
			Presenter->CloseCodex();
			Prep->HandlePanelClosed();
			Prep->RemoveFromParent();
		};
		UFF_BuildCatalogSubsystem* Catalog = Game->GetSubsystem<UFF_BuildCatalogSubsystem>();
		if (!TestNotNull(TEXT("Construction catalog"), Catalog)) return false;
		// Flushing packages does not dispatch StreamableManager's frame-delayed
		// completion delegates. They tick in the engine's null-world pass, which
		// this isolated world's TickTestWorld deliberately does not run. Deliver
		// those real callbacks without changing delay CVars or bypassing catalog loading.
		constexpr int32 MaxCatalogCompletionFrames = 8;
		for (int32 CompletionFrame = 0;
			Catalog->GetLoadState() == EFF_BuildCatalogLoadState::Loading && CompletionFrame < MaxCatalogCompletionFrames;
			++CompletionFrame)
		{
			FlushAsyncLoading();
			if (!TestTrue(TEXT("Advance catalog completion frame"), AdvanceFrame())) return false;
			FTickableGameObject::TickObjects(nullptr, LEVELTICK_All, false, 0.0f);
		}
		if (!TestTrue(TEXT("Catalog definitions finish loading"), Catalog->IsCatalogReady())) return false;
		if (!TestEqual(TEXT("Prep owns a live catalog session"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen)) return false;
		UEditableTextBox* Search = Cast<UEditableTextBox>(Prep->GetWidgetFromName(TEXT("Search_Facilities")));
		UFF_FacilityCodexLauncher* CatalogLauncher = Cast<UFF_FacilityCodexLauncher>(
			Prep->GetWidgetFromName(TEXT("Widget_FacilityCodexCatalogLauncher")));
		UButton* CatalogOpenButton = CatalogLauncher
			? Cast<UButton>(CatalogLauncher->GetWidgetFromName(TEXT("Button_OpenCodex"))) : nullptr;
		if (!TestNotNull(TEXT("Catalog search box"), Search) ||
			!TestNotNull(TEXT("Catalog header codex button"), CatalogOpenButton)) return false;
		const FText SearchBefore = FText::FromString(TEXT("부스"));
		Search->SetText(SearchBefore);
		if (!TestTrue(TEXT("Authored booth search spans multiple pages"), Prep->GetCatalogPageCount() > 1)) return false;
		Prep->SetCatalogPage(1);
		const int32 PageBefore = Prep->GetCatalogPageIndex();
		const TArray<UObject*> PageItemsBefore = Prep->GetCatalogPageItems();
		const ESlateVisibility VisibilityBefore = Prep->GetVisibility();
		CatalogOpenButton->OnClicked.Broadcast();
		if (!TestTrue(TEXT("Search-adjacent launcher opens the codex"), Presenter->IsCodexOpen())) return false;
		TestEqual(TEXT("Opening codex retains CatalogOpen state"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
		TestTrue(TEXT("Catalog input is suspended beneath the codex"), Build->IsInputSuspendedForPauseMenu());
		TestTrue(TEXT("Catalog instance remains mounted beneath the input shield"), Prep->IsInViewport());
		TestEqual(TEXT("Catalog cannot show behind the medium-sized codex"), Prep->GetVisibility(), ESlateVisibility::Hidden);
		TestTrue(TEXT("Repeated codex open preserves the existing catalog lease"), Presenter->OpenCodex(Controller));
		TestEqual(TEXT("Repeated open keeps the original catalog hidden"), Prep->GetVisibility(), ESlateVisibility::Hidden);
		CodexWidgets.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
		UFF_FacilityCodexScreen* CatalogCodex = CodexWidgets.IsEmpty()
			? nullptr : Cast<UFF_FacilityCodexScreen>(CodexWidgets[0]);
		if (!TestNotNull(TEXT("Catalog-launched codex screen"), CatalogCodex)) return false;
		TestEqual(TEXT("Catalog search never becomes an implicit codex filter"),
			CatalogCodex->GetVisibleEntryCount(), Codex->GetEntriesView().Num());
		Presenter->CloseCodex();
		TestTrue(TEXT("Advance catalog close restoration"), AdvanceFrame());
		TestFalse(TEXT("Catalog input resumes after closing codex"), Build->IsInputSuspendedForPauseMenu());
		TestEqual(TEXT("Closing codex returns to the same catalog state"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
		TestTrue(TEXT("Catalog search text survives codex open and close"), Search->GetText().EqualTo(SearchBefore));
		TestEqual(TEXT("The second catalog page survives codex open and close"), Prep->GetCatalogPageIndex(), PageBefore);
		TestTrue(TEXT("Catalog item identities and order survive codex open and close"), Prep->GetCatalogPageItems() == PageItemsBefore);
		TestTrue(TEXT("The same catalog instance is still mounted after closing codex"), Prep->IsInViewport());
		TestEqual(TEXT("Closing codex restores the original catalog visibility"), Prep->GetVisibility(), VisibilityBefore);

		// Follow both authored detail links through the presenter, including the
		// next-tick input handoff. Focusing a card must never activate placement.
		for (const bool bTargetLink : {true, false})
		{
			Search->SetText(FText::FromString(TEXT("No catalog facility matches this filter")));
			CatalogOpenButton->OnClicked.Broadcast();
			if (!TestTrue(TEXT("Codex opens for facility catalog navigation"), Presenter->IsCodexOpen())) return false;
			CodexWidgets.Reset();
			UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
			UFF_FacilityCodexScreen* NavigationScreen = CodexWidgets.IsEmpty()
				? nullptr : Cast<UFF_FacilityCodexScreen>(CodexWidgets[0]);
			if (!TestNotNull(TEXT("Navigation codex screen"), NavigationScreen)) return false;
			FFF_FacilityCodexEntry NavigationEntry;
			if (!TestTrue(TEXT("Selected navigation recipe exists"),
				Codex->FindEntry(NavigationScreen->GetSelectedRuleId(), NavigationEntry))) return false;
			const FName ExpectedId = bTargetLink ? NavigationEntry.TargetFacilityId : NavigationEntry.PartnerFacilityId;
			UButton* FacilityLink = Cast<UButton>(NavigationScreen->GetWidgetFromName(
				bTargetLink ? TEXT("Button_TargetCatalog") : TEXT("Button_PartnerCatalog")));
			if (!TestNotNull(TEXT("Authored facility catalog link"), FacilityLink)) return false;
			NavigationScreen->OnFacilityCatalogRequested.Broadcast(TEXT("Facility.Codex.Missing"));
			TestTrue(TEXT("An unknown facility cannot dismiss the codex"), Presenter->IsCodexOpen());
			FacilityLink->OnClicked.Broadcast();
			TestFalse(TEXT("Facility detail click closes the codex"), Presenter->IsCodexOpen());
			TestTrue(TEXT("Navigation retains the closing-click input guard"), Build->IsInputSuspendedForPauseMenu());
			if (!TestTrue(TEXT("Advance facility catalog navigation"), AdvanceFrame())) return false;
			TestFalse(TEXT("Catalog navigation releases modal input"), Viewport->IgnoreInput());
			TestFalse(TEXT("Catalog navigation resumes build input"), Build->IsInputSuspendedForPauseMenu());
			TestEqual(TEXT("A detail link opens catalog browsing without placing"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);
			TestEqual(TEXT("The clicked facility is selected in the catalog"), Prep->GetSelectedCatalogFacilityId(), ExpectedId);
			TestTrue(TEXT("Navigation clears a search that would hide the facility"), Search->GetText().IsEmpty());
			TestTrue(TEXT("Navigation never selects a facility for installation"), Build->GetSelectedFacilityDefinitionId().IsNone());
			TestTrue(TEXT("Navigation keeps the same catalog panel mounted"), Prep->IsInViewport());
			TestEqual(TEXT("Navigation restores catalog visibility"), Prep->GetVisibility(), VisibilityBefore);
		}

		const FName SelectedBeforeSnapshot = Prep->GetSelectedCatalogFacilityId();
		CatalogOpenButton->OnClicked.Broadcast();
		CodexWidgets.Reset();
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
		UFF_FacilityCodexScreen* PendingScreen = CodexWidgets.IsEmpty()
			? nullptr : Cast<UFF_FacilityCodexScreen>(CodexWidgets[0]);
		if (!TestNotNull(TEXT("Codex before navigation cancellation"), PendingScreen)) return false;
		FFF_FacilityCodexEntry PendingEntry;
		if (!TestTrue(TEXT("Pending navigation recipe exists"), Codex->FindEntry(PendingScreen->GetSelectedRuleId(), PendingEntry)) ||
			!TestTrue(TEXT("Cancelled request would change selection"), PendingEntry.TargetFacilityId != SelectedBeforeSnapshot)) return false;
		UButton* PendingLink = Cast<UButton>(PendingScreen->GetWidgetFromName(TEXT("Button_TargetCatalog")));
		if (!TestNotNull(TEXT("Target link before snapshot cancellation"), PendingLink)) return false;
		PendingLink->OnClicked.Broadcast();
		Zones->OnWorldSnapshotRestored.Broadcast();
		if (!TestTrue(TEXT("Advance after cancelled catalog navigation"), AdvanceFrame())) return false;
		TestEqual(TEXT("A snapshot cancels navigation before it changes catalog selection"),
			Prep->GetSelectedCatalogFacilityId(), SelectedBeforeSnapshot);
		TestFalse(TEXT("Cancelled navigation leaves no modal input lease"), Build->IsInputSuspendedForPauseMenu());

		if (!TestTrue(TEXT("Codex opens before an owner visibility change"), Presenter->OpenCodex(Controller))) return false;
		Prep->SetVisibility(ESlateVisibility::Collapsed);
		Presenter->CloseCodex();
		TestEqual(TEXT("A newer owner visibility decision wins over modal restoration"), Prep->GetVisibility(), ESlateVisibility::Collapsed);
		TestTrue(TEXT("Advance after owner visibility change"), AdvanceFrame());
		Prep->SetVisibility(VisibilityBefore);

		// A separately owned presenter exercises deinitialization without deinitializing
		// the world's real subsystem before the remaining lifecycle cases below.
		TStrongObjectPtr<UFF_FacilityCodexPresenterSubsystem> TeardownPresenter(NewObject<UFF_FacilityCodexPresenterSubsystem>(World));
		if (!TestTrue(TEXT("Teardown presenter acquires the existing catalog surface"), TeardownPresenter->OpenCodex(Controller))) return false;
		TestEqual(TEXT("Catalog is hidden during the teardown session"), Prep->GetVisibility(), ESlateVisibility::Hidden);
		TeardownPresenter->Deinitialize();
		TestEqual(TEXT("Deinitialize releases visibility of the same still-open catalog"), Prep->GetVisibility(), VisibilityBefore);
		TestTrue(TEXT("Advance after presenter deinitialization"), AdvanceFrame());
		TestEqual(TEXT("Deinitialize does not change the existing catalog mode"), Build->GetBuildModeState(), EFF_BuildModeState::CatalogOpen);

		if (!TestTrue(TEXT("Codex opens before catalog removal"), Presenter->OpenCodex(Controller))) return false;
		Prep->HandlePanelClosed();
		Prep->RemoveFromParent();
		Presenter->CloseCodex();
		TestTrue(TEXT("Advance after catalog removal"), AdvanceFrame());
		CollectGarbage(RF_NoFlags);
		TestTrue(TEXT("The fixture retains its detached catalog through preview-world garbage collection"), IsValid(Prep));
		TestFalse(TEXT("Closing codex never remounts a removed catalog"), Prep->IsInViewport());
		TestEqual(TEXT("An externally closed catalog mode is never reopened"), Build->GetBuildModeState(), EFF_BuildModeState::Closed);
		TArray<UUserWidget*> RemainingCatalogs;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, RemainingCatalogs, UFF_PrepPanelBuild::StaticClass(), true);
		TestTrue(TEXT("Restoration creates no replacement catalog instance"), RemainingCatalogs.IsEmpty());
	}
	Build->EnterBuildMode();

	if (!TestTrue(TEXT("Codex reopens after restore"), Presenter->OpenCodex(Controller))) return false;
	Stage->CurrentStage = EFF_FestivalStage::Live;
	Stage->OnStageChanged.Broadcast(EFF_FestivalStage::Setup, EFF_FestivalStage::Live);
	TestFalse(TEXT("Stage transition closes the codex"), Presenter->IsCodexOpen());
	TestTrue(TEXT("Advance past stage input restoration"), AdvanceFrame());
	TestFalse(TEXT("Stage teardown releases the deferred build lease"), Build->IsInputSuspendedForPauseMenu());
	TestEqual(TEXT("A live transition does not resurrect the old build session"),
		Build->GetBuildModeState(), EFF_BuildModeState::Closed);
	if (!TestTrue(TEXT("Codex opens for snapshot teardown"), Presenter->OpenCodex(Controller))) return false;
	Zones->OnWorldSnapshotRestored.Broadcast();
	TestFalse(TEXT("World replacement closes the codex"), Presenter->IsCodexOpen());
	TestTrue(TEXT("Advance past snapshot input restoration"), AdvanceFrame());
	if (!TestTrue(TEXT("Codex opens for external screen teardown"), Presenter->OpenCodex(Controller))) return false;
	CodexWidgets.Reset();
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, CodexWidgets, UFF_FacilityCodexScreen::StaticClass(), true);
	if (!CodexWidgets.IsEmpty()) CodexWidgets[0]->RemoveFromParent();
	TestFalse(TEXT("Externally detached screen is no longer open"), Presenter->IsCodexOpen());
	TestFalse(TEXT("Screen destruction releases the input lease"), Viewport->IgnoreInput());
	TestTrue(TEXT("Advance past external removal input restoration"), AdvanceFrame());
	if (!TestTrue(TEXT("Codex opens for controller teardown"), Presenter->OpenCodex(Controller))) return false;
	Controller->OnEndPlay.Broadcast(Controller, EEndPlayReason::LevelTransition);
	TestFalse(TEXT("Controller teardown closes the codex"), Presenter->IsCodexOpen());
	TestFalse(TEXT("A persistent viewport does not retain UIOnly after controller teardown"), Viewport->IgnoreInput());
	TestFalse(TEXT("Controller teardown releases inspector suppression"), Info->IsPauseMenuPresentationSuppressed());
	TestTrue(TEXT("Advance past controller input restoration"), AdvanceFrame());
	TestFalse(TEXT("Controller teardown does not retain a build lease"), Build->IsInputSuspendedForPauseMenu());
	return true;
}

#endif
