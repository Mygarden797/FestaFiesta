#include "FF_MapGridComponent.h"

#include "FF_MapGridModel.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"

namespace
{
    // Pixel strokes use the normal render path in Development and Shipping builds.
    class FMapGridSceneProxy final : public FPrimitiveSceneProxy
    {
    public:
        explicit FMapGridSceneProxy(const UFFMapGridComponent* Component)
            : FPrimitiveSceneProxy(Component), Lines(Component->BatchedLines) { bWillEverBeLit = false; }
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
                if ((VisibilityMap & (1u << ViewIndex)) == 0) continue;
                FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
                for (const FBatchedLine& Line : Lines)
                    PDI->DrawTranslucentLine(Line.Start, Line.End, Line.Color, Line.DepthPriority,
                        Line.Thickness, 0.0f, true);
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

    uint32 ContextHash(const FFFZSCommand& Command, const FBox2D& Bounds,
        const FTransform& Transform, int32 RequestedStep, int32 DesiredStep, int32 Cells, int32 Lines, bool bWholeMap)
    {
        uint32 Hash = HashCombine(GetTypeHash(Command.ZoneId), GetTypeHash(Command.FacilitySpec.FacilityDefinitionId));
        for (const uint32 Value : { GetTypeHash(Bounds.Min), GetTypeHash(Bounds.Max),
            GetTypeHash(Transform.GetLocation()), GetTypeHash(Transform.GetRotation()),
            GetTypeHash(Transform.GetScale3D()), GetTypeHash(RequestedStep), GetTypeHash(DesiredStep),
            GetTypeHash(Cells), GetTypeHash(Lines), GetTypeHash(bWholeMap),
            GetTypeHash(static_cast<uint8>(Command.Type)), GetTypeHash(Command.FacilityInstanceId),
            GetTypeHash(static_cast<uint8>(Command.FacilitySpec.Category)),
            GetTypeHash(Command.FacilitySpec.PlacementObstacleHeightCm),
            GetTypeHash(Command.FacilitySpec.AllowsFacilityPlacementOnTop()) }) Hash = HashCombine(Hash, Value);
        for (const FName ZoneId : Command.FacilitySpec.AllowedZoneIds) Hash = HashCombine(Hash, GetTypeHash(ZoneId));
        return Hash;
    }

    bool IsMajorCoordinate(double Value, double Interval)
    {
        return Interval > 0.0 && FMath::Abs(Value - FMath::GridSnap(Value, Interval)) < 0.05;
    }
}

UFFMapGridComponent::UFFMapGridComponent()
{
    SetAbsolute(true, true, true);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    PrimaryComponentTick.bCanEverTick = false;
    bCalculateAccurateBounds = true;
}

FPrimitiveSceneProxy* UFFMapGridComponent::CreateSceneProxy() { return new FMapGridSceneProxy(this); }

void UFFMapGridComponent::HideGrid()
{
    SetHiddenInGame(true);
    if (!BatchedLines.IsEmpty()) { Flush(); ++PresentationRevision; }
    SpatialMask.Reset();
    SurfaceCache.Reset();
    bCacheValid = false;
    bNeedsAuthorityRefresh = true;
    LastSurfaceRefreshSeconds = -1.0;
    LastTraceAllowance = 0;
}

bool UFFMapGridComponent::RefreshGrid(UFFZSWorldSubsystem& WorldSystem, const FFFZSCommand& Command,
    const FBox2D& LocalBounds, const int32 RequestedStepCm, const int32 DesiredWorldStepCm,
    const int32 CellBudget, const int32 LineBudget, const FVector& SurfacePointWorld, const bool bWholeMap,
    const int32 SurfaceSampleBudget, const bool bPermitAuthorityRefresh)
{
    LastTraceAllowance = 0;
    const AFFZSGameplayZone* Zone = WorldSystem.FindZone(Command.ZoneId);
    if (!Zone || !LocalBounds.bIsValid || LocalBounds.Min.ContainsNaN() || LocalBounds.Max.ContainsNaN() ||
        CellBudget <= 0 || LineBudget < 2 || SurfacePointWorld.ContainsNaN())
    {
        HideGrid();
        return false;
    }
    const FTransform ZoneTransform = Zone->GetActorTransform();
    const uint32 Hash = ContextHash(Command, LocalBounds, ZoneTransform, RequestedStepCm,
        DesiredWorldStepCm, CellBudget, LineBudget, bWholeMap);
    const double Now = FPlatformTime::Seconds();
    const bool bContextChanged = !bCacheValid || Hash != CachedContextHash;
    const bool bRevisionChanged = CachedWorldRevision != WorldSystem.GetWorldRevision() ||
        CachedRegistryRevision != WorldSystem.GetFacilityRegistryRevision();
    bNeedsAuthorityRefresh = bContextChanged || bRevisionChanged || Now - LastAuthorityRefreshSeconds >= 2.0;
    if ((bContextChanged || bRevisionChanged) && !bPermitAuthorityRefresh)
    {
        // Never display a previous selection's mask or retain more than the new cell share.
        HideGrid();
        return false;
    }
    bool bRebuilt = false;
    if (bNeedsAuthorityRefresh && bPermitAuthorityRefresh)
    {
        FFFZSBuildableGridMask AuthorityMask;
        WorldSystem.BuildPlacementGridMask(Command, LocalBounds.Min, LocalBounds.Max,
            FMath::Clamp(RequestedStepCm, 25, 1000), SurfacePointWorld.Z, AuthorityMask);
        const FTransform PreviousTransform = PresentationTransform;
        const double PreviousStep = SpatialMask.GetStepCm();
        FFMapGridModel::CompactAuthorityMask(AuthorityMask, ZoneTransform, CellBudget,
            DesiredWorldStepCm, SpatialMask, PresentationTransform, WorldSystem.FindBuildArea() != nullptr);
        // Pure translations on the same lattice preserve samples, but changing the compact
        // coordinate mapping must discard samples to avoid attaching them to the wrong floor.
        if (!PreviousTransform.Equals(PresentationTransform, 0.001) || PreviousStep != SpatialMask.GetStepCm())
            SurfaceCache.Reset();
        CachedContextHash = Hash;
        CachedWorldRevision = WorldSystem.GetWorldRevision();
        CachedRegistryRevision = WorldSystem.GetFacilityRegistryRevision();
        LastAuthorityRefreshSeconds = Now;
        bNeedsAuthorityRefresh = false;
        bCacheValid = true;
        bRebuilt = true;
    }

    // Once a window is ready, avoid traversing its vertices on every UI timer tick. Pending
    // terrain work keeps the presenter's fast cadence; authority changes bypass this gate.
    if (!bRebuilt && !SurfaceCache.HasPendingSamples() && Now - LastSurfaceRefreshSeconds < 0.5)
        return false;
    FFFZSBuildableGridMask VisibleMask = SpatialMask;
    bool bSurfaceChanged = false;
    if (UWorld* World = GetWorld())
    {
        LastTraceAllowance = FMath::Clamp(SurfaceSampleBudget, 0, FFZSBuildGridSurface::MaximumTracesPerUpdate);
        bSurfaceChanged = SurfaceCache.Update(*World, PresentationTransform,
            SurfacePointWorld.Z, VisibleMask, LastTraceAllowance);
        LastSurfaceRefreshSeconds = Now;
        ++SurfaceUpdateCount;
    }
    if (!bRebuilt && !bSurfaceChanged) return false;

    TArray<FFFZSBuildableGridSegment> Segments;
    FFZSBuildableGrid::BuildSegments(VisibleMask, Segments);
    Segments.StableSort([](const FFFZSBuildableGridSegment& A, const FFFZSBuildableGridSegment& B)
        { return A.bBoundary && !B.bBoundary; });
    TArray<FBatchedLine> Lines;
    const int32 SafeLineBudget = FMath::Clamp(LineBudget, 2, FFMapGridModel::MaximumRenderedLines);
    Lines.Reserve(FMath::Min(Segments.Num() * 2, SafeLineBudget));
    const FVector2D Center = VisibleMask.GetMinLocalCm() +
        FVector2D(VisibleMask.GetNumX(), VisibleMask.GetNumY()) * VisibleMask.GetStepCm() * 0.5;
    const FVector2D Extent = FVector2D(VisibleMask.GetNumX(), VisibleMask.GetNumY()) * VisibleMask.GetStepCm() * 0.5;
    const double WorldStep = GetWorldStepCm();
    const double MajorInterval = FMath::Max(500.0, WorldStep * 4.0);
    const double Lift = FMath::Clamp(GetDefault<UFFZSDeveloperSettings>()->PlacementGridSurfaceLiftCm, 2.5f, 12.0f);
    const auto ToWorld = [&](const FVector2D& Point) { return SurfaceCache.GetWorldPoint(Point) + FVector::UpVector * Lift; };
    for (const FFFZSBuildableGridSegment& Segment : Segments)
    {
        const FVector AStyle = PresentationTransform.TransformPosition(FVector(Segment.StartLocalCm, 0.0));
        const FVector BStyle = PresentationTransform.TransformPosition(FVector(Segment.EndLocalCm, 0.0));
        const bool bMajor = IsMajorCoordinate(FMath::IsNearlyEqual(AStyle.X, BStyle.X) ? AStyle.X : AStyle.Y, MajorInterval);
        const float Thickness = Segment.bBoundary ? 2.4f : bMajor ? 1.8f : 1.2f;
        const FLinearColor Stroke = Segment.bBoundary ? FLinearColor(0.30f, 1.0f, 0.94f, 0.92f) :
            bMajor ? FLinearColor(0.22f, 0.94f, 1.0f, 0.80f) : FLinearColor(0.30f, 0.85f, 1.0f, 0.58f);
        const int32 SectionCount = FMath::Max(1, FMath::RoundToInt(
            FVector2D::Distance(Segment.StartLocalCm, Segment.EndLocalCm) / VisibleMask.GetStepCm()));
        FVector RunStart = ToWorld(Segment.StartLocalCm), RunEnd = RunStart, Direction = FVector::ZeroVector;
        int32 RunBand = -1;
        const auto AppendRun = [&](const FVector& A, const FVector& B, const int32 Band)
        {
            if (Band < 0 || A.Equals(B, 0.01) || Lines.Num() + 2 > SafeLineBudget) return;
            const float Opacity = Band / 8.0f;
            Lines.Emplace(A - FVector::UpVector * 0.15, B - FVector::UpVector * 0.15,
                FLinearColor(0.005f, 0.015f, 0.025f, 0.32f * Opacity), 0.0f, Thickness + 1.5f, SDPG_World);
            FLinearColor Color = Stroke;
            Color.A *= Opacity;
            Lines.Emplace(A, B, Color, 0.0f, Thickness, SDPG_World);
        };
        for (int32 Index = 0; Index < SectionCount && Lines.Num() + 2 <= SafeLineBudget; ++Index)
        {
            const FVector2D A = FMath::Lerp(Segment.StartLocalCm, Segment.EndLocalCm, double(Index) / SectionCount);
            const FVector2D B = FMath::Lerp(Segment.StartLocalCm, Segment.EndLocalCm, double(Index + 1) / SectionCount);
            const int32 Band = FMath::RoundToInt(FFMapGridModel::EdgeOpacity((A + B) * 0.5, Center, Extent, bWholeMap) * 8.0f);
            const FVector WorldA = ToWorld(A), WorldB = ToWorld(B);
            const FVector NewDirection = (WorldB - WorldA).GetSafeNormal();
            if (RunBand != Band || !Direction.Equals(NewDirection, 0.0001))
            {
                AppendRun(RunStart, RunEnd, RunBand);
                RunStart = WorldA;
                RunBand = Band;
                Direction = NewDirection;
            }
            RunEnd = WorldB;
        }
        AppendRun(RunStart, RunEnd, RunBand);
        if (Lines.Num() + 2 > SafeLineBudget) break;
    }
    bool bLinesChanged = Lines.Num() != BatchedLines.Num();
    for (int32 Index = 0; !bLinesChanged && Index < Lines.Num(); ++Index)
    {
        const FBatchedLine& A = Lines[Index];
        const FBatchedLine& B = BatchedLines[Index];
        bLinesChanged = !A.Start.Equals(B.Start, 0.01) || !A.End.Equals(B.End, 0.01) ||
            !A.Color.Equals(B.Color) || !FMath::IsNearlyEqual(A.Thickness, B.Thickness);
    }
    if (bLinesChanged)
    {
        BatchedLines = MoveTemp(Lines);
        UpdateBounds();
        MarkRenderStateDirty();
        ++PresentationRevision;
    }
    SetHiddenInGame(BatchedLines.IsEmpty());
    return bRebuilt;
}
