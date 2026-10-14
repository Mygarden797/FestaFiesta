#pragma once

#include "CoreMinimal.h"
#include "FFZALayoutOps.h"

// Pure selected-ring ownership repair. No actors, levels, or asset state are accessed.
namespace FFZAOverlapRepair
{
    // The preferred reference owns its entire shape; other groups' references then own spans
    // before their members. Later rings lose only the positive-area overlap with earlier owners.
    // Each Zone must remain one nonempty simple ring without holes. Newly cut points carry
    // composed keys into AuthoredPolygons. A finite movement bound may be requested; use
    // TNumericLimits<double>::Max() for unrestricted removal of already overlapped area.
    // Failure is atomic for BOTH output arrays; OutError describes the unsupported topology.
    FF_ZONEAUTHORING_API bool Repair(TArray<TArray<FVector2D>>& InOutPolygons,
        int32 PreferredReferencePolygonIndex, const TArray<FFZALayoutOps::FWeldGroup>& Groups,
        TArray<TArray<double>>& InOutSourcePointKeys,
        const TArray<TArray<FVector2D>>& AuthoredPolygons, double MaxAuthoredMoveCm,
        FString& OutError);
}
