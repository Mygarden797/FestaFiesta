#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Audio/FF_AudioEventLink.h"
#include "Audio/FF_AudioSettings.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/Core/FF_UIForegroundRules.h"
#include "UI/LiveHUD/FF_LiveUtilityDock.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UIUtilityFlyoutForegroundTest,
	"FestaFiesta.UI.Core.UtilityFlyoutForeground",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_UIUtilityFlyoutForegroundTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	UFF_AudioEventLink* AudioOwner = World ? World->GetSubsystem<UFF_AudioEventLink>() : nullptr;
	if (!TestNotNull(TEXT("Isolated game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
		!TestNotNull(TEXT("Production dock owner"), AudioOwner)) return false;
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
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Local player"), Player) || !TestNotNull(TEXT("Player controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	World->AddController(Controller);
	UFF_FestivalStageSubsystem* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	if (!TestNotNull(TEXT("Festival stage"), Stage)) return false;
	Stage->CurrentStage = EFF_FestivalStage::Live;

	// Mount the actual authored dock in its actual owner's slot. Do not BeginPlay:
	// this UI regression needs no festival, audio timer, map, or save mutations.
	UClass* DockClass = UFF_AudioSettings::Get()->PlaylistPlayerWidgetClass.LoadSynchronous();
	if (!TestTrue(TEXT("Configured playlist UI is the production utility dock"),
		DockClass && DockClass->IsChildOf(UFF_LiveUtilityDock::StaticClass()))) return false;
	UFF_LiveUtilityDock* Dock = CreateWidget<UFF_LiveUtilityDock>(Controller, DockClass);
	if (!TestNotNull(TEXT("Authored utility dock"), Dock)) return false;
	AudioOwner->PlaylistPlayerWidget = Dock;
	Dock->AddToViewport();
	ON_SCOPE_EXIT
	{
		Dock->CloseFlyout();
		Dock->RemoveFromParent();
		AudioOwner->PlaylistPlayerWidget = nullptr;
	};
	TestFalse(TEXT("Closed dock buttons do not suppress background facility UI"),
		FFUIForegroundRules::HasForegroundWindow(AudioOwner));
	for (const FName Name : { TEXT("Button_SNS"), TEXT("Button_Music") })
	{
		UButton* Button = Cast<UButton>(Dock->GetWidgetFromName(Name));
		if (!TestNotNull(*FString::Printf(TEXT("Authored %s button"), *Name.ToString()), Button)) return false;
		Button->OnClicked.Broadcast();
		TestTrue(*FString::Printf(TEXT("%s presents a foreground flyout"), *Name.ToString()), Dock->IsForegroundFlyoutOpen());
		TestTrue(TEXT("Audio-owned flyout suppresses facility UI even without a selected HUD tab"),
			FFUIForegroundRules::HasForegroundWindow(AudioOwner));
		TestTrue(TEXT("Ignoring build windows still detects SNS/music windows"),
			FFUIForegroundRules::HasForegroundWindow(AudioOwner, false));
		Dock->CloseFlyout();
		TestFalse(TEXT("Closing the flyout releases background UI suppression"),
			FFUIForegroundRules::HasForegroundWindow(AudioOwner));
	}

	// Exercise the authored build widget's body layer alongside the real flyouts.
	// Keep the catalog session isolated; no placement, BeginPlay, or save is needed.
	UFF_BuildModeSubsystem* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
	if (!TestNotNull(TEXT("Production build owner"), Build)) return false;
	TGuardValue<EFF_BuildModeState> CatalogState(Build->State, EFF_BuildModeState::CatalogOpen);
	TGuardValue<bool> ActiveBuildSession(Build->bBuildSessionActive, true);
	UClass* ToolsClass = Build->BuildToolsWidgetClass.LoadSynchronous();
	if (!TestTrue(TEXT("Configured build tools use the production widget class"),
		ToolsClass && ToolsClass->IsChildOf(UFF_BuildToolsWidget::StaticClass()))) return false;
	UFF_BuildToolsWidget* Tools = CreateWidget<UFF_BuildToolsWidget>(Controller, ToolsClass);
	if (!TestNotNull(TEXT("Authored build tools"), Tools)) return false;
	Tools->AddToViewport(FFBuildToolsModel::ForegroundViewportZOrder);
	ON_SCOPE_EXIT { Tools->RemoveFromParent(); };
	UCanvasPanel* Body = Cast<UCanvasPanel>(Tools->GetWidgetFromName(TEXT("Canvas_ForegroundBuildBody")));
	UBorder* Settings = Cast<UBorder>(Tools->GetWidgetFromName(TEXT("Panel_BuildSettings")));
	if (!TestNotNull(TEXT("Runtime body layer is discoverable in the original widget tree"), Body) ||
		!TestNotNull(TEXT("Authored settings panel"), Settings)) return false;
	bool bSettingsInsideBody = false;
	for (UWidget* Parent = Settings->GetParent(); Parent; Parent = Parent->GetParent())
		bSettingsInsideBody |= Parent == Body;
	TestTrue(TEXT("The authored settings panel is inside the suppressible body"), bSettingsInsideBody);
	Settings->SetVisibility(ESlateVisibility::Visible);
	Tools->RefreshTools();
	TestEqual(TEXT("Catalog keeps the foreground toolbar available"),
		Tools->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Catalog itself does not suppress the build body"),
		Body->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	for (const FName Name : { TEXT("Button_SNS"), TEXT("Button_Music") })
	{
		CastChecked<UButton>(Dock->GetWidgetFromName(Name))->OnClicked.Broadcast();
		Tools->RefreshTools();
		TestEqual(*FString::Printf(TEXT("%s hides the build body"), *Name.ToString()),
			Body->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("The persistent toolbar remains available over the flyout"),
			Tools->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("Suppression preserves the open settings panel's intent"),
			Settings->GetVisibility(), ESlateVisibility::Visible);
		Dock->CloseFlyout();
		Tools->RefreshTools();
		TestEqual(TEXT("Closing the flyout restores the build body"),
			Body->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
		TestEqual(TEXT("The open settings panel restores without being reopened"),
			Settings->GetVisibility(), ESlateVisibility::Visible);
	}
	Dock->ToggleMusic();
	Dock->RemoveFromParent();
	TestFalse(TEXT("A detached dock cannot retain foreground suppression"),
		FFUIForegroundRules::HasForegroundWindow(AudioOwner, false));
	return true;
}

#endif
