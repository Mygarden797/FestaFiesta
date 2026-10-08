// 역할: 실제 Pause 진입과 축제 선택 복귀를 호출해 떠나는 월드의 입력·시계 수명을 검사한다.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Save/FF_SaveSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "Tutorial/FF_DialogueScriptData.h"
#include "Tutorial/FF_TutorialSubsystem.h"
#include "UI/Atlas/FF_FestivalAtlasCatalog.h"
#include "UI/Core/FF_UIFlowSettings.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "UI/Settings/FF_FestivalSettingsSubsystem.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"

namespace
{
class FPauseTravelViewport final : public FViewport
{
public:
	explicit FPauseTravelViewport(FViewportClient* Client) : FViewport(Client) {}
	virtual FIntPoint GetSizeXY() const override { return FIntPoint(1280, 720); }
	virtual void* GetWindow() override { return nullptr; }
	virtual void MoveWindow(int32, int32, int32, int32) override {}
	virtual void Destroy() override {}
	virtual bool SetUserFocus(bool) override { return true; }
	virtual bool KeyState(FKey) const override { return false; }
	virtual int32 GetMouseX() const override { return 640; }
	virtual int32 GetMouseY() const override { return 360; }
	virtual void GetMousePos(FIntPoint& Position, bool = true) override { Position = FIntPoint(640, 360); }
	virtual void SetMouse(int32, int32) override {}
	virtual void ProcessInput(float) override {}
	virtual FVector2D VirtualDesktopPixelToViewport(FIntPoint Point) const override
	{
		return FVector2D(Point) / FVector2D(GetSizeXY());
	}
	virtual FIntPoint ViewportToVirtualDesktopPixel(FVector2D Point) const override
	{
		return FIntPoint(FMath::RoundToInt(Point.X * 1280), FMath::RoundToInt(Point.Y * 720));
	}
	virtual void InvalidateDisplay() override {}
	virtual FViewportFrame* GetViewportFrame() override { return nullptr; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PauseReturnToFestivalSelectionTest,
	"FestaFiesta.UI.Pause.ReturnToFestivalSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_PauseReturnToFestivalSelectionTest::RunTest(const FString& Parameters)
{
	// GameInstance initialization can migrate save metadata. Require the runner's
	// separate user directory before creating any subsystem or touching a profile.
	FString IsolatedUserDirectory;
	const FString OrdinarySaved = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved")));
	if (!FParse::Value(FCommandLine::Get(), TEXT("UserDir="), IsolatedUserDirectory)
		|| IsolatedUserDirectory.IsEmpty()
		|| FPaths::IsSamePath(FPaths::ProjectSavedDir(), OrdinarySaved))
	{
		AddError(TEXT("Pause travel regression requires -UserDir=<isolated absolute directory> to protect player saves."));
		return false;
	}

	// GameViewport stores a weak overlay reference; keep the host alive through world teardown.
	const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
	if (!TestNotNull(TEXT("Isolated game instance"), Game)) return false;
	// FTestWorldWrapper creates the engine context but does not attach it to GI.
	// Use the engine's public world-change hook so the real UIFlow travel finds it.
	Game->OnWorldChanged(nullptr, World);
	FWorldContext* Context = Game->GetWorldContext();
	if (!TestNotNull(TEXT("Game instance resolves its own world context"), Context)) return false;
	const FString PreviousTravelURL = Context->TravelURL;
	const auto PreviousTravelType = Context->TravelType;
	ON_SCOPE_EXIT
	{
		// No engine tick is run: OpenLevel only queues the destination in this context.
		Context->TravelURL = PreviousTravelURL;
		Context->TravelType = PreviousTravelType;
	};

	UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
	Context->GameViewport = Viewport;
	Viewport->Init(*Context, Game, false);
	Viewport->SetViewportOverlayWidget(nullptr, Overlay);
	FPauseTravelViewport InputViewport(Viewport);
	Viewport->Viewport = &InputViewport;
	ON_SCOPE_EXIT { Viewport->Viewport = nullptr; };
	const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
	FSlateApplication::Get().ClearKeyboardFocus();
	ON_SCOPE_EXIT
	{
		if (PreviousFocus.IsValid()) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
		else FSlateApplication::Get().ClearKeyboardFocus();
	};
	FString PlayerError;
	ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
	auto* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
	if (!TestNotNull(*FString::Printf(TEXT("Local player: %s"), *PlayerError), Player)
		|| !TestNotNull(TEXT("Festival controller"), Controller)) return false;
	Controller->SetPlayer(Player);
	Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
	Controller->SetPlayerState(World->SpawnActor<APlayerState>());
	Player->Origin = FVector2D::ZeroVector;
	Player->Size = FVector2D(1.0, 1.0);
	World->AddController(Controller);
	World->GetWorldSettings()->DefaultGameMode = AGameModeBase::StaticClass();
	if (!TestTrue(TEXT("Minimal real game mode owns engine pause"), World->SetGameMode(FURL()))) return false;

	auto* Pause = Game->GetSubsystem<UFF_FestivalSettingsSubsystem>();
	auto* Flow = Game->GetSubsystem<UFF_UIFlowSubsystem>();
	auto* Stage = Game->GetSubsystem<UFF_FestivalStageSubsystem>();
	auto* Save = Game->GetSubsystem<UFF_SaveSubsystem>();
	auto* Build = World->GetSubsystem<UFF_BuildModeSubsystem>();
	auto* Tutorial = World->GetSubsystem<UFF_TutorialSubsystem>();
	auto* Info = World->GetSubsystem<UFF_FacilityInfoSubsystem>();
	if (!TestNotNull(TEXT("Pause subsystem"), Pause) || !TestNotNull(TEXT("UI flow"), Flow)
		|| !TestNotNull(TEXT("Festival clock"), Stage) || !TestNotNull(TEXT("Session metadata"), Save)
		|| !TestNotNull(TEXT("Build mode"), Build) || !TestNotNull(TEXT("Tutorial"), Tutorial)
		|| !TestNotNull(TEXT("Facility presentation"), Info)) return false;

	// The current-map country must win over the first unlocked fallback country.
	auto* Catalog = NewObject<UFF_FestivalAtlasCatalog>(Game);
	FFF_AtlasCountryEntry FallbackCountry;
	FallbackCountry.CountryId = TEXT("PauseFallback");
	FallbackCountry.bUnlocked = true;
	Catalog->Countries.Add(FallbackCountry);
	FFF_AtlasCountryEntry CurrentCountry;
	CurrentCountry.CountryId = TEXT("PauseCurrentMap");
	CurrentCountry.bUnlocked = true;
	Catalog->Countries.Add(CurrentCountry);
	FFF_FestivalAtlasEntry FallbackFestival;
	FallbackFestival.FestivalId = TEXT("PauseFallbackFestival");
	FallbackFestival.CountryId = FallbackCountry.CountryId;
	FallbackFestival.bUnlocked = true;
	Catalog->Entries.Add(FallbackFestival);
	FFF_FestivalAtlasEntry CurrentFestival;
	CurrentFestival.FestivalId = TEXT("PauseCurrentFestival");
	CurrentFestival.CountryId = CurrentCountry.CountryId;
	CurrentFestival.Level = World;
	CurrentFestival.bUnlocked = true;
	Catalog->Entries.Add(CurrentFestival);
	auto* FlowSettings = GetMutableDefault<UFF_UIFlowSettings>();
	TGuardValue<TSoftObjectPtr<UFF_FestivalAtlasCatalog>> CatalogSetting(FlowSettings->AtlasCatalog,
		TSoftObjectPtr<UFF_FestivalAtlasCatalog>(Catalog));
	TGuardValue<TSoftObjectPtr<UWorld>> FrontendSetting(FlowSettings->FrontendLevel,
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Engine/Maps/Entry.Entry"))));
	Flow->SetActiveCountry(NAME_None);
	const bool bOriginalOptOut = Save->IsTutorialOptedOut();
	Build->EnterBuildMode();
	if (!TestEqual(TEXT("Fixture enters production build mode"), Build->GetBuildModeState(), EFF_BuildModeState::BuildMode)) return false;
	auto* Script = NewObject<UFF_DialogueScriptData>(Game);
	Script->ScriptId = TEXT("PauseReturnRegression");
	FFF_DialogueLine Line;
	Line.Text = FText::FromString(TEXT("축제 선택 복귀 검증"));
	Script->Lines.Add(Line);
	if (!TestTrue(TEXT("Production tutorial starts"), Tutorial->PlayScript(Script))
		|| !TestTrue(TEXT("Production Pause Hub opens"), Pause->OpenPauseMenu(Controller))) return false;
	TestTrue(TEXT("Hub owns engine pause"), UGameplayStatics::IsGamePaused(World));
	TestEqual(TEXT("Hub owns modal camera input"), Controller->GetInputBlockReason(), EFF_CameraInputBlock::ModalUi);
	TestTrue(TEXT("Build input is suspended under the hub"), Build->IsInputSuspendedForPauseMenu());
	TestTrue(TEXT("Facility presentation is suppressed under the hub"), Info->IsPauseMenuPresentationSuppressed());
	{
		TGuardValue<TSoftObjectPtr<UWorld>> InvalidDestination(FlowSettings->FrontendLevel, TSoftObjectPtr<UWorld>());
		AddExpectedError(TEXT("축제 선택 레벨을 찾을 수 없어 일시정지 상태를 유지합니다"), EAutomationExpectedErrorFlags::Contains, 1);
		TestFalse(TEXT("Missing destination does not leave the current session"), Pause->ReturnToFestivalSelection());
		TestTrue(TEXT("Failed travel keeps the existing hub and pause"), Pause->IsPauseMenuOpen() && UGameplayStatics::IsGamePaused(World));
		TestTrue(TEXT("Failed travel preserves tutorial and clock"), Tutorial->IsDialogueActive() && Stage->bFestivalStarted);
		TestEqual(TEXT("Failed travel does not queue a map"), Context->TravelURL, PreviousTravelURL);
	}

	if (!TestTrue(TEXT("Pause Hub returns through production UIFlow"), Pause->ReturnToFestivalSelection())) return false;
	TestTrue(TEXT("Actual OpenLevel queues the configured frontend map"), Context->TravelURL.Contains(TEXT("/Engine/Maps/Entry")));
	TestEqual(TEXT("Travel has not loaded another world during this synchronous test"), Game->GetWorld(), World);
	TestEqual(TEXT("Current map selects its own country before unlocked fallback"), Flow->GetActiveCountryId(), CurrentCountry.CountryId);
	TestEqual(TEXT("UI flow is ready for the frontend"), Flow->GetMode(), EFF_UIFlowMode::Frontend);
	TestTrue(TEXT("Active festival is cleared"), Flow->GetActiveFestivalId().IsNone());
	TestFalse(TEXT("Pause screens are released before travel"), Pause->IsPauseMenuOpen());
	TestFalse(TEXT("Engine pause is released before travel"), UGameplayStatics::IsGamePaused(World));
	TestTrue(TEXT("Modal input is released before travel"), Controller->GetInputBlockReason() != EFF_CameraInputBlock::ModalUi);
	TestFalse(TEXT("Facility presentation lease is released"), Info->IsPauseMenuPresentationSuppressed());
	TestEqual(TEXT("Leaving ends the construction session"), Build->GetBuildModeState(), EFF_BuildModeState::Closed);
	TestFalse(TEXT("Leaving stops the tutorial without marking it skipped"), Tutorial->IsDialogueActive());
	TestEqual(TEXT("Tutorial opt-out preference is preserved"), Save->IsTutorialOptedOut(), bOriginalOptOut);
	TestFalse(TEXT("GameInstance festival clock stops in the frontend"), Stage->bFestivalStarted);
	// Direct Tick exercises the persisted clock without running the engine's travel loop.
	Stage->CurrentStage = EFF_FestivalStage::Live;
	const float PreviousLiveSeconds = Stage->ElapsedLiveSeconds;
	Stage->Tick(1.0f);
	TestEqual(TEXT("Stopped festival time cannot advance while returning"), Stage->ElapsedLiveSeconds, PreviousLiveSeconds);
	const FString QueuedDestination = Context->TravelURL;
	TestFalse(TEXT("A second return request cannot queue another travel"), Pause->ReturnToFestivalSelection());
	TestEqual(TEXT("Duplicate request preserves the first destination"), Context->TravelURL, QueuedDestination);
	return true;
}

#endif
