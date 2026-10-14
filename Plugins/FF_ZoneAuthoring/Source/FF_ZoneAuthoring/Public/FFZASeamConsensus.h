// Simultaneous shared-seam planning over plain selected/collected rings. No world or asset access.
#pragma once

#include "CoreMinimal.h"
#include "FFZALayoutOps.h"

namespace FFZASeamConsensus
{
    struct FF_ZONEAUTHORING_API FResult
    {
        TArray<TArray<FVector2D>> Polygons;
        TArray<TArray<double>> SourcePointKeys;
        TArray<FFZALayoutOps::FWeldGroup> Groups;
        TArray<int32> RejectedPolygonIndices;
        int32 PassCount = 0;
        bool bComplete = false;
        // Required work was skipped due to a hard processing budget. Callers must NEVER infer
        // success from a later short diagnostic after this flag, even when its pair counts are zero.
        bool bBlocked = false;
        FString FailureReason;
    };

    // Both sides of each exposed nearby gap move to the SAME consensus segment. Shared original
    // corners connect incident seams into multiway junctions; distinct AUTHORED corners of one
    // Zone never merge. Synthetic cuts on one authored edge can merge into a single T junction.
    // All keys refer to AuthoredPolygons, including closing-edge and subsequent-pass cuts.
    // The search radius and each point's TOTAL authored displacement are RepairRadiusCm.
    // Iteration starts from the previous valid result, with bounded work and no anchor/ID ordering.
    // bComplete means zero inspected gap pairs. Remaining positive-area overlap belongs to the
    // caller's Boolean stage. Groups have INDEX_NONE references because this solver has no root.
    // False denotes invalid input. A valid input can return true with bComplete=false and a
    // read-only last-valid proposal; callers must validate the complete pipeline before writes.
    // bBlocked denotes a hard budget failure and cannot be recovered by silently skipping seams.
    // Optional MovementOrigins are finite XY anchors aligned point-for-point with AuthoredPolygons.
    // Their geometry need not be a valid ring. They remain fixed through every internal pass and
    // bound movement independently of source/height provenance. Invalid or initially exceeded
    // origin constraints return false with the unchanged read-only input.
    FF_ZONEAUTHORING_API bool Build(const TArray<TArray<FVector2D>>& AuthoredPolygons,
        double RepairRadiusCm, FResult& OutResult,
        const TArray<TArray<FVector2D>>* MovementOrigins = nullptr);
}
