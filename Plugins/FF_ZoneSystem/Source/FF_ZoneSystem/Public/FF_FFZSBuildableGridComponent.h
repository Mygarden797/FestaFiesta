// FF_FFZSBuildableGridComponent.h
// 역할: 건설 가능 격자를 렌더링하고 월드·설정 변경 때만 표시 캐시를 갱신한다.
#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "FF_FFZSTypes.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSBuildGridSurface.h"
#include "FF_FFZSBuildableGridComponent.generated.h"

class UFFZSWorldSubsystem;

// Shared by the catalog overview and the placement ghost. One persistent scene proxy per zone,
// no actors per cell, no authority commands, and no work while the build session is hidden.
UCLASS(Transient)
class FF_ZONESYSTEM_API UFFZSBuildableGridComponent final : public ULineBatchComponent
{
    GENERATED_BODY()

public:
    UFFZSBuildableGridComponent();
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

    void RefreshGrid(UFFZSWorldSubsystem& WorldSystem, const FFFZSCommand& Command,
        const FVector2D& CenterLocalCm, const FVector2D& HalfExtentCm, int32 GridStepCm,
        const FVector& SurfacePointWorld, const FVector& SurfaceNormalWorld,
        int32 SurfaceSampleBudget = FFZSBuildGridSurface::MaximumTracesPerUpdate,
        FName SelectionCoordinateZoneId = NAME_None);
    void HideGrid();
    void InvalidateGrid() { bCacheValid = false; }
    // Changes only when visible line geometry changes, not when background checks run.
    uint64 GetPresentationRevision() const { return PresentationRevision; }

private:
    bool bCacheValid = false;
    uint32 CachedContextHash = 0;
    uint32 CachedPresentationHash = 0;
    int64 CachedRevision = -1;
    uint64 CachedRegistryRevision = 0;
    uint64 PresentationRevision = 0;
    double LastRefreshTimeSeconds = -1.0;
    FVector2D CachedMin = FVector2D::ZeroVector;
    FVector2D CachedMax = FVector2D::ZeroVector;
    FVector CachedSurfaceNormal = FVector::UpVector;
    double CachedPlaneDistance = 0.0;
    FTransform CachedZoneTransform = FTransform::Identity;
    FFFZSBuildableGridMask CachedSpatialMask;
    FFFZSBuildGridSurfaceCache SurfaceCache;
};
