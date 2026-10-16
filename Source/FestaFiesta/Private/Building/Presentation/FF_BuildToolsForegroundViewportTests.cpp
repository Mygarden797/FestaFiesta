// Render the actual viewport stack: a later window must not intercept the build
// toolbar, while Pause must still hide those controls and release their hit targets.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetTree.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Building/Presentation/FF_BuildFacilityCostTooltip.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"
#include "ImageUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SVirtualWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsForegroundViewportTest,
    "FestaFiesta.Building.BuildTools.ForegroundViewport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildToolsForegroundViewportTest::RunTest(const FString& Parameters)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: viewport hit-test coverage requires -RenderOffscreen with a real RHI."));
        return true;
    }
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
    UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Isolated game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
        !TestNotNull(TEXT("Production build owner"), Build)) return false;
    FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
    if (!TestNotNull(TEXT("Real viewport context"), Context)) return false;
    const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context->GameViewport = Viewport;
    Viewport->Init(*Context, Game, false);
    Viewport->SetViewportOverlayWidget(nullptr, Overlay);
    FString PlayerError;
    ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(*FString::Printf(TEXT("Real local player: %s"), *PlayerError), Player) ||
        !TestNotNull(TEXT("Real owning controller"), Controller)) return false;
    Controller->SetPlayer(Player);
    Player->Origin = FVector2D::ZeroVector;
    Player->Size = FVector2D(1.f, 1.f);
    World->AddController(Controller);

    // Use the production creation and AddToViewport path, not a directly rendered
    // tools widget whose isolated canvas cannot reveal viewport ordering failures.
    Build->State = EFF_BuildModeState::BuildMode;
    Build->bBuildSessionActive = true;
    Build->CreateBuildTools(*Controller);
    UFF_BuildToolsWidget* Tools = Build->BuildToolsWidget;
    if (!TestNotNull(TEXT("Production mounted build toolbar"), Tools)) return false;
    Tools->RefreshTools();
    // Reuse a concrete, passive native widget: its lifecycle only updates optional
    // text bindings and has no world timers, input handlers, or subsystem mutations.
    TStrongObjectPtr<UFF_BuildFacilityCostTooltip> Cover(
        CreateWidget<UFF_BuildFacilityCostTooltip>(Controller, UFF_BuildFacilityCostTooltip::StaticClass()));
    if (!TestNotNull(TEXT("Later viewport window"), Cover.Get())) return false;
    Cover->WidgetTree = NewObject<UWidgetTree>(Cover.Get());
    UButton* CoverButton = Cover->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_ViewportCover"));
    Cover->WidgetTree->RootWidget = CoverButton;
    Cover->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Cover->AddToViewport(100);
    ON_SCOPE_EXIT
    {
        Cover->RemoveFromParent();
        if (Build->IsInputSuspendedForPauseMenu()) Build->RestoreInputAfterPauseMenu();
        Build->RemoveBuildTools();
        Build->State = EFF_BuildModeState::Closed;
        Build->bBuildSessionActive = false;
    };
    Tools->WidgetTree->ForEachWidget([](UWidget* Widget)
    {
        const auto Ready = [](const FSlateBrush& Brush)
        { if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Material->EnsureIsComplete(); };
        if (auto* Border = Cast<UBorder>(Widget)) Ready(Border->Background);
        if (auto* Button = Cast<UButton>(Widget)) Ready(Button->GetStyle().Normal);
        if (auto* Image = Cast<UImage>(Widget)) Ready(Image->GetBrush());
    });
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();
    const FVector2D Size(1920.f, 1080.f);
    auto Window = SNew(SVirtualWindow).Size(Size);
    Window->SetContent(Overlay);
    FWidgetRenderer Renderer(false, true);
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
    if (!TestNotNull(TEXT("Real viewport render target"), Target.Get())) return false;
    FHittestGrid Grid;
    const auto Draw = [&]
    {
        for (int32 Frame = 0; Frame < 4; ++Frame)
            Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false);
        FlushRenderingCommands();
    };
    const auto HasWidget = [](const TArray<FWidgetAndPointer>& Path, const UWidget& Widget)
    {
        return Path.ContainsByPredicate([&](const FWidgetAndPointer& Entry)
        { return &Entry.Widget.Get() == Widget.GetCachedWidget().Get(); });
    };
    Draw();
    const FVector2D CoverSize = CoverButton->GetCachedGeometry().GetLocalSize();
    if (!TestTrue(TEXT("Later window covers the toolbar's viewport area"), CoverSize.X >= Size.X - 1.f && CoverSize.Y >= Size.Y - 1.f))
        return false;
    TMap<UWidget*, FVector2D> ToolbarTargets;
    for (const FName Name : {FName(TEXT("Button_MultiSelect")), FName(TEXT("Button_ToolbarHelp")), FName(TEXT("Button_ToolbarCatalog")), FName(TEXT("Button_BuildBack"))})
    {
        UWidget* Button = Tools->GetWidgetFromName(Name);
        if (!TestNotNull(*Name.ToString(), Button)) return false;
        const FGeometry& Geometry = Button->GetCachedGeometry();
        if (!TestTrue(Name.ToString() + TEXT(" has a rendered hit target"), Geometry.GetLocalSize().GetMin() > 0.f)) return false;
        const FVector2D Center = Geometry.LocalToAbsolute(Geometry.GetLocalSize() * .5f);
        ToolbarTargets.Add(Button, Center);
        const auto Path = Grid.GetBubblePath(Center, 0.f, false);
        TestTrue(Name.ToString() + TEXT(" remains clickable above a later viewport window"), HasWidget(Path, *Button));
        TestFalse(Name.ToString() + TEXT(" is not intercepted by the covering window"), HasWidget(Path, *CoverButton));
    }
    TestTrue(TEXT("Pause suspends the active build session"), Build->SuspendInputForPauseMenu());
    Tools->RefreshTools();
    TestEqual(TEXT("Pause keeps the foreground toolbar collapsed during refresh"), Tools->GetVisibility(), ESlateVisibility::Collapsed);
    Draw();
    for (const auto& Entry : ToolbarTargets)
    {
        const auto Path = Grid.GetBubblePath(Entry.Value, 0.f, false);
        TestFalse(Entry.Key->GetName() + TEXT(" releases its hit target while Pause is open"), HasWidget(Path, *Entry.Key));
        TestTrue(Entry.Key->GetName() + TEXT(" allows the underlying window to receive input during Pause"), HasWidget(Path, *CoverButton));
    }
    Build->RestoreInputAfterPauseMenu();
    Cover->SetVisibility(ESlateVisibility::Collapsed);
    UClass* InspectorClass = UFF_FacilityInfoSubsystem::GetDefaultPanelClassPath().TryLoadClass<UFF_FacilityInfoPanel>();
    TStrongObjectPtr<UFF_FacilityInfoPanel> Inspector(CreateWidget<UFF_FacilityInfoPanel>(Controller, InspectorClass));
    if (!TestNotNull(TEXT("Production facility inspector"), Inspector.Get())) return false;
    Inspector->AddToViewport(95);
    ON_SCOPE_EXIT { Inspector->RemoveFromParent(); };
    FFF_FacilityInfoViewData InfoData;
    InfoData.FacilityInstanceId = FGuid::NewGuid();
    InfoData.Context = EFF_FacilityInfoContext::Build;
    InfoData.DisplayName = FText::FromString(TEXT("대기열 시설"));
    InfoData.bCanMove = true;
    InfoData.bCanRemove = true;
    InfoData.bHasQueueSettings = true;
    InfoData.bCanEditQueueSettings = true;
    InfoData.QueueSettings.bEnabled = true;
    Inspector->ApplyViewData(InfoData);
    Inspector->SetPanelOpen(true);
    Build->State = EFF_BuildModeState::Moving;
    Tools->RefreshTools();
    auto* Scroll = Cast<UScrollBox>(Inspector->GetWidgetFromName(TEXT("ScrollBox_Details")));
    UWidget* Footer = Inspector->GetWidgetFromName(TEXT("Button_Move"));
    UWidget* ApplyQueue = Inspector->GetWidgetFromName(TEXT("Button_ApplyFacilityQueueSettings"));
    if (!TestNotNull(TEXT("Scrollable inspector body"), Scroll) || !TestNotNull(TEXT("Fixed move footer"), Footer) ||
        !TestNotNull(TEXT("Queue apply control"), ApplyQueue)) return false;
    for (const FVector2D InspectSize : {FVector2D(1280.f, 720.f), FVector2D(1920.f, 1080.f)})
    {
        Window->Resize(InspectSize);
        TStrongObjectPtr<UTextureRenderTarget2D> InspectTarget(FWidgetRenderer::CreateTargetFor(InspectSize, TF_Bilinear, true));
        if (!TestNotNull(TEXT("Inspector viewport render target"), InspectTarget.Get())) return false;
        for (int32 Frame = 0; Frame < 12; ++Frame)
        {
            Scroll->ScrollToEnd();
            Renderer.DrawWindow(InspectTarget.Get(), Grid, Window, 1.f, InspectSize, .016f, false);
        }
        FlushRenderingCommands();
        TestEqual(TEXT("Moving does not overlay the selected facility with a second inspector"),
            Tools->GetWidgetFromName(TEXT("Border_PlacementInfo"))->GetVisibility(), ESlateVisibility::Collapsed);
        TestEqual(TEXT("Release-to-confirm moving has no overlapping install dock"),
            Tools->GetWidgetFromName(TEXT("Border_Tools"))->GetVisibility(), ESlateVisibility::Collapsed);
        const FSlateRect ScrollBounds = Scroll->GetCachedGeometry().GetRenderBoundingRect();
        const FSlateRect FooterBounds = Footer->GetCachedGeometry().GetRenderBoundingRect();
        const FSlateRect ApplyBounds = ApplyQueue->GetCachedGeometry().GetRenderBoundingRect();
        TestTrue(TEXT("The scrollable facility details stop above the fixed action footer"),
            ScrollBounds.Bottom <= FooterBounds.Top + 1.f);
        TestTrue(TEXT("Queue apply remains fully reachable above the footer at each window size"),
            ApplyBounds.Top >= ScrollBounds.Top - 1.f && ApplyBounds.Bottom <= ScrollBounds.Bottom + 1.f &&
            ApplyBounds.GetSize().Y >= 20.f);
        TArray<FColor> Pixels;
        FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
        if (TestTrue(TEXT("Combined inspector/toolbar capture supplies pixels"),
            InspectTarget->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags)))
        {
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(int32(InspectSize.X), int32(InspectSize.Y), Pixels, Png);
            const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/FacilityInfo"));
            IFileManager::Get().MakeDirectory(*Directory, true);
            const FString Path = Directory / FString::Printf(TEXT("HeldDragInspector_%d.png"), int32(InspectSize.Y));
            TestTrue(TEXT("Combined inspector screenshot saved"), FFileHelper::SaveArrayToFile(Png, *Path));
            AddInfo(FPaths::ConvertRelativePathToFull(Path));
        }
    }
    Inspector->SetPanelOpen(false);
    Build->State = EFF_BuildModeState::Placing;
    Tools->RefreshTools();
    TestEqual(TEXT("New facility placement retains its required-space inspector"),
        Tools->GetWidgetFromName(TEXT("Border_PlacementInfo"))->GetVisibility(), ESlateVisibility::Visible);
    TestEqual(TEXT("New facility placement retains its install dock"),
        Tools->GetWidgetFromName(TEXT("Border_Tools"))->GetVisibility(), ESlateVisibility::Visible);
    return true;
}

#endif
