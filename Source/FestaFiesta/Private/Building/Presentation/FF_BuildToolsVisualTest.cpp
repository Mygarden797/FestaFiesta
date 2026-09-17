// Renders the actual Designer asset and checks its laid-out control bounds. This is a
// playerless UI fixture: sample labels/enabled controls do not invoke any building command.

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Building/Presentation/FF_BuildToolsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Editor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"

namespace
{
    class FScopedBuildToolsPlayerlessInitialization
    {
    public:
        explicit FScopedBuildToolsPlayerlessInitialization(UWidgetBlueprintGeneratedClass& InClass)
            : Class(InClass), bPrevious(InClass.bCanCallInitializedWithoutPlayerContext)
        {
            Class.bCanCallInitializedWithoutPlayerContext = true;
        }
        ~FScopedBuildToolsPlayerlessInitialization()
        {
            Class.bCanCallInitializedWithoutPlayerContext = bPrevious;
        }
    private:
        UWidgetBlueprintGeneratedClass& Class;
        bool bPrevious;
    };

    const FName BuildToolsControls[] = {
        TEXT("Button_Confirm"), TEXT("Button_Grid"), TEXT("Button_Catalog"),
        TEXT("Button_SnapMeter"), TEXT("Button_SnapPrecision"), TEXT("Button_SnapFree"),
        TEXT("Button_RotateLeft"), TEXT("Editable_Angle"), TEXT("Button_RotateRight"),
        TEXT("Button_NudgeLeft"), TEXT("Button_NudgeDown"), TEXT("Button_NudgeUp"), TEXT("Button_NudgeRight"),
        TEXT("Button_FreeRotateLeft"), TEXT("Button_FreeRotateRight")
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildToolsRenderedPreviewTest,
    "FestaFiesta.Building.BuildTools.RenderedPreview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildToolsRenderedPreviewTest::RunTest(const FString& Parameters)
{
    if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
    {
        AddInfo(TEXT("SKIPPED: Build tools PNG capture needs a real RHI. Run with -RenderOffscreen."));
        return true;
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    UWidgetBlueprintGeneratedClass* WidgetClass = Cast<UWidgetBlueprintGeneratedClass>(
        LoadClass<UFF_BuildToolsWidget>(nullptr,
            TEXT("/Game/FestaFiesta/UI/Building/WBP_FF_BuildTools.WBP_FF_BuildTools_C")));
    if (!TestNotNull(TEXT("Editor world for playerless visual fixture"), World) ||
        !TestNotNull(TEXT("Actual WBP_FF_BuildTools asset (create with FF.BuildToolsUMG first)"), WidgetClass))
    {
        return false;
    }
    FScopedBuildToolsPlayerlessInitialization Initialization(*WidgetClass);
    UFF_BuildToolsWidget* Widget = CreateWidget<UFF_BuildToolsWidget>(World, WidgetClass);
    if (!TestNotNull(TEXT("Actual toolbar instance"), Widget))
    {
        return false;
    }
    // NativeConstruct projects Closed in this playerless world. Materialize it first, then
    // supply a visible preview example without touching runtime state or calling action handlers.
    const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
    Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Widget->SetIsEnabled(true);
    for (const FName Name : BuildToolsControls)
    {
        UWidget* Control = Widget->GetWidgetFromName(Name);
        if (TestNotNull(*FString::Printf(TEXT("Designer control %s exists"), *Name.ToString()), Control))
        {
            Control->SetIsEnabled(true);
        }
    }
    const auto SetFixtureText = [Widget](const FName Name, const TCHAR* Text)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name)))
        {
            Label->SetText(FText::FromString(Text));
        }
    };
    SetFixtureText(TEXT("Text_Mode"), TEXT("시설 배치"));
    SetFixtureText(TEXT("Text_Legend"), TEXT("현재 100cm 맞춤 · 초록: 배치 가능 · 빨강: 배치 불가"));
    SetFixtureText(TEXT("Text_Grid"), TEXT("격자 켜짐"));
    SetFixtureText(TEXT("Text_Confirm"), TEXT("배치 확정"));
    SetFixtureText(TEXT("Text_FacilityName"), TEXT("바다 공방"));
    SetFixtureText(TEXT("Text_Cost"), TEXT("설치 300 Gold"));
    SetFixtureText(TEXT("Text_ActiveSnap"), TEXT("임시 25cm 적용 중"));
    // The playerless Closed state has no valid placement/grid. Project those fixture
    // selection visuals explicitly, just as RefreshTools does for a valid runtime preview.
    for (const TPair<FName, FName>& Names : {
        TPair<FName, FName>(TEXT("Button_Confirm"), TEXT("Text_Confirm")),
        TPair<FName, FName>(TEXT("Button_Grid"), TEXT("Text_Grid")) })
    {
        if (auto* Button = Cast<UFF_BuildToolsActionButton>(Widget->GetWidgetFromName(Names.Key)))
        {
            Button->ApplySelection(true, Cast<UTextBlock>(Widget->GetWidgetFromName(Names.Value)));
        }
    }
    if (auto* Legend = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_Legend"))))
    {
        Legend->SetColorAndOpacity(Widget->ReadyStatusColor);
    }
    if (UEditableTextBox* Angle = Cast<UEditableTextBox>(Widget->GetWidgetFromName(TEXT("Editable_Angle"))))
    {
        Angle->SetText(FText::FromString(TEXT("37.5")));
    }
    UBorder* Panel = Cast<UBorder>(Widget->GetWidgetFromName(TEXT("Border_Tools")));
    if (!TestNotNull(TEXT("Designer toolbar panel"), Panel))
    {
        return false;
    }
    Widget->ForceLayoutPrepass();
    constexpr int32 Width = 1280;
    constexpr int32 Height = 720;
    // Explicitly request the sRGB target used by facility/paid-program captures. The
    // convenience DrawWidget overload would create a linear target for this renderer.
    FWidgetRenderer Renderer(false, true);
    UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Width, Height), TF_Bilinear, true);
    if (!TestNotNull(TEXT("sRGB toolbar capture target"), Target)) { return false; }
    Renderer.DrawWidget(Target, SlateWidget, FVector2D(Width, Height), 0.0f, false);
    TArray<FColor> Pixels;
    FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
    FReadSurfaceDataFlags ReadFlags;
    ReadFlags.SetLinearToGamma(false);
    if (!TestTrue(TEXT("Actual WBP renders into a complete image"),
        Resource && Resource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == Width * Height))
    {
        return false;
    }
    const FGeometry& PanelGeometry = Panel->GetCachedGeometry();
    const FVector2D PanelTop = PanelGeometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D PanelBottom = PanelGeometry.LocalToAbsolute(PanelGeometry.GetLocalSize());
    TestTrue(TEXT("Toolbar panel has a real arranged size"),
        PanelGeometry.GetLocalSize().X > 100.0 && PanelGeometry.GetLocalSize().Y > 30.0);
    TestTrue(TEXT("Bottom anchored panel fits the 1280 by 720 viewport"),
        PanelTop.X >= 0.0 && PanelTop.Y >= 0.0 && PanelBottom.X <= Width + 1.0 && PanelBottom.Y <= Height + 1.0);
    AddInfo(FString::Printf(TEXT("Toolbar panel: %.1f x %.1f at (%.1f, %.1f), bottom (%.1f, %.1f)"),
        PanelGeometry.GetLocalSize().X, PanelGeometry.GetLocalSize().Y,
        PanelTop.X, PanelTop.Y, PanelBottom.X, PanelBottom.Y));
    for (const FName Name : BuildToolsControls)
    {
        const UWidget* Control = Widget->GetWidgetFromName(Name);
        if (!Control)
        {
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s is enabled in the placement fixture"), *Name.ToString()), Control->GetIsEnabled());
        const FGeometry& Geometry = Control->GetCachedGeometry();
        const FVector2D Top = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
        const FVector2D Bottom = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
        TestTrue(*FString::Printf(TEXT("%s retains a usable control height"), *Name.ToString()),
            Geometry.GetLocalSize().X > 10.0 && Geometry.GetLocalSize().Y >= 20.0);
        TestTrue(*FString::Printf(TEXT("%s stays fully inside the toolbar panel"), *Name.ToString()),
            Top.X >= PanelTop.X - 0.5 && Top.Y >= PanelTop.Y - 0.5 &&
            Bottom.X <= PanelBottom.X + 0.5 && Bottom.Y <= PanelBottom.Y + 0.5);
        AddInfo(FString::Printf(TEXT("%s: x %.1f..%.1f, y %.1f..%.1f, size %.1f x %.1f"),
            *Name.ToString(), Top.X, Bottom.X, Top.Y, Bottom.Y,
            Geometry.GetLocalSize().X, Geometry.GetLocalSize().Y));
    }
    int32 VisiblePixels = 0;
    for (const FColor& Pixel : Pixels)
    {
        VisiblePixels += Pixel.R > 20 || Pixel.G > 20 || Pixel.B > 20 ? 1 : 0;
    }
    TestTrue(TEXT("Captured toolbar contains visible palette/text pixels"), VisiblePixels > 10000);
    const auto CheckSelectionColor = [this, Widget](const TArray<FColor>& Capture, const int32 CaptureWidth,
        const int32 CaptureHeight, const TCHAR* CaptureName)
    {
        const auto* Confirm = Cast<UFF_BuildToolsActionButton>(Widget->GetWidgetFromName(TEXT("Button_Confirm")));
        if (!TestNotNull(TEXT("Selected confirmation button for palette verification"), Confirm)) { return; }
        const FGeometry& Geometry = Confirm->GetCachedGeometry();
        // Sample inside the solid fill, left of the label and clear of the rounded outline.
        const FVector2D Point = Geometry.LocalToAbsolute(FVector2D(8.0, Geometry.GetLocalSize().Y * 0.5));
        const int32 X = FMath::FloorToInt(Point.X);
        const int32 Y = FMath::FloorToInt(Point.Y);
        if (!TestTrue(TEXT("Palette sample lies inside the capture"), X >= 0 && Y >= 0 &&
            X < CaptureWidth && Y < CaptureHeight)) { return; }
        const FColor Actual = Capture[Y * CaptureWidth + X];
        // The approved selection brush is #315747. Derive the expected sRGB bytes from
        // the authored brush so intentional subsequent Designer palette edits remain valid.
        const FColor Expected = Confirm->SelectionStyle.Normal.TintColor.GetSpecifiedColor().ToFColor(true);
        const bool bMatches = FMath::Abs(int32(Actual.R) - int32(Expected.R)) <= 2 &&
            FMath::Abs(int32(Actual.G) - int32(Expected.G)) <= 2 &&
            FMath::Abs(int32(Actual.B) - int32(Expected.B)) <= 2 && Actual.A >= 250;
        TestTrue(*FString::Printf(TEXT("%s: selected fill round-trips to authored sRGB (actual #%02X%02X%02X, expected #%02X%02X%02X)"),
            CaptureName, Actual.R, Actual.G, Actual.B, Expected.R, Expected.G, Expected.B), bMatches);
    };
    CheckSelectionColor(Pixels, Width, Height, TEXT("Toolbar"));
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/BuildableGrid"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, TEXT("BuildTools.png"));
    TArray64<uint8> Png;
    FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
    const bool bSaved = FFileHelper::SaveArrayToFile(Png, *Path);
    TestTrue(TEXT("Actual toolbar PNG exists on disk"), bSaved && IFileManager::Get().FileSize(*Path) > 0);
    AddInfo(FString::Printf(TEXT("Build tools Designer fixture PNG: %s"), *FPaths::ConvertRelativePathToFull(Path)));

    // Also export the same real panel subtree at its measured size for legible close-up review.
    const int32 DetailWidth = FMath::Max(1, FMath::CeilToInt(PanelGeometry.GetLocalSize().X));
    const int32 DetailHeight = FMath::Max(1, FMath::CeilToInt(PanelGeometry.GetLocalSize().Y));
    UTextureRenderTarget2D* DetailTarget = FWidgetRenderer::CreateTargetFor(FVector2D(DetailWidth, DetailHeight), TF_Bilinear, true);
    if (!TestNotNull(TEXT("sRGB toolbar detail capture target"), DetailTarget)) { return false; }
    Renderer.DrawWidget(DetailTarget, Panel->TakeWidget(), FVector2D(DetailWidth, DetailHeight), 0.0f, false);
    FTextureRenderTargetResource* DetailResource = DetailTarget ? DetailTarget->GameThread_GetRenderTargetResource() : nullptr;
    Pixels.Reset();
    if (DetailResource && DetailResource->ReadPixels(Pixels, ReadFlags) && Pixels.Num() == DetailWidth * DetailHeight)
    {
        CheckSelectionColor(Pixels, DetailWidth, DetailHeight, TEXT("Toolbar close-up"));
        Png.Reset();
        FImageUtils::PNGCompressImageArray(DetailWidth, DetailHeight, Pixels, Png);
        const FString DetailPath = FPaths::Combine(Directory, TEXT("BuildToolsDetail.png"));
        TestTrue(TEXT("Toolbar close-up PNG saved"), FFileHelper::SaveArrayToFile(Png, *DetailPath));
        AddInfo(FString::Printf(TEXT("Build tools close-up: %s"), *FPaths::ConvertRelativePathToFull(DetailPath)));
    }
    return bSaved;
}

#endif
