#pragma once

#include "CoreMinimal.h"

class APlayerController;
class UWorld;

/** Camera presentation only. No camera settings, native rig state or placement authority is changed. */
namespace FFMapCamera
{
    FF_MAPOVERVIEW_API bool TryGetMapBounds(UWorld* World, FBox& OutBounds);
    FF_MAPOVERVIEW_API bool ShowWholeMap(APlayerController* Controller);
    FF_MAPOVERVIEW_API bool RestoreCamera(APlayerController* Controller);
    FF_MAPOVERVIEW_API bool IsWholeMapView(const APlayerController* Controller);
    FF_MAPOVERVIEW_API bool FocusMapPoint(APlayerController* Controller, const FVector& WorldPoint);
    FF_MAPOVERVIEW_API bool RefreshWholeMapBounds(APlayerController* Controller, const FBox& Bounds);

    /**
     * Fits every corner in perspective, including terrain height and the near side of a tilted map.
     * SafeNdcBounds uses camera NDC: X right, Y up, [-1,1] is the entire viewport.
     * The asymmetric safe rectangle reserves space for the minimap and construction controls.
     */
    FF_MAPOVERVIEW_API bool ComputeWholeMapView(const FBox& MapBounds, const FRotator& Rotation,
        double HorizontalFovDegrees, double AspectRatio, const FBox2D& SafeNdcBounds,
        FTransform& OutCameraTransform);
}
