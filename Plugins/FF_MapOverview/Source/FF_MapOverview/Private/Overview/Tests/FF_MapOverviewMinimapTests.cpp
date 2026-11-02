#include "FF_MapOverviewModel.h"
#include "FF_MapOverviewWidget.h"
#include "FF_MapOverviewMarkerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    UFFMapOverviewWidget* CreateAuthoredMinimap(FAutomationTestBase& Test)
    {
        UClass* Class = LoadClass<UFFMapOverviewWidget>(nullptr,
            TEXT("/FF_MapOverview/UI/WBP_FF_MapOverview.WBP_FF_MapOverview_C"));
        if (!Test.TestNotNull(TEXT("Actual editable minimap WBP generated class loads"), Class)) return nullptr;
        if (!Test.TestTrue(TEXT("Minimap class originates from a Blueprint asset"), Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint))) return nullptr;
        UFFMapOverviewWidget* Widget = NewObject<UFFMapOverviewWidget>(GetTransientPackage(), Class);
        if (!Test.TestNotNull(TEXT("Actual WBP instance"), Widget)
            || !Test.TestTrue(TEXT("WBP instance initializes its authored widget tree"), Widget->Initialize())) return nullptr;
        return Widget;
    }

    bool ReadWidgetPixels(FAutomationTestBase& Test, FWidgetRenderer& Renderer,
        const TSharedRef<SWidget>& Slate, const FIntPoint Resolution, FImage& OutImage)
    {
        UTextureRenderTarget2D* Target = Renderer.DrawWidget(Slate, FVector2D(Resolution));
        if (!Test.TestNotNull(TEXT("Authored WBP renders to a target"), Target)) return false;
        FlushRenderingCommands();
        return Test.TestTrue(TEXT("Authored WBP pixels are readable"), FImageUtils::GetRenderTargetImage(Target, OutImage));
    }

    int64 CountChangedBytes(const FImage& Before, const FImage& After)
    {
        if (Before.SizeX != After.SizeX || Before.SizeY != After.SizeY || Before.Format != After.Format
            || Before.RawData.Num() != After.RawData.Num()) return 0;
        int64 Count = 0;
        for (int64 Index = 0; Index < Before.RawData.Num(); ++Index)
            if (Before.RawData[Index] != After.RawData[Index]) ++Count;
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewProjectionTest, "FF_MapOverview.Minimap.WholeVenueProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewProjectionTest::RunTest(const FString&)
{
    // An elongated venue is the difficult case: using width/height independently
    // would distort it, while fitting only the longest side would crop the corners.
    FFFMapOverviewProjection Projection;
    const FBox Bounds(FVector(-50000, -7000, 100), FVector(90000, 12000, 300));
    TestTrue(TEXT("Valid whole venue"), Projection.SetBounds(Bounds));
    for (const double X : {Bounds.Min.X, Bounds.Max.X})
        for (const double Y : {Bounds.Min.Y, Bounds.Max.Y})
            TestTrue(TEXT("Every corner fits inside the circle"),
                (Projection.WorldToMap(FVector(X, Y, 0)) - FVector2D(0.5, 0.5)).Size() <= 0.420001);
    const FVector Point(30000, 1000, 200);
    FVector RoundTrip;
    TestTrue(TEXT("Point on map is navigable"), Projection.MapToWorld(Projection.WorldToMap(Point), RoundTrip));
    TestTrue(TEXT("Navigation preserves world coordinates"), RoundTrip.Equals(Point, 0.0001));
    TestTrue(TEXT("+X is north"), Projection.WorldToMap(Point + FVector(100, 0, 0)).Y < Projection.WorldToMap(Point).Y);
    TestTrue(TEXT("+Y is east"), Projection.WorldToMap(Point + FVector(0, 100, 0)).X > Projection.WorldToMap(Point).X);
    TestFalse(TEXT("Circle rim cannot navigate outside venue"), Projection.MapToWorld(FVector2D(0.94, 0.5), RoundTrip));
    TestFalse(TEXT("Invalid bounds are rejected"), Projection.SetBounds(FBox(ForceInit)));
    TestFalse(TEXT("Degenerate venue is rejected"), Projection.SetBounds(FBox(FVector::ZeroVector, FVector(100, 0, 100))));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewViewportClipTest, "FF_MapOverview.Minimap.ViewportClipping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewViewportClipTest::RunTest(const FString&)
{
    FFFMapOverviewProjection Projection;
    Projection.SetBounds(FBox(FVector(-1000, -1000, 0), FVector(1000, 1000, 0)));
    FVector2D Start(-10, 0.5), End(10, 0.5);
    TestTrue(TEXT("Huge camera footprint clips to venue"), Projection.ClipMapLine(Start, End));
    TestTrue(TEXT("Start stays inside circular map"), (Start - FVector2D(0.5, 0.5)).Size() < 0.43);
    TestTrue(TEXT("End stays inside circular map"), (End - FVector2D(0.5, 0.5)).Size() < 0.43);
    TestTrue(TEXT("Crossing retains straight horizontal edge"), FMath::IsNearlyEqual(Start.Y, 0.5) && FMath::IsNearlyEqual(End.Y, 0.5));
    Start = FVector2D(-10, -10); End = FVector2D(10, -10);
    TestFalse(TEXT("Entirely outside camera edge is hidden"), Projection.ClipMapLine(Start, End));
    Start = FVector2D(0.5, 0.5); End = Start;
    TestTrue(TEXT("Zero length inside edge is safe"), Projection.ClipMapLine(Start, End));
    TArray<FVector2D> Clipped;
    Projection.ClipWorldPolygon({FVector2D(-2000, -500), FVector2D(2000, -500),
        FVector2D(2000, 500), FVector2D(-2000, 500)}, Clipped);
    TestEqual(TEXT("Edited venue clips a rectangular facility without losing shape"), Clipped.Num(), 4);
    for (const FVector2D& Point : Clipped)
        TestTrue(TEXT("Facility footprint remains inside map"), Point.X >= -1000 && Point.X <= 1000 && Point.Y >= -1000 && Point.Y <= 1000);
    Projection.ClipWorldPolygon({FVector2D(2000, 2000), FVector2D(3000, 2000),
        FVector2D(3000, 3000), FVector2D(2000, 3000)}, Clipped);
    TestTrue(TEXT("Out-of-venue facility cannot escape the circular map"), Clipped.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewLocalWindowTest, "FF_MapOverview.Minimap.Local100mProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewLocalWindowTest::RunTest(const FString&)
{
    FFFMapOverviewProjection Projection;
    const FVector Player(120345.0, -78560.0, 1234.0);
    if (!TestTrue(TEXT("Default local window is valid"), Projection.SetLocalBounds(Player))) return false;
    TestTrue(TEXT("Projection identifies a local window"), Projection.bLocalWindow);
    TestEqual(TEXT("Local window width is exactly 100 m"), Projection.Bounds.GetSize().X, 10000.0);
    TestEqual(TEXT("Local window height is exactly 100 m"), Projection.Bounds.GetSize().Y, 10000.0);
    TestTrue(TEXT("Local window stays centered on player location"), Projection.Bounds.GetCenter().Equals(Player, 0.0001));
    TestEqual(TEXT("Local map scale gives a 100 m circular diameter"), Projection.Scale, 0.9 / 10000.0, 1.e-12);
    TestEqual(TEXT("Player appears at center"), Projection.WorldToMap(Player), FVector2D(0.5, 0.5));
    TestTrue(TEXT("50 m north appears at top circle edge"),
        Projection.WorldToMap(Player + FVector(5000.0, 0.0, 0.0)).Equals(FVector2D(0.5, 0.05), 1.e-9));
    TestTrue(TEXT("50 m east appears at right circle edge"),
        Projection.WorldToMap(Player + FVector(0.0, 5000.0, 0.0)).Equals(FVector2D(0.95, 0.5), 1.e-9));
    TestTrue(TEXT("A marker just inside 50 m is visible"), Projection.IsWorldPointVisible(Player + FVector(4999.0, 0.0, 0.0)));
    TestFalse(TEXT("A marker just beyond 50 m is hidden"), Projection.IsWorldPointVisible(Player + FVector(5001.0, 0.0, 0.0)));
    TestFalse(TEXT("A marker 100 m away is hidden"), Projection.IsWorldPointVisible(Player + FVector(10000.0, 0.0, 0.0)));
    TestFalse(TEXT("Square corner outside circular minimap is hidden"), Projection.IsWorldPointVisible(Player + FVector(4900.0, 4900.0, 0.0)));
    TestTrue(TEXT("Marker altitude does not hide its valid horizontal position"), Projection.IsWorldPointVisible(Player + FVector(0.0, 0.0, 10000.0)));
    const FVector Nearby = Player + FVector(1000.0, -3000.0, 0.0);
    FVector RoundTrip;
    TestTrue(TEXT("Local map click maps back to world"), Projection.MapToWorld(Projection.WorldToMap(Nearby), RoundTrip));
    TestTrue(TEXT("Local navigation preserves world XY and player plane"), RoundTrip.Equals(Nearby, 0.0001));
    TestFalse(TEXT("An out-of-range marker is not a navigable map click"),
        Projection.MapToWorld(Projection.WorldToMap(Player + FVector(10000.0, 0.0, 0.0)), RoundTrip));

    FVector2D Start(0.5, -1.0), End(0.5, 2.0);
    TestTrue(TEXT("Large camera edge is clipped to local circle"), Projection.ClipMapLine(Start, End));
    TestTrue(TEXT("Clipped edge starts 50 m north"), Start.Equals(FVector2D(0.5, 0.05), 1.e-8));
    TestTrue(TEXT("Clipped edge ends 50 m south"), End.Equals(FVector2D(0.5, 0.95), 1.e-8));
    Start = FVector2D(0.0, 0.0); End = FVector2D(1.0, 1.0);
    TestTrue(TEXT("Diagonal camera edge clips to circle, not just bounding square"), Projection.ClipMapLine(Start, End));
    TestTrue(TEXT("Both diagonal endpoints lie on circular mask"),
        FMath::IsNearlyEqual((Start - FVector2D(0.5, 0.5)).Size(), 0.45, 1.e-8)
        && FMath::IsNearlyEqual((End - FVector2D(0.5, 0.5)).Size(), 0.45, 1.e-8));
    Start = FVector2D(0.05, 0.05); End = FVector2D(0.1, 0.05);
    TestFalse(TEXT("Camera edge in square corner outside circle is omitted"), Projection.ClipMapLine(Start, End));
    TArray<FVector2D> Clipped;
    Projection.ClipWorldPolygon({FVector2D(Player.X - 10000.0, Player.Y - 10000.0),
        FVector2D(Player.X + 10000.0, Player.Y - 10000.0), FVector2D(Player.X + 10000.0, Player.Y + 10000.0),
        FVector2D(Player.X - 10000.0, Player.Y + 10000.0)}, Clipped);
    TestTrue(TEXT("Large geometry keeps a smoothly clipped local region"), Clipped.Num() >= 16);
    for (const FVector2D& Point : Clipped)
        TestTrue(TEXT("Clipped geometry stays within 50 m player radius"),
            FVector2D::Distance(Point, FVector2D(Player.X, Player.Y)) <= 5000.001);
    Projection.ClipWorldPolygon({FVector2D(Player.X + 4500.0, Player.Y + 4500.0),
        FVector2D(Player.X + 4800.0, Player.Y + 4500.0), FVector2D(Player.X + 4800.0, Player.Y + 4800.0),
        FVector2D(Player.X + 4500.0, Player.Y + 4800.0)}, Clipped);
    TestTrue(TEXT("Facility footprint outside circular local range is hidden"), Clipped.IsEmpty());

    const FVector MovedPlayer = Player + FVector(12000.0, 0.0, 0.0);
    TestTrue(TEXT("Movement updates local bounds"), Projection.SetLocalBounds(MovedPlayer));
    TestEqual(TEXT("Moved player remains centered"), Projection.WorldToMap(MovedPlayer), FVector2D(0.5, 0.5));
    TestTrue(TEXT("New window follows actual movement"), Projection.Bounds.GetCenter().Equals(MovedPlayer, 0.0001));
    TestEqual(TEXT("Movement retains fixed 100 m width"), Projection.Bounds.GetSize().X, 10000.0);
    TestFalse(TEXT("Previously visible old-player location leaves local window"), Projection.IsWorldPointVisible(Player));
    TestTrue(TEXT("Newly nearby facility becomes visible"), Projection.IsWorldPointVisible(MovedPlayer + FVector(1000.0, 0.0, 0.0)));
    TestFalse(TEXT("Zero-size local view is rejected"), Projection.SetLocalBounds(Player, 0.0));
    TestFalse(TEXT("Invalid local view has no visible markers"), Projection.IsWorldPointVisible(Player));
    TestTrue(TEXT("Whole-map mathematical projection remains available"),
        Projection.SetBounds(FBox(Player - FVector(50000.0, 50000.0, 0.0), Player + FVector(50000.0, 50000.0, 0.0))));
    TestFalse(TEXT("Whole-map projection clears local-only mode"), Projection.bLocalWindow);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewLocalTerrainUVTest, "FF_MapOverview.Minimap.LocalTerrainUV",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewLocalTerrainUVTest::RunTest(const FString&)
{
    // A square, north-up scene capture can be reused while the 100 m viewport moves over it.
    const FBox Terrain(FVector(-30000.0, -90000.0, 0.0), FVector(70000.0, 10000.0, 100.0));
    const FVector Player(26000.0, -37000.0, 400.0);
    FFFMapOverviewProjection Projection;
    Projection.SetLocalBounds(Player);
    FVector2D UVCenter;
    double UVScale = 0.0;
    if (!TestTrue(TEXT("Local window obtains reusable terrain UV transform"),
        Projection.GetTerrainUVTransform(Terrain, UVCenter, UVScale))) return false;
    TestTrue(TEXT("Terrain UV center uses east-positive U and north-negative V"), UVCenter.Equals(FVector2D(0.53, 0.44), 1.e-9));
    TestEqual(TEXT("100 m window maps to the correct fraction of capture texture"), UVScale, 1.0 / 9.0, 1.e-12);
    for (const FVector Offset : {FVector::ZeroVector, FVector(3000.0, -1200.0, 0.0), FVector(-4000.0, 2500.0, 800.0)})
    {
        const FVector WorldPoint = Player + Offset;
        const FVector2D TextureFromMap = (Projection.WorldToMap(WorldPoint) - FVector2D(0.5, 0.5)) * UVScale + UVCenter;
        const FVector CaptureCenter = Terrain.GetCenter();
        const FVector2D DirectWorldUV(0.5 + (WorldPoint.Y - CaptureCenter.Y) / Terrain.GetSize().Y,
            0.5 - (WorldPoint.X - CaptureCenter.X) / Terrain.GetSize().X);
        TestTrue(TEXT("Moving-window texture sample matches independent world-to-capture projection"), TextureFromMap.Equals(DirectWorldUV, 1.e-9));
    }
    const double OriginalScale = UVScale;
    Projection.SetLocalBounds(Player + FVector(1000.0, 2000.0, 500.0));
    TestTrue(TEXT("Player movement updates terrain UV center without new capture bounds"), Projection.GetTerrainUVTransform(Terrain, UVCenter, UVScale));
    TestTrue(TEXT("10 m north and 20 m east shift terrain sample correctly"), UVCenter.Equals(FVector2D(0.55, 0.43), 1.e-9));
    TestEqual(TEXT("Movement keeps the same 100 m texture scale"), UVScale, OriginalScale, 1.e-12);
    TestFalse(TEXT("Missing terrain capture bounds are rejected"), Projection.GetTerrainUVTransform(FBox(ForceInit), UVCenter, UVScale));
    TestFalse(TEXT("Degenerate terrain capture bounds are rejected"), Projection.GetTerrainUVTransform(FBox(FVector::ZeroVector, FVector(0.0, 100.0, 0.0)), UVCenter, UVScale));
    Projection.SetLocalBounds(Player, 0.0);
    TestFalse(TEXT("Invalid local window cannot supply a texture transform"), Projection.GetTerrainUVTransform(Terrain, UVCenter, UVScale));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewBlueprintLayoutTest, "FF_MapOverview.Minimap.BlueprintHudLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewBlueprintLayoutTest::RunTest(const FString&)
{
    TStrongObjectPtr<UFFMapOverviewWidget> Instance(CreateAuthoredMinimap(*this));
    UFFMapOverviewWidget* Widget = Instance.Get();
    if (!Widget) return false;
    // Keep the SObjectWidget alive just as the viewport does. Dropping this SharedRef
    // immediately runs NativeDestruct and legitimately unbinds the runtime callbacks.
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    TestEqual(TEXT("HUD root allows world input outside its children"), Widget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
    const UCanvasPanel* Root = Widget->WidgetTree ? Cast<UCanvasPanel>(Widget->WidgetTree->RootWidget) : nullptr;
    if (!TestNotNull(TEXT("Layout comes from the WBP Designer canvas"), Root)) return false;
    const UWidget* Panel = Widget->GetWidgetFromName(TEXT("MapPanel"));
    if (!TestNotNull(TEXT("Designer map panel exists"), Panel)) return false;
    const UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Panel->Slot);
    TestNotNull(TEXT("Authored map panel has a canvas slot"), Slot);
    if (Slot)
    {
        TestEqual(TEXT("Map anchors at upper right"), Slot->GetAnchors().Minimum, FVector2D(1, 0));
        TestEqual(TEXT("Map maximum anchor is upper right"), Slot->GetAnchors().Maximum, FVector2D(1, 0));
        TestEqual(TEXT("Map aligns to its upper right corner"), Slot->GetAlignment(), FVector2D(1, 0));
        TestTrue(TEXT("Map is inset from the right edge"), Slot->GetPosition().X < 0);
    }
    UFFMapOverviewCanvas* Canvas = Cast<UFFMapOverviewCanvas>(Widget->GetWidgetFromName(TEXT("MapSurface")));
    TestNotNull(TEXT("Authored map surface exists"), Canvas);
    if (Canvas) TestTrue(TEXT("Raw build click guard sees map as interactive"), Canvas->TakeWidget()->IsInteractable());
    TestNotNull(TEXT("WBP supplies editable marker canvas"), Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("MarkerLayer"))));
    TestNotNull(TEXT("WBP supplies editable terrain image"), Cast<UImage>(Widget->GetWidgetFromName(TEXT("TerrainImage"))));
    TestNotNull(TEXT("Authored compass label exists"), Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("North"))));
    for (const FName Name : {FName(TEXT("NearbyButton")), FName(TEXT("OverviewButton")),
        FName(TEXT("NearbyLabel")), FName(TEXT("OverviewLabel")), FName(TEXT("WholeButton")),
        FName(TEXT("WholeLabel")), FName(TEXT("GridButton")), FName(TEXT("GridLabel")), FName(TEXT("BuildControls")),
        FName(TEXT("ScaleLabel"))})
        TestNull(FString::Printf(TEXT("Removed minimap action %s is absent from authored WBP"), *Name.ToString()), Widget->GetWidgetFromName(Name));
    UClass* MarkerClass = LoadClass<UFFMapOverviewMarkerWidget>(nullptr,
        TEXT("/FF_MapOverview/UI/WBP_FF_MapMarker.WBP_FF_MapMarker_C"));
    if (!TestNotNull(TEXT("Actual editable marker WBP generated class loads"), MarkerClass)) return false;
    TestTrue(TEXT("Facility marker class comes from the authored WBP"), Widget->FacilityMarkerClass.Get() == MarkerClass);
    TestTrue(TEXT("Camera marker class comes from the authored WBP"), Widget->CameraMarkerClass.Get() == MarkerClass);
    TStrongObjectPtr<UFFMapOverviewMarkerWidget> Marker(
        NewObject<UFFMapOverviewMarkerWidget>(GetTransientPackage(), MarkerClass));
    if (!TestTrue(TEXT("Marker WBP initializes"), Marker->Initialize())) return false;
    const TSharedRef<SWidget> MarkerSlate = Marker->TakeWidget();
    UImage* Background = Cast<UImage>(Marker->GetWidgetFromName(TEXT("Background")));
    UImage* Icon = Cast<UImage>(Marker->GetWidgetFromName(TEXT("Icon")));
    if (!TestNotNull(TEXT("Designer marker background"), Background)
        || !TestNotNull(TEXT("Designer marker glyph"), Icon)) return false;
    UObject* BackgroundResource = Background->GetBrush().GetResourceObject();
    Marker->FacilityColor = FLinearColor(0.8f, 0.1f, 0.6f, 1.0f);
    Marker->SetMarkerData(EFFMapOverviewMarkerKind::Facility, FText::FromString(TEXT("Fixture facility")));
    TestEqual(TEXT("Marker uses Designer facility palette"), Background->GetColorAndOpacity(), Marker->FacilityColor);
    Marker->CameraColor = FLinearColor(0.1f, 0.8f, 0.9f, 1.0f);
    Marker->SetMarkerData(EFFMapOverviewMarkerKind::Camera, FText::FromString(TEXT("Fixture camera")));
    TestEqual(TEXT("Camera marker uses Designer camera palette"), Background->GetColorAndOpacity(), Marker->CameraColor);
    TestTrue(TEXT("Camera marker uses Designer-authored glyph"), Icon->GetBrush().GetResourceObject() == Marker->CameraBrush.GetResourceObject());
    TestTrue(TEXT("Updating marker data preserves Designer background brush"), Background->GetBrush().GetResourceObject() == BackgroundResource);
    Marker->ReleaseSlateResources(true);
    Widget->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewDesignerTreePreservedTest, "FF_MapOverview.Minimap.DesignerTreePreserved",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewDesignerTreePreservedTest::RunTest(const FString&)
{
    TStrongObjectPtr<UFFMapOverviewWidget> Instance(CreateAuthoredMinimap(*this));
    UFFMapOverviewWidget* Widget = Instance.Get();
    if (!Widget) return false;
    UWidgetTree* OriginalTree = Widget->WidgetTree;
    UCanvasPanel* Root = Cast<UCanvasPanel>(Widget->GetRootWidget());
    UWidget* Panel = Widget->GetWidgetFromName(TEXT("MapPanel"));
    UCanvasPanelSlot* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
    UFFMapOverviewCanvas* Surface = Cast<UFFMapOverviewCanvas>(Widget->GetWidgetFromName(TEXT("MapSurface")));
    UTextBlock* CompassLabel = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("North")));
    if (!TestNotNull(TEXT("Authored Designer root"), Root) || !TestNotNull(TEXT("Authored panel slot"), PanelSlot)
        || !TestNotNull(TEXT("Authored surface"), Surface) || !TestNotNull(TEXT("Authored compass label"), CompassLabel)) return false;
    const FSlateFontInfo AuthoredFont = CompassLabel->GetFont();
    TestNotNull(TEXT("Compass label has a real authored UI font resource"), AuthoredFont.FontObject.Get());
    TestTrue(TEXT("Authored UI font has a nonzero size"), AuthoredFont.Size > 0.0f);
    const FName OriginalSlotName = PanelSlot->GetFName();
    TSharedPtr<SWidget> Slate = Widget->TakeWidget();
    TestTrue(TEXT("Native construction preserves authored font metadata"), CompassLabel->GetFont() == AuthoredFont);

    // Edit only the instance's ordinary Designer properties and add an extra Designer child.
    // A native runtime tree generator would replace or duplicate these during a Slate rebuild.
    const float AuthoredRightInset = Widget->GetMapPanelRightInsetSlateUnits();
    const FVector2D DesignerPosition = PanelSlot->GetPosition() + FVector2D(-43.0, 77.0);
    PanelSlot->SetPosition(DesignerPosition);
    TestTrue(TEXT("Designer panel placement changes camera-safe inset without a hardcoded width"),
        Widget->GetMapPanelRightInsetSlateUnits() > AuthoredRightInset + 40.0f);
    UTextBlock* Sentinel = OriginalTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DesignerSentinel"));
    Sentinel->SetText(FText::FromString(TEXT("designer-owned child")));
    Root->AddChildToCanvas(Sentinel)->SetPosition(FVector2D(20.0, 20.0));
    const int32 ChildCount = Root->GetChildrenCount();
    const FLinearColor DesignerTint(0.74f, 0.12f, 0.51f, 0.75f);
    Surface->ZoneFillColor = DesignerTint;
    FSlateFontInfo DesignerFont = AuthoredFont;
    DesignerFont.Size += 3.0f;
    CompassLabel->SetFont(DesignerFont);
    Slate.Reset();
    Widget->ReleaseSlateResources(true);
    Slate = Widget->TakeWidget();
    Widget->RefreshMap();
    TestTrue(TEXT("Runtime preserves the Blueprint-owned widget tree"), Widget->WidgetTree == OriginalTree);
    TestTrue(TEXT("Runtime preserves the exact Designer root"), Widget->GetRootWidget() == Root);
    TestTrue(TEXT("Runtime preserves the exact authored surface"), Widget->GetWidgetFromName(TEXT("MapSurface")) == Surface);
    TestTrue(TEXT("Runtime preserves an extra Designer child"), Widget->GetWidgetFromName(TEXT("DesignerSentinel")) == Sentinel);
    TestEqual(TEXT("Runtime does not duplicate Designer children"), Root->GetChildrenCount(), ChildCount);
    TestEqual(TEXT("Designer slot edits survive Slate rebuilding and refresh"), PanelSlot->GetPosition(), DesignerPosition);
    TestTrue(TEXT("Runtime preserves the exact Designer panel and its slot"), Widget->GetWidgetFromName(TEXT("MapPanel")) == Panel
        && Panel->Slot == PanelSlot);
    TestEqual(TEXT("Runtime preserves the Designer slot object name"), PanelSlot->GetFName(), OriginalSlotName);
    TestEqual(TEXT("Runtime preserves editable map surface color"), Surface->ZoneFillColor, DesignerTint);
    TestTrue(TEXT("Runtime preserves edited UI font resource, typeface, size and other metadata"), CompassLabel->GetFont() == DesignerFont);
    Surface->SurfaceSize = FVector2D(288.0, 288.0);
    Surface->SynchronizeProperties();
    Surface->TakeWidget()->SlatePrepass(1.0f);
    const FVector2D DesiredSize = Surface->TakeWidget()->GetDesiredSize();
    TestTrue(TEXT("Editable surface size controls its Slate desired size"), DesiredSize.Equals(Surface->SurfaceSize, 0.01));
    Widget->ReleaseSlateResources(true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapOverviewRenderedHudTest, "FF_MapOverview.Minimap.RenderedHud",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapOverviewRenderedHudTest::RunTest(const FString&)
{
    if (!FApp::CanEverRender())
    {
        AddInfo(TEXT("Rendering is unavailable on this automation host; numerical/layout tests remain active."));
        return true;
    }
    TStrongObjectPtr<UFFMapOverviewWidget> Instance(CreateAuthoredMinimap(*this));
    UFFMapOverviewWidget* Widget = Instance.Get();
    if (!Widget) return false;
    const TSharedRef<SWidget> Slate = Widget->TakeWidget();
    UFFMapOverviewCanvas* Canvas = Cast<UFFMapOverviewCanvas>(Widget->GetWidgetFromName(TEXT("MapSurface")));
    if (!TestNotNull(TEXT("Actual WBP map surface is available for rendered verification"), Canvas)) return false;
    Canvas->bDrawVectorLandFallback = true;
    Canvas->bDrawGroundFootprints = true;
    Canvas->bDrawViewFootprint = true;
    Canvas->ZoneFillColor = FLinearColor::White;
    Canvas->RoadColor = FLinearColor::White;
    Canvas->SynchronizeProperties();
    TSharedPtr<FFFMapOverviewSnapshot> Map = MakeShared<FFFMapOverviewSnapshot>();
    Map->Projection.SetBounds(FBox(FVector(-5000, -4500, 0), FVector(5000, 4500, 0)));
    auto AddQuad = [&](const FVector2D Min, const FVector2D Max, const FLinearColor Color, const uint8 GeometryLayer = 0)
    {
        const uint32 Offset = Map->Vertices.Num();
        for (const FVector2D Point : {Min, FVector2D(Max.X, Min.Y), Max, FVector2D(Min.X, Max.Y)})
            Map->Vertices.Add({Map->Projection.WorldToMap(FVector(Point.X, Point.Y, 0)), Color, GeometryLayer});
        for (const uint32 Index : {0u, 1u, 2u, 0u, 2u, 3u}) Map->Indices.Add(Offset + Index);
    };
    AddQuad(FVector2D(-5000, -4500), FVector2D(5000, 4500), FLinearColor(0.13f, 0.31f, 0.26f));
    AddQuad(FVector2D(-5000, -250), FVector2D(5000, 250), FLinearColor(0.79f, 0.74f, 0.58f), 1);
    AddQuad(FVector2D(-200, -4500), FVector2D(200, 4500), FLinearColor(0.79f, 0.74f, 0.58f), 1);
    UCanvasPanel* MarkerLayer = Cast<UCanvasPanel>(Widget->GetWidgetFromName(TEXT("MarkerLayer")));
    UClass* MarkerClass = Widget->FacilityMarkerClass.Get();
    if (!TestNotNull(TEXT("Authored marker layer for rendering"), MarkerLayer)
        || !TestNotNull(TEXT("Authored marker WBP class for rendering"), MarkerClass)) return false;
    for (const double X : {-3000.0, -1000.0, 1000.0, 3000.0})
        for (const double Y : {-2500.0, 2000.0})
        {
            UFFMapOverviewMarkerWidget* Marker = NewObject<UFFMapOverviewMarkerWidget>(Widget, MarkerClass);
            if (!TestTrue(TEXT("Rendered marker WBP initializes"), Marker->Initialize())) return false;
            Marker->SetMarkerData(EFFMapOverviewMarkerKind::Facility, FText::FromString(TEXT("Fixture facility")));
            const FVector2D Point = Map->Projection.WorldToMap(FVector(X, Y, 0.0));
            UCanvasPanelSlot* MarkerSlot = MarkerLayer->AddChildToCanvas(Marker);
            MarkerSlot->SetAnchors(FAnchors(Point.X, Point.Y));
            MarkerSlot->SetAlignment(FVector2D(0.5, 0.5));
            MarkerSlot->SetSize(Marker->MarkerSize);
            ++Map->FacilityCount;
        }
    Map->bHasCamera = true;
    Map->CameraFocus = FVector2D(0.55, 0.52);
    const TArray<FVector2D> Frame = {FVector2D(0.47, 0.44), FVector2D(0.65, 0.43),
        FVector2D(0.64, 0.60), FVector2D(0.48, 0.59)};
    for (int32 Index = 0; Index < Frame.Num(); ++Index)
        Map->CameraFootprint.Add({Frame[Index], Frame[(Index + 1) % Frame.Num()]});
    Canvas->SetSnapshot(Map);
    const FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MapOverview"), TEXT("WidgetVerification"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    FWidgetRenderer Renderer(true);
    for (const FIntPoint Resolution : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
    {
        FImage Image;
        if (!ReadWidgetPixels(*this, Renderer, Slate, Resolution, Image)) return false;
        const FString Path = FPaths::Combine(OutputDirectory, FString::Printf(TEXT("Minimap_%dx%d.png"), Resolution.X, Resolution.Y));
        TestTrue(TEXT("Rendered layout saved for visual inspection"), FImageUtils::SaveImageByExtension(*Path, Image));
        AddInfo(FString::Printf(TEXT("Rendered authored WBP at %dx%d: %s"), Resolution.X, Resolution.Y, *Path));
    }
    FImage Baseline;
    if (!ReadWidgetPixels(*this, Renderer, Slate, FIntPoint(1280, 720), Baseline)) return false;
    Canvas->ZoneFillColor = FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
    Canvas->SynchronizeProperties();
    Canvas->RefreshPaint();
    FImage DesignerColor;
    if (!ReadWidgetPixels(*this, Renderer, Slate, FIntPoint(1280, 720), DesignerColor)) return false;
    TestTrue(TEXT("Designer surface palette edits change actual rendered pixels"), CountChangedBytes(Baseline, DesignerColor) > 1000);
    const FString ColorPath = FPaths::Combine(OutputDirectory, TEXT("Minimap_DesignerPalette.png"));
    TestTrue(TEXT("Designer palette render saved for inspection"), FImageUtils::SaveImageByExtension(*ColorPath, DesignerColor));
    UWidget* Panel = Widget->GetWidgetFromName(TEXT("MapPanel"));
    UCanvasPanelSlot* PanelSlot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
    if (!TestNotNull(TEXT("Designer panel slot is available for rendered layout editing"), PanelSlot)) return false;
    PanelSlot->SetPosition(PanelSlot->GetPosition() + FVector2D(-150.0, 80.0));
    FImage DesignerLayout;
    if (!ReadWidgetPixels(*this, Renderer, Slate, FIntPoint(1280, 720), DesignerLayout)) return false;
    TestTrue(TEXT("Designer panel placement edits change actual rendered layout"), CountChangedBytes(DesignerColor, DesignerLayout) > 1000);
    const FString LayoutPath = FPaths::Combine(OutputDirectory, TEXT("Minimap_DesignerLayout.png"));
    TestTrue(TEXT("Designer layout render saved for inspection"), FImageUtils::SaveImageByExtension(*LayoutPath, DesignerLayout));
    FlushRenderingCommands();
    Widget->ReleaseSlateResources(true);
    return true;
}

#endif
