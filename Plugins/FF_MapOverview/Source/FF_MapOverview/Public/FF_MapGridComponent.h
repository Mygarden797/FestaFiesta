#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "FF_FFZSBuildGridSurface.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSTypes.h"
#include "FF_MapGridComponent.generated.h"

class UFFZSWorldSubsystem;

UCLASS(Transient)
class FF_MAPOVERVIEW_API UFFMapGridComponent final : public ULineBatchComponent
{
    GENERATED_BODY()
public:
    UFFMapGridComponent();
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

    // Returns true only when this call consumed its one authority-mask rebuild slot.
    bool RefreshGrid(UFFZSWorldSubsystem& WorldSystem, const FFFZSCommand& Command,
        const FBox2D& LocalBounds, int32 RequestedStepCm, int32 DesiredWorldStepCm,
        int32 CellBudget, int32 LineBudget, const FVector& SurfacePointWorld, bool bWholeMap,
        int32 SurfaceSampleBudget, bool bPermitAuthorityRefresh);
    void HideGrid();
    bool HasPendingSamples() const { return bNeedsAuthorityRefresh || SurfaceCache.HasPendingSamples(); }
    int32 GetCellCount() const { return SpatialMask.GetCellCount(); }
    int32 GetLineCount() const { return BatchedLines.Num(); }
    double GetWorldStepCm() const { return SpatialMask.GetStepCm() * PresentationTransform.GetScale3D().X; }
    uint64 GetPresentationRevision() const { return PresentationRevision; }
    uint64 GetSurfaceUpdateCount() const { return SurfaceUpdateCount; }
    int32 GetLastTraceAllowance() const { return LastTraceAllowance; }

private:
    FFFZSBuildableGridMask SpatialMask;
    FFFZSBuildGridSurfaceCache SurfaceCache;
    FTransform PresentationTransform = FTransform::Identity;
    uint32 CachedContextHash = 0;
    int64 CachedWorldRevision = -1;
    uint64 CachedRegistryRevision = 0;
    double LastAuthorityRefreshSeconds = -1.0;
    double LastSurfaceRefreshSeconds = -1.0;
    uint64 PresentationRevision = 0;
    uint64 SurfaceUpdateCount = 0;
    int32 LastTraceAllowance = 0;
    bool bCacheValid = false;
    bool bNeedsAuthorityRefresh = true;
};
