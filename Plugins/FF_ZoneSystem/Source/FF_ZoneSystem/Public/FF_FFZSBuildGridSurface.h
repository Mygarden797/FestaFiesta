#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"

class UWorld;
class UPrimitiveComponent;
struct FFFZSBuildableGridMask;

namespace FFZSBuildGridSurface
{
    // Budgets assume the owning presenter schedules pending work at least every 0.08 seconds.
    FF_ZONESYSTEM_API double CalculateSampleLifetimeSeconds(int32 RequiredVertexCount, int32 SamplesPerUpdate = 512);
    FF_ZONESYSTEM_API double CalculateRefreshLeadSeconds(int32 RequiredVertexCount, int32 SamplesPerUpdate = 512);
}

// Presentation-only terrain samples, shared by overview and placement grids. No actor, tick,
// physical cell allocation or placement mutation is owned here.
class FF_ZONESYSTEM_API FFFZSBuildGridSurfaceCache
{
public:
    // Pass a fresh authority mask: unknown/expired/invalid terrain cells are removed in place.
    // Returns whether the cache or visible mask changed. At most 512 collision traces per call;
    // callers schedule pending work, rather than calling this from a component tick. Physical
    // queries use the authority zone transform; ReferenceWorldZ only supplies the disabled-check plane.
    bool Update(UWorld& World, const FTransform& ZoneTransform, double ReferenceWorldZ,
        FFFZSBuildableGridMask& InOutMask, int32 MaxNewSamples = 512);

    // Raw ground world position. The renderer owns surface lift and must split merged runs at
    // lattice vertices. Between vertices, interpolation is available only when both ends exist.
    FVector GetWorldPoint(const FVector2D& LocalCm) const;
    bool HasPendingSamples() const { return bPendingSamples; }
    void Reset();

private:
    struct FSample
    {
        FVector WorldPoint = FVector::ZeroVector;
        double UpDot = -1.0;
        double SampledAtSeconds = 0.0;
        double ExpiresAtSeconds = 0.0;
        bool bHit = false;
        TWeakObjectPtr<UPrimitiveComponent> SupportComponent;
        int32 SupportInstance = INDEX_NONE;
    };

    TMap<FIntPoint, FSample> Samples;
    TWeakObjectPtr<UWorld> CachedWorld;
    FTransform CachedTransform = FTransform::Identity;
    double CachedReferenceWorldZ = 0.0;
    double CachedStepCm = 100.0;
    double LastUpdateSeconds = 0.0;
    uint32 CachedSettingsHash = 0;
    int32 CachedTraceBudget = 512;
    bool bCacheValid = false;
    bool bGroundChecksEnabled = true;
    bool bPendingSamples = false;
    FCollisionQueryParams GroundQueryParams;
    double LastIgnoreRefreshSeconds = -1.0;
    int64 CachedWorldRevision = -1;
};
