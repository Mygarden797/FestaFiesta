#include "FF_FFZSBuildableGridComponent.h"

#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangePresentation.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSPlacementGridModel.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "HAL/PlatformTime.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneManagement.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "MeshElementCollector.h"

namespace
{
    float ProjectGridSpacing(const UWorld* World, const FTransform& ZoneTransform,
        const FVector& SurfacePointWorld, const int32 StepCm)
    {
        const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
        FVector2D Center, X, Y;
        if (!Controller || !Controller->IsLocalController() ||
            !Controller->ProjectWorldLocationToScreen(SurfacePointWorld, Center) ||
            !Controller->ProjectWorldLocationToScreen(SurfacePointWorld +
                ZoneTransform.TransformVectorNoScale(FVector(StepCm, 0, 0)), X) ||
            !Controller->ProjectWorldLocationToScreen(SurfacePointWorld +
                ZoneTransform.TransformVectorNoScale(FVector(0, StepCm, 0)), Y)) return 0.0f;
        return static_cast<float>(FMath::Min(FVector2D::Distance(Center, X), FVector2D::Distance(Center, Y)));
    }

    uint32 GridStyleHash(const UFFZSBuildPresentationSettings& Style)
    {
        uint32 Hash = 0;
        for (const FLinearColor& Color : { Style.MinorLineColor, Style.MajorLineColor, Style.BoundaryLineColor })
        {
            Hash = HashCombine(Hash, GetTypeHash(Color.R));
            Hash = HashCombine(Hash, GetTypeHash(Color.G));
            Hash = HashCombine(Hash, GetTypeHash(Color.B));
            Hash = HashCombine(Hash, GetTypeHash(Color.A));
        }
        for (const float Value : { Style.MinorLineThickness, Style.MajorLineThickness,
            Style.BoundaryLineThickness, Style.HaloOpacity, Style.SelectionDetailPaddingCm })
        {
            Hash = HashCombine(Hash, GetTypeHash(Value));
        }
        return Hash;
    }

    // A production scene proxy: unlike the engine's debug line batcher, this uses the
    // normal render path in every build and keeps stroke width in pixels when zooming.
    class FFFZSBuildGridSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FFFZSBuildGridSceneProxy(const UFFZSBuildableGridComponent* Component)
            : FPrimitiveSceneProxy(Component), Lines(Component->BatchedLines)
        {
            bWillEverBeLit = false;
        }
        virtual SIZE_T GetTypeHash() const override
        {
            static size_t TypeIdentity;
            return reinterpret_cast<size_t>(&TypeIdentity);
        }
        virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
            const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
        {
            for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
            {
                if ((VisibilityMap & (1u << ViewIndex)) == 0) { continue; }
                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                for (const FBatchedLine& Line : Lines)
                {
                    PDI->DrawTranslucentLine(Line.Start, Line.End, Line.Color,
                        Line.DepthPriority, Line.Thickness, 0.0f, true);
                }
            }
        }
        virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
        {
            FPrimitiveViewRelevance Relevance;
            Relevance.bDrawRelevance = IsShown(View);
            Relevance.bDynamicRelevance = true;
            Relevance.bSeparateTranslucency = Relevance.bNormalTranslucency = true;
            return Relevance;
        }
        virtual uint32 GetMemoryFootprint() const override
        {
            return sizeof(*this) + FPrimitiveSceneProxy::GetAllocatedSize() + Lines.GetAllocatedSize();
        }
    private:
        const TArray<FBatchedLine> Lines;
    };
}

FPrimitiveSceneProxy* UFFZSBuildableGridComponent::CreateSceneProxy()
{
    return new FFFZSBuildGridSceneProxy(this);
}

UFFZSBuildableGridComponent::UFFZSBuildableGridComponent()
{
    SetAbsolute(true, true, true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    PrimaryComponentTick.bCanEverTick = false;
    bCalculateAccurateBounds = true;
}

void UFFZSBuildableGridComponent::HideGrid()
{
    if (!BatchedLines.IsEmpty())
    {
        Flush();
        ++PresentationRevision;
    }
    SetHiddenInGame(true);
    bCacheValid = false;
}

void UFFZSBuildableGridComponent::RefreshGrid(UFFZSWorldSubsystem& WorldSystem,
    const FFFZSCommand& Command, const FVector2D& CenterLocalCm,
    const FVector2D& HalfExtentCm, const int32 GridStepCm,
    const FVector& SurfacePointWorld, const FVector& SurfaceNormalWorld, const int32 SurfaceSampleBudget,
    const FName SelectionCoordinateZoneId)
{
    const AFFZSGameplayZone* Zone = WorldSystem.FindZone(Command.ZoneId);
    if (!Zone || CenterLocalCm.ContainsNaN() || HalfExtentCm.ContainsNaN() ||
        HalfExtentCm.X <= 0.0 || HalfExtentCm.Y <= 0.0 || SurfacePointWorld.ContainsNaN())
    {
        HideGrid();
        return;
    }

    const int32 Step = FFZSPlacementGridModel::SanitizeGridStepCm(GridStepCm);
    const FVector2D Min(FFZSPlacementGridModel::AlignDownToGrid(CenterLocalCm.X - HalfExtentCm.X, Step),
        FFZSPlacementGridModel::AlignDownToGrid(CenterLocalCm.Y - HalfExtentCm.Y, Step));
    const FVector2D Max(FFZSPlacementGridModel::AlignUpToGrid(CenterLocalCm.X + HalfExtentCm.X, Step),
        FFZSPlacementGridModel::AlignUpToGrid(CenterLocalCm.Y + HalfExtentCm.Y, Step));
    const FVector Normal = SurfaceNormalWorld.GetSafeNormal();
    if (Normal.ContainsNaN() || Normal.Z < 0.25)
    {
        HideGrid();
        return;
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const bool bUnifiedBuilding = WorldSystem.FindBuildArea() != nullptr;
    const float Lift = FMath::Clamp(Settings->PlacementGridSurfaceLiftCm, 0.5f, 20.0f);
    const int32 MajorInterval = FMath::Max(Step,
        FMath::RoundToInt(static_cast<double>(Settings->PlacementGridMajorIntervalCm) / Step) * Step);
    uint32 ContextHash = HashCombine(GetTypeHash(Command.ZoneId), GetTypeHash(Step));
    ContextHash = HashCombine(ContextHash, GetTypeHash(bUnifiedBuilding));
    ContextHash = HashCombine(ContextHash, GetTypeHash(Command.FacilitySpec.FacilityDefinitionId));
    ContextHash = HashCombine(ContextHash, GetTypeHash(static_cast<uint8>(Command.FacilitySpec.Category)));
    ContextHash = HashCombine(ContextHash, GetTypeHash(Command.FacilityInstanceId));
    ContextHash = HashCombine(ContextHash, GetTypeHash(static_cast<uint8>(Command.Type)));
    ContextHash = HashCombine(ContextHash, GetTypeHash(Command.FacilitySpec.PlacementObstacleHeightCm));
    ContextHash = HashCombine(ContextHash, GetTypeHash(Command.FacilitySpec.AllowsFacilityPlacementOnTop()));
    ContextHash = HashCombine(ContextHash, GetTypeHash(MajorInterval));
    ContextHash = HashCombine(ContextHash, GetTypeHash(Lift));
    for (const FName AllowedZone : Command.FacilitySpec.AllowedZoneIds)
    {
        ContextHash = HashCombine(ContextHash, GetTypeHash(AllowedZone));
    }
    const double Now = FPlatformTime::Seconds();
    const double PlaneDistance = FVector::DotProduct(Normal, SurfacePointWorld);
    const FTransform ZoneTransform = Zone->GetActorTransform();
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    const int32 ProjectedStep = bCacheValid && CachedContextHash == ContextHash && CachedSpatialMask.GetStepCm() > 0.0
        ? FMath::Max(1, FMath::RoundToInt(CachedSpatialMask.GetStepCm())) : Step;
    const int32 MinorStride = FFZSBuildRangePresentation::ResolveMinorLineStride(
        ProjectGridSpacing(GetWorld(), ZoneTransform, SurfacePointWorld, ProjectedStep), Style.MinimumMinorSpacingPixels);
    const AFFZSGameplayZone* SelectionZone = SelectionCoordinateZoneId.IsNone()
        ? Zone : WorldSystem.FindZone(SelectionCoordinateZoneId);
    const bool bHasSelection = SelectionZone && !Command.FacilitySpec.FacilityDefinitionId.IsNone();
    const FFFZSOrientedRect SelectionRect = FFZSOrientedGeometry::MakeFromLocalRect(
        FFFZSCellRect(FIntPoint::ZeroValue, Command.FacilitySpec.FootprintSizeCells),
        Command.Placement.OriginCell, UFFZSGridLibrary::YawCentidegreesToDegrees(
            UFFZSGridLibrary::GetEffectiveYawCentidegrees(Command.Placement)));
    const double SelectionRadius = SelectionRect.HalfExtentCells.Size() +
        FMath::Clamp(Style.SelectionDetailPaddingCm, 0.0f, 2000.0f);
    // Overview components each use their own zone; the placement stays in its original coordinate zone.
    const FVector SelectionLocal = bHasSelection ? ZoneTransform.InverseTransformPosition(
        SelectionZone->GetActorTransform().TransformPosition(FVector(SelectionRect.CenterCell, 0.0))) : FVector::ZeroVector;
    const FVector2D SelectionCenter(SelectionLocal.X, SelectionLocal.Y);
    uint32 PresentationHash = HashCombine(GridStyleHash(Style), GetTypeHash(MinorStride));
    PresentationHash = HashCombine(PresentationHash, GetTypeHash(bHasSelection));
    if (bHasSelection)
    {
        PresentationHash = HashCombine(PresentationHash, GetTypeHash(SelectionCenter));
        PresentationHash = HashCombine(PresentationHash, GetTypeHash(SelectionRadius));
    }
    // Movement within one visual cell reuses the scene proxy. The bounded refresh catches
    // moved scenery/protected areas whose edits do not issue an authority world command.
    const bool bSpatialCacheCurrent = bCacheValid && CachedContextHash == ContextHash &&
        CachedRevision == WorldSystem.GetWorldRevision() && CachedMin.Equals(Min, 0.01) &&
        CachedRegistryRevision == WorldSystem.GetFacilityRegistryRevision() &&
        CachedMax.Equals(Max, 0.01) && CachedSurfaceNormal.Equals(Normal, 0.001) &&
        FMath::IsNearlyEqual(CachedPlaneDistance, PlaneDistance, 0.5) &&
        CachedZoneTransform.Equals(ZoneTransform) && Now - LastRefreshTimeSeconds < 0.5;
    if (bSpatialCacheCurrent && !SurfaceCache.HasPendingSamples() && CachedPresentationHash == PresentationHash)
    {
        return;
    }

    if (!bSpatialCacheCurrent)
    {
        WorldSystem.BuildPlacementGridMask(Command, Min, Max, Step, SurfacePointWorld.Z, CachedSpatialMask);
        LastRefreshTimeSeconds = Now;
    }
    // Terrain removes unknown/steep/missing cells from a copy, never from the authority cache.
    // Budgeted samples continue on subsequent input ticks until the local window is ready.
    FFFZSBuildableGridMask Mask = CachedSpatialMask;
    if (UWorld* World = GetWorld())
    {
        SurfaceCache.Update(*World, ZoneTransform, SurfacePointWorld.Z, Mask, SurfaceSampleBudget);
    }
    else
    {
        Mask.Reset();
    }
    TArray<FFFZSBuildableGridSegment> Segments;
    FFZSBuildableGrid::BuildSegments(Mask, Segments);

    const auto ToWorld = [this, Lift](const FVector2D& Point)
    {
        return SurfaceCache.GetWorldPoint(Point) + FVector::UpVector * Lift;
    };

    TArray<FBatchedLine> Lines;
    Lines.Reserve(Segments.Num() * 2);
    const FVector2D StableCenter = (Min + Max) * 0.5;
    const FVector2D StableExtent = (Max - Min) * 0.5;
    const int32 VisibleStep = FMath::Max(1, FMath::RoundToInt(Mask.GetStepCm()));
    const int32 VisibleMajorInterval = bUnifiedBuilding
        ? FMath::Max(VisibleStep, FMath::RoundToInt(
            static_cast<double>(Settings->PlacementGridMajorIntervalCm) / VisibleStep) * VisibleStep)
        : MajorInterval;
    for (const FFFZSBuildableGridSegment& Segment : Segments)
    {
        // Styling shares the same world lattice as placement and mask alignment, including
        // coordinate regions whose origins are offset or whose axes turn by 90 degrees.
        const FVector StyleStart = bUnifiedBuilding
            ? ZoneTransform.TransformPosition(FVector(Segment.StartLocalCm, 0.0)) : FVector(Segment.StartLocalCm, 0.0);
        const FVector StyleEnd = bUnifiedBuilding
            ? ZoneTransform.TransformPosition(FVector(Segment.EndLocalCm, 0.0)) : FVector(Segment.EndLocalCm, 0.0);
        const bool bVertical = FMath::IsNearlyEqual(StyleStart.X, StyleEnd.X);
        const bool bMajor = FFZSPlacementGridModel::IsMajorGridCoordinate(
            bVertical ? StyleStart.X : StyleStart.Y, VisibleMajorInterval);
        const bool bKeepMinorLine = MinorStride == 1 || (MinorStride > 1 &&
            FFZSPlacementGridModel::IsMajorGridCoordinate(
                bVertical ? StyleStart.X : StyleStart.Y, VisibleStep * MinorStride));
        // Sample every lattice endpoint to follow terrain rather than spanning hills with
        // a single chord. Merge straight runs inside each fade band before scene submission.
        const int32 SectionCount = FMath::Max(1, FMath::RoundToInt(
            FVector2D::Distance(Segment.StartLocalCm, Segment.EndLocalCm) / Mask.GetStepCm()));
        FVector RunStart = ToWorld(Segment.StartLocalCm);
        FVector RunEnd = RunStart;
        FVector RunDirection = FVector::ZeroVector;
        int32 RunBand = -1;
        const float Thickness = FMath::Clamp(Segment.bBoundary ? Style.BoundaryLineThickness :
            bMajor ? Style.MajorLineThickness : Style.MinorLineThickness, 0.5f, 4.0f);
        const FLinearColor StrokeColor = Segment.bBoundary ? Style.BoundaryLineColor :
            bMajor ? Style.MajorLineColor : Style.MinorLineColor;
        const auto AppendRun = [&](const FVector& A, const FVector& B, const int32 Band)
        {
            if (Band < 0 || A.Equals(B, 0.01))
            {
                return;
            }
            const float Opacity = FMath::Clamp(Band / 8.0f, 0.0625f, 1.0f);
            // Separate halo depth from the color stroke so equal-depth primitives cannot
            // cover each other in a platform-dependent order.
            const FVector HaloOffset = FVector::UpVector * 0.1;
            const float HaloAlpha = FMath::Clamp(Style.HaloOpacity, 0.0f, 1.0f) * Opacity;
            if (HaloAlpha > 0.0f)
            {
                Lines.Emplace(A - HaloOffset, B - HaloOffset, FLinearColor(0.005f, 0.02f, 0.025f, HaloAlpha),
                    0.0f, Thickness + 0.8f, SDPG_World);
            }
            FLinearColor Color = StrokeColor;
            Color.A = FMath::Clamp(Color.A * Opacity, 0.0f, 1.0f);
            Lines.Emplace(A, B, Color, 0.0f, Thickness, SDPG_World);
        };
        for (int32 Index = 0; Index < SectionCount; ++Index)
        {
            const FVector2D A = FMath::Lerp(Segment.StartLocalCm, Segment.EndLocalCm,
                static_cast<double>(Index) / SectionCount);
            const FVector2D B = FMath::Lerp(Segment.StartLocalCm, Segment.EndLocalCm,
                static_cast<double>(Index + 1) / SectionCount);
            const FVector2D Mid = (A + B) * 0.5;
            const bool bNearSelection = bHasSelection && SelectionRect.IsValid() &&
                FVector2D::DistSquared(Mid, SelectionCenter) <= FMath::Square(SelectionRadius);
            if (!Segment.bBoundary && !bMajor && !bKeepMinorLine && !bNearSelection)
            {
                AppendRun(RunStart, RunEnd, RunBand);
                RunBand = -1;
                continue;
            }
            const float Opacity = FFZSPlacementGridModel::ResolveEdgeOpacity(Mid.X, StableCenter.X, StableExtent.X) *
                FFZSPlacementGridModel::ResolveEdgeOpacity(Mid.Y, StableCenter.Y, StableExtent.Y);
            const int32 Band = FMath::Max(1, FMath::RoundToInt(Opacity * 8.0f));
            const FVector WorldA = ToWorld(A);
            const FVector WorldB = ToWorld(B);
            const FVector Direction = (WorldB - WorldA).GetSafeNormal();
            if (RunBand != Band || !Direction.Equals(RunDirection, 0.0001))
            {
                AppendRun(RunStart, RunEnd, RunBand);
                RunStart = WorldA;
                RunBand = Band;
                RunDirection = Direction;
            }
            RunEnd = WorldB;
        }
        AppendRun(RunStart, RunEnd, RunBand);
    }
    bool bLinesChanged = Lines.Num() != BatchedLines.Num();
    for (int32 Index = 0; !bLinesChanged && Index < Lines.Num(); ++Index)
    {
        const FBatchedLine& A = Lines[Index];
        const FBatchedLine& B = BatchedLines[Index];
        bLinesChanged = !A.Start.Equals(B.Start, 0.01) || !A.End.Equals(B.End, 0.01) ||
            !A.Color.Equals(B.Color) || !FMath::IsNearlyEqual(A.Thickness, B.Thickness) ||
            A.DepthPriority != B.DepthPriority;
    }
    // Checks may repeat to detect external scenery changes, but an unchanged result must
    // retain its render proxy. Publish an actual change once, without an empty Flush phase.
    if (bLinesChanged)
    {
        BatchedLines = MoveTemp(Lines);
        MarkRenderStateDirty();
        ++PresentationRevision;
    }
    SetHiddenInGame(BatchedLines.IsEmpty());
    CachedContextHash = ContextHash;
    CachedPresentationHash = PresentationHash;
    CachedRevision = WorldSystem.GetWorldRevision();
    CachedRegistryRevision = WorldSystem.GetFacilityRegistryRevision();
    CachedMin = Min;
    CachedMax = Max;
    CachedSurfaceNormal = Normal;
    CachedPlaneDistance = PlaneDistance;
    CachedZoneTransform = ZoneTransform;
    bCacheValid = true;
}
