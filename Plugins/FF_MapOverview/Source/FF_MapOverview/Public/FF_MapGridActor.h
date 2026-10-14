#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FF_MapGridActor.generated.h"

class UFFMapGridComponent;
class UFFZSWorldSubsystem;
struct FFFZSCommand;

/** Session-owned grid presenter. No authority writes, component ticks or cell actors. */
UCLASS(Transient, NotBlueprintable)
class FF_MAPOVERVIEW_API AFFMapGridActor final : public AActor
{
    GENERATED_BODY()
public:
    AFFMapGridActor();
    void RefreshGrid(UFFZSWorldSubsystem& WorldSystem, const FVector& SurfacePointWorld,
        const FVector& SurfaceNormalWorld, int32 GridStepCm, bool bWholeMap,
        const FFFZSCommand* PlacementContext = nullptr);
    void HideGrid();
    bool HasPendingSamples() const;
    int32 GetRetainedCellCount() const;
    int32 GetRenderedLineCount() const;
    int32 GetLastTraceBudget() const { return LastTraceBudget; }
    int32 GetLastAuthorityRebuildCount() const { return LastAuthorityRebuildCount; }
    double GetOverviewSpacingCm() const;
    // LOD diagnostics before any authority-cell-budget coarsening.
    int32 GetZoomStepCm() const { return PreviousZoomStepCm; }
    double GetZoomSpacingPixels() const
    {
        return LastBaseStepCm > 0 ? LastProjectedBaseSpacingPixels * PreviousZoomStepCm / LastBaseStepCm : 0.0;
    }
    double GetProjectedBaseSpacingPixels() const { return LastProjectedBaseSpacingPixels; }
    int32 GetBaseStepCm() const { return LastBaseStepCm; }

private:
    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UFFMapGridComponent>> Grids;

    TMap<FName, FBox2D> CachedZoneBounds;
    double LastBoundsRefreshSeconds = -1.0;
    int64 BoundsWorldRevision = -1;
    FName UnifiedCoordinateZoneId = NAME_None;
    double LastProjectedBaseSpacingPixels = 0.0;
    int32 LastBaseStepCm = 100;
    int32 PreviousZoomStepCm = 0;
    int32 RoundRobinIndex = 0;
    int32 LastTraceBudget = 0;
    int32 LastAuthorityRebuildCount = 0;
};
