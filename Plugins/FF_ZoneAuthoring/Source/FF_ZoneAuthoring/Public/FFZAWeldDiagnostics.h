// Read-only post-weld inspection of the caller's selected rings. No world, actors, or assets.
#pragma once

#include "CoreMinimal.h"

namespace FFZAWeldDiagnostics
{
    enum class EIssueType : uint8
    {
        Gap,
        Overlap,
        Rejected
    };

    struct FF_ZONEAUTHORING_API FBoundaryIssue
    {
        EIssueType Type = EIssueType::Gap;
        // Original caller indices. A rejection has no partner (PolygonIndexB == INDEX_NONE).
        int32 PolygonIndexA = INDEX_NONE;
        int32 PolygonIndexB = INDEX_NONE;
        FVector2D StartA = FVector2D::ZeroVector;
        FVector2D EndA = FVector2D::ZeroVector;
        FVector2D StartB = FVector2D::ZeroVector;
        FVector2D EndB = FVector2D::ZeroVector;
        double MaximumGapCm = 0.0;
        // Total intersection area of this pair, repeated on its perimeter spans; do not sum spans.
        double OverlapAreaCm2 = 0.0;
        bool bRelatedToRejection = false;
    };

    struct FF_ZONEAUTHORING_API FResult
    {
        TArray<FBoundaryIssue> Issues;
        bool bTruncated = false;
        // Counts are complete within this input/range, even when the displayed spans are capped.
        int32 GapPairCount = 0;
        int32 OverlapPairCount = 0;
        int32 RejectedPolygonCount = 0;
        double GapInspectionCm = 0.0;
    };

    // Finds facing boundary spans within the weld kernel's 45-degree angle limit, with a nonzero longitudinal extent and gaps
    // within GapInspectionCm. Corner-only proximity and exact shared edges are not gaps.
    // Strip portions occupied by selected rings (including their own concave arms) are excluded;
    // remaining narrower gaps are inspected against the intervening ring's own boundaries.
    // Positive-area intersections are measured by cached ring triangulation and triangle clipping.
    // Overlap spans are actual intersection perimeter pieces; both A/B endpoints hold the same
    // piece for drawing once. Rejected spans identify the rejected ring, not an offending subspan.
    // Invalid/nonfinite rings are excluded from pair inspection. This is a bounded, selected-scope
    // diagnostic, never proof that an entire map or gaps beyond the inspection range are covered.
    FF_ZONEAUTHORING_API void Analyze(const TArray<TArray<FVector2D>>& ResultPolys,
        const TArray<int32>& RejectedIndices, double GapInspectionCm, FResult& OutResult,
        int32 MaxIssues = 512);
}
