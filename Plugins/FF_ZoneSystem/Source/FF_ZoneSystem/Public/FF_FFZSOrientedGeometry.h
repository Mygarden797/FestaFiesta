#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "FF_FFZSTypes.h"

/**
 * Pure, allocation-free geometry helpers for facility-local rectangles placed at an arbitrary Yaw.
 *
 * Local rectangles use the same half-open integer-cell convention as FFFZSCellRect. Rotation is
 * around the centre of local cell (0, 0), then OriginCell is applied. Consequently 0/90/180/270
 * degree results preserve the legacy RotateRect + translation lattice exactly.
 */
namespace FFZSOrientedGeometry
{
    static constexpr int32 CornerCount = 4;
    using FCornerArray = TStaticArray<FVector2D, CornerCount>;

    /** Builds an oriented rectangle from a facility-local half-open cell rectangle. */
    FF_ZONESYSTEM_API FFFZSOrientedRect MakeFromLocalRect(
        const FFFZSCellRect& LocalRect,
        FIntPoint OriginCell,
        float YawDegrees);

    /** Returns corners in local winding order: -X/-Y, +X/-Y, +X/+Y, -X/+Y. */
    FF_ZONESYSTEM_API FCornerArray GetCorners(const FFFZSOrientedRect& Rect);

    /** Returns the exact axis-aligned continuous-cell bounds, or an invalid box for invalid input. */
    FF_ZONESYSTEM_API FBox2D GetAabb(const FFFZSOrientedRect& Rect);

    /**
     * Returns a half-open integer-cell rectangle that conservatively encloses Rect.
     * Cardinal rotations are exactly equivalent to the legacy RotateRect + OriginCell result.
     * Invalid or int32-overflowing input returns an invalid FFFZSCellRect.
     */
    FF_ZONESYSTEM_API FFFZSCellRect ToConservativeCellRect(const FFFZSOrientedRect& Rect);

    /**
     * Exact OBB-vs-OBB positive-area test using the separating axis theorem.
     * Edge and corner contact are deliberately not overlap, matching FFFZSCellRect::Intersects.
     */
    FF_ZONESYSTEM_API bool IntersectsPositiveArea(
        const FFFZSOrientedRect& A,
        const FFFZSOrientedRect& B);
}
