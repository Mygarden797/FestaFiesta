#include "FF_FFZSBuildGridSurface.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "HAL/PlatformTime.h"

namespace
{
    constexpr int32 MaximumTracesPerUpdate = 512;
    constexpr double PendingUpdateIntervalSeconds = 0.08;

    double ResolveSamplePhaseSeconds(const FIntPoint& Key)
    {
        return static_cast<double>(GetTypeHash(Key) % 1009) / 1008.0 * 0.5;
    }

    double SafePositive(const double Value, const double Fallback)
    {
        return FMath::IsFinite(Value) && Value > 0.0 ? Value : Fallback;
    }

    struct FPendingSurfaceSample
    {
        FIntPoint Key = FIntPoint::ZeroValue;
        double DistanceSquared = 0.0;
        double ExpiresAtSeconds = 0.0;
        bool bMissing = false;
    };
}

double FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(
    const int32 RequiredVertexCount, const int32 SamplesPerUpdate)
{
    const double PassSeconds = FMath::CeilToDouble(static_cast<double>(FMath::Max(0, RequiredVertexCount)) /
        FMath::Clamp(SamplesPerUpdate, 1, MaximumTracesPerUpdate)) * PendingUpdateIntervalSeconds;
    return FMath::Max(2.0, PassSeconds * 2.0 + 0.5);
}

double FFZSBuildGridSurface::CalculateRefreshLeadSeconds(
    const int32 RequiredVertexCount, const int32 SamplesPerUpdate)
{
    const double PassSeconds = FMath::CeilToDouble(static_cast<double>(FMath::Max(0, RequiredVertexCount)) /
        FMath::Clamp(SamplesPerUpdate, 1, MaximumTracesPerUpdate)) * PendingUpdateIntervalSeconds;
    return PassSeconds + 0.5;
}

void FFFZSBuildGridSurfaceCache::Reset()
{
    Samples.Reset();
    CachedWorld.Reset();
    CachedTransform = FTransform::Identity;
    CachedReferenceWorldZ = 0.0;
    CachedStepCm = 100.0;
    LastUpdateSeconds = 0.0;
    CachedSettingsHash = 0;
    CachedTraceBudget = MaximumTracesPerUpdate;
    bCacheValid = false;
    bGroundChecksEnabled = true;
    bPendingSamples = false;
    GroundQueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(FFBuildGridSurface), false);
    LastIgnoreRefreshSeconds = -1.0;
    CachedWorldRevision = -1;
}

bool FFFZSBuildGridSurfaceCache::Update(UWorld& World, const FTransform& ZoneTransform,
    const double ReferenceWorldZ, FFFZSBuildableGridMask& InOutMask, const int32 MaxNewSamples)
{
    if (ZoneTransform.ContainsNaN() || !FMath::IsFinite(ReferenceWorldZ) ||
        !FMath::IsFinite(InOutMask.GetStepCm()) || InOutMask.GetStepCm() <= 0.0 ||
        InOutMask.GetCellCount() <= 0)
    {
        const bool bChanged = bCacheValid || InOutMask.GetCellCount() > 0;
        Reset();
        InOutMask.Reset();
        return bChanged;
    }

    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const double TraceAboveCm = SafePositive(Settings->GroundTraceAboveCm, 1000.0);
    const double TraceBelowCm = SafePositive(Settings->GroundTraceBelowCm, 1000.0);
    const double MaxSlopeDegrees = FMath::IsFinite(Settings->MaxGroundSlopeDegrees)
        ? FMath::Clamp(static_cast<double>(Settings->MaxGroundSlopeDegrees), 0.0, 89.9) : 5.0;
    const double MaxHeightVarianceCm = FMath::IsFinite(Settings->MaxGroundHeightVarianceCm)
        ? FMath::Max(0.0, static_cast<double>(Settings->MaxGroundHeightVarianceCm)) : 20.0;
    const double MinimumUpDot = FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees));
    uint32 SettingsHash = HashCombine(GetTypeHash(TraceAboveCm), GetTypeHash(TraceBelowCm));
    SettingsHash = HashCombine(SettingsHash, GetTypeHash(MaxSlopeDegrees));
    SettingsHash = HashCombine(SettingsHash, GetTypeHash(MaxHeightVarianceCm));
    SettingsHash = HashCombine(SettingsHash, GetTypeHash(Settings->bEnableGroundChecks));
    SettingsHash = HashCombine(SettingsHash, GetTypeHash(Settings->bRequireEveryGroundSample));
    SettingsHash = HashCombine(SettingsHash, GetTypeHash(InOutMask.bIncludeRoadSupport));

    // Ground traces start from the same transformed zone-local zero plane as ValidateGround.
    // Cursor/preview height cannot change which floor lies inside that authoritative trace band.
    bool bChanged = !bCacheValid || CachedWorld.Get() != &World ||
        !CachedTransform.Equals(ZoneTransform, 0.001) ||
        !FMath::IsNearlyEqual(CachedStepCm, InOutMask.GetStepCm(), 0.001) ||
        CachedSettingsHash != SettingsHash;
    if (bChanged)
    {
        Reset();
        CachedWorld = &World;
        CachedTransform = ZoneTransform;
        CachedReferenceWorldZ = ReferenceWorldZ;
        CachedStepCm = InOutMask.GetStepCm();
        CachedSettingsHash = SettingsHash;
        bGroundChecksEnabled = Settings->bEnableGroundChecks;
        bCacheValid = true;
    }
    LastUpdateSeconds = FPlatformTime::Seconds();
    bPendingSamples = false;
    if (MaxNewSamples > 0)
    {
        CachedTraceBudget = FMath::Clamp(MaxNewSamples, 1, MaximumTracesPerUpdate);
    }
    if (!bGroundChecksEnabled)
    {
        // Match the existing explicit authoring/debug ground-check toggle. No physical query
        // is needed; the caller's reference height supplies the planar presentation.
        bChanged |= !FMath::IsNearlyEqual(CachedReferenceWorldZ, ReferenceWorldZ, 0.01);
        CachedReferenceWorldZ = ReferenceWorldZ;
        return bChanged;
    }

    const FIntPoint FirstVertex(
        FMath::RoundToInt(InOutMask.GetMinLocalCm().X / CachedStepCm),
        FMath::RoundToInt(InOutMask.GetMinLocalCm().Y / CachedStepCm));
    const FVector2D CenterKey(FirstVertex.X + InOutMask.GetNumX() * 0.5,
        FirstVertex.Y + InOutMask.GetNumY() * 0.5);
    TSet<FIntPoint> RequiredVertices;
    RequiredVertices.Reserve(FMath::Min(InOutMask.GetBuildableCellCount() * 4,
        (InOutMask.GetNumX() + 1) * (InOutMask.GetNumY() + 1)));
    for (int32 Y = 0; Y < InOutMask.GetNumY(); ++Y)
    {
        for (int32 X = 0; X < InOutMask.GetNumX(); ++X)
        {
            if (!InOutMask.IsBuildable(X, Y))
            {
                continue;
            }
            const FIntPoint Key = FirstVertex + FIntPoint(X, Y);
            RequiredVertices.Add(Key);
            RequiredVertices.Add(Key + FIntPoint(1, 0));
            RequiredVertices.Add(Key + FIntPoint(0, 1));
            RequiredVertices.Add(Key + FIntPoint(1, 1));
        }
    }
    // The input mask has a hard cell budget. Retain only its vertices so walking across an
    // arbitrarily large map cannot grow the terrain cache beyond one local window.
    for (auto It = Samples.CreateIterator(); It; ++It)
    {
        if (!RequiredVertices.Contains(It.Key()))
        {
            It.RemoveCurrent();
        }
    }

    // A fixed two-second lifetime cannot cover a large 16K-cell window under a 512-trace
    // update budget. Allow two complete passes, and renew a full pass before hard expiry.
    // Recompute from each sample's original time so a growing window preserves valid work.
    const double SampleLifetimeSeconds = FFZSBuildGridSurface::CalculateSampleLifetimeSeconds(
        RequiredVertices.Num(), CachedTraceBudget);
    const double RefreshLeadSeconds = FFZSBuildGridSurface::CalculateRefreshLeadSeconds(
        RequiredVertices.Num(), CachedTraceBudget);
    for (TPair<FIntPoint, FSample>& Pair : Samples)
    {
        if (Pair.Value.ExpiresAtSeconds > LastUpdateSeconds)
        {
            // Increasing the budgeted lifetime may preserve a still-fresh sample, but it
            // must never resurrect a floor whose previous hard expiration was already hit.
            Pair.Value.ExpiresAtSeconds = Pair.Value.SampledAtSeconds + SampleLifetimeSeconds +
                ResolveSamplePhaseSeconds(Pair.Key);
        }
    }

    TArray<FPendingSurfaceSample> Work;
    Work.Reserve(RequiredVertices.Num());
    for (const FIntPoint& Key : RequiredVertices)
    {
        const FSample* Existing = Samples.Find(Key);
        if (!Existing || Existing->ExpiresAtSeconds - RefreshLeadSeconds <= LastUpdateSeconds)
        {
            FPendingSurfaceSample& Pending = Work.AddDefaulted_GetRef();
            Pending.Key = Key;
            Pending.bMissing = Existing == nullptr;
            Pending.ExpiresAtSeconds = Existing ? Existing->ExpiresAtSeconds : 0.0;
            Pending.DistanceSquared = FVector2D::DistSquared(FVector2D(Key.X, Key.Y), CenterKey);
        }
    }
    Work.Sort([](const FPendingSurfaceSample& A, const FPendingSurfaceSample& B)
    {
        if (A.bMissing != B.bMissing)
        {
            return A.bMissing;
        }
        if (!A.bMissing && A.ExpiresAtSeconds != B.ExpiresAtSeconds)
        {
            return A.ExpiresAtSeconds < B.ExpiresAtSeconds;
        }
        if (A.DistanceSquared != B.DistanceSquared)
        {
            return A.DistanceSquared < B.DistanceSquared;
        }
        return A.Key.Y != B.Key.Y ? A.Key.Y < B.Key.Y : A.Key.X < B.Key.X;
    });

    const int32 TraceCount = FMath::Min(Work.Num(), FMath::Clamp(MaxNewSamples, 0, MaximumTracesPerUpdate));
    if (TraceCount > 0)
    {
        // Reservations were already removed by the authority mask. Their visible roofs,
        // selection boxes and preview meshes must never become an alternative "ground".
        const UFFZSWorldSubsystem* System = World.GetSubsystem<UFFZSWorldSubsystem>();
        const int64 Revision = System ? System->GetWorldRevision() : 0;
        if (LastIgnoreRefreshSeconds < 0.0 || CachedWorldRevision != Revision ||
            LastUpdateSeconds - LastIgnoreRefreshSeconds >= 0.5)
        {
            GroundQueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(FFBuildGridSurface), false);
            for (TActorIterator<AFFZSFacilityActor> It(&World); It; ++It)
            {
                if (!InOutMask.bIncludeRoadSupport || It->GetFacilitySpec().Category != EFFZSFacilityCategory::Road)
                {
                    GroundQueryParams.AddIgnoredActor(*It);
                }
            }
            LastIgnoreRefreshSeconds = LastUpdateSeconds;
            CachedWorldRevision = Revision;
        }
        for (int32 Index = 0; Index < TraceCount; ++Index)
        {
            const FIntPoint Key = Work[Index].Key;
            FVector Origin = CachedTransform.TransformPosition(FVector(
                Key.X * CachedStepCm, Key.Y * CachedStepCm, 0.0));
            FHitResult Hit;
            FSample Sample;
            Sample.bHit = World.LineTraceSingleByChannel(Hit,
                Origin + FVector::UpVector * TraceAboveCm,
                Origin - FVector::UpVector * TraceBelowCm, ECC_Visibility, GroundQueryParams) &&
                !Hit.bStartPenetrating && !Hit.ImpactPoint.ContainsNaN() &&
                !Hit.ImpactNormal.ContainsNaN() && !Hit.ImpactNormal.IsNearlyZero();
            if (Sample.bHit)
            {
                Sample.WorldPoint = Hit.ImpactPoint;
                Sample.UpDot = FVector::DotProduct(Hit.ImpactNormal.GetSafeNormal(), FVector::UpVector);
                Sample.SupportComponent = Hit.GetComponent();
                Sample.SupportInstance = Hit.Item;
            }
            // Vertex-specific phases spread renewal of a flat initial batch. Invalid hits
            // also renew, allowing repaired ground to become visible without cursor movement.
            Sample.SampledAtSeconds = LastUpdateSeconds;
            Sample.ExpiresAtSeconds = LastUpdateSeconds + SampleLifetimeSeconds + ResolveSamplePhaseSeconds(Key);
            Samples.Add(Key, Sample);
        }
        bChanged = true;
    }
    bPendingSamples = TraceCount < Work.Num();

    for (int32 Y = 0; Y < InOutMask.GetNumY(); ++Y)
    {
        for (int32 X = 0; X < InOutMask.GetNumX(); ++X)
        {
            if (!InOutMask.IsBuildable(X, Y))
            {
                continue;
            }
            const FIntPoint Key = FirstVertex + FIntPoint(X, Y);
            const FSample* Corners[] = { Samples.Find(Key), Samples.Find(Key + FIntPoint(1, 0)),
                Samples.Find(Key + FIntPoint(0, 1)), Samples.Find(Key + FIntPoint(1, 1)) };
            bool bValidGround = true;
            double MinimumHeight = TNumericLimits<double>::Max();
            double MaximumHeight = -TNumericLimits<double>::Max();
            for (const FSample* Corner : Corners)
            {
                // Even permissive whole-facility sampling must not invent missing visual
                // corners. All four known, fresh, real hits are required to draw this cell.
                if (!Corner || Corner->ExpiresAtSeconds <= LastUpdateSeconds || !Corner->bHit ||
                    Corner->UpDot + UE_SMALL_NUMBER < MinimumUpDot)
                {
                    bValidGround = false;
                    break;
                }
                MinimumHeight = FMath::Min(MinimumHeight, Corner->WorldPoint.Z);
                MaximumHeight = FMath::Max(MaximumHeight, Corner->WorldPoint.Z);
            }
            if (!bValidGround || MaximumHeight - MinimumHeight > MaxHeightVarianceCm + UE_SMALL_NUMBER)
            {
                InOutMask.SetBuildable(X, Y, false);
                bChanged = true;
            }
        }
    }
    // Rasterize only the cells inside each broad-phase obstacle's projected bounds. Height
    // comes from these cells' real ground samples, not the camera/cursor's unrelated floor.
    // Four samples on the same support surface exempt that mesh/instance, so a gently
    // sloping floor is not mistaken for a building merely because its world AABB is tall.
    const double CollisionHeight = FMath::Max(10.0, InOutMask.ObstacleHeightCm - 4.0);
    for (const FFFZSBuildGridObstacle& Obstacle : InOutMask.DeferredWorldObstacles)
    {
        for (int32 Y = Obstacle.FirstCell.Y; Y < Obstacle.EndCell.Y; ++Y)
        {
            for (int32 X = Obstacle.FirstCell.X; X < Obstacle.EndCell.X; ++X)
            {
                if (!InOutMask.IsBuildable(X, Y))
                {
                    continue;
                }
                const FIntPoint Key = FirstVertex + FIntPoint(X, Y);
                const FSample* Corners[] = { Samples.Find(Key), Samples.Find(Key + FIntPoint(1, 0)),
                    Samples.Find(Key + FIntPoint(0, 1)), Samples.Find(Key + FIntPoint(1, 1)) };
                bool bIsSupport = Obstacle.Component.IsValid();
                double GroundHeight = 0.0;
                for (const FSample* Corner : Corners)
                {
                    check(Corner && Corner->bHit); // Only verified cells survive the pass above.
                    GroundHeight += Corner->WorldPoint.Z * 0.25;
                    bIsSupport &= Corner->SupportComponent == Obstacle.Component &&
                        (Obstacle.InstanceIndex == INDEX_NONE || Corner->SupportInstance == Obstacle.InstanceIndex);
                }
                if (!bIsSupport && Obstacle.WorldBounds.Max.Z > GroundHeight + 3.0 &&
                    Obstacle.WorldBounds.Min.Z < GroundHeight + 3.0 + CollisionHeight)
                {
                    InOutMask.SetBuildable(X, Y, false);
                    bChanged = true;
                }
            }
        }
    }
    return bChanged;
}

FVector FFFZSBuildGridSurfaceCache::GetWorldPoint(const FVector2D& LocalCm) const
{
    if (LocalCm.ContainsNaN())
    {
        return CachedTransform.GetTranslation();
    }
    FVector Fallback = CachedTransform.TransformPosition(FVector(LocalCm.X, LocalCm.Y, 0.0));
    Fallback.Z = CachedReferenceWorldZ;
    if (!bCacheValid || !bGroundChecksEnabled)
    {
        return Fallback;
    }
    const FVector2D KeyPoint = LocalCm / CachedStepCm;
    const FIntPoint Rounded(FMath::RoundToInt(KeyPoint.X), FMath::RoundToInt(KeyPoint.Y));
    const auto FindGround = [this](const FIntPoint& Key) -> const FSample*
    {
        const FSample* Sample = Samples.Find(Key);
        return Sample && Sample->bHit && Sample->ExpiresAtSeconds > LastUpdateSeconds ? Sample : nullptr;
    };
    if (KeyPoint.Equals(FVector2D(Rounded.X, Rounded.Y), 0.0001))
    {
        const FSample* Sample = FindGround(Rounded);
        return Sample ? Sample->WorldPoint : Fallback;
    }
    // Optional edge interpolation supports renderer fade subdivisions without extra traces.
    if (FMath::IsNearlyEqual(KeyPoint.X, static_cast<double>(Rounded.X), 0.0001))
    {
        const int32 Y = FMath::FloorToInt(KeyPoint.Y);
        const FSample* A = FindGround(FIntPoint(Rounded.X, Y));
        const FSample* B = FindGround(FIntPoint(Rounded.X, Y + 1));
        return A && B ? FMath::Lerp(A->WorldPoint, B->WorldPoint, KeyPoint.Y - Y) : Fallback;
    }
    if (FMath::IsNearlyEqual(KeyPoint.Y, static_cast<double>(Rounded.Y), 0.0001))
    {
        const int32 X = FMath::FloorToInt(KeyPoint.X);
        const FSample* A = FindGround(FIntPoint(X, Rounded.Y));
        const FSample* B = FindGround(FIntPoint(X + 1, Rounded.Y));
        return A && B ? FMath::Lerp(A->WorldPoint, B->WorldPoint, KeyPoint.X - X) : Fallback;
    }
    return Fallback;
}
