#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "Containers/StaticArray.h"
#include "FF_FFZSTypes.h"

class UFFZSBuildPresentationSettings;

// Takes already-resolved geometry. No world queries, authority writes or alternative queue formula.
namespace FFZSBuildRangePresentation
{
    enum class ERegion : uint8 { Footprint, Clearance, Queue };
    using FWorldCorners = TStaticArray<FVector, 4>;

    bool HasRegionFailure(const FFFZSPlacementValidation& Validation, ERegion Region);
    int32 ResolveMinorLineStride(float ProjectedStepPixels, float MinimumSpacingPixels);
    void AppendOutline(TArray<FBatchedLine>& Lines, const FWorldCorners& Corners,
        const FLinearColor& Color, bool bDashed, bool bAccentCorners,
        const UFFZSBuildPresentationSettings& Style);
    void AppendQueueSlots(TArray<FBatchedLine>& Lines, const FFFZSFacilitySpec& Spec,
        const FTransform& FacilityTransform, const FLinearColor& Color,
        const UFFZSBuildPresentationSettings& Style);
}
