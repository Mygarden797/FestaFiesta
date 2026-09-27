// FF_FFZSBuildAreaGeometry.cpp
// 역할: 건설 영역 폴리곤의 합집합과 포함 여부를 계산해 지역 사이 경계를 일관되게 판정한다.
#include "FF_FFZSBuildAreaGeometry.h"

#include "Algo/Unique.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "FF_FFZSOrientedGeometry.h"

namespace
{
    using UE::Geometry::FGeneralPolygon2d;
    using UE::Geometry::FPolygon2d;
    // Same inclusive boundary tolerance as AFFZSGameplayZone; never rasterize the 1 cm build grid.
    constexpr double BuildAreaBoundaryToleranceCm = 0.1;

    bool ContainsInclusive(const FGeneralPolygon2d& Polygon, const FVector2D& Point)
    {
        const double ToleranceSquared = FMath::Square(BuildAreaBoundaryToleranceCm);
        if (Polygon.GetOuter().DistanceSquared(Point) <= ToleranceSquared)
        {
            return true;
        }
        if (!Polygon.GetOuter().Contains(Point))
        {
            return false;
        }
        for (const FPolygon2d& Hole : Polygon.GetHoles())
        {
            if (Hole.DistanceSquared(Point) <= ToleranceSquared)
            {
                return true;
            }
            if (Hole.Contains(Point))
            {
                return false;
            }
        }
        return true;
    }

    // Clip each boundary segment against the rectangle's open interior. A concave notch or even a
    // completely enclosed hole must reject the rectangle, although all four corners may be inside.
    bool RingEntersRectInterior(const FPolygon2d& Ring, const FFFZSOrientedRect& Rect)
    {
        const double Radians = FMath::DegreesToRadians(static_cast<double>(Rect.YawDegrees));
        const FVector2D AxisX(FMath::Cos(Radians), FMath::Sin(Radians));
        const FVector2D AxisY(-AxisX.Y, AxisX.X);
        // Keep a positive interior even for valid sub-tolerance rectangles.
        const FVector2D Extent(
            Rect.HalfExtentCells.X - FMath::Min(BuildAreaBoundaryToleranceCm, Rect.HalfExtentCells.X * 0.01),
            Rect.HalfExtentCells.Y - FMath::Min(BuildAreaBoundaryToleranceCm, Rect.HalfExtentCells.Y * 0.01));
        const auto ToLocal = [&Rect, &AxisX, &AxisY](const FVector2D& Point)
        {
            const FVector2D Delta = Point - Rect.CenterCell;
            return FVector2D(FVector2D::DotProduct(Delta, AxisX), FVector2D::DotProduct(Delta, AxisY));
        };
        const TArray<FVector2D>& Vertices = Ring.GetVertices();
        for (int32 Index = 0; Index < Vertices.Num(); ++Index)
        {
            const FVector2D Start = ToLocal(Vertices[Index]);
            const FVector2D Delta = ToLocal(Vertices[(Index + 1) % Vertices.Num()]) - Start;
            double Enter = 0.0;
            double Exit = 1.0;
            bool bIntersects = true;
            for (int32 Axis = 0; Axis < 2; ++Axis)
            {
                if (FMath::Abs(Delta[Axis]) <= UE_DOUBLE_SMALL_NUMBER)
                {
                    if (FMath::Abs(Start[Axis]) >= Extent[Axis])
                    {
                        bIntersects = false;
                        break;
                    }
                    continue;
                }
                double A = (-Extent[Axis] - Start[Axis]) / Delta[Axis];
                double B = (Extent[Axis] - Start[Axis]) / Delta[Axis];
                if (A > B)
                {
                    Swap(A, B);
                }
                Enter = FMath::Max(Enter, A);
                Exit = FMath::Min(Exit, B);
                if (Enter >= Exit)
                {
                    bIntersects = false;
                    break;
                }
            }
            if (bIntersects && Enter < Exit)
            {
                return true;
            }
        }
        return false;
    }

    bool ContainsRect(const FGeneralPolygon2d& Polygon, const FFFZSOrientedRect& Rect)
    {
        for (const FVector2D& Corner : FFZSOrientedGeometry::GetCorners(Rect))
        {
            if (!ContainsInclusive(Polygon, Corner))
            {
                return false;
            }
        }
        if (RingEntersRectInterior(Polygon.GetOuter(), Rect))
        {
            return false;
        }
        for (const FPolygon2d& Hole : Polygon.GetHoles())
        {
            if (RingEntersRectInterior(Hole, Rect))
            {
                return false;
            }
        }
        return true;
    }
}

bool FFFZSBuildAreaGeometry::Rebuild(const TArray<FFFZSBuildAreaSourcePolygon>& Sources)
{
    Slices.Reset();
    Bounds = FBox(ForceInit);
    AreaSquareMeters = 0.0;
    if (Sources.IsEmpty())
    {
        return false;
    }
    TArray<FGeneralPolygon2d> SourcePolygons;
    TArray<double> Heights;
    SourcePolygons.Reserve(Sources.Num());
    for (const FFFZSBuildAreaSourcePolygon& Source : Sources)
    {
        if (Source.WorldVertices.Num() < 3 || !FMath::IsFinite(Source.MinZ) ||
            !FMath::IsFinite(Source.MaxZ) || Source.MinZ >= Source.MaxZ)
        {
            return false;
        }
        for (const FVector2D& Point : Source.WorldVertices)
        {
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
            {
                return false;
            }
        }
        FPolygon2d Polygon(Source.WorldVertices);
        if (FMath::Abs(Polygon.SignedArea()) <= UE_DOUBLE_SMALL_NUMBER)
        {
            return false;
        }
        if (Polygon.IsClockwise())
        {
            Polygon.Reverse();
        }
        SourcePolygons.Emplace(Polygon);
        Heights.Add(Source.MinZ);
        Heights.Add(Source.MaxZ);
    }
    TArray<FGeneralPolygon2d> HorizontalUnion;
    if (!UE::Geometry::PolygonsUnion(SourcePolygons, HorizontalUnion, false) || HorizontalUnion.IsEmpty())
    {
        return false;
    }
    Heights.Sort();
    Heights.SetNum(Algo::Unique(Heights));
    // Work in a temporary array: one failed boolean must never leave a partially valid authority.
    TArray<FSlice> NewSlices;
    for (int32 HeightIndex = 0; HeightIndex < Heights.Num() * 2 - 1; ++HeightIndex)
    {
        const double MinZ = Heights[HeightIndex / 2];
        const double MaxZ = Heights[(HeightIndex + 1) / 2];
        const double SampleZ = (MinZ + MaxZ) * 0.5;
        TArray<FGeneralPolygon2d> ActivePolygons;
        for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
        {
            if (SampleZ >= Sources[SourceIndex].MinZ && SampleZ <= Sources[SourceIndex].MaxZ)
            {
                ActivePolygons.Add(SourcePolygons[SourceIndex]);
            }
        }
        if (ActivePolygons.IsEmpty())
        {
            continue;
        }
        FSlice Slice;
        Slice.MinZ = MinZ;
        Slice.MaxZ = MaxZ;
        if (!UE::Geometry::PolygonsUnion(ActivePolygons, Slice.Polygons, false) || Slice.Polygons.IsEmpty())
        {
            return false;
        }
        for (const FGeneralPolygon2d& Polygon : Slice.Polygons)
        {
            const auto Box = Polygon.Bounds();
            Slice.PolygonBounds.Emplace(Box.Min, Box.Max);
        }
        NewSlices.Add(MoveTemp(Slice));
    }
    for (const FGeneralPolygon2d& Polygon : HorizontalUnion)
    {
        AreaSquareMeters += FMath::Abs(Polygon.SignedArea()) / 10000.0;
        const auto Box = Polygon.Bounds();
        Bounds += FVector(Box.Min.X, Box.Min.Y, Heights[0]);
        Bounds += FVector(Box.Max.X, Box.Max.Y, Heights.Last());
    }
    Slices = MoveTemp(NewSlices);
    return IsReady();
}

bool FFFZSBuildAreaGeometry::ContainsWorldPoint(const FVector& WorldPoint) const
{
    if (WorldPoint.ContainsNaN())
    {
        return false;
    }
    const FVector2D Point(WorldPoint.X, WorldPoint.Y);
    for (const FSlice& Slice : Slices)
    {
        if (WorldPoint.Z < Slice.MinZ || WorldPoint.Z > Slice.MaxZ)
        {
            continue;
        }
        for (int32 Index = 0; Index < Slice.Polygons.Num(); ++Index)
        {
            if (Slice.PolygonBounds[Index].ExpandBy(BuildAreaBoundaryToleranceCm).IsInsideOrOn(Point) &&
                ContainsInclusive(Slice.Polygons[Index], Point))
            {
                return true;
            }
        }
    }
    return false;
}

bool FFFZSBuildAreaGeometry::ContainsWorldOrientedRect(
    const FFFZSOrientedRect& WorldRectCm, const double WorldZCm) const
{
    if (!WorldRectCm.IsValid() || !FMath::IsFinite(WorldZCm))
    {
        return false;
    }
    const FBox2D RectBounds = FFZSOrientedGeometry::GetAabb(WorldRectCm);
    for (const FSlice& Slice : Slices)
    {
        if (WorldZCm < Slice.MinZ || WorldZCm > Slice.MaxZ)
        {
            continue;
        }
        for (int32 Index = 0; Index < Slice.Polygons.Num(); ++Index)
        {
            const FBox2D ExpandedBounds = Slice.PolygonBounds[Index].ExpandBy(BuildAreaBoundaryToleranceCm);
            if (ExpandedBounds.IsInsideOrOn(RectBounds.Min) && ExpandedBounds.IsInsideOrOn(RectBounds.Max) &&
                ContainsRect(Slice.Polygons[Index], WorldRectCm))
            {
                return true;
            }
        }
    }
    return false;
}
