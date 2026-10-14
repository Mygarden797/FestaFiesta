// Pure boundary snap/noding; this file never accesses actors, assets, or world state.
#include "FFZALayoutOps.h"
#include "FFZAPolygonMath.h"

namespace FFZALayoutOps
{
namespace
{
    // Numerical equality is independent of the user-facing search radius, in centimetres.
    constexpr double Epsilon = 1.e-4;
    constexpr double EpsilonSquared = Epsilon * Epsilon;
    constexpr double MinimumFacingDot = 0.7071067811865475; // 45 degrees

    double Dot(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
    double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
    bool SamePoint(const FVector2D& A, const FVector2D& B) { return FVector2D::DistSquared(A, B) <= EpsilonSquared; }

    struct FBounds
    {
        FVector2D Min;
        FVector2D Max;
        FBounds(const FVector2D& A, const FVector2D& B)
            : Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y))
            , Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)) {}
        explicit FBounds(const TArray<FVector2D>& Points) : FBounds(Points[0], Points[0])
        {
            for (const FVector2D& Point : Points)
            {
                Min.X = FMath::Min(Min.X, Point.X); Min.Y = FMath::Min(Min.Y, Point.Y);
                Max.X = FMath::Max(Max.X, Point.X); Max.Y = FMath::Max(Max.Y, Point.Y);
            }
        }
        bool Overlaps(const FBounds& Other, const double Padding) const
        {
            return Min.X - Padding <= Other.Max.X && Max.X + Padding >= Other.Min.X &&
                Min.Y - Padding <= Other.Max.Y && Max.Y + Padding >= Other.Min.Y;
        }
    };

    double SignedArea(const TArray<FVector2D>& Points)
    {
        // Translation avoids cancellation for maps far from the world origin.
        double TwiceArea = 0.0;
        for (int32 I = 1; I + 1 < Points.Num(); ++I)
        {
            TwiceArea += Cross(Points[I] - Points[0], Points[I + 1] - Points[0]);
        }
        return TwiceArea * 0.5;
    }

    bool IsValidRing(const TArray<FVector2D>& Points, double& OutArea)
    {
        if (Points.Num() < 3) { return false; }
        for (int32 I = 0; I < Points.Num(); ++I)
        {
            const FVector2D& Point = Points[I];
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)) { return false; }
            const FVector2D Previous = Point - Points[(I + Points.Num() - 1) % Points.Num()];
            const FVector2D Next = Points[(I + 1) % Points.Num()] - Point;
            // IsSimplePolygon excludes non-adjacent touches; also exclude adjacent backtracking.
            if (Dot(Previous, Next) < 0.0 && FMath::Abs(Cross(Previous, Next)) <= Epsilon * (Previous.Size() + Next.Size()))
            {
                return false;
            }
        }
        OutArea = SignedArea(Points);
        return FMath::IsFinite(OutArea) && FMath::Abs(OutArea) > EpsilonSquared &&
            FFZAPolygonMath::IsSimplePolygon(Points, Epsilon);
    }

    struct FEdgeMatch { int32 Reference; int32 Edge; double StartT; double EndT; };
    struct FSplitPoint { double T; FVector2D Point; int32 Reference; double PathOrder; };

    // Clip a reference edge to the target edge's extent and tolerance band. Partial shared edges
    // are found even if neither original target endpoint lies within the search radius.
    bool FindMatch(const FVector2D& P, const FVector2D& Q, const double TargetArea,
        const FVector2D& A, const FVector2D& B, const double ReferenceArea, const double Tolerance,
        double& OutStartU, double& OutEndU)
    {
        const FVector2D Direction = Q - P;
        const FVector2D OtherDirection = B - A;
        const double Length = Direction.Size();
        const double OtherLength = OtherDirection.Size();
        if (Length <= Epsilon || OtherLength <= Epsilon) { return false; }
        const double DirectionDot = Dot(Direction, OtherDirection) / (Length * OtherLength);
        const double WindingProduct = (TargetArea > 0.0 ? 1.0 : -1.0) * (ReferenceArea > 0.0 ? 1.0 : -1.0);
        if (DirectionDot * WindingProduct > -MinimumFacingDot) { return false; }
        OutStartU = 0.0;
        OutEndU = 1.0;
        auto ClipRange = [&OutStartU, &OutEndU](const double ValueA, const double ValueB, const double Min, const double Max)
        {
            const double Delta = ValueB - ValueA;
            if (FMath::Abs(Delta) <= 1.e-12) { return ValueA >= Min && ValueA <= Max; }
            const double U0 = (Min - ValueA) / Delta;
            const double U1 = (Max - ValueA) / Delta;
            OutStartU = FMath::Max(OutStartU, FMath::Min(U0, U1));
            OutEndU = FMath::Min(OutEndU, FMath::Max(U0, U1));
            return OutStartU <= OutEndU;
        };
        const FVector2D UnitDirection = Direction / Length;
        if (!ClipRange(Dot(A - P, UnitDirection), Dot(B - P, UnitDirection), 0.0, Length) ||
            (OutEndU - OutStartU) * OtherLength <= Epsilon) { return false; }
        // Match the whole overlapping piece or leave it alone. Clipping only the tolerance band
        // creates artificial seam ends that can advance further on every subsequent button press.
        const FVector2D Start = A + OtherDirection * OutStartU;
        const FVector2D End = A + OtherDirection * OutEndU;
        return FMath::Abs(Cross(UnitDirection, Start - P)) <= Tolerance &&
            FMath::Abs(Cross(UnitDirection, End - P)) <= Tolerance;
    }

    void ConsiderVertexSnap(const FVector2D& Original, const FVector2D& Candidate, const double ToleranceSquared,
        FVector2D& InOutPoint, double& InOutDistanceSquared)
    {
        const double DistanceSquared = FVector2D::DistSquared(Original, Candidate);
        // Strict improvement makes exact segment membership stable and equal-distance ties deterministic.
        if (DistanceSquared <= ToleranceSquared && DistanceSquared < InOutDistanceSquared)
        {
            InOutPoint = Candidate;
            InOutDistanceSquared = DistanceSquared;
        }
    }

    double InterpolateKey(const TArray<double>& Keys, const int32 OriginalCount, const int32 Edge, const double T)
    {
        const double EndKey = Edge + 1 < Keys.Num() ? Keys[Edge + 1] : static_cast<double>(OriginalCount);
        return FMath::Lerp(Keys[Edge], EndKey, T);
    }

    bool NodeReference(const TArray<FVector2D>& Reference, const TArray<double>& Keys, const int32 OriginalCount,
        const TArray<FVector2D>& Target, TArray<FVector2D>& OutPoints, TArray<double>& OutKeys)
    {
        OutPoints.Reset(); OutKeys.Reset();
        for (int32 Edge = 0; Edge < Reference.Num(); ++Edge)
        {
            const FVector2D& A = Reference[Edge];
            const FVector2D& B = Reference[(Edge + 1) % Reference.Num()];
            const FVector2D Direction = B - A;
            const double LengthSquared = Direction.SizeSquared();
            const FBounds EdgeBounds(A, B);
            TArray<FSplitPoint> Splits;
            for (const FVector2D& Point : Target)
            {
                if (!EdgeBounds.Overlaps(FBounds(Point, Point), Epsilon) || SamePoint(Point, A) || SamePoint(Point, B)) { continue; }
                const double T = Dot(Point - A, Direction) / LengthSquared;
                if (T > 0.0 && T < 1.0 && SamePoint(Point, A + Direction * T))
                {
                    Splits.Add({T, Point, INDEX_NONE, 0.0});
                }
            }
            Splits.Sort([](const FSplitPoint& Left, const FSplitPoint& Right) { return Left.T < Right.T; });
            OutPoints.Add(A); OutKeys.Add(Keys[Edge]);
            for (const FSplitPoint& Split : Splits)
            {
                if (!SamePoint(OutPoints.Last(), Split.Point))
                {
                    // Copy the accepted shared point, rather than recomputing a slightly different projection.
                    OutPoints.Add(Split.Point);
                    OutKeys.Add(InterpolateKey(Keys, OriginalCount, Edge, Split.T));
                }
            }
        }
        return OutPoints.Num() != Reference.Num();
    }

    bool HasEligibleMatch(const TArray<FVector2D>& Target, const double TargetArea, const FBounds& TargetBounds,
        const TArray<FVector2D>& Reference, const double ReferenceArea, const FBounds& ReferenceBounds,
        const double Tolerance)
    {
        if (!TargetBounds.Overlaps(ReferenceBounds, Tolerance)) { return false; }
        for (int32 Edge = 0; Edge < Target.Num(); ++Edge)
        {
            const FVector2D& P = Target[Edge];
            const FVector2D& Q = Target[(Edge + 1) % Target.Num()];
            const FBounds EdgeBounds(P, Q);
            if (!EdgeBounds.Overlaps(ReferenceBounds, Tolerance)) { continue; }
            for (int32 OtherEdge = 0; OtherEdge < Reference.Num(); ++OtherEdge)
            {
                const FVector2D& A = Reference[OtherEdge];
                const FVector2D& B = Reference[(OtherEdge + 1) % Reference.Num()];
                double Start, End;
                if (EdgeBounds.Overlaps(FBounds(A, B), Tolerance) &&
                    FindMatch(P, Q, TargetArea, A, B, ReferenceArea, Tolerance, Start, End)) { return true; }
            }
        }
        return false;
    }

    TArray<int32> BuildSelectedOrder(const TArray<TArray<FVector2D>>& Polygons, const TArray<double>& Areas,
        const TArray<bool>& Valid, const TArray<FBounds>& Bounds, const int32 Reference, const double Tolerance,
        TArray<int32>& OutDisconnected)
    {
        // Sort immutable authored geometry once. Breadth-first traversal does not depend on selection
        // order or mutable actor IDs; bounds filter pairs before the edge-level connectivity test.
        TArray<int32> Pending;
        Pending.Reserve(Polygons.Num());
        for (int32 I = 0; I < Polygons.Num(); ++I) { if (I != Reference && Valid[I]) { Pending.Add(I); } }
        Pending.Sort([&](const int32 Left, const int32 Right)
        {
            const FBounds& A = Bounds[Left];
            const FBounds& B = Bounds[Right];
            if (A.Min.X != B.Min.X) { return A.Min.X < B.Min.X; }
            if (A.Min.Y != B.Min.Y) { return A.Min.Y < B.Min.Y; }
            if (A.Max.X != B.Max.X) { return A.Max.X < B.Max.X; }
            if (A.Max.Y != B.Max.Y) { return A.Max.Y < B.Max.Y; }
            if (Polygons[Left].Num() != Polygons[Right].Num()) { return Polygons[Left].Num() < Polygons[Right].Num(); }
            for (int32 Point = 0; Point < Polygons[Left].Num(); ++Point)
            {
                if (Polygons[Left][Point].X != Polygons[Right][Point].X) { return Polygons[Left][Point].X < Polygons[Right][Point].X; }
                if (Polygons[Left][Point].Y != Polygons[Right][Point].Y) { return Polygons[Left][Point].Y < Polygons[Right][Point].Y; }
            }
            return false; // Identical authored geometry is interchangeable.
        });
        TArray<int32> Order;
        Order.Reserve(Pending.Num());
        TArray<int32> Frontier{Reference};
        TArray<bool> Connected;
        Connected.Init(false, Polygons.Num()); Connected[Reference] = true;
        while (!Frontier.IsEmpty())
        {
            TArray<int32> NextFrontier;
            for (const int32 Target : Pending)
            {
                if (Connected[Target]) { continue; }
                for (const int32 Anchor : Frontier)
                {
                    if (HasEligibleMatch(Polygons[Target], Areas[Target], Bounds[Target],
                        Polygons[Anchor], Areas[Anchor], Bounds[Anchor], Tolerance))
                    {
                        Connected[Target] = true; NextFrontier.Add(Target); Order.Add(Target); break;
                    }
                }
            }
            Frontier = MoveTemp(NextFrontier);
        }
        for (const int32 Target : Pending) { if (!Connected[Target]) { OutDisconnected.Add(Target); } }
        return Order;
    }

    struct FWeldTraversal
    {
        int32 Reference = INDEX_NONE;
        TArray<int32> Targets;
    };

    enum class EWeldMode { Ordered, SelectedComponent, SelectedGroups };

    struct FCanonicalRing
    {
        int32 Start = 0;
        int32 Direction = 1;
    };

    bool PointLess(const FVector2D& A, const FVector2D& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }

    FCanonicalRing CanonicalRing(const TArray<FVector2D>& Ring)
    {
        FCanonicalRing Result;
        for (int32 I = 1; I < Ring.Num(); ++I)
        {
            if (PointLess(Ring[I], Ring[Result.Start])) { Result.Start = I; }
        }
        // Simple rings have unique vertices. The two neighbours therefore determine a stable
        // traversal independent of spline start point or winding without allocating a copy.
        const int32 Next = (Result.Start + 1) % Ring.Num();
        const int32 Previous = (Result.Start + Ring.Num() - 1) % Ring.Num();
        Result.Direction = PointLess(Ring[Next], Ring[Previous]) ? 1 : -1;
        return Result;
    }

    void BuildGroupedTraversals(const TArray<TArray<FVector2D>>& Polygons, const TArray<double>& Areas,
        const TArray<bool>& Valid, const TArray<FBounds>& Bounds, const int32 ManualReference,
        const double Tolerance, TArray<FWeldGroup>& OutGroups, TArray<FWeldTraversal>& OutTraversals)
    {
        TArray<FCanonicalRing> Canonical;
        TArray<TArray<FBounds>> EdgeBounds;
        TArray<TArray<int32>> Adjacency;
        TArray<int32> Sorted;
        Canonical.SetNum(Polygons.Num()); EdgeBounds.SetNum(Polygons.Num()); Adjacency.SetNum(Polygons.Num());
        Sorted.Reserve(Polygons.Num());
        for (int32 I = 0; I < Polygons.Num(); ++I)
        {
            if (!Valid[I]) { continue; }
            Sorted.Add(I); Canonical[I] = CanonicalRing(Polygons[I]);
            EdgeBounds[I].Reserve(Polygons[I].Num());
            for (int32 Edge = 0; Edge < Polygons[I].Num(); ++Edge)
            {
                EdgeBounds[I].Emplace(Polygons[I][Edge], Polygons[I][(Edge + 1) % Polygons[I].Num()]);
            }
        }
        auto GeometryLess = [&](const int32 Left, const int32 Right)
        {
            const FBounds& A = Bounds[Left];
            const FBounds& B = Bounds[Right];
            if (A.Min.X != B.Min.X) { return A.Min.X < B.Min.X; }
            if (A.Min.Y != B.Min.Y) { return A.Min.Y < B.Min.Y; }
            if (A.Max.X != B.Max.X) { return A.Max.X < B.Max.X; }
            if (A.Max.Y != B.Max.Y) { return A.Max.Y < B.Max.Y; }
            const int32 Count = Polygons[Left].Num();
            if (Count != Polygons[Right].Num()) { return Count < Polygons[Right].Num(); }
            for (int32 I = 0; I < Count; ++I)
            {
                const FVector2D& P = Polygons[Left][(Canonical[Left].Start + Canonical[Left].Direction * I + Count) % Count];
                const FVector2D& Q = Polygons[Right][(Canonical[Right].Start + Canonical[Right].Direction * I + Count) % Count];
                if (P != Q) { return PointLess(P, Q); }
            }
            return false; // Identical geometry is interchangeable, independent of actor/selection IDs.
        };
        Sorted.Sort(GeometryLess);
        // Sweep authored X bounds once. Cache edge bounds and record an undirected graph only
        // after an actual facing-edge test; winding or pair orientation cannot omit a neighbour.
        auto EligiblePair = [&](const int32 A, const int32 B)
        {
            for (int32 EdgeA = 0; EdgeA < Polygons[A].Num(); ++EdgeA)
            {
                if (!EdgeBounds[A][EdgeA].Overlaps(Bounds[B], Tolerance)) { continue; }
                const FVector2D& P = Polygons[A][EdgeA];
                const FVector2D& Q = Polygons[A][(EdgeA + 1) % Polygons[A].Num()];
                for (int32 EdgeB = 0; EdgeB < Polygons[B].Num(); ++EdgeB)
                {
                    if (!EdgeBounds[A][EdgeA].Overlaps(EdgeBounds[B][EdgeB], Tolerance)) { continue; }
                    const FVector2D& R = Polygons[B][EdgeB];
                    const FVector2D& S = Polygons[B][(EdgeB + 1) % Polygons[B].Num()];
                    double Start, End;
                    if (FindMatch(P, Q, Areas[A], R, S, Areas[B], Tolerance, Start, End) ||
                        FindMatch(R, S, Areas[B], P, Q, Areas[A], Tolerance, Start, End)) { return true; }
                }
            }
            return false;
        };
        for (int32 I = 0; I < Sorted.Num(); ++I)
        {
            const int32 A = Sorted[I];
            for (int32 J = I + 1; J < Sorted.Num(); ++J)
            {
                const int32 B = Sorted[J];
                if (Bounds[B].Min.X > Bounds[A].Max.X + Tolerance) { break; }
                if (Bounds[A].Overlaps(Bounds[B], Tolerance) && EligiblePair(A, B))
                {
                    Adjacency[A].Add(B); Adjacency[B].Add(A);
                }
            }
        }
        TArray<bool> Visited;
        Visited.Init(false, Polygons.Num());
        for (const int32 Seed : Sorted)
        {
            if (Visited[Seed]) { continue; }
            FWeldGroup Group;
            Group.PolygonIndices.Add(Seed); Visited[Seed] = true;
            for (int32 I = 0; I < Group.PolygonIndices.Num(); ++I)
            {
                for (const int32 Neighbour : Adjacency[Group.PolygonIndices[I]])
                {
                    if (!Visited[Neighbour]) { Visited[Neighbour] = true; Group.PolygonIndices.Add(Neighbour); }
                }
            }
            Group.PolygonIndices.Sort(GeometryLess);
            Group.bUsesManualReference = Group.PolygonIndices.Contains(ManualReference);
            Group.ReferencePolygonIndex = Group.bUsesManualReference ? ManualReference : Group.PolygonIndices[0];
            if (!Group.bUsesManualReference)
            {
                for (const int32 Candidate : Group.PolygonIndices)
                {
                    const int32 Reference = Group.ReferencePolygonIndex;
                    if (FMath::Abs(Areas[Candidate]) > FMath::Abs(Areas[Reference]) ||
                        (FMath::Abs(Areas[Candidate]) == FMath::Abs(Areas[Reference]) && GeometryLess(Candidate, Reference)))
                    {
                        Group.ReferencePolygonIndex = Candidate;
                    }
                }
            }
            OutGroups.Add(MoveTemp(Group));
        }
        OutGroups.Sort([&](const FWeldGroup& A, const FWeldGroup& B)
        {
            if (A.bUsesManualReference != B.bUsesManualReference) { return A.bUsesManualReference; }
            return GeometryLess(A.ReferencePolygonIndex, B.ReferencePolygonIndex);
        });
        // Breadth-first levels are sorted geometrically, matching selected-mode ownership rules.
        // Traversal reuses the graph; no component revalidates input or repeats all edge-pair tests.
        Visited.Init(false, Polygons.Num());
        OutTraversals.Reserve(OutGroups.Num());
        for (const FWeldGroup& Group : OutGroups)
        {
            FWeldTraversal Traversal;
            Traversal.Reference = Group.ReferencePolygonIndex;
            TArray<int32> Frontier{Traversal.Reference};
            Visited[Traversal.Reference] = true;
            while (!Frontier.IsEmpty())
            {
                TArray<int32> NextFrontier;
                for (const int32 Anchor : Frontier)
                {
                    for (const int32 Neighbour : Adjacency[Anchor])
                    {
                        if (!Visited[Neighbour]) { Visited[Neighbour] = true; NextFrontier.Add(Neighbour); }
                    }
                }
                NextFrontier.Sort(GeometryLess);
                Traversal.Targets.Append(NextFrontier);
                Frontier = MoveTemp(NextFrontier);
            }
            OutTraversals.Add(MoveTemp(Traversal));
        }
    }

    bool HasInteriorOverlap(const TArray<FVector2D>& A, const double AreaA,
        const TArray<FVector2D>& B, const double AreaB)
    {
        // No triangulation/cache allocation is needed for simple rings. Proper crossings, strict
        // interior vertices/midpoints, or coincident same-side edges imply positive-area overlap.
        for (int32 EdgeA = 0; EdgeA < A.Num(); ++EdgeA)
        {
            const FVector2D& P = A[EdgeA];
            const FVector2D& Q = A[(EdgeA + 1) % A.Num()];
            const FVector2D DirectionA = Q - P;
            const double LengthA = DirectionA.Size();
            const FBounds EdgeBounds(P, Q);
            for (int32 EdgeB = 0; EdgeB < B.Num(); ++EdgeB)
            {
                const FVector2D& R = B[EdgeB];
                const FVector2D& S = B[(EdgeB + 1) % B.Num()];
                if (!EdgeBounds.Overlaps(FBounds(R, S), Epsilon)) { continue; }
                const FVector2D DirectionB = S - R;
                const double LengthB = DirectionB.Size();
                const double Denominator = Cross(DirectionA, DirectionB);
                if (FMath::Abs(Denominator) > Epsilon * (LengthA + LengthB))
                {
                    const double T = Cross(R - P, DirectionB) / Denominator;
                    const double U = Cross(R - P, DirectionA) / Denominator;
                    if (T > Epsilon / LengthA && T < 1.0 - Epsilon / LengthA &&
                        U > Epsilon / LengthB && U < 1.0 - Epsilon / LengthB) { return true; }
                }
                else if (Dot(DirectionA, DirectionB) * (AreaA > 0.0 ? 1.0 : -1.0) * (AreaB > 0.0 ? 1.0 : -1.0) > 0.0 &&
                    FMath::Abs(Cross(DirectionA / LengthA, R - P)) <= Epsilon &&
                    FMath::Abs(Cross(DirectionA / LengthA, S - P)) <= Epsilon)
                {
                    const double Start = Dot(R - P, DirectionA) / LengthA;
                    const double End = Dot(S - P, DirectionA) / LengthA;
                    if (FMath::Min(LengthA, FMath::Max(Start, End)) - FMath::Max(0.0, FMath::Min(Start, End)) > Epsilon) { return true; }
                }
            }
        }
        auto ContainsInterior = [](const TArray<FVector2D>& Ring, const TArray<FVector2D>& Probes)
        {
            for (int32 I = 0; I < Probes.Num(); ++I)
            {
                for (const FVector2D Probe : {Probes[I], (Probes[I] + Probes[(I + 1) % Probes.Num()]) * 0.5})
                {
                    if (!FFZAPolygonMath::IsPointOnBoundary(Ring, Probe, Epsilon) &&
                        FFZAPolygonMath::IsPointInside(Ring, Probe, 0.0)) { return true; }
                }
            }
            return false;
        };
        return ContainsInterior(A, B) || ContainsInterior(B, A);
    }
}

static int32 WeldPolygonsInternal(TArray<TArray<FVector2D>>& InOutPolygons, const double ToleranceCm,
    TArray<bool>& OutChangedPolygons, FWeldStats* OutStats, const EWeldMode Mode, const int32 ManualReference)
{
    const bool bSelectedOnly = Mode != EWeldMode::Ordered;
    FWeldStats Stats;
    const int32 PolygonCount = InOutPolygons.Num();
    OutChangedPolygons.Init(false, PolygonCount);
    Stats.SourcePointKeys.SetNum(PolygonCount);
    TArray<int32> OriginalCounts;
    TArray<double> Areas;
    TArray<bool> Valid;
    TArray<FBounds> Bounds;
    OriginalCounts.SetNum(PolygonCount); Areas.SetNumZeroed(PolygonCount); Valid.Init(false, PolygonCount);
    Bounds.Reserve(PolygonCount);
    for (int32 I = 0; I < PolygonCount; ++I)
    {
        OriginalCounts[I] = InOutPolygons[I].Num();
        for (int32 Point = 0; Point < OriginalCounts[I]; ++Point) { Stats.SourcePointKeys[I].Add(static_cast<double>(Point)); }
        Valid[I] = IsValidRing(InOutPolygons[I], Areas[I]);
        if (!Valid[I]) { ++Stats.InvalidInputPolygonCount; Stats.InvalidInputPolygonIndices.Add(I); }
        Bounds.Add(Valid[I] ? FBounds(InOutPolygons[I]) : FBounds(FVector2D::ZeroVector, FVector2D::ZeroVector));
    }
    Stats.bInvalidReference = bSelectedOnly && (!Valid.IsValidIndex(ManualReference) || !Valid[ManualReference]);
    if (Stats.bInvalidReference || !FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0 || !FMath::IsFinite(ToleranceCm * ToleranceCm))
    {
        if (OutStats) { *OutStats = MoveTemp(Stats); }
        return 0;
    }
    const double ToleranceSquared = ToleranceCm * ToleranceCm;
    TArray<FWeldTraversal> Traversals;
    if (Mode == EWeldMode::SelectedGroups)
    {
        BuildGroupedTraversals(InOutPolygons, Areas, Valid, Bounds, ManualReference, ToleranceCm, Stats.Groups, Traversals);
    }
    else
    {
        FWeldTraversal Traversal;
        Traversal.Reference = ManualReference;
        if (bSelectedOnly)
        {
            Traversal.Targets = BuildSelectedOrder(InOutPolygons, Areas, Valid, Bounds, ManualReference,
                ToleranceCm, Stats.DisconnectedPolygonIndices);
        }
        else { for (int32 Target = 1; Target < PolygonCount; ++Target) { Traversal.Targets.Add(Target); } }
        Traversals.Add(MoveTemp(Traversal));
    }
    for (const FWeldTraversal& Traversal : Traversals)
    {
        TArray<int32> AcceptedReferences;
        if (bSelectedOnly) { AcceptedReferences.Add(Traversal.Reference); }
        for (const int32 Target : Traversal.Targets)
        {
            if (!Valid[Target]) { continue; }
            const TArray<FVector2D>& Original = InOutPolygons[Target];
            TArray<int32> References;
            if (bSelectedOnly)
            {
                for (const int32 Reference : AcceptedReferences)
                {
                    if (Bounds[Target].Overlaps(Bounds[Reference], ToleranceCm)) { References.Add(Reference); }
                }
            }
            else
            {
                for (int32 Reference = 0; Reference < Target; ++Reference)
                {
                    if (Valid[Reference] && Bounds[Target].Overlaps(Bounds[Reference], ToleranceCm)) { References.Add(Reference); }
                }
            }
            if (References.IsEmpty())
            {
                if (bSelectedOnly) { Stats.DisconnectedPolygonIndices.Add(Target); }
                continue;
            }
            TArray<FVector2D> SnappedVertices = Original;
            TArray<double> SnapDistances;
            SnapDistances.Init(ToleranceSquared + 1.0, Original.Num());
            TArray<TArray<FEdgeMatch>> EdgeMatches;
            EdgeMatches.SetNum(Original.Num());
            bool bHasMatch = false;
            for (int32 Edge = 0; Edge < Original.Num(); ++Edge)
            {
                const int32 Next = (Edge + 1) % Original.Num();
                const FVector2D& P = Original[Edge];
                const FVector2D& Q = Original[Next];
                const FVector2D Direction = Q - P;
                const double LengthSquared = Direction.SizeSquared();
                const FBounds EdgeBounds(P, Q);
                for (const int32 Reference : References)
                {
                    const TArray<FVector2D>& Other = InOutPolygons[Reference];
                    if (!EdgeBounds.Overlaps(Bounds[Reference], ToleranceCm)) { continue; }
                    for (int32 OtherEdge = 0; OtherEdge < Other.Num(); ++OtherEdge)
                    {
                        const FVector2D& A = Other[OtherEdge];
                        const FVector2D& B = Other[(OtherEdge + 1) % Other.Num()];
                        double StartU, EndU;
                        if (!EdgeBounds.Overlaps(FBounds(A, B), ToleranceCm) ||
                            !FindMatch(P, Q, Areas[Target], A, B, Areas[Reference], ToleranceCm, StartU, EndU)) { continue; }
                        const FVector2D Start = StartU == 0.0 ? A : A + (B - A) * StartU;
                        const FVector2D End = EndU == 1.0 ? B : A + (B - A) * EndU;
                        const double StartT = FMath::Clamp(Dot(Start - P, Direction) / LengthSquared, 0.0, 1.0);
                        const double EndT = FMath::Clamp(Dot(End - P, Direction) / LengthSquared, 0.0, 1.0);
                        EdgeMatches[Edge].Add({Reference, OtherEdge, FMath::Min(StartT, EndT), FMath::Max(StartT, EndT)});
                        bHasMatch = true;
                        // Corners and segment projections participate in the same distance comparison.
                        ConsiderVertexSnap(P, FFZAPolygonMath::ClosestPointOnSegment(P, A, B), ToleranceSquared,
                            SnappedVertices[Edge], SnapDistances[Edge]);
                        ConsiderVertexSnap(Q, FFZAPolygonMath::ClosestPointOnSegment(Q, A, B), ToleranceSquared,
                            SnappedVertices[Next], SnapDistances[Next]);
                    }
                }
            }
            if (!bHasMatch)
            {
                if (bSelectedOnly) { Stats.DisconnectedPolygonIndices.Add(Target); }
                continue;
            }
            TArray<FVector2D> Candidate;
            TArray<double> CandidateKeys;
            int32 TargetMoved = 0;
            for (int32 Edge = 0; Edge < Original.Num(); ++Edge)
            {
                Candidate.Add(SnappedVertices[Edge]); CandidateKeys.Add(Stats.SourcePointKeys[Target][Edge]);
                TargetMoved += !SamePoint(Original[Edge], SnappedVertices[Edge]) ? 1 : 0;
                const FVector2D Direction = Original[(Edge + 1) % Original.Num()] - Original[Edge];
                const double ParameterEpsilon = Epsilon / Direction.Size();
                const TArray<FEdgeMatch>& Matches = EdgeMatches[Edge];
                TArray<double> Breakpoints{0.0, 1.0};
                for (const FEdgeMatch& Match : Matches)
                {
                    Breakpoints.Add(Match.StartT); Breakpoints.Add(Match.EndT);
                }
                Breakpoints.Sort();
                for (int32 I = Breakpoints.Num() - 1; I > 0; --I)
                {
                    if (Breakpoints[I] - Breakpoints[I - 1] <= ParameterEpsilon) { Breakpoints.RemoveAt(I); }
                }
                auto AppendPoint = [&](const FVector2D& Point, const double T)
                {
                    if (T <= ParameterEpsilon || T >= 1.0 - ParameterEpsilon) { return; }
                    if (!SamePoint(Candidate.Last(), Point) && !SamePoint(SnappedVertices[(Edge + 1) % Original.Num()], Point))
                    {
                        Candidate.Add(Point);
                        CandidateKeys.Add(InterpolateKey(Stats.SourcePointKeys[Target], OriginalCounts[Target], Edge, T));
                    }
                };
                auto AppendSharedPoint = [&](const FEdgeMatch& Match, const double T)
                {
                    const TArray<FVector2D>& Other = InOutPolygons[Match.Reference];
                    const FVector2D& A = Other[Match.Edge];
                    const FVector2D& B = Other[(Match.Edge + 1) % Other.Num()];
                    const double U = FMath::Clamp(Dot(Original[Edge] + Direction * T - A, Direction) /
                        Dot(B - A, Direction), 0.0, 1.0);
                    const double EndpointEpsilon = Epsilon / (B - A).Size();
                    const FVector2D Point = U <= EndpointEpsilon ? A : U >= 1.0 - EndpointEpsilon ? B : A + (B - A) * U;
                    AppendPoint(Point, T);
                };
                // Choose ownership on OPEN intervals, then emit both sides of each transition in
                // traversal order. A nearer small neighbour must not erase the other ring's shoulder.
                for (int32 Span = 0; Span + 1 < Breakpoints.Num(); ++Span)
                {
                    const double MidT = (Breakpoints[Span] + Breakpoints[Span + 1]) * 0.5;
                    const FVector2D Source = Original[Edge] + Direction * MidT;
                    int32 BestMatch = INDEX_NONE;
                    double BestDistance = TNumericLimits<double>::Max();
                    for (int32 I = 0; I < Matches.Num(); ++I)
                    {
                        const FEdgeMatch& Match = Matches[I];
                        if (MidT < Match.StartT || MidT > Match.EndT) { continue; }
                        const TArray<FVector2D>& Other = InOutPolygons[Match.Reference];
                        const double Distance = FVector2D::DistSquared(Source,
                            FFZAPolygonMath::ClosestPointOnSegment(Source, Other[Match.Edge], Other[(Match.Edge + 1) % Other.Num()]));
                        if (Distance < BestDistance)
                        {
                            BestMatch = I; BestDistance = Distance;
                        }
                    }
                    if (BestMatch != INDEX_NONE)
                    {
                        AppendSharedPoint(Matches[BestMatch], Breakpoints[Span]);
                        AppendSharedPoint(Matches[BestMatch], Breakpoints[Span + 1]);
                    }
                    else
                    {
                        // An unsupported interval retains its authored geometry. Bridging between two
                        // matched pieces through an out-of-range shelf can create new polygon overlap.
                        AppendPoint(Original[Edge] + Direction * Breakpoints[Span], Breakpoints[Span]);
                        AppendPoint(Original[Edge] + Direction * Breakpoints[Span + 1], Breakpoints[Span + 1]);
                    }
                }
            }
            double CandidateArea;
            if (!IsValidRing(Candidate, CandidateArea) || (CandidateArea > 0.0) != (Areas[Target] > 0.0))
            {
                ++Stats.RejectedPolygonCount; Stats.RejectedPolygonIndices.Add(Target); continue;
            }
            if (bSelectedOnly)
            {
                bool bOverlapsAnchor = false;
                const FBounds CandidateBounds(Candidate);
                for (const int32 Reference : AcceptedReferences)
                {
                    if (CandidateBounds.Overlaps(Bounds[Reference], Epsilon) &&
                        HasInteriorOverlap(Candidate, CandidateArea, InOutPolygons[Reference], Areas[Reference]))
                    {
                        bOverlapsAnchor = true; break;
                    }
                }
                if (bOverlapsAnchor)
                {
                    ++Stats.RejectedPolygonCount; Stats.RejectedPolygonIndices.Add(Target); continue;
                }
            }
            // Stage the reference nodes before commit, so a failed proposal changes neither side.
            TArray<TArray<FVector2D>> NodedReferences;
            TArray<TArray<double>> NodedKeys;
            NodedReferences.SetNum(References.Num()); NodedKeys.SetNum(References.Num());
            bool bReferenceValid = true;
            for (int32 I = 0; I < References.Num(); ++I)
            {
                const int32 Reference = References[I];
                if (NodeReference(InOutPolygons[Reference], Stats.SourcePointKeys[Reference], OriginalCounts[Reference],
                    Candidate, NodedReferences[I], NodedKeys[I]))
                {
                    double ReferenceArea;
                    if (!IsValidRing(NodedReferences[I], ReferenceArea) || (ReferenceArea > 0.0) != (Areas[Reference] > 0.0))
                    {
                        bReferenceValid = false; break;
                    }
                }
                else { NodedReferences[I].Reset(); }
            }
            if (!bReferenceValid) { ++Stats.RejectedPolygonCount; Stats.RejectedPolygonIndices.Add(Target); continue; }
            const int32 TargetInserted = Candidate.Num() - Original.Num();
            if (TargetMoved > 0 || TargetInserted > 0)
            {
                for (int32 Point = 0; Point < Candidate.Num(); ++Point)
                {
                    const double Key = CandidateKeys[Point];
                    const int32 SourceEdge = FMath::FloorToInt(Key);
                    const FVector2D AuthoredPoint = FMath::Lerp(Original[SourceEdge],
                        Original[(SourceEdge + 1) % Original.Num()], Key - SourceEdge);
                    Stats.MaximumMoveCm = FMath::Max(Stats.MaximumMoveCm, FVector2D::Distance(AuthoredPoint, Candidate[Point]));
                }
                InOutPolygons[Target] = MoveTemp(Candidate);
                Stats.SourcePointKeys[Target] = MoveTemp(CandidateKeys);
                Areas[Target] = CandidateArea; Bounds[Target] = FBounds(InOutPolygons[Target]);
                OutChangedPolygons[Target] = true;
                Stats.MovedVertexCount += TargetMoved; Stats.InsertedVertexCount += TargetInserted;
            }
            if (bSelectedOnly) { AcceptedReferences.Add(Target); }
            for (int32 I = 0; I < References.Num(); ++I)
            {
                if (!NodedReferences[I].IsEmpty())
                {
                    const int32 Reference = References[I];
                    Stats.InsertedVertexCount += NodedReferences[I].Num() - InOutPolygons[Reference].Num();
                    InOutPolygons[Reference] = MoveTemp(NodedReferences[I]);
                    Stats.SourcePointKeys[Reference] = MoveTemp(NodedKeys[I]); OutChangedPolygons[Reference] = true;
                }
            }
        }
    }
    for (const bool bChanged : OutChangedPolygons) { Stats.ChangedPolygonCount += bChanged ? 1 : 0; }
    const int32 MovedCount = Stats.MovedVertexCount;
    if (OutStats) { *OutStats = MoveTemp(Stats); }
    return MovedCount;
}

int32 WeldPolygons(TArray<TArray<FVector2D>>& InOutPolygons, const double ToleranceCm,
    TArray<bool>& OutChangedPolygons, FWeldStats* OutStats)
{
    return WeldPolygonsInternal(InOutPolygons, ToleranceCm, OutChangedPolygons, OutStats, EWeldMode::Ordered, INDEX_NONE);
}

int32 WeldSelectedPolygons(TArray<TArray<FVector2D>>& InOutPolygons, const int32 ReferencePolygonIndex,
    const double ToleranceCm, TArray<bool>& OutChangedPolygons, FWeldStats* OutStats)
{
    return WeldPolygonsInternal(InOutPolygons, ToleranceCm, OutChangedPolygons, OutStats,
        EWeldMode::SelectedComponent, ReferencePolygonIndex);
}

int32 WeldSelectedPolygonGroups(TArray<TArray<FVector2D>>& InOutPolygons, const int32 PreferredReferencePolygonIndex,
    const double ToleranceCm, TArray<bool>& OutChangedPolygons, FWeldStats* OutStats)
{
    return WeldPolygonsInternal(InOutPolygons, ToleranceCm, OutChangedPolygons, OutStats,
        EWeldMode::SelectedGroups, PreferredReferencePolygonIndex);
}
}
