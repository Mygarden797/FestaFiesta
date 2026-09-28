// FF_FFZSBuildGridSurface.h
// 역할: 격자 아래 실제 지면을 제한된 횟수로 추적하고 결과를 재사용해 프레임 비용을 줄인다.
#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"

class UWorld;
class UPrimitiveComponent;
struct FFFZSBuildableGridMask;

namespace FFZSBuildGridSurface
{
    // 개별 격자와 여러 Zone 전체 격자가 나누어 쓰는 한 번의 지면 추적 예산이다.
    // 실제 설치 면적/정밀도는 바꾸지 않는다. 예산을 바꾸면 첫 화면 완성 시간만 달라진다.
    inline constexpr int32 MaximumTracesPerUpdate = 512;
    inline constexpr double PendingUpdateIntervalSeconds = 0.08;

    // Budgets assume the owning presenter schedules pending work at least every 0.08 seconds.
    FF_ZONESYSTEM_API double CalculateSampleLifetimeSeconds(int32 RequiredVertexCount, int32 SamplesPerUpdate = MaximumTracesPerUpdate);
    FF_ZONESYSTEM_API double CalculateRefreshLeadSeconds(int32 RequiredVertexCount, int32 SamplesPerUpdate = MaximumTracesPerUpdate);
}

// Presentation-only terrain samples, shared by overview and placement grids. No actor, tick,
// physical cell allocation or placement mutation is owned here.
class FF_ZONESYSTEM_API FFFZSBuildGridSurfaceCache
{
public:
    // Pass a fresh authority mask: unknown/invalid terrain cells are removed in place.
    // Confirmed samples remain visible during budgeted background verification; elapsed time
    // alone never hides a valid floor. Changed/removed collision support invalidates immediately.
    // Returns whether the cache or visible mask changed. At most 512 collision traces per call;
    // callers schedule pending work, rather than calling this from a component tick. Physical
    // queries use the authority zone transform; ReferenceWorldZ only supplies the disabled-check plane.
    bool Update(UWorld& World, const FTransform& ZoneTransform, double ReferenceWorldZ,
        FFFZSBuildableGridMask& InOutMask, int32 MaxNewSamples = FFZSBuildGridSurface::MaximumTracesPerUpdate);

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
        bool bSupportCurrent = false;
        uint32 SupportGeometryHash = 0;
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
    int32 CachedTraceBudget = FFZSBuildGridSurface::MaximumTracesPerUpdate;
    bool bCacheValid = false;
    bool bGroundChecksEnabled = true;
    bool bPendingSamples = false;
    FCollisionQueryParams GroundQueryParams;
    double LastIgnoreRefreshSeconds = -1.0;
    int64 CachedWorldRevision = -1;
};
