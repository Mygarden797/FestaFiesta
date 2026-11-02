// FF_BuildToolsVisualTest.cpp
// 역할: 건설 도구 모음의 상태별 화면을 실제로 렌더링해 확인한다.

// Render each real authored HUD state without a player or building authority.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
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
#include "UObject/UnrealType.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SVirtualWindow.h"

namespace FFBuildToolsVisual
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
bool IsRendered(const UWidget* Widget)
{
    for (const UWidget* Current = Widget; Current; Current = Current->GetParent())
        if (Current->GetVisibility() == ESlateVisibility::Collapsed || Current->GetVisibility() == ESlateVisibility::Hidden) return false;
    return Widget != nullptr;
}
bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    return Inner.Left >= Outer.Left - 1 && Inner.Right <= Outer.Right + 1 &&
        Inner.Top >= Outer.Top - 1 && Inner.Bottom <= Outer.Bottom + 1;
}
bool Overlaps(const FSlateRect& A, const FSlateRect& B)
{
    return A.Left < B.Right - .5f && A.Right > B.Left + .5f && A.Top < B.Bottom - .5f && A.Bottom > B.Top + .5f;
}
FSlateRect Bounds(const UWidget* Widget) { return Widget->GetPaintSpaceGeometry().GetRenderBoundingRect(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRenderedPreviewTest,
    "FestaFiesta.Building.BuildTools.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildToolsRenderedPreviewTest::RunTest(const FString& Parameters)
{
    using namespace FFBuildToolsVisual;
    (void)Parameters;
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    { AddError(TEXT("Rendered preview requires -RenderOffscreen and a real RHI.")); return false; }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr, FFBuildUIAssetPaths::BuildToolsClass));
    if (!TestNotNull(TEXT("Editor world"), World) || !TestNotNull(TEXT("Saved build tools class"), Class)) return false;
    FPlayerlessInitialization Initialization(*Class);
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production build tools"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    // 화면만 시드한다. 게임 월드가 없는 이 캡처에서 권위 상태 갱신은 실행하지 않는다.
    Slate->SetCanTick(false);
    Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Widget->SetIsEnabled(true);
    UWidget* Top = Widget->GetWidgetFromName(TEXT("Border_BuildTopBar"));
    UWidget* Back = Widget->GetWidgetFromName(TEXT("Border_BuildBack"));
    UWidget* Info = Widget->GetWidgetFromName(TEXT("Border_PlacementInfo"));
    UWidget* Settings = Widget->GetWidgetFromName(TEXT("Panel_BuildSettings"));
    UWidget* Help = Widget->GetWidgetFromName(TEXT("Border_PlacementHelp"));
    UWidget* HelpSurface = Widget->GetWidgetFromName(TEXT("Size_BuildWorldHelp"));
    UWidget* Toolbar = Widget->GetWidgetFromName(TEXT("Widget_ToolbarFeatureIcons"));
    if (!TestNotNull(TEXT("Current floating toolbar"), Top) || !TestNotNull(TEXT("Back navigation"), Back) ||
        !TestNotNull(TEXT("Current placement information"), Info) || !TestNotNull(TEXT("Settings drawer"), Settings) ||
        !TestNotNull(TEXT("Full instruction body"), Help) || !TestNotNull(TEXT("Instruction surface"), HelpSurface) ||
        !TestNotNull(TEXT("Active nested icon toolbar"), Toolbar)) return false;
    TestNull(TEXT("World-click placement has no obsolete confirmation dock"), Widget->GetWidgetFromName(TEXT("Border_Tools")));
    const auto SetText = [&](const TCHAR* Name, const FText& Text)
    {
        auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(FString(TEXT("Current presentation text: ")) + Name, Label)) Label->SetText(Text);
    };
    SetText(TEXT("Text_FacilityName"), FText::FromString(TEXT("구름 솜사탕 부스")));
    SetText(TEXT("Text_Cost"), FText::FromString(TEXT("설치 비용 400 Gold")));
    SetText(TEXT("Text_Footprint"), FText::FromString(TEXT("시설 크기 3.2 × 1.97 m")));
    SetText(TEXT("Text_Legend"), FText::FromString(TEXT("배치 가능 · 눌러서 설치")));
    TFunction<void(UUserWidget&)> Ready = [&](UUserWidget& Owner)
    {
        if (!Owner.WidgetTree) return;
        Owner.WidgetTree->ForEachWidget([&](UWidget* Child)
        {
            const auto BrushReady = [](const FSlateBrush& Brush)
            { if (auto* Material = Cast<UMaterialInterface>(Brush.GetResourceObject())) Material->EnsureIsComplete(); };
            if (auto* Border = Cast<UBorder>(Child)) BrushReady(Border->Background);
            if (auto* Button = Cast<UButton>(Child)) { Button->SetIsEnabled(true); BrushReady(Button->GetStyle().Normal); }
            if (auto* Image = Cast<UImage>(Child)) BrushReady(Image->GetBrush());
            if (auto* Nested = Cast<UUserWidget>(Child)) Ready(*Nested);
        });
    };
    Ready(*Widget);
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildTools");
    IFileManager::Get().MakeDirectory(*Directory, true);
    for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        const FVector2D Size(Resolution);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!TestNotNull(TEXT("Preview render target"), Target.Get())) return false;
        FHittestGrid HitGrid;
        for (const FName State : {FName(TEXT("CatalogToolbar")), FName(TEXT("Placement")), FName(TEXT("Instructions")), FName(TEXT("Settings"))})
        {
            const bool bSettings = State == TEXT("Settings");
            const bool bPlacement = State == TEXT("Placement") || State == TEXT("Instructions");
            const bool bInstructions = State == TEXT("Instructions");
            Top->SetVisibility(ESlateVisibility::Visible);
            Back->SetVisibility(ESlateVisibility::Visible);
            Info->SetVisibility(bPlacement ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            Settings->SetVisibility(bSettings ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            HelpSurface->SetVisibility(bInstructions ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
            Help->SetVisibility(bInstructions ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
            for (int32 Frame = 0; Frame < 6; ++Frame)
            { Widget->ForceLayoutPrepass(); Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false); }
            FlushRenderingCommands();
            const FString Context = FString::Printf(TEXT("%s %dx%d: "), *State.ToString(), Resolution.X, Resolution.Y);
            const FSlateRect Viewport(0.f, 0.f, Resolution.X, Resolution.Y);
            TestTrue(Context + TEXT("active icon toolbar fits the viewport"), IsRendered(Top) && Contains(Viewport, Bounds(Top)));
            TestTrue(Context + TEXT("back navigation remains visible"), IsRendered(Back) && Contains(Viewport, Bounds(Back)));
            TestTrue(Context + TEXT("actual nested icon bar has rendered geometry"), Toolbar->GetCachedGeometry().GetLocalSize().GetMin() > 0.f);
            if (bPlacement)
            {
                TestTrue(Context + TEXT("the current placement inspector fits the viewport"), Contains(Viewport, Bounds(Info)));
                TestFalse(Context + TEXT("placement information leaves toolbar controls clear"), Overlaps(Bounds(Info), Bounds(Top)));
                for (const TCHAR* Name : {TEXT("Text_FacilityName"), TEXT("Text_Cost"), TEXT("Text_Footprint")})
                {
                    UWidget* Label = Widget->GetWidgetFromName(Name);
                    TestTrue(Context + TEXT("placement information is visible: ") + Name,
                        Label && IsRendered(Label) && Label->GetCachedGeometry().GetLocalSize().GetMin() > 0.f && Contains(Bounds(Info), Bounds(Label)));
                }
            }
            if (bInstructions)
            {
                auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildHelp")));
                TestTrue(Context + TEXT("the full instruction surface is reachable"), Scroll && IsRendered(Scroll) &&
                    Scroll->GetCachedGeometry().GetLocalSize().GetMin() > 0.f && Contains(Viewport, Bounds(Help)));
                TestFalse(Context + TEXT("instructions leave the active inspector clear"), Overlaps(Bounds(Help), Bounds(Info)));
            }
            if (bSettings)
            {
                auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_BuildSettings")));
                auto* Close = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_BuildSettingsClose")));
                TestTrue(Context + TEXT("settings retain a readable viewport"), Scroll && Scroll->GetCachedGeometry().GetLocalSize().GetMin() > 0.f);
                TestTrue(Context + TEXT("settings close action stays on screen"), Close && Contains(Viewport, Bounds(Close)));
                if (Close)
                {
                    const FVector2D Center = Close->GetCachedGeometry().LocalToAbsolute(Close->GetCachedGeometry().GetLocalSize() * .5f);
                    const auto Path = HitGrid.GetBubblePath(Center, 0.f, false);
                    TestTrue(Context + TEXT("settings can actually be closed by pointer"), Path.ContainsByPredicate([&](const FWidgetAndPointer& Hit)
                        { return &Hit.Widget.Get() == Close->GetCachedWidget().Get(); }));
                }
            }
            TestEqual(Context + TEXT("only current settings reserve auxiliary input"), Widget->IsAuxiliaryPanelOpen(), bSettings);
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
            if (!TestTrue(Context + TEXT("rendered image is complete"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) &&
                Pixels.Num() == Resolution.X * Resolution.Y)) return false;
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
            TestTrue(Context + TEXT("screenshot saved"), FFileHelper::SaveArrayToFile(Png,
                *(Directory / FString::Printf(TEXT("BuildTools_%s_%d.png"), *State.ToString(), Resolution.Y))));
        }
    }
    return true;
}
#endif
