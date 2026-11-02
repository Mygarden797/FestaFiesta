// FF_FFZSBuildRangePresentation.h
// 역할: 시설 본체·여유 공간·대기열·배치 실패 영역의 선을 공통 규칙으로 그린다.
#pragma once

#include "CoreMinimal.h"
#include "Components/LineBatchComponent.h"
#include "Containers/StaticArray.h"
#include "FF_FFZSTypes.h"

class UFFZSBuildPresentationSettings;
struct FFFZSBuildAreaLineSegment;

// Takes already-resolved geometry. No world queries, authority writes or alternative queue formula.
namespace FFZSBuildRangePresentation
{
    enum class ERegion : uint8 { Footprint, Clearance, Queue, Audience };
    using FWorldCorners = TStaticArray<FVector, 4>;

    bool HasRegionFailure(const FFFZSPlacementValidation& Validation, ERegion Region);
    bool IsFailureForRegion(EFFZSPlacementFailure Reason, ERegion Region);
    EFFZSPlacementFailure ResolveDisplayedFailureReason(const FFFZSPlacementValidation& Validation,
        EFFZSPlacementFailure RequestedReason);
    // Each authored reservation retains its own identity; audience space is never safety clearance.
    void AppendPlacementRanges(TArray<FBatchedLine>& Lines, const FFFZSPlacementValidation& Validation,
        const FTransform& ZoneTransform, double SurfaceZ, const UFFZSBuildPresentationSettings& Style);
    void BuildFailureHatchSegments(const FFFZSOrientedRect& WorldRegion,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments);
    // Small enclosed holes can lie between regular hatch strokes. Seed bounded extra strokes
    // at true boundary fragments, then let the authority clip them to unbuildable land.
    void AppendBoundaryHatchSeeds(const FFFZSOrientedRect& WorldRegion,
        const TArray<FFFZSBuildAreaLineSegment>& BoundarySegments,
        TArray<FFFZSBuildAreaLineSegment>& InOutSegments);
    void AppendPlanarSegments(TArray<FBatchedLine>& Lines, const TArray<FFFZSBuildAreaLineSegment>& Segments,
        double SurfaceZ, const FLinearColor& Color, float Thickness);
    int32 ResolveMinorLineStride(float ProjectedStepPixels, float MinimumSpacingPixels);
    void AppendOutline(TArray<FBatchedLine>& Lines, const FWorldCorners& Corners,
        const FLinearColor& Color, bool bDashed, bool bAccentCorners,
        const UFFZSBuildPresentationSettings& Style, bool bForeground = false);
    // Follow the authored polyline, with an arrow toward its head. Never join across a bend.
    void AppendQueuePath(TArray<FBatchedLine>& Lines, const FFFZSFacilitySpec& Spec,
        const FTransform& FacilityTransform, const FLinearColor& Color,
        const UFFZSBuildPresentationSettings& Style, bool bForeground = false);
    void AppendQueueSlots(TArray<FBatchedLine>& Lines, const FFFZSFacilitySpec& Spec,
        const FTransform& FacilityTransform, const FLinearColor& Color,
        const UFFZSBuildPresentationSettings& Style, bool bForeground = false);
    // Bounded, depth-independent diagonals identify failure even without color perception.
    void AppendFailureHatching(TArray<FBatchedLine>& Lines, const FWorldCorners& Corners,
        const FLinearColor& Color);
    // Project diagnostic collision bounds, not mesh silhouettes or an exact contact manifold.
    // Large blockers are clipped to the affected reservation's nearby area for readability.
    bool BuildObstructionFootprints(const FFFZSPlacementObstruction& Obstruction,
        TArray<FVector2D>& OutNearbyBounds, TArray<FVector2D>& OutOverlap);
    bool AppendObstructionFeedback(TArray<FBatchedLine>& Lines,
        const FFFZSPlacementObstruction& Obstruction, double SurfaceZ,
        const FLinearColor& BoundsColor, const FLinearColor& OverlapColor,
        FFFZSOrientedRect* OutRectangularOverlap = nullptr);
}
