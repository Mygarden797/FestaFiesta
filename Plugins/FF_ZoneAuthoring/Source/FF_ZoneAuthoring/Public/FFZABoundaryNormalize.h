// Coordinated numerical cleanup for generated boundaries; never reads or writes a world.
#pragma once

#include "CoreMinimal.h"

namespace FFZABoundaryNormalize
{
    // Nearby output nodes receive one exact XY across all Zones. Source height keys stay
    // local to each original Zone. Short adjacent output edges are removed atomically;
    // distant original corners, winding and a valid single ring remain mandatory.
    FF_ZONEAUTHORING_API bool Normalize(TArray<TArray<FVector2D>>& InOutPolygons,
        TArray<TArray<double>>& InOutSourcePointKeys,
        const TArray<TArray<FVector2D>>& AuthoredPolygons,
        double ToleranceCm, FString& OutError);
}
