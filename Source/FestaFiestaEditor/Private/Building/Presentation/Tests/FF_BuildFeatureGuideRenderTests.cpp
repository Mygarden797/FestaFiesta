// Verify the real authored guide and its topic selector at the supported HUD resolutions.
#include "CoreMinimal.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildFeatureGuide.h"
#include "Building/Presentation/FF_BuildFeatureGuideButton.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Building/Presentation/FF_BuildUIAssetPaths.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
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
    auto* Toolbar = Cast<UUserWidget>(Widget->GetWidgetFromName(TEXT("Widget_ToolbarFeatureIcons")));
    auto* Panel = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Panel_FeatureGuide")));
    auto* Card = Widget->GetWidgetFromName(TEXT("Border_FeatureGuideCard"));
    auto* Combo = Cast<UComboBoxString>(Widget->GetWidgetFromName(TEXT("Combo_FeatureGuideTopic")));
    auto* Title = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_FeatureGuideTitle")));
    auto* Body = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_FeatureGuideBody")));
    auto* Scroll = Cast<UScrollBox>(Widget->GetWidgetFromName(TEXT("Scroll_FeatureGuideBody")));
    auto* Confirm = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_FeatureGuideConfirm")));
    auto* Close = Cast<UButton>(Widget->GetWidgetFromName(TEXT("Button_FeatureGuideClose")));
    if (!TestNotNull(TEXT("Floating toolbar"), Top) || !TestNotNull(TEXT("Active nested icon toolbar"), Toolbar) ||
        !TestNotNull(TEXT("Nested toolbar tree"), Toolbar->WidgetTree.Get()) || !TestNotNull(TEXT("Modal guide"), Panel) ||
        !TestNotNull(TEXT("Guide card"), Card) || !TestNotNull(TEXT("Topic selector"), Combo) ||
        !TestNotNull(TEXT("Guide title"), Title) || !TestNotNull(TEXT("Guide body"), Body) ||
        !TestNotNull(TEXT("Scrollable explanation"), Scroll) || !TestNotNull(TEXT("Confirm"), Confirm) ||
        !TestNotNull(TEXT("Close"), Close)) return false;
    Top->SetVisibility(ESlateVisibility::Visible);
    // 현재 아이콘 바의 내부 버튼을 측정한다. 숨긴 이전 Pearl 캡션은 클릭 대상이 아니다.
    TArray<UButton*> ToolbarButtons;
    Toolbar->WidgetTree->ForEachWidget([&](UWidget* Candidate)
    {
        if (auto* Guide = Cast<UFF_BuildFeatureGuideButton>(Candidate))
        {
            const auto* Topic = FFBuildFeatureGuide::Find(Guide->FeatureTopicId);
            auto* Button = Cast<UButton>(Guide->GetWidgetFromName(TEXT("Button_Guide")));
            if (TestNotNull(TEXT("Authored feature has a valid topic"), Topic) &&
                TestNotNull(TEXT("Authored feature has an internal action button"), Button))
            {
                Guide->RefreshGuidePresentation();
                TestEqual(TEXT("The icon exposes its current topic without a clipped caption"),
                    Button->GetToolTipText().ToString(), Topic->Title.ToString());
                TestTrue(TEXT("The active icon resolves a real authored brush"), Guide->ResolveIconBrush().GetResourceObject() != nullptr);
                Button->SetIsEnabled(true);
                ToolbarButtons.Add(Button);
            }
        }
    });
    TestTrue(TEXT("The current toolbar exposes all principal feature targets"), ToolbarButtons.Num() >= 7);
    TestEqual(TEXT("Every guide topic remains available in the selector"), Combo->GetOptionCount(), FFBuildFeatureGuide::Topics().Num());
    TestTrue(TEXT("Confirm supports keyboard focus"), Confirm->GetIsFocusable());
    TestTrue(TEXT("Close supports keyboard focus"), Close->GetIsFocusable());
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
        for (const UButton* Button : ToolbarButtons)
        {
            const FString Case = FString::Printf(TEXT("%s %dx%d: "), *Button->GetPathName(), Resolution.X, Resolution.Y);
            const FVector2D ButtonSize = Button->GetCachedGeometry().GetLocalSize();
            TestTrue(Case + TEXT("icon retains a usable logical hit target"), ButtonSize.X >= 32.f && ButtonSize.Y >= 32.f);
            TestTrue(Case + TEXT("active icon fits the floating toolbar"), Contains(Bounds(Top), Bounds(Button)));
            const FVector2D Center = Button->GetCachedGeometry().LocalToAbsolute(ButtonSize * .5f);
            const auto Path = Grid.GetBubblePath(Center, 0.f, false);
            TestTrue(Case + TEXT("visible icon is a real hit target"), Path.ContainsByPredicate([&](const FWidgetAndPointer& Hit)
                { return &Hit.Widget.Get() == Button->GetCachedWidget().Get(); }));
        }
        Capture(TEXT("Toolbar"));
        Widget->OpenFeatureGuide();
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
            // 현재 도움말은 오른쪽 정보 카드에 열린다. 카드 입력은 소유하면서 바깥 월드
            // 입력을 차단하는 네이티브 동작은 아래 IsPointerOverTools 검사로 따로 확인한다.
            const FSlateRect PanelBounds = Bounds(Panel);
            TestTrue(Case + TEXT("guide input surface follows its authored card inside the viewport"),
                PanelBounds.GetSize().GetMin() > 0.f && Contains(Viewport, PanelBounds) && Contains(PanelBounds, Bounds(Card)));
            const auto GuidePath = Grid.GetBubblePath(FVector2D(PanelBounds.Left + 4.f, PanelBounds.Top + 4.f), 0.f, false);
            TestTrue(Case + TEXT("the visible guide card owns its pointer surface"), GuidePath.ContainsByPredicate([&](const FWidgetAndPointer& Hit)
                { return &Hit.Widget.Get() == Panel->GetCachedWidget().Get(); }));
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
