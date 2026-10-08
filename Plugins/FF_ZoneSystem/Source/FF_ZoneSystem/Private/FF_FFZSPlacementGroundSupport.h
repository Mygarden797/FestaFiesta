// Transient evidence shared by terrain validation and collision validation. Never saved.
#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "LandscapeHeightfieldCollisionComponent.h"

struct FFFZSPlacementGroundSupport
{
    TMap<TPair<const UPrimitiveComponent*, int32>, int32> SampleCounts;
    int32 NumSamples = 0;
    double MinHeight = DBL_MAX;
    double MaxHeight = -DBL_MAX;
    double MaximumSlopeDegrees = 0.0;
    double UnsampledRiseCm = 0.0;
    bool bValidated = false;

    bool Contains(const UPrimitiveComponent* Component, const int32 InstanceIndex) const
    {
        if (!Component) { return false; }
        if (Component->IsA<ULandscapeHeightfieldCollisionComponent>())
        {
            // Adjacent collision sections of the same landscape are one terrain surface.
            for (const auto& Pair : SampleCounts)
            {
                if (Pair.Key.Key && Pair.Key.Key->IsA<ULandscapeHeightfieldCollisionComponent>() &&
                    Pair.Key.Key->GetOwner() == Component->GetOwner())
                {
                    return true;
                }
            }
            return false;
        }
        if (Component->IsA<UInstancedStaticMeshComponent>() && InstanceIndex == INDEX_NONE) { return false; }
        const int32* Count = SampleCounts.Find(TPair<const UPrimitiveComponent*, int32>(Component, InstanceIndex));
        // A prop reached by only some rays is still an obstacle, even on the floor's actor.
        return Count && *Count == NumSamples;
    }
};
