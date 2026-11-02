// Tile-only highest-support query. Ground samples establish which geometry is terrain;
// a footprint sweep finds its highest contact, including features between those samples.
#pragma once

#include "CoreMinimal.h"

class UWorld;
class UPrimitiveComponent;
struct FHitResult;
struct FCollisionQueryParams;
struct FFFZSGroundValidationSettings;
struct FFFZSOrientedRect;
struct FFFZSPlacementGroundSupport;

enum class EFFZSTileSupportResult : uint8
{
    Valid,
    MissingGround,
    Obstructed
};

namespace FFZSTileGroundPlacement
{
    bool IsAuthoredTerrain(const UPrimitiveComponent* Component);

    // One rolling row of samples is enough to detect unexplained steps on an untagged
    // floor. A wall merged into that floor must not become terrain just because a ray hits it.
    struct FGroundSample
    {
        FVector Point = FVector::ZeroVector;
        double Slope = 0.0;
        bool bAuthoredTerrain = false;

        bool IsContinuousWith(const FGroundSample& Other) const;
    };

    EFFZSTileSupportResult FindHighestSupport(UWorld& World, const FTransform& ZoneTransform,
        const TArray<FFFZSOrientedRect>& Footprint, const FFFZSGroundValidationSettings& Ground,
        const FCollisionQueryParams& Query, const FFFZSPlacementGroundSupport& Support,
        double& OutHeight, FHitResult& OutObstruction);
}
