#include "FF_MapGridActor.h"

#include "FF_MapGridComponent.h"
#include "FF_MapGridModel.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"

namespace
{
    const FName SelectionGridKey(TEXT("MapOverviewSelectionDetail"));
    const FName UnifiedGridKey(TEXT("MapOverviewUnifiedLand"));
    struct FGridRequest
    {
        FName Key;
        FName ZoneId;
        FBox2D Bounds;
        int32 DesiredStep = 100;
        bool bWholeMap = false;
    };

    double ProjectSpacing(const UWorld* World, const FVector& Center, const int32 Step)
    {
        const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        FVector2D A, X, Y;
        if (!PC || !PC->IsLocalController() || !PC->ProjectWorldLocationToScreen(Center, A) ||
            !PC->ProjectWorldLocationToScreen(Center + FVector(Step, 0, 0), X) ||
            !PC->ProjectWorldLocationToScreen(Center + FVector(0, Step, 0), Y)) return 0.0;
        return FMath::Min(FVector2D::Distance(A, X), FVector2D::Distance(A, Y));
    }

    FBox2D WorldBoundsInZone(const FBox& WorldBounds, const AFFZSGameplayZone& Zone)
    {
        FBox2D Bounds(EForceInit::ForceInit);
        for (int32 Corner = 0; Corner < 4; ++Corner)
        {
            const FVector Local = Zone.GetActorTransform().InverseTransformPosition(FVector(
                (Corner & 1) ? WorldBounds.Max.X : WorldBounds.Min.X,
                (Corner & 2) ? WorldBounds.Max.Y : WorldBounds.Min.Y, Zone.GetActorLocation().Z));
            Bounds += FVector2D(Local.X, Local.Y);
        }
        return Bounds;
    }
}

AFFMapGridActor::AFFMapGridActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("MapGridRoot")));
    SetActorEnableCollision(false);
}

void AFFMapGridActor::HideGrid()
{
    for (const auto& Pair : Grids) if (Pair.Value) Pair.Value->HideGrid();
    SetActorHiddenInGame(true);
    LastTraceBudget = 0;
    LastAuthorityRebuildCount = 0;
}

bool AFFMapGridActor::HasPendingSamples() const
{
    if (IsHidden()) return false;
    for (const auto& Pair : Grids) if (Pair.Value && Pair.Value->HasPendingSamples()) return true;
    return false;
}

int32 AFFMapGridActor::GetRetainedCellCount() const
{
    int32 Cells = 0;
    for (const auto& Pair : Grids) if (Pair.Value) Cells += Pair.Value->GetCellCount();
    return Cells;
}

int32 AFFMapGridActor::GetRenderedLineCount() const
{
    int32 Lines = 0;
    for (const auto& Pair : Grids) if (Pair.Value) Lines += Pair.Value->GetLineCount();
    return Lines;
}

double AFFMapGridActor::GetOverviewSpacingCm() const
{
    double Spacing = 0.0;
    for (const auto& Pair : Grids)
        if (Pair.Value && Pair.Key != SelectionGridKey && Pair.Value->GetCellCount() > 0)
            Spacing = FMath::Max(Spacing, Pair.Value->GetWorldStepCm());
    return Spacing;
}

void AFFMapGridActor::RefreshGrid(UFFZSWorldSubsystem& WorldSystem,
    const FVector& SurfacePointWorld, const FVector& SurfaceNormalWorld, const int32 GridStepCm,
    const bool bWholeMap, const FFFZSCommand* PlacementContext)
{
    if (SurfacePointWorld.ContainsNaN() || SurfaceNormalWorld.ContainsNaN() || SurfaceNormalWorld.GetSafeNormal().Z < 0.25)
    {
        HideGrid();
        return;
    }
    const int32 BaseStep = FMath::Clamp(GridStepCm, 25, 1000);
    LastBaseStepCm = BaseStep;
    LastProjectedBaseSpacingPixels = ProjectSpacing(GetWorld(), SurfacePointWorld, BaseStep);
    PreviousZoomStepCm = FFMapGridModel::ResolveZoomStep(BaseStep,
        LastProjectedBaseSpacingPixels, PreviousZoomStepCm);
    const TArray<FName> ZoneIds = WorldSystem.GetRegisteredZoneIds();
    const double Now = FPlatformTime::Seconds();
    if (BoundsWorldRevision != WorldSystem.GetWorldRevision() || Now - LastBoundsRefreshSeconds >= 1.0 ||
        CachedZoneBounds.Num() != ZoneIds.Num() || ZoneIds.ContainsByPredicate(
            [&](FName Id) { return !CachedZoneBounds.Contains(Id); }))
    {
        CachedZoneBounds.Reset();
        for (const FName Id : ZoneIds)
            if (const AFFZSGameplayZone* Zone = WorldSystem.FindZone(Id))
                CachedZoneBounds.Add(Id, FBox2D(Zone->GetBoundaryPolygonLocalCm()));
        BoundsWorldRevision = WorldSystem.GetWorldRevision();
        LastBoundsRefreshSeconds = Now;
    }
    TArray<FGridRequest> Requests;
    if (const AFFZSBuildArea* Area = WorldSystem.FindBuildArea())
    {
        UnifiedCoordinateZoneId = WorldSystem.ResolveZoneAtWorldLocation(SurfacePointWorld, UnifiedCoordinateZoneId);
        if (UnifiedCoordinateZoneId.IsNone() && !ZoneIds.IsEmpty()) UnifiedCoordinateZoneId = ZoneIds[0];
        if (const AFFZSGameplayZone* Zone = WorldSystem.FindZone(UnifiedCoordinateZoneId))
        {
            const FVector Local = Zone->GetActorTransform().InverseTransformPosition(SurfacePointWorld);
            const FBox2D Bounds = bWholeMap ? WorldBoundsInZone(Area->GetBuildBounds(), *Zone) :
                FFMapGridModel::MakeLocalWindow(FVector2D(Local.X, Local.Y), BaseStep);
            if (Bounds.bIsValid) Requests.Add({ UnifiedGridKey, UnifiedCoordinateZoneId, Bounds, PreviousZoomStepCm, bWholeMap });
        }
    }
    else
    {
        for (const FName Id : ZoneIds)
        {
            const AFFZSGameplayZone* Zone = WorldSystem.FindZone(Id);
            const FBox2D* ZoneBounds = CachedZoneBounds.Find(Id);
            if (!Zone || !ZoneBounds || !ZoneBounds->bIsValid) continue;
            const FVector Local = Zone->GetActorTransform().InverseTransformPosition(SurfacePointWorld);
            const FBox2D Window = FFMapGridModel::MakeLocalWindow(FVector2D(Local.X, Local.Y), BaseStep);
            if (!bWholeMap && !ZoneBounds->Intersect(Window)) continue;
            const FBox2D Bounds = bWholeMap ? *ZoneBounds : FBox2D(
                FVector2D(FMath::Max(ZoneBounds->Min.X, Window.Min.X), FMath::Max(ZoneBounds->Min.Y, Window.Min.Y)),
                FVector2D(FMath::Min(ZoneBounds->Max.X, Window.Max.X), FMath::Min(ZoneBounds->Max.Y, Window.Max.Y)));
            Requests.Add({ Id, Id, Bounds, PreviousZoomStepCm, bWholeMap });
        }
    }
    // Keep the selected footprint precise even while the whole-map overview is coarse.
    if ((bWholeMap || PreviousZoomStepCm > BaseStep) && PlacementContext &&
        !PlacementContext->FacilitySpec.FacilityDefinitionId.IsNone())
    {
        if (const AFFZSGameplayZone* Zone = WorldSystem.FindZone(PlacementContext->ZoneId))
        {
            const FFFZSOrientedRect Footprint = FFZSOrientedGeometry::MakeFromLocalRect(
                FFFZSCellRect(FIntPoint::ZeroValue, PlacementContext->FacilitySpec.FootprintSizeCells),
                PlacementContext->Placement.OriginCell, UFFZSGridLibrary::YawCentidegreesToDegrees(
                    UFFZSGridLibrary::GetEffectiveYawCentidegrees(PlacementContext->Placement)));
            const FBox2D FootprintBounds = FFZSOrientedGeometry::GetAabb(Footprint);
            if (FootprintBounds.bIsValid)
                Requests.Add({ SelectionGridKey, PlacementContext->ZoneId,
                    FBox2D(FootprintBounds.Min - FVector2D(800.0), FootprintBounds.Max + FVector2D(800.0)), BaseStep, false });
        }
    }
    for (auto It = Grids.CreateIterator(); It; ++It)
    {
        if (!Requests.ContainsByPredicate([&](const FGridRequest& Request) { return Request.Key == It.Key(); }))
        {
            It.Value()->DestroyComponent();
            It.RemoveCurrent();
        }
    }
    if (Requests.IsEmpty()) { HideGrid(); return; }

    const int32 Count = Requests.Num();
    const int32 RebuildIndex = RoundRobinIndex % Count;
    RoundRobinIndex = (RoundRobinIndex + 1) % Count;
    LastTraceBudget = 0;
    LastAuthorityRebuildCount = 0;
    const bool bHasDetail = Requests.Last().Key == SelectionGridKey;
    const int32 DetailCells = bHasDetail ? 4096 : 0;
    const int32 OverviewCount = FMath::Max(1, Count - (bHasDetail ? 1 : 0));
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FGridRequest& Request = Requests[Index];
        const bool bDetail = Request.Key == SelectionGridKey;
        const int32 CellBudget = bDetail ? DetailCells :
            (FFMapGridModel::MaximumCells - DetailCells) / OverviewCount +
            (Index < (FFMapGridModel::MaximumCells - DetailCells) % OverviewCount ? 1 : 0);
        const int32 LineBudget = FFMapGridModel::MaximumRenderedLines / Count +
            (Index < FFMapGridModel::MaximumRenderedLines % Count ? 1 : 0);
        const int32 RelativeIndex = (Index - RebuildIndex + Count) % Count;
        const int32 TraceBudget = FFZSBuildGridSurface::MaximumTracesPerUpdate / Count +
            (RelativeIndex < FFZSBuildGridSurface::MaximumTracesPerUpdate % Count ? 1 : 0);
        TObjectPtr<UFFMapGridComponent>& Grid = Grids.FindOrAdd(Request.Key);
        if (!Grid)
        {
            Grid = NewObject<UFFMapGridComponent>(this, NAME_None, RF_Transient);
            Grid->SetupAttachment(GetRootComponent());
            AddInstanceComponent(Grid);
            Grid->RegisterComponent();
        }
        FFFZSCommand Command = PlacementContext ? *PlacementContext : FFFZSCommand();
        Command.ZoneId = Request.ZoneId;
        const FVector2D Size = Request.Bounds.GetSize();
        // A small region keeps at least a few cells when the global view is far away.
        const int32 DesiredStep = FMath::Max(BaseStep, FMath::Min(Request.DesiredStep,
            FMath::FloorToInt(FMath::Min(Size.X, Size.Y) / 4.0)));
        const int32 RequestedStep = FMath::Clamp(DesiredStep, 25, 1000);
        FVector ReferencePoint = SurfacePointWorld;
        if (Request.bWholeMap && !WorldSystem.FindBuildArea())
            if (const AFFZSGameplayZone* Zone = WorldSystem.FindZone(Request.ZoneId))
                ReferencePoint.Z = Zone->GetActorLocation().Z;
        LastAuthorityRebuildCount += Grid->RefreshGrid(WorldSystem, Command, Request.Bounds,
            RequestedStep, DesiredStep, CellBudget, LineBudget, ReferencePoint, Request.bWholeMap,
            TraceBudget, Index == RebuildIndex) ? 1 : 0;
        LastTraceBudget += Grid->GetLastTraceAllowance();
    }
    SetActorHiddenInGame(false);
}
