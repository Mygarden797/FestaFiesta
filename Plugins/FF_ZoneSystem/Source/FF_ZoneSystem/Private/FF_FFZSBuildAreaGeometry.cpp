// FF_FFZSBuildAreaGeometry.cpp
// 역할: 건설 영역 폴리곤의 합집합과 포함 여부를 계산해 지역 사이 경계를 일관되게 판정한다.
#include "FF_FFZSBuildAreaGeometry.h"

#include "Algo/Unique.h"
#include "Curve/PolygonIntersectionUtils.h"
#include "FF_FFZSBuildArea.h"
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

    bool ClipSegmentToBox(const FVector2D& Start, const FVector2D& End, const FBox2D& Box,
        double& OutEnter, double& OutExit)
    {
        if (!Box.bIsValid || Box.Min.ContainsNaN() || Box.Max.ContainsNaN() ||
            Start.ContainsNaN() || End.ContainsNaN()) return false;
        OutEnter = 0.0;
        OutExit = 1.0;
        const FVector2D Delta = End - Start;
        for (int32 Axis = 0; Axis < 2; ++Axis)
        {
            if (FMath::Abs(Delta[Axis]) <= UE_DOUBLE_SMALL_NUMBER)
            {
                if (Start[Axis] < Box.Min[Axis] || Start[Axis] > Box.Max[Axis]) return false;
                continue;
            }
            double Enter = (Box.Min[Axis] - Start[Axis]) / Delta[Axis];
            double Exit = (Box.Max[Axis] - Start[Axis]) / Delta[Axis];
            if (Enter > Exit) Swap(Enter, Exit);
            OutEnter = FMath::Max(OutEnter, Enter);
            OutExit = FMath::Min(OutExit, Exit);
            if (OutEnter >= OutExit) return false;
        }
        return Delta.SizeSquared() > UE_DOUBLE_SMALL_NUMBER;
    }

    void AppendClippedRing(const FPolygon2d& Ring, const FBox2D& ViewBounds,
        TArray<FFFZSBuildAreaLineSegment>& OutSegments)
    {
        const TArray<FVector2D>& Vertices = Ring.GetVertices();
        for (int32 Index = 0; Index < Vertices.Num(); ++Index)
        {
            const FVector2D& Start = Vertices[Index];
            const FVector2D& End = Vertices[(Index + 1) % Vertices.Num()];
            double Enter, Exit;
            if (ClipSegmentToBox(Start, End, ViewBounds, Enter, Exit))
            {
                OutSegments.Add({FMath::Lerp(Start, End, Enter), FMath::Lerp(Start, End, Exit)});
            }
        }
    }

    double Cross2D(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    FPolygon2d RemoveUnionBoundarySpikes(const FPolygon2d& Ring)
    {
        TArray<FVector2D> Vertices = Ring.GetVertices();
        // Clipper can retain a retraced connector at a shared source edge. It has no area,
        // but treating it as a boundary rejects valid rectangles and draws an internal seam.
        // Remove only duplicate points and numerical 180-degree backtracks, never ordinary
        // concave corners or narrow holes. This is far below the 0.1cm authority tolerance.
        constexpr double NumericalToleranceSquared = 1.e-14;
        bool bRemoved = true;
        while (bRemoved && Vertices.Num() >= 3)
        {
            bRemoved = false;
            for (int32 Index = 0; Index < Vertices.Num(); ++Index)
            {
                const FVector2D ToPrevious = Vertices[(Index + Vertices.Num() - 1) % Vertices.Num()] - Vertices[Index];
                const FVector2D ToNext = Vertices[(Index + 1) % Vertices.Num()] - Vertices[Index];
                const double PreviousLengthSquared = ToPrevious.SizeSquared();
                const double NextLengthSquared = ToNext.SizeSquared();
                const bool bDuplicate = FMath::Min(PreviousLengthSquared, NextLengthSquared) <= NumericalToleranceSquared;
                const bool bRetraced = FVector2D::DotProduct(ToPrevious, ToNext) > 0.0 &&
                    FMath::Square(Cross2D(ToPrevious, ToNext)) <= NumericalToleranceSquared *
                        FMath::Max(PreviousLengthSquared, NextLengthSquared);
                if (bDuplicate || bRetraced)
                {
                    Vertices.RemoveAt(Index);
                    bRemoved = true;
                    break;
                }
            }
        }
        return FPolygon2d(Vertices);
    }

    void SplitUnionBoundaryLoops(const FPolygon2d& Input, TArray<FPolygon2d>& OutLoops)
    {
        TArray<FPolygon2d> Pending;
        Pending.Add(Input);
        while (!Pending.IsEmpty())
        {
            const FPolygon2d Ring = RemoveUnionBoundarySpikes(Pending.Pop(EAllowShrinking::No));
            if (Ring.VertexCount() < 3) continue;
            const TArray<FVector2D>& Vertices = Ring.GetVertices();
            bool bSplit = false;
            for (int32 First = 0; First < Vertices.Num() && !bSplit; ++First)
            {
                for (int32 Second = First + 2; Second < Vertices.Num(); ++Second)
                {
                    if (!Vertices[First].Equals(Vertices[Second], 1.e-7)) continue;
                    // A weakly-simple outer may walk into a hole and back through the same
                    // contact point. Split that walk first; its retraced connector then becomes
                    // a removable 180-degree tip on the extracted hole loop.
                    TArray<FVector2D> Inner;
                    TArray<FVector2D> Remainder;
                    for (int32 Index = First; Index < Second; ++Index) Inner.Add(Vertices[Index]);
                    for (int32 Index = Second; Index < Vertices.Num(); ++Index) Remainder.Add(Vertices[Index]);
                    for (int32 Index = 0; Index < First; ++Index) Remainder.Add(Vertices[Index]);
                    Pending.Emplace(Inner);
                    Pending.Emplace(Remainder);
                    bSplit = true;
                    break;
                }
            }
            if (!bSplit && FMath::Abs(Ring.SignedArea()) > UE_DOUBLE_SMALL_NUMBER) OutLoops.Add(Ring);
        }
    }

    bool NormalizeUnionBoundaries(TArray<FGeneralPolygon2d>& Polygons)
    {
        TArray<FPolygon2d> Loops;
        for (const FGeneralPolygon2d& Polygon : Polygons)
        {
            SplitUnionBoundaryLoops(Polygon.GetOuter(), Loops);
            for (const FPolygon2d& Hole : Polygon.GetHoles()) SplitUnionBoundaryLoops(Hole, Loops);
        }
        TArray<FGeneralPolygon2d> Normalized;
        for (const FPolygon2d& Loop : Loops)
            if (!Loop.IsClockwise()) Normalized.Emplace(Loop);
        if (Normalized.IsEmpty()) return false;
        for (const FPolygon2d& Hole : Loops)
        {
            if (!Hole.IsClockwise()) continue;
            int32 OwnerIndex = INDEX_NONE;
            double OwnerArea = TNumericLimits<double>::Max();
            for (int32 Index = 0; Index < Normalized.Num(); ++Index)
            {
                const FPolygon2d& Outer = Normalized[Index].GetOuter();
                const double Area = FMath::Abs(Outer.SignedArea());
                if (Area >= OwnerArea) continue;
                bool bContainsHole = true;
                for (const FVector2D& Point : Hole.GetVertices())
                {
                    if (!Outer.Contains(Point) && Outer.DistanceSquared(Point) > 1.e-14)
                    {
                        bContainsHole = false;
                        break;
                    }
                }
                if (bContainsHole)
                {
                    OwnerIndex = Index;
                    OwnerArea = Area;
                }
            }
            if (OwnerIndex == INDEX_NONE) return false;
            Normalized[OwnerIndex].AddHole(Hole, false, false);
        }
        Polygons = MoveTemp(Normalized);
        return true;
    }

    void AddSegmentCrossings(const FVector2D& Start, const FVector2D& End,
        const FFFZSBuildAreaLineSegment& Boundary, TArray<double>& OutParameters)
    {
        const FVector2D Delta = End - Start;
        const FVector2D Edge = Boundary.End - Boundary.Start;
        const FVector2D Offset = Boundary.Start - Start;
        const double Denominator = Cross2D(Delta, Edge);
        if (FMath::Abs(Denominator) > UE_DOUBLE_SMALL_NUMBER)
        {
            const double T = Cross2D(Offset, Edge) / Denominator;
            const double U = Cross2D(Offset, Delta) / Denominator;
            if (T > 0.0 && T < 1.0 && U >= 0.0 && U <= 1.0) OutParameters.Add(T);
        }
        else if (FMath::Abs(Cross2D(Offset, Delta)) <= UE_DOUBLE_SMALL_NUMBER)
        {
            // A line along the boundary can leave it at either vertex. Keep both split points.
            const double LengthSquared = Delta.SizeSquared();
            if (LengthSquared <= UE_DOUBLE_SMALL_NUMBER) return;
            for (const FVector2D& Point : {Boundary.Start, Boundary.End})
            {
                const double T = FVector2D::DotProduct(Point - Start, Delta) / LengthSquared;
                if (T > 0.0 && T < 1.0) OutParameters.Add(T);
            }
        }
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
    if (!UE::Geometry::PolygonsUnion(SourcePolygons, HorizontalUnion, false) || HorizontalUnion.IsEmpty() ||
        !NormalizeUnionBoundaries(HorizontalUnion))
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
        if (!UE::Geometry::PolygonsUnion(ActivePolygons, Slice.Polygons, false) || Slice.Polygons.IsEmpty() ||
            !NormalizeUnionBoundaries(Slice.Polygons))
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

const FFFZSBuildAreaGeometry::FSlice* FFFZSBuildAreaGeometry::FindSlice(const double WorldZCm) const
{
    if (!FMath::IsFinite(WorldZCm)) return nullptr;
    // At a shared vertical endpoint the zero-thickness slice contains the full inclusive union.
    // Drawing adjacent interval slices too would expose edges which are internal on this plane.
    for (const FSlice& Slice : Slices)
    {
        if (Slice.MinZ == WorldZCm && Slice.MaxZ == WorldZCm) return &Slice;
    }
    for (const FSlice& Slice : Slices)
    {
        if (WorldZCm >= Slice.MinZ && WorldZCm <= Slice.MaxZ) return &Slice;
    }
    return nullptr;
}

void FFFZSBuildAreaGeometry::GetBoundarySegments(const double WorldZCm, const FBox2D& ViewBounds,
    TArray<FFFZSBuildAreaLineSegment>& OutSegments) const
{
    OutSegments.Reset();
    const FSlice* Slice = FindSlice(WorldZCm);
    if (!Slice || !ViewBounds.bIsValid) return;
    for (int32 Index = 0; Index < Slice->Polygons.Num(); ++Index)
    {
        if (!Slice->PolygonBounds[Index].Intersect(ViewBounds)) continue;
        const FGeneralPolygon2d& Polygon = Slice->Polygons[Index];
        AppendClippedRing(Polygon.GetOuter(), ViewBounds, OutSegments);
        for (const FPolygon2d& Hole : Polygon.GetHoles()) AppendClippedRing(Hole, ViewBounds, OutSegments);
    }
}

void FFFZSBuildAreaGeometry::GetOutsideSegments(const FFFZSOrientedRect& WorldRectCm,
    const double WorldZCm, const TArray<FFFZSBuildAreaLineSegment>& InputSegments,
    TArray<FFFZSBuildAreaLineSegment>& OutSegments) const
{
    OutSegments.Reset();
    if (!WorldRectCm.IsValid() || !FMath::IsFinite(WorldZCm) ||
        ContainsWorldOrientedRect(WorldRectCm, WorldZCm)) return;

    TArray<FFFZSBuildAreaLineSegment> Boundary;
    GetBoundarySegments(WorldZCm, FFZSOrientedGeometry::GetAabb(WorldRectCm), Boundary);
    const double Radians = FMath::DegreesToRadians(static_cast<double>(WorldRectCm.YawDegrees));
    const FVector2D AxisX(FMath::Cos(Radians), FMath::Sin(Radians));
    const FVector2D AxisY(-AxisX.Y, AxisX.X);
    const auto ToRectLocal = [&WorldRectCm, &AxisX, &AxisY](const FVector2D& Point)
    {
        const FVector2D Delta = Point - WorldRectCm.CenterCell;
        return FVector2D(FVector2D::DotProduct(Delta, AxisX), FVector2D::DotProduct(Delta, AxisY));
    };
    const FBox2D LocalBounds(-WorldRectCm.HalfExtentCells, WorldRectCm.HalfExtentCells);
    TArray<double> Parameters;
    Parameters.Reserve(Boundary.Num() + 2);
    for (const FFFZSBuildAreaLineSegment& Input : InputSegments)
    {
        double Enter, Exit;
        if (!ClipSegmentToBox(ToRectLocal(Input.Start), ToRectLocal(Input.End), LocalBounds, Enter, Exit)) continue;
        const FVector2D Start = FMath::Lerp(Input.Start, Input.End, Enter);
        const FVector2D End = FMath::Lerp(Input.Start, Input.End, Exit);
        Parameters.Reset();
        Parameters.Add(0.0);
        Parameters.Add(1.0);
        for (const FFFZSBuildAreaLineSegment& Edge : Boundary) AddSegmentCrossings(Start, End, Edge, Parameters);
        Parameters.Sort();
        for (int32 Index = 1; Index < Parameters.Num(); ++Index)
        {
            const double A = Parameters[Index - 1];
            const double B = Parameters[Index];
            if (B - A <= UE_DOUBLE_SMALL_NUMBER) continue;
            const FVector2D Midpoint = FMath::Lerp(Start, End, (A + B) * 0.5);
            if (!ContainsWorldPoint(FVector(Midpoint, WorldZCm)))
            {
                OutSegments.Add({FMath::Lerp(Start, End, A), FMath::Lerp(Start, End, B)});
            }
        }
    }
}
