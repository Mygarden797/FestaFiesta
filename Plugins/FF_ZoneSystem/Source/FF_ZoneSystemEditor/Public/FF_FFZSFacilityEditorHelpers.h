#pragma once

#include "CoreMinimal.h"

/**
 * Side-effect-free calculations shared by the Facility Definition Details tools.
 * All distances use Unreal centimeters. Keeping these operations separate makes
 * the authoring behavior testable without constructing editor widgets or assets.
 */
struct FF_ZONESYSTEMEDITOR_API FFFZSFacilityEditorHelpers
{
    /** Converts a mesh's local bounds size to conservative 1 cm footprint cells. */
    static bool TryCalculateFootprintCells(
        const FVector& MeshBoundsSizeCm,
        FIntPoint& OutFootprintSizeCells);

    /**
     * Calculates a centered footprint that contains a SourceMeshSize visual after its authored
     * rotation and offset are applied. Offset matters because facility footprints are centered on
     * the facility origin; this therefore uses twice the greatest absolute X/Y bound.
     */
    static bool TryCalculateFootprintCellsFromSourceBounds(
        const FBox& SourceBounds,
        const FRotator& VisualRotation,
        const FVector& VisualOffsetCm,
        FIntPoint& OutFootprintSizeCells);

    /** Returns the path length required to place Capacity slots including slot zero at the head. */
    static bool TryCalculateRequiredQueueLengthCm(
        int32 Capacity,
        double SlotSpacingCm,
        double& OutRequiredLengthCm);

    /** Calculates how many evenly-spaced slots fit on the complete piecewise-linear path. */
    static bool TryCalculateMaxWaitingAgents(
        const TArray<FVector>& QueuePointsCm,
        double SlotSpacingCm,
        int32& OutMaxWaitingAgents);

    /**
     * Truncates or extends the path tail so its total length equals RequiredLengthCm.
     * Existing bends before the new tail are retained; extension follows the final
     * non-degenerate segment. At least two output points are always preserved.
     */
    static bool TryResizeQueueTail(
        const TArray<FVector>& QueuePointsCm,
        double RequiredLengthCm,
        TArray<FVector>& OutQueuePointsCm);
};
