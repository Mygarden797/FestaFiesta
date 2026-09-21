// Render each real authored HUD state without a player or building authority.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
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
FSlateRect Bounds(const UWidget* Widget) { return Widget->GetPaintSpaceGeometry().GetLayoutBoundingRect(); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRenderedPreviewTest,
    "FestaFiesta.Building.BuildTools.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildToolsRenderedPreviewTest::RunTest(const FString& Parameters)
{
    using namespace FFBuildToolsVisual;
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: HUD screenshots require a real RHI; use -RenderOffscreen."));
        return true;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    auto* Class = Cast<UWidgetBlueprintGeneratedClass>(LoadClass<UFF_BuildToolsWidget>(nullptr,
        TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools.WBP_FF_BuildTools_C")));
    if (!TestNotNull(TEXT("Playerless editor world"), World) || !TestNotNull(TEXT("Saved compact build HUD"), Class)) return false;
    FPlayerlessInitialization Context(*Class);
    TStrongObjectPtr<UFF_BuildToolsWidget> Widget(CreateWidget<UFF_BuildToolsWidget>(World, Class));
    if (!TestNotNull(TEXT("Production tools widget"), Widget.Get())) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Widget->SetIsEnabled(true);
    auto* Data = FindFProperty<FStructProperty>(Class, TEXT("ViewData"));
    if (!TestNotNull(TEXT("Presentation-only fixture seam"), Data)) return false;
    Data->ContainerPtrToValuePtr<FFF_BuildToolsViewData>(Widget.Get())->bPreviewActive = true;
    auto Text = [&](FName Name, const TCHAR* Value)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name))) Label->SetText(FText::FromString(Value));
    };
    Text(TEXT("Text_Mode"), TEXT("시설 배치"));
    Text(TEXT("Text_FacilityName"), TEXT("구름 솜사탕 부스"));
    Text(TEXT("Text_Cost"), TEXT("설치 400 Gold"));
    Text(TEXT("Text_Balance"), TEXT("보유 7,200 Gold  ·  확정 후 6,800 Gold"));
    Text(TEXT("Text_Legend"), TEXT("배치 가능 · 좌클릭 / Enter로 확정"));
    Text(TEXT("Text_ActiveSnap"), TEXT("100 cm 맞춤"));
    Text(TEXT("Text_Confirm"), TEXT("배치 확정"));
    Text(TEXT("Text_Footprint"), TEXT("시설 크기  3.2 × 1.97 m\n안전 여유  1 m"));
    Text(TEXT("Text_Queue"), TEXT("대기 정원  5명\n동시 이용  1명  ·  1인 서비스  6초"));
    Text(TEXT("Text_ControlHelp"), TEXT("좌클릭 / Enter  설치 확정\nQ / E  45°씩 회전\nShift + Q / E  자유 회전\nCtrl  25 cm · Alt  1 cm\nT  직접 조정 · G  격자 표시\nB / 우클릭  시설 목록\nW A S D  카메라 이동\nEsc  일시정지"));
    auto* Dock = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_Tools")));
    auto* Info = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_PlacementInfo")));
    auto* Help = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_PlacementHelp")));
    if (!TestNotNull(TEXT("Compact tool dock"), Dock) || !TestNotNull(TEXT("Compact facility summary"), Info)) return false;
    TArray<UWidget*> Controls;
    TSet<UMaterialInterface*> Materials;
    auto Material = [&](const FSlateBrush& Brush)
    { if (auto* Value = Cast<UMaterialInterface>(Brush.GetResourceObject())) Materials.Add(Value); };
    Widget->WidgetTree->ForEachWidget([&](UWidget* Child)
    {
        if (UButton* Button = Cast<UButton>(Child))
        {
            Button->SetIsEnabled(true); Controls.Add(Button);
            Material(Button->GetStyle().Normal); Material(Button->GetStyle().Hovered);
            Material(Button->GetStyle().Pressed); Material(Button->GetStyle().Disabled);
        }
        if (const UBorder* Border = Cast<UBorder>(Child)) Material(Border->Background);
        if (const UImage* Image = Cast<UImage>(Child)) Material(Image->GetBrush());
    });
    auto* Confirm = Cast<UFF_BuildToolsActionButton>(Widget->GetWidgetFromName(TEXT("Button_Confirm")));
    if (Confirm)
    {
        Confirm->ApplySelection(true, Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_Confirm"))));
        Material(Confirm->SelectionStyle.Normal);
    }
    for (UMaterialInterface* Value : Materials) Value->EnsureIsComplete();
    if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
    FlushRenderingCommands();
    const FString Directory = FPaths::ProjectSavedDir() / TEXT("Automation/BuildableGrid");
    IFileManager::Get().MakeDirectory(*Directory, true);
    auto Click = [&](FName Name)
    {
        UButton* Button = Cast<UButton>(Widget->GetWidgetFromName(Name));
        if (TestNotNull(*Name.ToString(), Button)) Button->OnClicked.Broadcast();
    };
    bool bAllSaved = true;
    for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        const FVector2D Size(Resolution.X, Resolution.Y);
        const float Dpi = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Resolution);
        auto Window = SNew(SVirtualWindow).Size(Size);
        Window->SetContent(SNew(SDPIScaler).DPIScale(Dpi)[Slate]);
        FWidgetRenderer Renderer(false, true);
        TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, true));
        if (!TestNotNull(TEXT("HUD render target"), Target.Get())) return false;
        FHittestGrid HitGrid;
        for (FName Name : {TEXT("Border_PlacementHelp"), TEXT("Panel_PlacementFacts")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Collapsed);
        for (FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage"),
            TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Collapsed);
        auto Capture = [&](const TCHAR* State)
        {
            for (int32 Frame = 0; Frame < 4; ++Frame)
            {
                Widget->ForceLayoutPrepass();
                Renderer.DrawWindow(Target.Get(), HitGrid, Window, 1.f, Size, .016f, false);
            }
            const FString ContextName = FString::Printf(TEXT("%s %dx%d"), State, Resolution.X, Resolution.Y);
            const FSlateRect Viewport(0, 0, Resolution.X, Resolution.Y);
            const FSlateRect DockRect = Bounds(Dock), InfoRect = Bounds(Info);
            TestTrue(ContextName + TEXT(": summary and dock fit the viewport"), Contains(Viewport, DockRect) && Contains(Viewport, InfoRect));
            TestFalse(ContextName + TEXT(": summary and dock do not overlap"), Overlaps(DockRect, InfoRect));
            TestTrue(ContextName + TEXT(": dock leaves bottom navigation clear"), DockRect.Bottom <= Resolution.Y - 80.f * Dpi);
            TestTrue(ContextName + TEXT(": world center remains clear"),
                DockRect.Left > Resolution.X * .65f && InfoRect.Right < Resolution.X * .35f);
            if (IsRendered(Help))
            {
                const FSlateRect HelpRect = Bounds(Help);
                TestTrue(ContextName + TEXT(": help follows the left summary without overlap"),
                    Contains(Viewport, HelpRect) && FMath::IsNearlyEqual(HelpRect.Left, InfoRect.Left, 1.f) &&
                    FMath::IsNearlyEqual(HelpRect.Right, InfoRect.Right, 1.f) && HelpRect.Top >= InfoRect.Bottom + 8.f * Dpi);
                TestTrue(ContextName + TEXT(": help leaves the bottom navigation clear"), HelpRect.Bottom <= Resolution.Y - 80.f * Dpi);
                TestFalse(ContextName + TEXT(": left help and right tools are separate"), Overlaps(HelpRect, DockRect));
            }
            TArray<UWidget*> VisibleControls;
            for (UWidget* Control : Controls)
            {
                if (!IsRendered(Control)) continue;
                VisibleControls.Add(Control);
                const FSlateRect Rect = Bounds(Control);
                const FString Label = ContextName + TEXT(": ") + Control->GetName();
                TestTrue(Label + TEXT(" has a usable target"), Control->GetCachedGeometry().GetLocalSize().X >= 30 && Control->GetCachedGeometry().GetLocalSize().Y >= 20);
                TestTrue(Label + TEXT(" remains inside its own surface"), Contains(
                    Control->GetName() == TEXT("Button_Facts") || Control->GetName() == TEXT("Button_Help") ||
                    Control->GetName() == TEXT("Button_DirectAdjustment") || Control->GetName() == TEXT("Button_FindNearby") ||
                    Control->GetName() == TEXT("Button_NextPlacementProblem") ? InfoRect : DockRect, Rect));
                if (const UButton* Button = Cast<UButton>(Control))
                    if (const UWidget* Content = Button->GetContent()) TestTrue(Label + TEXT(" contains its label"), Contains(Rect, Bounds(Content)));
            }
            for (int32 A = 0; A < VisibleControls.Num(); ++A)
                for (int32 B = A + 1; B < VisibleControls.Num(); ++B)
                    TestFalse(ContextName + TEXT(": controls do not overlap: ") + VisibleControls[A]->GetName() + TEXT(" / ") + VisibleControls[B]->GetName(),
                        Overlaps(Bounds(VisibleControls[A]), Bounds(VisibleControls[B])));
            for (FName Name : {TEXT("Text_ControlHelp"), TEXT("Text_Footprint"), TEXT("Text_Queue"), TEXT("Text_RangeLegend"),
                TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage")})
            {
                UWidget* Label = Widget->GetWidgetFromName(Name);
                if (IsRendered(Label)) TestTrue(ContextName + TEXT(": expanded information stays inside its surface: ") + Name.ToString(),
                    Contains(Name == TEXT("Text_ControlHelp") ? Bounds(Help) : InfoRect, Bounds(Label)));
            }
            for (FName Name : {TEXT("Button_Radial"), TEXT("Panel_Radial"), TEXT("Panel_Details")})
                TestNull(ContextName + TEXT(": removed tool menu has no render or hit-test surface: ") + Name.ToString(),
                    Widget->GetWidgetFromName(Name));
            TestFalse(ContextName + TEXT(": legacy menu calls cannot reopen a panel"), Widget->IsAuxiliaryPanelOpen());
            TArray<FColor> Pixels;
            FReadSurfaceDataFlags Flags; Flags.SetLinearToGamma(false);
            if (!TestTrue(ContextName + TEXT(": complete rendered image"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels, Flags) && Pixels.Num() == Resolution.X * Resolution.Y))
            { bAllSaved = false; return; }
            if (Confirm)
            {
                const FVector2D Point = Confirm->GetCachedGeometry().LocalToAbsolute(FVector2D(10, Confirm->GetCachedGeometry().GetLocalSize().Y * .5));
                const int32 X = FMath::FloorToInt(Point.X), Y = FMath::FloorToInt(Point.Y);
                if (X >= 0 && Y >= 0 && X < Resolution.X && Y < Resolution.Y)
                    TestTrue(ContextName + TEXT(": primary action remains opaque enough to read"), Pixels[Y * Resolution.X + X].A >= 210);
            }
            TArray64<uint8> Png;
            FImageUtils::PNGCompressImageArray(Resolution.X, Resolution.Y, Pixels, Png);
            const FString Path = Directory / FString::Printf(TEXT("BuildTools_%s_%d.png"), State, Resolution.Y);
            const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
            bAllSaved &= bSaved;
            TestTrue(ContextName + TEXT(": screenshot saved"), bSaved);
            AddInfo(FString::Printf(TEXT("Saved %s"), *FPaths::ConvertRelativePathToFull(Path)));
        };
        Click(TEXT("Button_Help")); Capture(TEXT("DefaultHelp"));
        Click(TEXT("Button_Help")); Capture(TEXT("Compact"));
        Widget->ToggleRadialMenu(); Capture(TEXT("RemovedMenuEntry"));
        Click(TEXT("Button_Help")); Capture(TEXT("Help"));
        Click(TEXT("Button_Facts")); Capture(TEXT("FactsAndHelp"));
        Click(TEXT("Button_Help")); Capture(TEXT("Facts"));
        Click(TEXT("Button_Help")); Widget->ToggleRadialMenu(); Capture(TEXT("HelpAfterRemovedMenuEntry"));
        Text(TEXT("Text_Legend"), TEXT("대기 줄이 들어갈 공간이 없어요 · 1/2"));
        Text(TEXT("Text_PlacementAdvice"), TEXT("대기 줄 공간이 부족해요 · 입구 앞을 비워 주세요"));
        Text(TEXT("Text_PlacementObstruction"), TEXT("겹치는 대상: 구름 솜사탕 부스"));
        Text(TEXT("Text_AssistanceMessage"), TEXT("주변 6m의 후보 위치에서 자리를 찾지 못했어요. 회전하거나 T로 위치를 다시 선택해 주세요."));
        for (FName Name : {TEXT("Text_PlacementAdvice"), TEXT("Text_PlacementObstruction"), TEXT("Text_AssistanceMessage"),
            TEXT("Button_FindNearby"), TEXT("Button_NextPlacementProblem")})
            if (UWidget* Panel = Widget->GetWidgetFromName(Name)) Panel->SetVisibility(ESlateVisibility::Visible);
        Capture(TEXT("PlacementProblemAndHelp"));
        Text(TEXT("Text_Legend"), TEXT("배치 가능 · 좌클릭 / Enter로 확정"));
    }
    return bAllSaved;
}
#endif
