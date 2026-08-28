#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"

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

    /**
     * Rewrites the surrounding layout for a new footprint size so every authored element keeps its
     * distance to the facility edge it was placed against. Reserved rectangles use the lower-left
     * grid frame while queue points and anchors use the centered frame, so the same size change
     * needs a different correction on each. Fails without a usable result when the facility still
     * carries legacy QueueReservedCells, when either size is not positive, or when a corrected
     * coordinate would leave the representable range. The caller must still validate the result.
     */
    static bool TryResizeFootprintKeepingLayout(
        const FFFZSFacilitySpec& Spec,
        FIntPoint NewFootprintSizeCells,
        FFFZSFacilitySpec& OutSpec);

    /**
     * Rebuilds the queue reservation as a corridor of axis-aligned rectangles that follow the
     * authored path segment by segment, so a bent queue reserves only the area it walks through
     * instead of one large bounding box. Only the part of the path that the maximum capacity
     * actually uses is covered, each segment is widened by MarginCm, and a corridor that runs
     * alongside the building is clamped to the footprint edge so it never overlaps the body.
     * Fails without a usable result when the inputs cannot produce a covering corridor, when a
     * coordinate leaves the representable range, or when the corridor needs more rectangles than
     * the per-facility safety limit allows. The caller must still validate the resulting Spec.
     */
    static bool TryBuildQueueRectsFromPath(
        const FFFZSFacilitySpec& Spec,
        int32 MarginCm,
        TArray<FFFZSCellRect>& OutRects);

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
