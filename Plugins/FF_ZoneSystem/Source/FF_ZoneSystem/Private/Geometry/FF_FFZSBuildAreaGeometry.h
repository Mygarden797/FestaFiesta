// FF_FFZSBuildAreaGeometry.h
// 역할: 건설 영역 폴리곤의 합집합과 포함 여부를 계산해 지역 사이 경계를 일관되게 판정한다.
#pragma once

#include "CoreMinimal.h"
#include "Curve/GeneralPolygon2.h"
#include "FF_FFZSTypes.h"

struct FFFZSBuildAreaLineSegment;

// Construction authority is the exact union of the regions, including holes and disconnected pieces.
// Boolean geometry is rebuilt only when the source regions change. Queries allocate no polygon arrays.
struct FFFZSBuildAreaSourcePolygon
{
    TArray<FVector2D> WorldVertices;
    double MinZ = 0.0;
    double MaxZ = 0.0;
};

class FFFZSBuildAreaGeometry
{
public:
    bool Rebuild(const TArray<FFFZSBuildAreaSourcePolygon>& Sources);
    bool ContainsWorldPoint(const FVector& WorldPoint) const;
    bool ContainsWorldOrientedRect(const FFFZSOrientedRect& WorldRectCm, double WorldZCm) const;
    void GetBoundarySegments(double WorldZCm, const FBox2D& ViewBounds,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments) const;
    void GetOutsideSegments(const FFFZSOrientedRect& WorldRectCm, double WorldZCm,
        const TArray<FFFZSBuildAreaLineSegment>& InputSegments,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments) const;
    bool IsReady() const { return !Slices.IsEmpty(); }
    const FBox& GetBounds() const { return Bounds; }
    double GetAreaSquareMeters() const { return AreaSquareMeters; }

private:
    struct FSlice
    {
        // Zero-thickness slices preserve inclusive vertical boundaries of the original regions.
        double MinZ = 0.0;
        double MaxZ = 0.0;
        TArray<UE::Geometry::FGeneralPolygon2d> Polygons;
        TArray<FBox2D> PolygonBounds;
    };
    const FSlice* FindSlice(double WorldZCm) const;
    TArray<FSlice> Slices;
    FBox Bounds = FBox(ForceInit);
    double AreaSquareMeters = 0.0;
};
