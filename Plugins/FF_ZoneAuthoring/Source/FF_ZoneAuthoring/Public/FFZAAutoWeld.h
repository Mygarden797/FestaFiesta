// Pure selected-scope repair planning. This API never accesses actors, levels, or assets.
#pragma once

#include "CoreMinimal.h"
#include "FFZALayoutOps.h"
#include "FFZAWeldDiagnostics.h"

namespace FFZAAutoWeld
{
    struct FF_ZONEAUTHORING_API FResult
    {
        TArray<TArray<FVector2D>> Polygons;
        TArray<bool> ChangedPolygons;
        FFZALayoutOps::FWeldStats Stats;
        FFZAWeldDiagnostics::FResult Diagnostics;
        bool bCanCommit = false;
        bool bExpandedRadiusUsed = false;
        bool bAreaFallbackUsed = false;
        double RepairRadiusCm = 0.0;
        FString FailureReason;
    };

    // Try the authored tolerance first. Remaining inspected seams are repaired from the ORIGINAL
    // rings with a fixed 2*tolerance radius, rather than advancing geometry on repeated button presses.
    // The chosen reference retains its boundary shape; other components choose geometric roots.
    // Safe overlap differences must remain a single, nonempty simple ring with original height keys.
    // bCanCommit is set only after whole-selection validation finds zero inspected gaps/overlaps,
    // zero invalid inputs, valid provenance, and bounded seam-extension displacement. Removing
    // existing overlap is not distance-limited; unsupported empty/hole/multipart cuts still fail.
    // False means invalid arguments/reference. A true result can contain a failed read-only proposal;
    // callers MUST gate all writes on bCanCommit and expose FailureReason when it is false.
    FF_ZONEAUTHORING_API bool BuildResult(const TArray<TArray<FVector2D>>& AuthoredPolygons,
        int32 PreferredReferencePolygonIndex, double ToleranceCm, FResult& OutResult);

    // Reference-free planning for every supplied Zone. Both sides of an exposed shared boundary
    // meet on one common seam, including multi-Zone junctions. The caller supplies the complete
    // fixed world snapshot; no actors are collected or changed by this pure function.
    // If moving shared corners cannot produce valid rings, rebuild the nearby empty gap areas
    // with polygon unions from the original snapshot. Final topology/provenance/residual gates
    // still apply; bAreaFallbackUsed identifies this alternate plan for the editor preview.
    FF_ZONEAUTHORING_API bool BuildAutomaticResult(const TArray<TArray<FVector2D>>& AuthoredPolygons,
        double ToleranceCm, FResult& OutResult);
}
