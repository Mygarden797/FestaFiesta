// Topology-preserving fallback for exposed gaps. Operates only on caller-owned arrays.
#pragma once

#include "CoreMinimal.h"

namespace FFZAGapAreaRepair
{
    struct FF_ZONEAUTHORING_API FResult
    {
        TArray<TArray<FVector2D>> Polygons;
        // Original input ring index plus edge fraction, for each output point's own height.
        TArray<TArray<double>> SourcePointKeys;
        bool bComplete = false;
        bool bBlocked = false;
        FString FailureReason;
    };

    // Fills exposed facing-boundary strips by polygon union without moving original corners.
    // Balanced halves are tried atomically first, then either whole-strip owner in geometric
    // order. Each Zone remains one nonempty,
    // simple ring; uncovered holes/splits are reported rather than silently discarded.
    // Completion concerns gaps within RepairRadiusCm; callers inspect overlaps separately.
    FF_ZONEAUTHORING_API bool Build(const TArray<TArray<FVector2D>>& AuthoredPolygons,
        double RepairRadiusCm, FResult& OutResult);

    // Recheck the entire resulting perimeter after any coordinated cleanup. Endpoint-only
    // bounds do not detect long chords crossing a distant original concavity.
    FF_ZONEAUTHORING_API bool ValidateRepairBand(const TArray<TArray<FVector2D>>& AuthoredPolygons,
        const TArray<TArray<FVector2D>>& RepairedPolygons, double RepairRadiusCm, FString& OutError);
}
