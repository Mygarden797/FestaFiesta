#pragma once

#include "CoreMinimal.h"

struct FFFZSCellRect;

// Shared, side-effect-free geometry operations for the piecewise-linear queue path stored in
// FFFZSFacilitySpec. Distances and points use Unreal centimeters.
struct FF_ZONESYSTEM_API FFFZSQueueGeometry
{
    // Canonical segment predicate shared by length, sampling, runtime projection, and editor tools.
    static bool IsUsableLinearSegmentLengthCm(double SegmentLengthCm);

    // Returns the accumulated length up to the first non-finite point. When requested, the invalid
    // point index is returned so callers can preserve the existing validation behavior.
    static double GetLinearPolylineLengthCm(
        const TArray<FVector>& Points,
        int32* OutFirstNonFinitePointIndex = nullptr);

    // Samples a distance from the first point. Negative distances clamp to the head and distances
    // beyond the path clamp to the tail. Degenerate segments are skipped.
    static bool TrySampleLinearPolylineAtDistance(
        const TArray<FVector>& Points,
        double DistanceCm,
        FVector& OutPoint);

    // Samples the same canonical position and also returns the unit tangent of the authored
    // segment that owns that distance. At an exact bend, the incoming segment owns the point.
    // Returns false when the path has no usable segment, allowing one-slot callers to choose an
    // explicit semantic fallback without inventing a chord across a bend.
    static bool TrySampleLinearPolylineAtDistanceWithTangent(
        const TArray<FVector>& Points,
        double DistanceCm,
        FVector& OutPoint,
        FVector& OutUnitTangent);

    // Samples SlotCount points at SlotSpacingCm intervals using the same head/tail clamping rules.
    // The output is reset on entry and remains empty when the arguments are invalid.
    static bool BuildLinearQueueSlotPoints(
        const TArray<FVector>& Points,
        int32 SlotCount,
        double SlotSpacingCm,
        TArray<FVector>& OutSlotPoints);

    // Converts an actor-centred local-centimetre point into the footprint-lower-left
    // reservation-grid coordinate used by QueueReservedRects.
    static FVector2D LocalPointCmToReservationGrid(
        const FVector& LocalPointCm,
        FIntPoint FootprintSizeCells);

    // Uses the same boundary tolerance as Spec validation so editor/runtime projections agree.
    static bool IsLocalPointInsideReservation(
        const FVector& LocalPointCm,
        FIntPoint FootprintSizeCells,
        const TArray<FFFZSCellRect>& ReservedRects,
        double BoundaryToleranceCm = 0.01);
};
