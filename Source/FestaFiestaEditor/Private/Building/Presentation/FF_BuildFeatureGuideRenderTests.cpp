// Verify the real authored guide and its topic selector at the supported HUD resolutions.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildFeatureGuide.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Building/Presentation/FF_BuildingFeatureDiscoverySubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Input/HittestGrid.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "ShaderCompiler.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SVirtualWindow.h"

namespace FFBuildFeatureGuideRender
{
struct FPlayerlessInitialization
{
    explicit FPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
        : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
    { Class.bCanCallInitializedWithoutPlayerContext = true; }
    ~FPlayerlessInitialization() { Class.bCanCallInitializedWithoutPlayerContext = bPrevious; }
    UWidgetBlueprintGeneratedClass& Class;
    bool bPrevious;
};
FSlateRect Bounds(const UWidget* Widget) { return Widget->GetPaintSpaceGeometry().GetRenderBoundingRect(); }
bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    return Inner.Left >= Outer.Left - 1.f && Inner.Right <= Outer.Right + 1.f &&
        Inner.Top >= Outer.Top - 1.f && Inner.Bottom <= Outer.Bottom + 1.f;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildFeatureGuideRenderTest,
    "FestaFiesta.Building.FeatureGuide.RenderedLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildFeatureGuideRenderTest::RunTest(const FString& Parameters)
{
    using namespace FFBuildFeatureGuideRender;
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: feature guide screenshots require -RenderOffscreen with a real RHI."));
        return true;
    }
    auto* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Saved BuildTools class"), Class)) return false;
    FPlayerlessInitialization Context(*Class);
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production tools instance"), Widget.Get())) return false;
    const auto Slate = Widget->TakeWidget();
    // Freeze only authority-driven editor-world ticking; guide actions below are the real runtime handlers.
    Slate->SetCanTick(false);
    Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Widget->SetIsEnabled(true);
    auto* Top = Widget->GetWidgetFromName(TEXT("Border_BuildTopBar"));
    auto* Help = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_ToolbarHelp")));
    auto* Open = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_FeatureGuide")));
    auto* Panel = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Panel_FeatureGuide")));
    auto* Card = Widget->GetWidgetFromName(TEXT("Border_FeatureGuideCard"));
    auto* Combo = Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("Combo_FeatureGuideTopic")));
    auto* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_FeatureGuideTitle")));
    auto* Body = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_FeatureGuideBody")));
    auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_FeatureGuideBody")));
    auto* Confirm = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_FeatureGuideConfirm")));
    auto* Close = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_FeatureGuideClose")));
    if (!TestNotNull(TEXT("Toolbar"), Top) || !TestNotNull(TEXT("Existing help"), Help) ||
        !TestNotNull(TEXT("Guide entry"), Open) || !TestNotNull(TEXT("Modal guide"), Panel) ||
        !TestNotNull(TEXT("Guide card"), Card) || !TestNotNull(TEXT("Topic selector"), Combo) ||
        !TestNotNull(TEXT("Guide title"), Title) || !TestNotNull(TEXT("Guide body"), Body) ||
        !TestNotNull(TEXT("Body scroll"), Scroll) || !TestNotNull(TEXT("Confirm"), Confirm) ||
        !TestNotNull(TEXT("Close"), Close)) return false;
    Top->SetVisibility(ESlateVisibility::Visible);
    // Isolate first-use styling from the player's saved profile. This transient
    // subsystem is deliberately not initialized, and this fixture never confirms a topic.
    TStrongObjectPtr<UGameInstance> HintGameInstance(NewObject<UGameInstance>());
    TStrongObjectPtr<UFF_BuildingFeatureDiscoverySubsystem> Discovery(
        NewObject<UFF_BuildingFeatureDiscoverySubsystem>(HintGameInstance.Get()));
    TMap<UButton*, FLinearColor> OriginalButtonTints;
    TMap<UTextBlock*, FSlateColor> OriginalCaptions;
    TMap<UBorder*, FSlateBrush> OriginalIconBrushes;
    TMap<UBorder*, FLinearColor> OriginalIconTints;
    TMap<UTextBlock*, FSlateColor> OriginalIconText;
    TMap<UBorder*, UButton*> Badges;
    int32 ToolbarIndex = 0;
    for (const auto& Topic : FFBuildFeatureGuide::Topics())
    {
        if (Topic.ToolbarButton.IsNone()) continue;
        auto* Button = Cast<UButton>(Widget->GetWidgetFromName(Topic.ToolbarButton));
        if (!TestNotNull(TEXT("First-use toolbar button"), Button)) return false;
        OriginalButtonTints.Add(Button, Button->GetBackgroundColor());
        auto* Caption = Cast<UTextBlock>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("Label_PearlTool%d"), ++ToolbarIndex))));
        if (!TestNotNull(TEXT("Authored toolbar caption"), Caption)) return false;
        OriginalCaptions.Add(Caption, Caption->GetColorAndOpacity());
        auto* Icon = Widget->GetWidgetFromName(Topic.ToolbarIcon);
        auto* Badge = Cast<UBorder>(Widget->GetWidgetFromName(
            FName(*FString::Printf(TEXT("Badge_%s"), *Topic.Id.ToString()))));
        if (!TestNotNull(TEXT("Authored toolbar icon"), Icon) || !TestNotNull(TEXT("Authored first-use badge"), Badge)) return false;
        auto* Mark = Cast<UTextBlock>(Badge->GetContent());
        if (!TestNotNull(TEXT("Badge exclamation text"), Mark)) return false;
        TestEqual(Topic.Id.ToString() + TEXT(" badge uses an exclamation mark"), Mark->GetText().ToString(), FString(TEXT("!")));
        TestEqual(Topic.Id.ToString() + TEXT(" badge is initially hidden without discovery state"),
            Badge->GetVisibility(), ESlateVisibility::Collapsed);
        Badges.Add(Badge, Button);
        UWidgetTree::ForWidgetAndChildren(Icon, [&](UWidget* Child)
        {
            if (Child == Badge || Child == Mark) return;
            if (auto* Border = Cast<UBorder>(Child))
            {
                OriginalIconBrushes.Add(Border, Border->Background);
                OriginalIconTints.Add(Border, Border->GetBrushColor());
            }
            if (auto* Text = Cast<UTextBlock>(Child)) OriginalIconText.Add(Text, Text->GetColorAndOpacity());
        });
    }
    Widget->FeatureDiscovery = Discovery.Get();
    Widget->RefreshFeatureHints();
    TestEqual(TEXT("Seven first-use toolbar icons are covered"), OriginalButtonTints.Num(), 7);
    TestEqual(TEXT("Seven independent first-use badges are authored"), Badges.Num(), 7);
    TestTrue(TEXT("Original icon border styles are covered"), !OriginalIconBrushes.IsEmpty());
    TestTrue(TEXT("Original icon text styles are covered"), !OriginalIconText.IsEmpty());
    for (const auto& Entry : Badges)
        TestEqual(Entry.Key->GetName() + TEXT(" is visible without intercepting clicks"),
            Entry.Key->GetVisibility(), ESlateVisibility::HitTestInvisible);
    for (const auto& Entry : OriginalButtonTints)
        TestTrue(Entry.Key->GetName() + TEXT(" preserves its background during first-use emphasis"),
            Entry.Key->GetBackgroundColor().Equals(Entry.Value));
    for (const auto& Entry : OriginalCaptions)
        TestTrue(Entry.Key->GetName() + TEXT(" preserves its caption color during first-use emphasis"),
            Entry.Key->GetColorAndOpacity() == Entry.Value);
    for (const auto& Entry : OriginalIconBrushes)
        TestTrue(Entry.Key->GetName() + TEXT(" preserves its original icon brush"), Entry.Key->Background == Entry.Value);
    for (const auto& Entry : OriginalIconTints)
        TestTrue(Entry.Key->GetName() + TEXT(" preserves its original icon tint"), Entry.Key->GetBrushColor().Equals(Entry.Value));
    for (const auto& Entry : OriginalIconText)
        TestTrue(Entry.Key->GetName() + TEXT(" preserves its original icon text color"), Entry.Key->GetColorAndOpacity() == Entry.Value);
    TestEqual(TEXT("Every guide topic is available in the real selector"), Combo->GetOptionCount(), FFBuildFeatureGuide::Topics().Num());
    TestTrue(TEXT("Confirm accepts keyboard focus"), Confirm->GetIsFocusable());
    TestTrue(TEXT("Close accepts keyboard focus"), Close->GetIsFocusable());
    Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        const auto Ready = [](const FSlateBrush& Brush)
        { if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Material->EnsureIsComplete(); };
        if (auto* Border = Cast<UBorder>(Child)) Ready(Border->Background);
        if (auto* Button = Cast<UButton>(Child)) { Ready(Button->GetStyle().Normal); Button->SetIsEnabled(true); }
        if (auto* Image = Cast<UImage>(Child)) Ready(Image->GetBrush());
    });
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildFeatureGuide");
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        const FVector2D Size(Resolution.X, Resolution.Y);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!TestNotNull(TEXT("Guide render target"), Target.Get())) return false;
        FHittestGrid Grid;
        const auto Draw = [&]
        {
            for (int32 Frame = 0; Frame < 4; ++Frame)
            { Widget->ForceLayoutPrepass(); Renderer.DrawWindow(Target.Get(), Grid, Window, 1.f, Size, .016f, false); }
            FlushRenderingCommands();
        };
        const auto Capture = [&](const FString& Name)
        {
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
            if (!TestTrue(TEXT("Guide pixels readable"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags))) return;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
            TestTrue(TEXT("Guide screenshot saved"), FFileHelper::SaveArrayToFile(Png,
                *(Directory / FString::Printf(TEXT("%s_%dx%d.png"), *Name, Resolution.X, Resolution.Y))));
        };
        Widget->CloseFeatureGuide();
        Draw();
        const FSlateRect Viewport(0.f, 0.f, Resolution.X, Resolution.Y);
        TestTrue(TEXT("Extended toolbar fits viewport"), Contains(Viewport, Bounds(Top)));
        TestTrue(TEXT("Guide appears immediately right of help"), Bounds(Open).Left >= Bounds(Help).Right &&
            Bounds(Open).Left - Bounds(Help).Right <= 12.f * Dpi);
        for (const auto& Entry : Badges)
        {
            const FString Case = FString::Printf(TEXT("%s %dx%d: "), *Entry.Key->GetName(), Resolution.X, Resolution.Y);
            const FSlateRect BadgeBounds = Bounds(Entry.Key);
            TestTrue(Case + TEXT("badge fits toolbar"), Contains(Bounds(Top), BadgeBounds));
            TestTrue(Case + TEXT("badge fits its clickable button"), Contains(Bounds(Entry.Value), BadgeBounds));
            const FVector2D BadgeCenter((BadgeBounds.Left + BadgeBounds.Right) * .5f,
                (BadgeBounds.Top + BadgeBounds.Bottom) * .5f);
            const auto HitPath = Grid.GetBubblePath(BadgeCenter, 0.f, false);
            TestTrue(Case + TEXT("clicking the badge reaches its toolbar button"),
                HitPath.ContainsByPredicate([&](const FWidgetAndPointer& Hit)
                { return &Hit.Widget.Get() == Entry.Value->GetCachedWidget().Get(); }));
            TestFalse(Case + TEXT("badge does not intercept pointer input"),
                HitPath.ContainsByPredicate([&](const FWidgetAndPointer& Hit)
                { return &Hit.Widget.Get() == Entry.Key->GetCachedWidget().Get(); }));
        }
        Capture(TEXT("Toolbar"));
        Open->OnClicked.Broadcast();
        TestTrue(TEXT("Saved toolbar button opens guide"), Widget->IsFeatureGuideOpen());
        for (const auto& Topic : FFBuildFeatureGuide::Topics())
        {
            Combo->SetSelectedOption(Topic.Title.ToString());
            Scroll->ScrollToStart();
            Draw();
            const FString Case = FString::Printf(TEXT("%s %dx%d: "), *Topic.Id.ToString(), Resolution.X, Resolution.Y);
            TestEqual(Case + TEXT("selector changes title"), Title->GetText().ToString(), Topic.Title.ToString());
            TestEqual(Case + TEXT("selector changes full explanation"), Body->GetText().ToString(), Topic.Body.ToString());
            TestTrue(Case + TEXT("card fits viewport"), Contains(Viewport, Bounds(Card)));
            TestTrue(Case + TEXT("modal covers full viewport"), Contains(Bounds(Panel), Viewport));
            TestTrue(Case + TEXT("world pointer is blocked"), Widget->IsPointerOverTools());
            for (UWidget* Control : {static_cast<UWidget*>(Combo), static_cast<UWidget*>(Title),
                static_cast<UWidget*>(Confirm), static_cast<UWidget*>(Close), static_cast<UWidget*>(Scroll)})
                TestTrue(Case + Control->GetName() + TEXT(" fits card"), Contains(Bounds(Card), Bounds(Control)));
            TestTrue(Case + TEXT("body has readable scroll area"), Scroll->GetCachedGeometry().GetLocalSize().Y >= 230.f);
            if (Topic.Id == TEXT("LightingControls") || Topic.Id == TEXT("MultiSelection")) Capture(Topic.Id.ToString());
        }
        Close->OnClicked.Broadcast();
        TestFalse(TEXT("Saved close button dismisses guide"), Widget->IsFeatureGuideOpen());
    }
    return true;
}
#endif
