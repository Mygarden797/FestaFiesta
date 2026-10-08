// FF_FacilityCodexIntroductionUiTests.cpp
// 역할: 처음 여는 도감의 강조·설명·입력·재방문 상태를 실제 WBP로 검증한다.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Building/Codex/FF_FacilityCodexProgressSubsystem.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "Slate/SceneViewport.h"
#include "Slate/WidgetRenderer.h"
#include "Tests/AutomationCommon.h"
#include "TextureResource.h"
#include "UI/FacilityCodex/FF_FacilityCodexLauncher.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresenterSubsystem.h"
#include "UI/FacilityCodex/FF_FacilityCodexScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SViewport.h"

namespace
{
	class FCodexIntroductionProfile
	{
	public:
		FCodexIntroductionProfile()
			: Path(FPaths::ConvertRelativePathToFull(FPaths::CreateTempFilename(
				*FPaths::ProjectSavedDir(), TEXT("CodexIntroductionUiTest"), TEXT(".ini"))))
			, PreviousPath(GGameUserSettingsIni)
		{
			FPaths::NormalizeFilename(Path);
			if (GConfig) GConfig->Add(Path, FConfigFile());
			GGameUserSettingsIni = Path;
		}
		~FCodexIntroductionProfile()
		{
			if (GConfig) GConfig->UnloadFile(Path);
			GGameUserSettingsIni = PreviousPath;
			IFileManager::Get().Delete(*Path);
		}
		bool IsIntroductionSaved() const
		{
			FConfigFile Saved;
			Saved.Read(Path);
			bool bSeen = false;
			return Saved.GetBool(TEXT("FestaFiesta.FacilityCodex"), TEXT("bHasSeenIntroduction"), bSeen) && bSeen;
		}
	private:
		FString Path;
		FString PreviousPath;
	};

	UFF_FacilityCodexScreen* FindCodexScreen(UWorld* World)
	{
		TArray<UUserWidget*> Screens;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Screens, UFF_FacilityCodexScreen::StaticClass(), true);
		return Screens.Num() == 1 ? Cast<UFF_FacilityCodexScreen>(Screens[0]) : nullptr;
	}

	bool CaptureLauncher(FAutomationTestBase& Test, UFF_FacilityCodexLauncher& Launcher, const bool bCatalog)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
		auto* Bounds = Cast<USizeBox>(Launcher.GetWidgetFromName(TEXT("Size_Launcher")));
		auto* Hint = Cast<UBorder>(Launcher.GetWidgetFromName(TEXT("Border_IntroductionHint")));
		auto* Button = Cast<UButton>(Launcher.GetWidgetFromName(TEXT("Button_OpenCodex")));
		if (!Test.TestNotNull(TEXT("Authored launcher bounds"), Bounds) ||
			!Test.TestNotNull(TEXT("Authored launcher reminder"), Hint) ||
			!Test.TestNotNull(TEXT("Authored launcher action"), Button)) return false;
		if (bCatalog)
		{
			// Match the active bottom codex tab's production compact bounds. Keep the
			// saved label and styling; this instance belongs only to the test world.
			Bounds->SetWidthOverride(150.f);
			Bounds->SetHeightOverride(34.f);
		}
		const TSharedRef<SWidget> Slate = Launcher.TakeWidget();
		Launcher.ForceLayoutPrepass();
		const FVector2D Desired = Launcher.GetDesiredSize();
		const FIntPoint Resolution(FMath::CeilToInt(Desired.X), FMath::CeilToInt(Desired.Y));
		if (!Test.TestTrue(TEXT("Authored launcher has measurable bounds"), Resolution.X > 0 && Resolution.Y > 0)) return false;
		const FVector2D Size(Resolution.X, Resolution.Y);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!Test.TestNotNull(TEXT("First-use launcher capture target"), Target.Get())) return false;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Launcher.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), Slate, Size, .016f, false);
		}
		FlushRenderingCommands();
		const FSlateRect ButtonRect = Button->GetCachedGeometry().GetLayoutBoundingRect();
		const FSlateRect HintRect = Hint->GetCachedGeometry().GetRenderBoundingRect();
		Test.TestTrue(bCatalog ? TEXT("Compact catalog reminder fits inside its button") : TEXT("Inspector reminder fits inside its button"),
			Hint->IsVisible() && HintRect.Right > HintRect.Left && HintRect.Bottom > HintRect.Top &&
			HintRect.Left >= ButtonRect.Left - .5f && HintRect.Top >= ButtonRect.Top - .5f &&
			HintRect.Right <= ButtonRect.Right + .5f && HintRect.Bottom <= ButtonRect.Bottom + .5f);
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false);
		if (!Test.TestTrue(TEXT("First-use launcher pixels are readable"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Test.TestTrue(TEXT("First-use launcher screenshot saved"), FFileHelper::SaveArrayToFile(Png,
			*(Directory / (bCatalog ? TEXT("FacilityCodexIntroductionCatalogLauncher.png") : TEXT("FacilityCodexIntroductionLauncher.png")))));
	}

	bool CaptureIntroduction(FAutomationTestBase& Test, UFF_FacilityCodexScreen& Screen, const FIntPoint Resolution)
	{
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))) return true;
		const FVector2D Size(Resolution.X, Resolution.Y);
		const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
		FWidgetRenderer Renderer(false, true);
		TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
		if (!Test.TestNotNull(TEXT("First-open capture target"), Target.Get())) return false;
		const TSharedRef<SWidget> CaptureWidget = SNew(SDPIScaler).DPIScale(Dpi)[Screen.TakeWidget()];
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			Screen.ForceLayoutPrepass();
			Renderer.DrawWidget(Target.Get(), CaptureWidget, Size, .016f, false);
		}
		FlushRenderingCommands();
		auto* Dismiss = Cast<UButton>(Screen.GetWidgetFromName(TEXT("Button_DismissIntroduction")));
		if (!Test.TestNotNull(TEXT("Visible introduction action"), Dismiss)) return false;
		const FSlateRect ActionRect = Dismiss->GetCachedGeometry().GetLayoutBoundingRect();
		Test.TestTrue(TEXT("The first-open explanation stays visible while rendered"), Screen.IsIntroductionVisible());
		Test.TestTrue(*FString::Printf(TEXT("Introduction action fits the %dx%d viewport"), Resolution.X, Resolution.Y),
			ActionRect.Left >= 0.f && ActionRect.Top >= 0.f && ActionRect.Right <= Size.X + 1.f &&
			ActionRect.Bottom <= Size.Y + 1.f && ActionRect.Right > ActionRect.Left && ActionRect.Bottom > ActionRect.Top);
		TArray<FColor> Pixels;
		FReadSurfaceDataFlags ReadFlags;
		ReadFlags.SetLinearToGamma(false);
		if (!Test.TestTrue(TEXT("First-open pixels are readable"),
			Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, ReadFlags))) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
		const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/FacilityCodex");
		IFileManager::Get().MakeDirectory(*Directory, true);
		return Test.TestTrue(TEXT("First-open explanation screenshot saved"), FFileHelper::SaveArrayToFile(
			Png, *(Directory / FString::Printf(TEXT("FacilityCodexIntroduction%d.png"), Resolution.Y))));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FacilityCodexIntroductionUiTest,
	"FestaFiesta.FacilityCodex.UI.FirstUseIntroduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityCodexIntroductionUiTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString InspectorLauncherPath = FString(FFBuildUIAssetPaths::FacilityCodexLauncherPackage) +
		TEXT(".WBP_FF_FacilityCodexLauncher_C");
	UClass* LauncherClasses[] = {
		LoadClass<UFF_FacilityCodexLauncher>(nullptr, *InspectorLauncherPath),
		LoadClass<UFF_FacilityCodexLauncher>(nullptr, FFBuildUIAssetPaths::FacilityCodexCatalogLauncherClass)
	};
	if (!TestNotNull(TEXT("Authored inspector launcher"), LauncherClasses[0]) ||
		!TestNotNull(TEXT("Authored catalog launcher"), LauncherClasses[1])) return false;

	// Each entry point gets a genuinely fresh profile. Both launchers are alive
	// together so the progress notification must remove both reminders immediately.
	for (int32 FirstLauncher = 0; FirstLauncher < 2; ++FirstLauncher)
	{
		FCodexIntroductionProfile Profile;
		FTestWorldWrapper Fixture;
		const uint64 OriginalFrameCounter = GFrameCounter;
		ON_SCOPE_EXIT { GFrameCounter = OriginalFrameCounter; };
		if (!Fixture.CreateTestWorld(EWorldType::Game))
		{
			Fixture.ForwardErrorMessages(this);
			return false;
		}
		UWorld* World = Fixture.GetTestWorld();
		UGameInstance* Game = World->GetGameInstance();
		auto* Presenter = World->GetSubsystem<UFF_FacilityCodexPresenterSubsystem>();
		auto* Progress = Game ? Game->GetSubsystem<UFF_FacilityCodexProgressSubsystem>() : nullptr;
		FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
		if (!TestNotNull(TEXT("First-use game instance"), Game) || !TestNotNull(TEXT("First-use presenter"), Presenter) ||
			!TestNotNull(TEXT("Disposable first-use profile"), Progress) || !TestNotNull(TEXT("Private world context"), Context)) return false;
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
		auto* Controller = World->SpawnActor<AFF_FestivalPlayerController>();
		if (!TestNotNull(*FString::Printf(TEXT("First-use local player: %s"), *PlayerError), Player) ||
			!TestNotNull(TEXT("First-use controller"), Controller)) return false;
		Controller->SetPlayer(Player);
		Controller->PlayerInput = NewObject<UPlayerInput>(Controller);
		World->AddController(Controller);
		const TSharedPtr<SWidget> PreviousFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
		ON_SCOPE_EXIT
		{
			Presenter->CloseCodex();
			if (PreviousFocus) FSlateApplication::Get().SetKeyboardFocus(PreviousFocus);
			else FSlateApplication::Get().ClearKeyboardFocus();
		};
		TStrongObjectPtr<UFF_FacilityCodexLauncher> InspectorLauncher(
			CreateWidget<UFF_FacilityCodexLauncher>(Controller, LauncherClasses[0]));
		TStrongObjectPtr<UFF_FacilityCodexLauncher> CatalogLauncher(
			CreateWidget<UFF_FacilityCodexLauncher>(Controller, LauncherClasses[1]));
		UFF_FacilityCodexLauncher* Launchers[] = {InspectorLauncher.Get(), CatalogLauncher.Get()};
		UBorder* Hints[2] = {};
		UButton* Buttons[2] = {};
		ON_SCOPE_EXIT
		{
			for (auto* Launcher : Launchers) if (Launcher) Launcher->RemoveFromParent();
		};
		const FGeometry TickGeometry = FGeometry::MakeRoot(FVector2D(1280, 720), FSlateLayoutTransform());
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (!TestNotNull(TEXT("Production launcher instance"), Launchers[Index])) return false;
			Launchers[Index]->AddToViewport();
			Hints[Index] = Cast<UBorder>(Launchers[Index]->GetWidgetFromName(TEXT("Border_IntroductionHint")));
			Buttons[Index] = Cast<UButton>(Launchers[Index]->GetWidgetFromName(TEXT("Button_OpenCodex")));
			if (!TestNotNull(TEXT("Authored first-use hint"), Hints[Index]) ||
				!TestNotNull(TEXT("Actual first-use entry button"), Buttons[Index])) return false;
			TestTrue(TEXT("A never-opened profile shows each launcher hint"), Hints[Index]->IsVisible());
			const FLinearColor HintColor = Hints[Index]->Background.TintColor.GetSpecifiedColor() * Hints[Index]->GetBrushColor();
			TestTrue(TEXT("The reminder is a red icon"), HintColor.R > HintColor.G && HintColor.R > HintColor.B);
			Launchers[Index]->TakeWidget()->Tick(TickGeometry, 0.0, 3.f);
			TestTrue(TEXT("The short pulse settles while the unclicked reminder remains visible"), Hints[Index]->IsVisible() &&
				Hints[Index]->GetRenderTransform().Scale.Equals(FVector2D(1.f, 1.f)));
			if (FirstLauncher == 0 && !CaptureLauncher(*this, *Launchers[Index], Index == 1)) return false;
		}
		TestFalse(TEXT("Displaying and ticking reminders never marks the guide as read"), Progress->HasSeenIntroduction());
		Controller->SetModalUiActive(true);
		Buttons[FirstLauncher]->OnClicked.Broadcast();
		TestFalse(TEXT("Another modal rejects the first codex click"), Presenter->IsCodexOpen());
		TestFalse(TEXT("A rejected click does not consume the first-use guide"), Progress->HasSeenIntroduction());
		TestFalse(TEXT("A rejected click does not persist a viewed guide"), Profile.IsIntroductionSaved());
		TestTrue(TEXT("A rejected click preserves both hints"), Hints[0]->IsVisible() && Hints[1]->IsVisible());
		Controller->SetModalUiActive(false);
		Buttons[FirstLauncher]->OnClicked.Broadcast();
		if (!TestTrue(TEXT("The actual first-use launcher opens its codex"), Presenter->IsCodexOpen())) return false;
		auto* Screen = FindCodexScreen(World);
		if (!TestNotNull(TEXT("One actual first-use codex screen"), Screen)) return false;
		TestTrue(TEXT("The first successful open presents the explanation"), Screen->IsIntroductionVisible());
		TestTrue(TEXT("A successfully displayed guide marks this profile seen"), Progress->HasSeenIntroduction());
		TestTrue(TEXT("The seen flag reaches the disposable profile on disk"), Profile.IsIntroductionSaved());
		TestEqual(TEXT("Opening from either entry removes the inspector reminder"), Hints[0]->GetVisibility(), ESlateVisibility::Collapsed);
		TestEqual(TEXT("Opening from either entry removes the catalog reminder"), Hints[1]->GetVisibility(), ESlateVisibility::Collapsed);
		if (FirstLauncher == 0 && (!CaptureIntroduction(*this, *Screen, FIntPoint(1280, 720)) ||
			!CaptureIntroduction(*this, *Screen, FIntPoint(1920, 1080)))) return false;
		const TSharedRef<SWidget> ScreenSlate = Screen->TakeWidget();
		const FName SelectedBefore = Screen->GetSelectedRuleId();
		const FKeyEvent ArrowKey(EKeys::Right, FModifierKeysState(), 0, false, 0, 0);
		TestTrue(TEXT("Guide consumes collection navigation keys"), ScreenSlate->OnPreviewKeyDown(TickGeometry, ArrowKey).IsEventHandled());
		TestEqual(TEXT("Guide does not navigate recipes behind the explanation"), Screen->GetSelectedRuleId(), SelectedBefore);
		TestTrue(TEXT("Collection navigation does not dismiss the guide"), Screen->IsIntroductionVisible());
		const FKeyEvent HeldEnter(EKeys::Enter, FModifierKeysState(), 0, true, 0, 0);
		ScreenSlate->OnPreviewKeyDown(TickGeometry, HeldEnter);
		TestTrue(TEXT("A held activation key cannot immediately skip the explanation"), Screen->IsIntroductionVisible());
		if (FirstLauncher == 0)
		{
			const FKeyEvent Escape(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
			TestTrue(TEXT("Escape is handled by the guide"), ScreenSlate->OnPreviewKeyDown(TickGeometry, Escape).IsEventHandled());
		}
		else
		{
			auto* Dismiss = Cast<UButton>(Screen->GetWidgetFromName(TEXT("Button_DismissIntroduction")));
			if (!TestNotNull(TEXT("Authored guide dismissal button"), Dismiss)) return false;
			Dismiss->OnClicked.Broadcast();
		}
		TestFalse(TEXT("Dismissal removes the explanation"), Screen->IsIntroductionVisible());
		TestTrue(TEXT("Dismissal keeps the collection open for reading"), Presenter->IsCodexOpen());
		for (const FKey Key : {EKeys::Enter, EKeys::SpaceBar})
		{
			if (!TestTrue(TEXT("Guide can be presented for keyboard coverage"), Screen->ShowIntroduction())) return false;
			const FKeyEvent Accept(Key, FModifierKeysState(), 0, false, 0, 0);
			TestTrue(TEXT("Keyboard acceptance is consumed"), ScreenSlate->OnPreviewKeyDown(TickGeometry, Accept).IsEventHandled());
			TestFalse(TEXT("Keyboard acceptance dismisses only the explanation"), Screen->IsIntroductionVisible());
			TestTrue(TEXT("Keyboard acceptance preserves the codex"), Presenter->IsCodexOpen());
		}
		Presenter->CloseCodex();
		++GFrameCounter;
		if (!TestTrue(TEXT("Advance first-use close restoration"), Fixture.TickTestWorld())) return false;
		Buttons[1 - FirstLauncher]->OnClicked.Broadcast();
		if (!TestTrue(TEXT("The other entry reopens the collection"), Presenter->IsCodexOpen())) return false;
		Screen = FindCodexScreen(World);
		if (!TestNotNull(TEXT("Returning-player codex screen"), Screen)) return false;
		TestFalse(TEXT("A later open does not repeat the explanation"), Screen->IsIntroductionVisible());
		TestFalse(TEXT("Neither launcher restores its first-use hint"), Hints[0]->IsVisible() || Hints[1]->IsVisible());
		// Widget reconstruction occurs when switching construction/inspector panels.
		InspectorLauncher->RemoveFromParent();
		InspectorLauncher->AddToViewport();
		TestFalse(TEXT("Reconstructing a launcher does not restore a consumed hint"), Hints[0]->IsVisible());
	}
	return true;
}

#endif
