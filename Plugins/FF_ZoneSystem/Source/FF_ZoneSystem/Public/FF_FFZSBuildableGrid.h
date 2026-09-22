// FF_FFZSBuildableGrid.h
// 역할: 건설 가능한 칸을 표시용 마스크에 기록하고 이어진 경계선을 만든다. 실제 설치 점유는 변경하지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"

class UPrimitiveComponent;

// Broad-phase scenery bounds, clipped only after terrain height is known. The support
// identity prevents a sloping mesh floor's large AABB from erasing its own valid surface.
struct FF_ZONESYSTEM_API FFFZSBuildGridObstacle
{
    FBox WorldBounds = FBox(EForceInit::ForceInit);
    FIntPoint FirstCell = FIntPoint::ZeroValue;
    FIntPoint EndCell = FIntPoint::ZeroValue;
    TWeakObjectPtr<UPrimitiveComponent> Component;
    int32 InstanceIndex = INDEX_NONE;
};

// Presentation-only cells. Authoritative placement remains the compressed 1 cm geometry.
// A strict budget and adaptive spacing keep a large overview from allocating physical cells.
struct FF_ZONESYSTEM_API FFFZSBuildableGridMask
{
    static constexpr int32 MaximumCellCount = 16384;

    bool Initialize(const FVector2D& MinCm, const FVector2D& MaxCm, int32 RequestedStepCm,
        const FVector2D& AlignmentOriginLocalCm = FVector2D::ZeroVector);
    void Reset();
    bool IsBuildable(int32 X, int32 Y) const;
    void SetBuildable(int32 X, int32 Y, bool bBuildable);

    const FVector2D& GetMinLocalCm() const { return MinLocalCm; }
    double GetStepCm() const { return StepCm; }
    int32 GetNumX() const { return NumX; }
    int32 GetNumY() const { return NumY; }
    int32 GetCellCount() const { return Cells.Num(); }
    int32 GetBuildableCellCount() const { return BuildableCellCount; }

    TArray<FFFZSBuildGridObstacle> DeferredWorldObstacles;
    double ObstacleHeightCm = 250.0;
    bool bIncludeGroundSurfaceSupport = true;

private:
    FVector2D MinLocalCm = FVector2D::ZeroVector;
    double StepCm = 100.0;
    int32 NumX = 0;
    int32 NumY = 0;
    int32 BuildableCellCount = 0;
    TBitArray<> Cells;
};

struct FF_ZONESYSTEM_API FFFZSBuildableGridSegment
{
    FVector2D StartLocalCm = FVector2D::ZeroVector;
    FVector2D EndLocalCm = FVector2D::ZeroVector;
    bool bBoundary = false;
};

namespace FFZSBuildableGrid
{
    // Emits each shared edge once. Consecutive edges with the same boundary state are merged;
    // holes stay empty and their contours can receive a separate renderer style.
    FF_ZONESYSTEM_API void BuildSegments(
        const FFFZSBuildableGridMask& Mask,
        TArray<FFFZSBuildableGridSegment>& OutSegments);
}
