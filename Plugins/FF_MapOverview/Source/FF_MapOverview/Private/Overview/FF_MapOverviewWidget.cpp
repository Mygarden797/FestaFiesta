#include "FF_MapOverviewWidget.h"
#include "FF_MapOverviewMarkerWidget.h"
#include "FF_MapOverviewSubsystem.h"

#include "FF_FFZSBuildArea.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SplineComponent.h"
#include "CompGeom/PolygonTriangulation.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

#define LOCTEXT_NAMESPACE "FFMapOverview"

class SFFMapOverviewCanvas final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFFMapOverviewCanvas) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}
    void SetSnapshot(TSharedPtr<FFFMapOverviewSnapshot> InSnapshot) { Snapshot = MoveTemp(InSnapshot); }
    void SetOwner(UFFMapOverviewWidget* InOwner) { Owner = InOwner; }
    void SetAppearance(UFFMapOverviewCanvas* InAppearance) { Appearance = InAppearance; }
    virtual FVector2D ComputeDesiredSize(float) const override
    {
        return Appearance.IsValid() ? Appearance->SurfaceSize : FVector2D::ZeroVector;
    }
    virtual bool IsInteractable() const override { return true; }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        if (UFFMapOverviewWidget* Widget = Owner.Get())
        {
            Widget->NotifyInteraction();
            if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
            {
                const FVector2D Size = Geometry.GetLocalSize();
                const FVector2D Point = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition());
                if (Size.X > 1.f && Size.Y > 1.f) Widget->HandleMapPress(Point / Size);
            }
        }
        return FReply::Handled().CaptureMouse(SharedThis(this));
    }
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override
    {
        return FReply::Handled().ReleaseMouseCapture();
    }
    virtual FReply OnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override
    {
        return OnMouseButtonDown(Geometry, Event);
    }
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& WidgetStyle, bool) const override
    {
        const UFFMapOverviewCanvas* Style = Appearance.Get();
        if (!Style || !Snapshot.IsValid() || !Snapshot->Projection.IsValid()) return Layer;
        const FVector2D Size = Geometry.GetLocalSize();
        const FLinearColor Tint = WidgetStyle.GetColorAndOpacityTint();
        // Slate only renders cached world geometry. Background, circular mask/rim,
        // typography, compass and all facility/camera glyphs belong to the WBP.
        if (Style->bDrawVectorLandFallback || Style->bDrawGroundFootprints)
        {
            TArray<FSlateVertex> Vertices;
            Vertices.Reserve(Snapshot->Vertices.Num());
            for (const FFFMapOverviewVertex& Point : Snapshot->Vertices)
            {
                const FLinearColor Color = Point.GeometryLayer == 1 ? Style->RoadColor :
                    Point.GeometryLayer == 2 ? Style->TileColor : Style->ZoneFillColor;
                Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),
                    FVector2f(Point.Point * Size), FVector2f(0.5f, 0.5f), (Point.Color * Color * Tint).ToFColor(true)));
            }
            TArray<SlateIndex> Indices;
            Indices.Reserve(Snapshot->Indices.Num());
            for (int32 Index = 0; Index + 2 < Snapshot->Indices.Num(); Index += 3)
            {
                const uint32 First = Snapshot->Indices[Index];
                if (!Snapshot->Vertices.IsValidIndex(First)) continue;
                const bool bLand = Snapshot->Vertices[First].GeometryLayer == 0;
                if (bLand ? !Style->bDrawVectorLandFallback : !Style->bDrawGroundFootprints) continue;
                for (int32 Corner = 0; Corner < 3; ++Corner)
                    Indices.Add(static_cast<SlateIndex>(Snapshot->Indices[Index + Corner]));
            }
            if (!Vertices.IsEmpty() && !Indices.IsEmpty())
            {
                const FSlateResourceHandle White = FSlateApplication::Get().GetRenderer()->GetResourceHandle(
                    *FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")));
                FSlateDrawElement::MakeCustomVerts(Elements, Layer, White, Vertices, Indices, nullptr, 0, 0);
            }
        }
        auto Line = [&](const FFFMapOverviewLine& Segment, const FLinearColor Color, const float Thickness, const int32 AtLayer)
        {
            const TArray<FVector2D> Points = {Segment.Start * Size, Segment.End * Size};
            FSlateDrawElement::MakeLines(Elements, AtLayer, Geometry.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, Color * Tint, true, Thickness);
        };
        if (Style->bDrawBoundary)
            for (const FFFMapOverviewLine& Segment : Snapshot->Boundary)
                Line(Segment, Style->BoundaryColor, Style->BoundaryThickness, Layer + 1);
        if (Style->bDrawViewFootprint && Snapshot->bHasCamera)
            for (const FFFMapOverviewLine& Segment : Snapshot->CameraFootprint)
                Line(Segment, Style->ViewFootprintColor, Style->ViewFootprintThickness, Layer + 2);
        return Layer + 2;
    }
private:
    TSharedPtr<FFFMapOverviewSnapshot> Snapshot;
    TWeakObjectPtr<UFFMapOverviewWidget> Owner;
    TWeakObjectPtr<UFFMapOverviewCanvas> Appearance;
};

void UFFMapOverviewCanvas::SetSnapshot(const TSharedPtr<FFFMapOverviewSnapshot>& InSnapshot)
{
    Snapshot = InSnapshot;
    if (MapSlate.IsValid()) MapSlate->SetSnapshot(Snapshot);
}
void UFFMapOverviewCanvas::SetOwner(UFFMapOverviewWidget* InOwner)
{
    Owner = InOwner;
    if (MapSlate.IsValid()) MapSlate->SetOwner(InOwner);
}
void UFFMapOverviewCanvas::RefreshPaint()
{
    if (MapSlate.IsValid()) MapSlate->Invalidate(EInvalidateWidgetReason::Paint);
}
void UFFMapOverviewCanvas::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (MapSlate.IsValid())
    {
        MapSlate->SetAppearance(this);
        MapSlate->Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }
}
TSharedRef<SWidget> UFFMapOverviewCanvas::RebuildWidget()
{
    SAssignNew(MapSlate, SFFMapOverviewCanvas);
    MapSlate->SetOwner(Owner.Get());
    MapSlate->SetSnapshot(Snapshot);
    MapSlate->SetAppearance(this);
    return MapSlate.ToSharedRef();
}
void UFFMapOverviewCanvas::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    MapSlate.Reset();
}

void UFFMapOverviewWidget::InitializeOverview(UFFMapOverviewSubsystem* InSubsystem)
{
    Subsystem = InSubsystem;
    CachedWorldRevision = MIN_int64;
    CachedRegistryRevision = MAX_uint64;
    CachedUiState = INDEX_NONE;
    if (!Snapshot.IsValid()) Snapshot = MakeShared<FFFMapOverviewSnapshot>();
    if (MapSurface) { MapSurface->SetOwner(this); MapSurface->SetSnapshot(Snapshot); RefreshMap(); }
}

FBox UFFMapOverviewWidget::GetLocalMapBounds() const
{
    return Snapshot.IsValid() ? Snapshot->Projection.Bounds : FBox(ForceInit);
}
FVector UFFMapOverviewWidget::GetLocalMapCenter() const
{
    return Snapshot.IsValid() ? Snapshot->Projection.Center : FVector::ZeroVector;
}

void UFFMapOverviewWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (!Subsystem.IsValid() && GetWorld()) Subsystem = GetWorld()->GetSubsystem<UFFMapOverviewSubsystem>();
    if (!Snapshot.IsValid()) Snapshot = MakeShared<FFFMapOverviewSnapshot>();
    if (MapSurface) { MapSurface->SetOwner(this); MapSurface->SetSnapshot(Snapshot); }
    RefreshMap();
}

void UFFMapOverviewWidget::NativeDestruct()
{
    if (MapSurface) MapSurface->SetOwner(nullptr);
    Super::NativeDestruct();
}

uint32 UFFMapOverviewWidget::ComputeBoundaryFingerprint() const
{
    const UFFZSWorldSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!Zones) return 0;
    uint32 Hash = 0;
    for (const FName Id : Zones->GetRegisteredZoneIds())
    {
        const AFFZSGameplayZone* Zone = Zones->FindZone(Id);
        if (!Zone || !Zone->BoundarySpline) continue;
        Hash = HashCombineFast(Hash, GetTypeHash(Id));
        const int32 Count = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Hash = HashCombineFast(Hash, GetTypeHash(Count));
        for (int32 Index = 0; Index < Count; ++Index)
            Hash = HashCombineFast(Hash, GetTypeHash(Zone->BoundarySpline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::World)));
    }
    return Hash;
}

void UFFMapOverviewWidget::RebuildStaticMap()
{
    UFFMapOverviewSubsystem* Overview = Subsystem.Get();
    UFFZSWorldSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!Overview || !Zones) return;
    WorldPolygons.Reset();
    WorldBoundary.Reset();
    WorldFacilityMarkers.Reset();
    auto CachePolygon = [&](TArray<FVector2D> Points, const uint8 GeometryLayer)
    {
        if (Points.Num() < 3 || Points.Num() > 8192) return;
        FFFMapOverviewWorldPolygon Polygon;
        Polygon.Points = MoveTemp(Points);
        Polygon.GeometryLayer = GeometryLayer;
        for (const FVector2D& Point : Polygon.Points) Polygon.Bounds += Point;
        WorldPolygons.Add(MoveTemp(Polygon));
    };
    const AFFZSBuildArea* Area = Zones->FindBuildArea();
    for (const FName Id : Zones->GetRegisteredZoneIds())
    {
        const AFFZSGameplayZone* Zone = Zones->FindZone(Id);
        if (!Zone) continue;
        TArray<FVector2D> Points;
        for (const FVector2D& Point : Zone->GetBoundaryPolygonLocalCm())
        {
            const FVector WorldPoint = Zone->GetActorTransform().TransformPosition(FVector(Point.X, Point.Y, 0));
            Points.Add(FVector2D(WorldPoint.X, WorldPoint.Y));
        }
        if (!Area)
            for (int32 Index = 0; Index < Points.Num(); ++Index)
                WorldBoundary.Add({Points[Index], Points[(Index + 1) % Points.Num()]});
        CachePolygon(MoveTemp(Points), 0);
    }
    if (Area)
    {
        const FBox Bounds = Overview->GetMapBounds();
        if (Bounds.IsValid)
        {
            TArray<FFFZSBuildAreaLineSegment> Segments;
            Area->GetBoundarySegments(Bounds.GetCenter().Z, FBox2D(FVector2D(Bounds.Min), FVector2D(Bounds.Max)), Segments);
            for (const FFFZSBuildAreaLineSegment& Segment : Segments) WorldBoundary.Add({Segment.Start, Segment.End});
        }
    }
    // Authority copies and actor/spec lookups occur only after registry/boundary changes.
    // Moving the local minimap reprojects these value caches without rebuilding the read model.
    const FFFZSReadModel ReadModel = Zones->BuildReadModel();
    for (const FFFZSFacilityReadModel& Row : ReadModel.Facilities)
    {
        const AFFZSFacilityActor* Facility = Zones->FindFacility(Row.FacilityInstanceId);
        if (!Facility || Facility->GetFacilityState() == EFFZSFacilityState::Removed) continue;
        const FFFZSFacilitySpec& Spec = Facility->GetFacilitySpec();
        if (!Spec.IsGroundSurface())
        {
            EFFMapOverviewMarkerKind Kind = EFFMapOverviewMarkerKind::Facility;
            switch (Spec.Category)
            {
            case EFFZSFacilityCategory::Performance:
            case EFFZSFacilityCategory::PhotoSpot: Kind = EFFMapOverviewMarkerKind::Attraction; break;
            case EFFZSFacilityCategory::Toilet:
            case EFFZSFacilityCategory::Rest:
            case EFFZSFacilityCategory::Safety: Kind = EFFMapOverviewMarkerKind::Service; break;
            default: break;
            }
            const FVector WorldPoint = Facility->GetActorLocation();
            WorldFacilityMarkers.Add({Row.FacilityInstanceId, FVector2D(WorldPoint.X, WorldPoint.Y),
                Row.DisplayName, static_cast<uint8>(Kind)});
            continue;
        }
        const FVector Half(Spec.FootprintSizeCells.X * 0.5, Spec.FootprintSizeCells.Y * 0.5, 0);
        TArray<FVector2D> Points;
        for (const FVector& Local : {FVector(-Half.X, -Half.Y, 0), FVector(Half.X, -Half.Y, 0),
            FVector(Half.X, Half.Y, 0), FVector(-Half.X, Half.Y, 0)})
        {
            const FVector Point = Facility->GetActorTransform().TransformPosition(Local);
            Points.Add(FVector2D(Point.X, Point.Y));
        }
        CachePolygon(MoveTemp(Points), Spec.Category == EFFZSFacilityCategory::Road ? 1 : 2);
    }
}

void UFFMapOverviewWidget::ReprojectLocalMap(const FVector& UserLocation)
{
    if (!Snapshot.IsValid() || !Snapshot->Projection.SetLocalBounds(UserLocation, LocalMapDiameterCm)) return;
    Snapshot->Vertices.Reset();
    Snapshot->Indices.Reset();
    Snapshot->Boundary.Reset();
    Snapshot->FacilityMarkers.Reset();
    Snapshot->FacilityCount = 0;
    const FBox& LocalBounds = Snapshot->Projection.Bounds;
    const FBox2D LocalXY(FVector2D(LocalBounds.Min), FVector2D(LocalBounds.Max));
    for (const FFFMapOverviewWorldPolygon& Polygon : WorldPolygons)
    {
        if (!MapSurface || (Polygon.GeometryLayer == 0 ? !MapSurface->bDrawVectorLandFallback : !MapSurface->bDrawGroundFootprints)) continue;
        if (!Polygon.Bounds.Intersect(LocalXY)) continue;
        TArray<FVector2D> Points;
        Snapshot->Projection.ClipWorldPolygon(Polygon.Points, Points);
        if (Points.Num() < 3) continue;
        if (static_cast<uint64>(Snapshot->Vertices.Num()) + Points.Num() > TNumericLimits<SlateIndex>::Max()) continue;
        TArray<UE::Geometry::FIndex3i> Triangles;
        PolygonTriangulation::TriangulateSimplePolygon(Points, Triangles, false);
        const uint32 Offset = Snapshot->Vertices.Num();
        for (const FVector2D& Point : Points)
            Snapshot->Vertices.Add({Snapshot->Projection.WorldToMap(FVector(Point.X, Point.Y, UserLocation.Z)),
                FLinearColor::White, Polygon.GeometryLayer});
        for (const UE::Geometry::FIndex3i& Triangle : Triangles)
        {
            Snapshot->Indices.Add(Offset + Triangle.A);
            Snapshot->Indices.Add(Offset + Triangle.B);
            Snapshot->Indices.Add(Offset + Triangle.C);
        }
    }
    if (MapSurface && MapSurface->bDrawBoundary)
    for (const FFFMapOverviewLine& Line : WorldBoundary)
    {
        FVector2D Start = Snapshot->Projection.WorldToMap(FVector(Line.Start.X, Line.Start.Y, UserLocation.Z));
        FVector2D End = Snapshot->Projection.WorldToMap(FVector(Line.End.X, Line.End.Y, UserLocation.Z));
        if (Snapshot->Projection.ClipMapLine(Start, End)) Snapshot->Boundary.Add({Start, End});
    }
    for (const FFFMapOverviewFacilityMarker& Data : WorldFacilityMarkers)
    {
        const FVector WorldPoint(Data.Point.X, Data.Point.Y, UserLocation.Z);
        if (!Snapshot->Projection.IsWorldPointVisible(WorldPoint)) continue;
        FFFMapOverviewFacilityMarker Marker = Data;
        Marker.Point = Snapshot->Projection.WorldToMap(WorldPoint);
        Snapshot->FacilityMarkers.Add(MoveTemp(Marker));
    }
    Snapshot->FacilityCount = Snapshot->FacilityMarkers.Num();
    RefreshMarkerWidgets();
}
void UFFMapOverviewWidget::RefreshMarkerWidgets()
{
    if (!MarkerLayer || !FacilityMarkerClass || !Snapshot.IsValid()) return;
    TSet<FGuid> Present;
    for (const FFFMapOverviewFacilityMarker& Data : Snapshot->FacilityMarkers)
    {
        Present.Add(Data.Id);
        TObjectPtr<UFFMapOverviewMarkerWidget>& Marker = FacilityMarkers.FindOrAdd(Data.Id);
        if (!Marker)
        {
            Marker = CreateWidget<UFFMapOverviewMarkerWidget>(GetOwningPlayer(), FacilityMarkerClass);
            if (!Marker) continue;
            Marker->SetVisibility(ESlateVisibility::HitTestInvisible);
            MarkerLayer->AddChildToCanvas(Marker);
        }
        Marker->SetMarkerData(static_cast<EFFMapOverviewMarkerKind>(Data.Kind), Data.DisplayName);
        if (UCanvasPanelSlot* LocalSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
        {
            LocalSlot->SetAnchors(FAnchors(Data.Point.X, Data.Point.Y));
            LocalSlot->SetAlignment(FVector2D(0.5, 0.5));
            LocalSlot->SetPosition(FVector2D::ZeroVector);
            LocalSlot->SetSize(Marker->MarkerSize);
        }
    }
    for (auto It = FacilityMarkers.CreateIterator(); It; ++It)
        if (!Present.Contains(It.Key()))
        {
            if (It.Value()) It.Value()->RemoveFromParent();
            It.RemoveCurrent();
        }
}

void UFFMapOverviewWidget::RefreshCameraMarker()
{
    if (!Snapshot.IsValid()) return;
    Snapshot->bHasCamera = false;
    Snapshot->CameraFootprint.Reset();
    APlayerController* PC = GetOwningPlayer();
    if (!PC || !Snapshot->Projection.IsValid())
    {
        if (CameraMarker) CameraMarker->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    int32 Width = 0, Height = 0;
    PC->GetViewportSize(Width, Height);
    if (Width < 1 || Height < 1)
    {
        if (CameraMarker) CameraMarker->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    FVector ActualFocus = FVector::ZeroVector;
    const bool bHasActualFocus = Subsystem.IsValid() && Subsystem->TryGetCameraFocus(ActualFocus);
    const double PlaneZ = bHasActualFocus ? ActualFocus.Z : Snapshot->Projection.Center.Z;
    auto Project = [&](const double X, const double Y, FVector2D& OutPoint)
    {
        FVector Origin, Direction;
        if (!PC->DeprojectScreenPositionToWorld(X, Y, Origin, Direction) || FMath::Abs(Direction.Z) < 0.0001) return false;
        const double Distance = (PlaneZ - Origin.Z) / Direction.Z;
        if (!FMath::IsFinite(Distance) || Distance < 0.0) return false;
        OutPoint = Snapshot->Projection.WorldToMap(Origin + Direction * Distance);
        return !OutPoint.ContainsNaN();
    };
    // This is a player-centered local map. Entering the separate whole-map camera
    // must not move its location marker to the festival's overview focus.
    Snapshot->CameraFocus = FVector2D(0.5, 0.5);
    Snapshot->bHasCamera = true;
    if (MarkerLayer && Snapshot->bHasCamera)
    {
        const TSubclassOf<UFFMapOverviewMarkerWidget> Class = CameraMarkerClass ? CameraMarkerClass : FacilityMarkerClass;
        if (!CameraMarker && Class)
        {
            CameraMarker = CreateWidget<UFFMapOverviewMarkerWidget>(PC, Class);
            if (CameraMarker)
            {
                CameraMarker->SetMarkerData(EFFMapOverviewMarkerKind::Camera, LOCTEXT("PlayerMarker", "현재 위치"));
                MarkerLayer->AddChildToCanvas(CameraMarker);
            }
        }
        if (CameraMarker)
        {
            CameraMarker->SetVisibility(ESlateVisibility::HitTestInvisible);
            if (UCanvasPanelSlot* LocalSlot = Cast<UCanvasPanelSlot>(CameraMarker->Slot))
            {
                LocalSlot->SetAnchors(FAnchors(Snapshot->CameraFocus.X, Snapshot->CameraFocus.Y));
                LocalSlot->SetAlignment(FVector2D(0.5, 0.5));
                LocalSlot->SetPosition(FVector2D::ZeroVector);
                LocalSlot->SetSize(CameraMarker->MarkerSize);
                LocalSlot->SetZOrder(1);
            }
        }
    }
    else if (CameraMarker) CameraMarker->SetVisibility(ESlateVisibility::Collapsed);
    TArray<FVector2D> Corners;
    for (const FVector2D& Pixel : {FVector2D(0, 0), FVector2D(Width, 0), FVector2D(Width, Height), FVector2D(0, Height)})
    {
        FVector2D Point;
        if (!Project(Pixel.X, Pixel.Y, Point)) return;
        Corners.Add(Point);
    }
    for (int32 Index = 0; Index < 4; ++Index)
    {
        FVector2D Start = Corners[Index], End = Corners[(Index + 1) % 4];
        if (Snapshot->Projection.ClipMapLine(Start, End)) Snapshot->CameraFootprint.Add({Start, End});
    }
}

void UFFMapOverviewWidget::RefreshTerrainImage()
{
    if (!TerrainImage || !Subsystem.IsValid()) return;
    UTextureRenderTarget2D* Texture = Subsystem->GetTerrainMapTexture();
    if (!Texture) return;
    if (UMaterialInstanceDynamic* Material = TerrainImage->GetDynamicMaterial())
    {
        if (Texture != CurrentTerrainTexture)
        {
            Material->SetTextureParameterValue(TEXT("MapTexture"), Texture);
            CurrentTerrainTexture = Texture;
        }
        FVector2D UVCenter;
        double UVScale = 1.0;
        if (Snapshot.IsValid() && Snapshot->Projection.GetTerrainUVTransform(Subsystem->GetTerrainMapBounds(), UVCenter, UVScale))
        {
            Material->SetVectorParameterValue(TEXT("MapUVCenter"), FLinearColor(UVCenter.X, UVCenter.Y, 0.f, 0.f));
            Material->SetScalarParameterValue(TEXT("MapUVScale"), static_cast<float>(UVScale));
        }
    }
}

void UFFMapOverviewWidget::RefreshMap()
{
    UFFMapOverviewSubsystem* Overview = Subsystem.Get();
    UFFZSWorldSubsystem* Zones = GetWorld() ? GetWorld()->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!Overview || !Zones || !MapSurface || !Snapshot.IsValid() || !IsVisible()) return;
    const double Now = GetWorld()->GetTimeSeconds();
    bool bBoundaryChanged = false;
    if (Now >= NextBoundaryCheck)
    {
        const uint32 Fingerprint = ComputeBoundaryFingerprint();
        bBoundaryChanged = Fingerprint != CachedBoundaryFingerprint;
        CachedBoundaryFingerprint = Fingerprint;
        NextBoundaryCheck = Now + 1.0;
    }
    const bool bAuthorityChanged = CachedWorldRevision != Zones->GetWorldRevision() ||
        CachedRegistryRevision != Zones->GetFacilityRegistryRevision() || bBoundaryChanged;
    if (bAuthorityChanged)
    {
        CachedWorldRevision = Zones->GetWorldRevision();
        CachedRegistryRevision = Zones->GetFacilityRegistryRevision();
        RebuildStaticMap();
    }
    FVector UserLocation = FVector::ZeroVector;
    if (!Overview->TryGetPlayerMapLocation(UserLocation)) return;
    const int32 GeometryOptions = (MapSurface->bDrawVectorLandFallback ? 1 : 0) |
        (MapSurface->bDrawGroundFootprints ? 2 : 0) | (MapSurface->bDrawBoundary ? 4 : 0);
    if (bAuthorityChanged || !Snapshot->Projection.IsValid() || !Snapshot->Projection.Center.Equals(UserLocation, 0.1) ||
        !FMath::IsNearlyEqual(Snapshot->Projection.Bounds.GetSize().X, LocalMapDiameterCm, 0.1) || CachedGeometryOptions != GeometryOptions)
    {
        CachedGeometryOptions = GeometryOptions;
        ReprojectLocalMap(UserLocation);
    }
    RefreshTerrainImage();
    RefreshCameraMarker();
    const bool bBuilding = Overview->IsBuildSessionActive();
    const int32 UiState = (Overview->IsWholeGridEnabled() ? 1 : 0) | (Overview->IsWholeMapView() ? 2 : 0) |
        (Overview->IsGridVisible() ? 4 : 0) | (bBuilding ? 8 : 0);
    if (UiState != CachedUiState)
    {
        CachedUiState = UiState;
        OnOverviewStateChanged(Overview->IsWholeGridEnabled(), Overview->IsWholeMapView(), Overview->IsGridVisible(), bBuilding);
    }
    MapSurface->RefreshPaint();
}

bool UFFMapOverviewWidget::IsPointerOverMapControls() const
{
    return (MapPanel && MapPanel->IsHovered()) || (MapSurface && MapSurface->IsHovered());
}
float UFFMapOverviewWidget::GetMapPanelRightInsetSlateUnits() const
{
    if (!MapPanel || !IsVisible()) return 0.f;
    const FGeometry& RootGeometry = GetCachedGeometry();
    const FGeometry& PanelGeometry = MapPanel->GetCachedGeometry();
    if (RootGeometry.GetLocalSize().X > 0.0 && PanelGeometry.GetLocalSize().X > 0.0)
    {
        const FVector2D Left = RootGeometry.AbsoluteToLocal(PanelGeometry.GetAbsolutePosition());
        return FMath::Clamp(static_cast<float>(RootGeometry.GetLocalSize().X - Left.X),
            0.f, static_cast<float>(RootGeometry.GetLocalSize().X));
    }
    if (const UCanvasPanelSlot* PanelCanvasSlot = Cast<UCanvasPanelSlot>(MapPanel->Slot))
    {
        const double Width = PanelCanvasSlot->GetAutoSize() ? MapPanel->GetDesiredSize().X : PanelCanvasSlot->GetSize().X;
        return FMath::Max(0.f, static_cast<float>(Width * PanelCanvasSlot->GetAlignment().X - PanelCanvasSlot->GetPosition().X));
    }
    return FMath::Max(0.f, static_cast<float>(MapPanel->GetDesiredSize().X));
}
void UFFMapOverviewWidget::NotifyInteraction() { if (Subsystem.IsValid()) Subsystem->NotifyMapInteraction(); }
FReply UFFMapOverviewWidget::NativeOnPreviewMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (IsPointerOverMapControls()) NotifyInteraction();
    return Super::NativeOnPreviewMouseButtonDown(Geometry, Event);
}
void UFFMapOverviewWidget::HandleMapPress(const FVector2D& NormalizedPoint)
{
    FVector Point;
    if (Snapshot.IsValid() && Snapshot->Projection.MapToWorld(NormalizedPoint, Point) && Subsystem.IsValid())
        Subsystem->FocusMapLocation(Point);
}

#undef LOCTEXT_NAMESPACE
