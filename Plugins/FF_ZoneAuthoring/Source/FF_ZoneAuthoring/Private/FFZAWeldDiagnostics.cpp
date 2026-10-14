#include "FFZAWeldDiagnostics.h"

#include "FFZAPolygonMath.h"

namespace FFZAWeldDiagnostics
{
namespace
{
    constexpr double DistanceEpsilonCm = 1e-4;
    constexpr double AreaEpsilonCm2 = 1e-6;
    // Match the weld kernel's 45-degree eligibility. A seam the kernel can attempt must not
    // disappear from the final completion gate merely because its two sides are angled.
    constexpr double FacingNormalDot = -0.7071067811865475;

    double Dot(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.X + A.Y * B.Y;
    }

    double Cross(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    bool IsFinite(const FVector2D& Point)
    {
        return FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y);
    }

    bool PointBefore(const FVector2D& A, const FVector2D& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }

    bool BoundsWithin(const FBox2D& A, const FBox2D& B, const double Radius)
    {
        return A.Min.X <= B.Max.X + Radius && A.Max.X + Radius >= B.Min.X &&
            A.Min.Y <= B.Max.Y + Radius && A.Max.Y + Radius >= B.Min.Y;
    }

    struct FEdge
    {
        // Coordinates are local to the polygon origin; translation is done once per pair.
        FVector2D Start;
        FVector2D End;
        FVector2D Direction;
        FVector2D InwardNormal;
        FBox2D Bounds = FBox2D(ForceInit);
        double Length = 0.0;
    };

    struct FTriangle
    {
        FVector2D Vertices[3];
        FBox2D Bounds = FBox2D(ForceInit);
    };

    struct FPolygonCache
    {
        int32 InputIndex = INDEX_NONE;
        FVector2D Origin = FVector2D::ZeroVector;
        TArray<FVector2D> LocalRing;
        TArray<FEdge> Edges;
        TArray<FTriangle> Triangles;
        FBox2D WorldBounds = FBox2D(ForceInit);
        int32 CanonicalStart = 0;
        int32 CanonicalStep = 1;
        bool bValid = false;
        bool bTriangulated = false;
    };

    FPolygonCache CachePolygon(const TArray<FVector2D>& Ring, const int32 InputIndex)
    {
        FPolygonCache Cache;
        Cache.InputIndex = InputIndex;
        if (Ring.Num() < 3)
        {
            return Cache;
        }
        for (const FVector2D& Point : Ring)
        {
            if (!IsFinite(Point))
            {
                return Cache;
            }
        }

        Cache.Origin = Ring[0];
        Cache.LocalRing.Reserve(Ring.Num());
        for (const FVector2D& Point : Ring)
        {
            const FVector2D LocalPoint = Point - Cache.Origin;
            if (!IsFinite(LocalPoint))
            {
                return Cache;
            }
            Cache.LocalRing.Add(LocalPoint);
            Cache.WorldBounds += Point;
        }
        // Local coordinates avoid catastrophic cancellation of area at large world offsets.
        const double SignedArea = FFZAPolygonMath::SignedArea(Cache.LocalRing);
        if (!FMath::IsFinite(SignedArea) || !FFZAPolygonMath::IsSimplePolygon(Cache.LocalRing, DistanceEpsilonCm))
        {
            return Cache;
        }
        const double Winding = SignedArea > 0.0 ? 1.0 : -1.0;
        Cache.Edges.Reserve(Ring.Num());
        for (int32 I = 0; I < Cache.LocalRing.Num(); ++I)
        {
            FEdge& Edge = Cache.Edges.AddDefaulted_GetRef();
            Edge.Start = Cache.LocalRing[I];
            Edge.End = Cache.LocalRing[(I + 1) % Cache.LocalRing.Num()];
            const FVector2D Delta = Edge.End - Edge.Start;
            Edge.Length = Delta.Size();
            if (!FMath::IsFinite(Edge.Length) || Edge.Length <= DistanceEpsilonCm)
            {
                return Cache;
            }
            Edge.Direction = Delta / Edge.Length;
            Edge.InwardNormal = FVector2D(-Edge.Direction.Y, Edge.Direction.X) * Winding;
            Edge.Bounds += Edge.Start;
            Edge.Bounds += Edge.End;
        }
        // Cache the same winding/start-independent world-space traversal used by seam
        // consensus. Equal-min-X sweep ties must select their projection axis by geometry,
        // never by input index, Zone ID, or selection order.
        for (int32 I = 1; I < Ring.Num(); ++I)
        {
            if (PointBefore(Ring[I], Ring[Cache.CanonicalStart])) { Cache.CanonicalStart = I; }
        }
        if (PointBefore(Ring[(Cache.CanonicalStart + Ring.Num() - 1) % Ring.Num()],
            Ring[(Cache.CanonicalStart + 1) % Ring.Num()]))
        {
            Cache.CanonicalStep = -1;
        }
        Cache.bValid = true;
        return Cache;
    }

    // Most selected rings only need edge gap inspection. Pay for triangulation at most once,
    // and only when an actual overlap or gap-coverage candidate needs a filled-shape test.
    void EnsureTriangles(FPolygonCache& Cache)
    {
        if (Cache.bTriangulated)
        {
            return;
        }
        Cache.bTriangulated = true;
        TArray<int32> TriangleIndices;
        if (!FFZAPolygonMath::Triangulate(Cache.LocalRing, TriangleIndices))
        {
            return;
        }
        Cache.Triangles.Reserve(TriangleIndices.Num() / 3);
        for (int32 I = 0; I < TriangleIndices.Num(); I += 3)
        {
            FTriangle& Triangle = Cache.Triangles.AddDefaulted_GetRef();
            for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
            {
                Triangle.Vertices[VertexIndex] = Cache.LocalRing[TriangleIndices[I + VertexIndex]];
                Triangle.Bounds += Triangle.Vertices[VertexIndex];
            }
        }
    }

    void AddIssue(FResult& Result, const FBoundaryIssue& Issue, const int32 MaxIssues)
    {
        if (Result.Issues.Num() < MaxIssues)
        {
            Result.Issues.Add(Issue);
        }
        else
        {
            Result.bTruncated = true;
        }
    }

    FBox2D TranslateBounds(const FBox2D& Bounds, const FVector2D& Offset)
    {
        return FBox2D(Bounds.Min + Offset, Bounds.Max + Offset);
    }

    double LocalArea(const TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return 0.0;
        }
        double TwiceArea = 0.0;
        const FVector2D Origin = Polygon[0];
        for (int32 I = 1; I + 1 < Polygon.Num(); ++I)
        {
            TwiceArea += Cross(Polygon[I] - Origin, Polygon[I + 1] - Origin);
        }
        return FMath::Abs(TwiceArea) * 0.5;
    }

    double IntersectionArea(FPolygonCache& A, FPolygonCache& B)
    {
        EnsureTriangles(A);
        EnsureTriangles(B);
        const FVector2D BOffset = B.Origin - A.Origin;
        double Area = 0.0;
        // Reuse clipping storage for every triangle pair. Neither original concave ring is
        // clipped as a whole, so disconnected intersection pieces cannot cancel or connect.
        TArray<FVector2D> Subject;
        TArray<FVector2D> Clip;
        TArray<FVector2D> Clipped;
        Subject.SetNumUninitialized(3);
        Clip.SetNumUninitialized(3);
        for (const FTriangle& TriangleA : A.Triangles)
        {
            for (int32 I = 0; I < 3; ++I)
            {
                Subject[I] = TriangleA.Vertices[I];
            }
            for (const FTriangle& TriangleB : B.Triangles)
            {
                if (!BoundsWithin(TriangleA.Bounds, TranslateBounds(TriangleB.Bounds, BOffset), 0.0))
                {
                    continue;
                }
                for (int32 I = 0; I < 3; ++I)
                {
                    Clip[I] = TriangleB.Vertices[I] + BOffset;
                }
                FFZAPolygonMath::ClipToConvex(Subject, Clip, Clipped);
                Area += LocalArea(Clipped);
            }
        }
        return Area;
    }

    // Clip an interval against a linearly varying value >= Minimum. This allows a near-parallel
    // seam that starts touching or crosses the inspection radius to report only its gap portion.
    bool ClipIntervalMinimum(const double ValueStart, const double ValueEnd, const double Minimum,
        double& InOutStart, double& InOutEnd)
    {
        if (ValueStart >= Minimum && ValueEnd >= Minimum)
        {
            return true;
        }
        if (ValueStart < Minimum && ValueEnd < Minimum)
        {
            return false;
        }
        const double Alpha = (Minimum - ValueStart) / (ValueEnd - ValueStart);
        if (ValueStart < Minimum)
        {
            InOutStart = FMath::Max(InOutStart, Alpha);
        }
        else
        {
            InOutEnd = FMath::Min(InOutEnd, Alpha);
        }
        return InOutEnd > InOutStart;
    }

    bool FindGap(const FEdge& A, const FEdge& B, const FVector2D& BOffset, const double Radius,
        FBoundaryIssue& OutIssue)
    {
        if (Dot(A.InwardNormal, B.InwardNormal) > FacingNormalDot ||
            !BoundsWithin(A.Bounds, TranslateBounds(B.Bounds, BOffset), Radius))
        {
            return false;
        }
        const FVector2D BStart = B.Start + BOffset;
        const FVector2D BEnd = B.End + BOffset;
        const double BProjectionStart = Dot(BStart - A.Start, A.Direction);
        const double BProjectionEnd = Dot(BEnd - A.Start, A.Direction);
        const double SpanStart = FMath::Max(0.0, FMath::Min(BProjectionStart, BProjectionEnd));
        const double SpanEnd = FMath::Min(A.Length, FMath::Max(BProjectionStart, BProjectionEnd));
        if (SpanEnd - SpanStart <= DistanceEpsilonCm)
        {
            return false;
        }

        const auto PointBAt = [&](const double Projection)
        {
            const double Alpha = (Projection - BProjectionStart) / (BProjectionEnd - BProjectionStart);
            return BStart + (BEnd - BStart) * Alpha;
        };
        const FVector2D AStart = A.Start + A.Direction * SpanStart;
        const FVector2D AEnd = A.Start + A.Direction * SpanEnd;
        const FVector2D MatchedBStart = PointBAt(SpanStart);
        const FVector2D MatchedBEnd = PointBAt(SpanEnd);
        const double GapAStart = -Dot(MatchedBStart - AStart, A.InwardNormal);
        const double GapAEnd = -Dot(MatchedBEnd - AEnd, A.InwardNormal);
        const double GapBStart = -Dot(AStart - MatchedBStart, B.InwardNormal);
        const double GapBEnd = -Dot(AEnd - MatchedBEnd, B.InwardNormal);
        double AlphaStart = 0.0;
        double AlphaEnd = 1.0;
        if (!ClipIntervalMinimum(GapAStart, GapAEnd, DistanceEpsilonCm, AlphaStart, AlphaEnd) ||
            !ClipIntervalMinimum(GapBStart, GapBEnd, DistanceEpsilonCm, AlphaStart, AlphaEnd) ||
            !ClipIntervalMinimum(-GapAStart, -GapAEnd, -Radius, AlphaStart, AlphaEnd) ||
            (AlphaEnd - AlphaStart) * (SpanEnd - SpanStart) <= DistanceEpsilonCm)
        {
            return false;
        }
        OutIssue.StartA = AStart + (AEnd - AStart) * AlphaStart;
        OutIssue.EndA = AStart + (AEnd - AStart) * AlphaEnd;
        OutIssue.StartB = MatchedBStart + (MatchedBEnd - MatchedBStart) * AlphaStart;
        OutIssue.EndB = MatchedBStart + (MatchedBEnd - MatchedBStart) * AlphaEnd;
        OutIssue.MaximumGapCm = FMath::Max((OutIssue.StartB - OutIssue.StartA).Size(),
            (OutIssue.EndB - OutIssue.EndA).Size());
        return OutIssue.MaximumGapCm > DistanceEpsilonCm;
    }

    struct FCoveredInterval
    {
        double Start = 0.0;
        double End = 0.0;
    };

    // A pair of facing edges is not a residual gap where a selected Zone occupies its strip,
    // including another arm of either concave ring. Project actual triangle/strip intersections
    // onto the seam axis, merge coverage,
    // and show only exposed intervals. Any narrower remaining voids are inspected as the
    // corresponding pair with that intervening Zone, instead of drawing through its interior.
    bool AddExposedGapSpans(const FBoundaryIssue& Gap, const FVector2D& Origin,
        TArray<FPolygonCache>& Caches, const TArray<int32>& SweepOrder, const TArray<double>& SweepPrefixMaxX,
        FResult& Result, const int32 MaxIssues)
    {
        const FVector2D AxisDelta = Gap.EndA - Gap.StartA;
        const double Length = AxisDelta.Size();
        const FVector2D Axis = AxisDelta / Length;
        const TArray<FVector2D> Strip = { Gap.StartA, Gap.EndA, Gap.EndB, Gap.StartB };
        const FBox2D LocalStripBounds = FFZAPolygonMath::GetBounds(Strip);
        const FBox2D WorldStripBounds = TranslateBounds(LocalStripBounds, Origin);
        TArray<FCoveredInterval> Covered;
        TArray<FVector2D> TrianglePoints;
        TArray<FVector2D> Clipped;
        TrianglePoints.SetNumUninitialized(3);
        // A prefix maximum lets the coverage scan skip all earlier boxes that end before the
        // strip. Unlike lower-bounding Min.X directly, this also keeps long spanning Zones.
        int32 FirstCandidate = 0;
        int32 LastCandidate = SweepPrefixMaxX.Num();
        while (FirstCandidate < LastCandidate)
        {
            const int32 Middle = FirstCandidate + (LastCandidate - FirstCandidate) / 2;
            if (SweepPrefixMaxX[Middle] < WorldStripBounds.Min.X)
            {
                FirstCandidate = Middle + 1;
            }
            else
            {
                LastCandidate = Middle;
            }
        }
        for (int32 SweepIndex = FirstCandidate; SweepIndex < SweepOrder.Num(); ++SweepIndex)
        {
            const int32 PolygonIndex = SweepOrder[SweepIndex];
            FPolygonCache& Other = Caches[PolygonIndex];
            if (Other.WorldBounds.Min.X > WorldStripBounds.Max.X)
            {
                break;
            }
            if (!BoundsWithin(Other.WorldBounds, WorldStripBounds, 0.0))
            {
                continue;
            }
            EnsureTriangles(Other);
            const FVector2D Offset = Other.Origin - Origin;
            for (const FTriangle& Triangle : Other.Triangles)
            {
                if (!BoundsWithin(TranslateBounds(Triangle.Bounds, Offset), LocalStripBounds, 0.0))
                {
                    continue;
                }
                for (int32 I = 0; I < 3; ++I)
                {
                    TrianglePoints[I] = Triangle.Vertices[I] + Offset;
                }
                FFZAPolygonMath::ClipToConvex(TrianglePoints, Strip, Clipped);
                // The gap's two own edges touch the strip with zero area and do not cover it.
                if (LocalArea(Clipped) <= AreaEpsilonCm2)
                {
                    continue;
                }
                FCoveredInterval Interval;
                Interval.Start = Length;
                Interval.End = 0.0;
                for (const FVector2D& Point : Clipped)
                {
                    const double Projection = FMath::Clamp(Dot(Point - Gap.StartA, Axis), 0.0, Length);
                    Interval.Start = FMath::Min(Interval.Start, Projection);
                    Interval.End = FMath::Max(Interval.End, Projection);
                }
                if (Interval.End - Interval.Start > DistanceEpsilonCm)
                {
                    Covered.Add(Interval);
                }
            }
        }
        Covered.Sort([](const FCoveredInterval& A, const FCoveredInterval& B)
        {
            return A.Start == B.Start ? A.End < B.End : A.Start < B.Start;
        });
        bool bHasExposedSpan = false;
        const auto EmitSpan = [&](const double Start, const double End)
        {
            if (End - Start <= DistanceEpsilonCm)
            {
                return;
            }
            FBoundaryIssue Issue = Gap;
            const double AlphaStart = Start / Length;
            const double AlphaEnd = End / Length;
            Issue.StartA = Gap.StartA + AxisDelta * AlphaStart + Origin;
            Issue.EndA = Gap.StartA + AxisDelta * AlphaEnd + Origin;
            Issue.StartB = Gap.StartB + (Gap.EndB - Gap.StartB) * AlphaStart + Origin;
            Issue.EndB = Gap.StartB + (Gap.EndB - Gap.StartB) * AlphaEnd + Origin;
            Issue.MaximumGapCm = FMath::Max((Issue.StartB - Issue.StartA).Size(), (Issue.EndB - Issue.EndA).Size());
            bHasExposedSpan = true;
            AddIssue(Result, Issue, MaxIssues);
        };
        double ExposedStart = 0.0;
        for (const FCoveredInterval& Interval : Covered)
        {
            if (Interval.Start > ExposedStart + DistanceEpsilonCm)
            {
                EmitSpan(ExposedStart, Interval.Start);
            }
            ExposedStart = FMath::Max(ExposedStart, Interval.End);
        }
        EmitSpan(ExposedStart, Length);
        return bHasExposedSpan;
    }

    void AppendEdgeIntersectionParameters(const FEdge& A, const FEdge& B,
        const FVector2D& BOffset, TArray<double>& OutParameters)
    {
        if (!BoundsWithin(A.Bounds, TranslateBounds(B.Bounds, BOffset), DistanceEpsilonCm))
        {
            return;
        }
        const FVector2D AB = A.End - A.Start;
        const FVector2D CD = B.End - B.Start;
        const FVector2D AC = B.Start + BOffset - A.Start;
        const double Denominator = Cross(AB, CD);
        // Distance-scaled parallel threshold, rather than comparing cm2 to a distance.
        if (FMath::Abs(Denominator) <= DistanceEpsilonCm * FMath::Max(A.Length, B.Length))
        {
            if (FMath::Abs(Cross(AB, AC)) <= DistanceEpsilonCm * A.Length)
            {
                OutParameters.Add(FMath::Clamp(Dot(AC, A.Direction) / A.Length, 0.0, 1.0));
                OutParameters.Add(FMath::Clamp(Dot(B.End + BOffset - A.Start, A.Direction) / A.Length, 0.0, 1.0));
            }
            return;
        }
        const double AlphaA = Cross(AC, CD) / Denominator;
        const double AlphaB = Cross(AC, AB) / Denominator;
        if (AlphaA >= 0.0 && AlphaA <= 1.0 && AlphaB >= 0.0 && AlphaB <= 1.0)
        {
            OutParameters.Add(AlphaA);
        }
    }

    void AddOverlapPerimeter(const FPolygonCache& Owner, const FPolygonCache& Other,
        const double PairArea, const bool bRelatedToRejection, FResult& Result, const int32 MaxIssues)
    {
        const FVector2D OtherOffset = Other.Origin - Owner.Origin;
        TArray<double> Parameters;
        for (const FEdge& Edge : Owner.Edges)
        {
            Parameters.Reset();
            Parameters.Add(0.0);
            Parameters.Add(1.0);
            for (const FEdge& OtherEdge : Other.Edges)
            {
                AppendEdgeIntersectionParameters(Edge, OtherEdge, OtherOffset, Parameters);
            }
            Parameters.Sort();
            double Start = Parameters[0];
            for (int32 I = 1; I < Parameters.Num(); ++I)
            {
                const double End = Parameters[I];
                if ((End - Start) * Edge.Length <= DistanceEpsilonCm)
                {
                    continue;
                }
                const FVector2D Middle = Edge.Start + (Edge.End - Edge.Start) * ((Start + End) * 0.5);
                if (FFZAPolygonMath::IsPointInside(Other.LocalRing, Middle - OtherOffset, DistanceEpsilonCm))
                {
                    FBoundaryIssue Issue;
                    Issue.Type = EIssueType::Overlap;
                    // Keep the perimeter owner so the editor can interpolate its actual height.
                    Issue.PolygonIndexA = Owner.InputIndex;
                    Issue.PolygonIndexB = Other.InputIndex;
                    Issue.StartA = Edge.Start + (Edge.End - Edge.Start) * Start + Owner.Origin;
                    Issue.EndA = Edge.Start + (Edge.End - Edge.Start) * End + Owner.Origin;
                    Issue.StartB = Issue.StartA;
                    Issue.EndB = Issue.EndA;
                    Issue.OverlapAreaCm2 = PairArea;
                    Issue.bRelatedToRejection = bRelatedToRejection;
                    AddIssue(Result, Issue, MaxIssues);
                }
                Start = End;
            }
        }
    }
}

void Analyze(const TArray<TArray<FVector2D>>& ResultPolys, const TArray<int32>& RejectedIndices,
    const double GapInspectionCm, FResult& OutResult, const int32 MaxIssues)
{
    OutResult = FResult();
    OutResult.GapInspectionCm = FMath::IsFinite(GapInspectionCm) ? FMath::Max(0.0, GapInspectionCm) : 0.0;
    const int32 IssueLimit = FMath::Max(0, MaxIssues);
    TSet<int32> Rejected;
    for (const int32 Index : RejectedIndices)
    {
        if (ResultPolys.IsValidIndex(Index))
        {
            Rejected.Add(Index);
        }
    }
    OutResult.RejectedPolygonCount = Rejected.Num();

    TArray<FPolygonCache> Caches;
    TArray<int32> SweepOrder;
    Caches.Reserve(ResultPolys.Num());
    SweepOrder.Reserve(ResultPolys.Num());
    for (int32 I = 0; I < ResultPolys.Num(); ++I)
    {
        Caches.Add(CachePolygon(ResultPolys[I], I));
        if (Caches.Last().bValid)
        {
            SweepOrder.Add(I);
        }
    }
    SweepOrder.Sort([&](const int32 A, const int32 B)
    {
        const FPolygonCache& CacheA = Caches[A];
        const FPolygonCache& CacheB = Caches[B];
        if (CacheA.WorldBounds.Min != CacheB.WorldBounds.Min)
        {
            return PointBefore(CacheA.WorldBounds.Min, CacheB.WorldBounds.Min);
        }
        if (CacheA.WorldBounds.Max != CacheB.WorldBounds.Max)
        {
            return PointBefore(CacheA.WorldBounds.Max, CacheB.WorldBounds.Max);
        }
        const TArray<FVector2D>& RingA = ResultPolys[A];
        const TArray<FVector2D>& RingB = ResultPolys[B];
        if (RingA.Num() != RingB.Num()) { return RingA.Num() < RingB.Num(); }
        for (int32 I = 0; I < RingA.Num(); ++I)
        {
            const FVector2D& P = RingA[(CacheA.CanonicalStart + I * CacheA.CanonicalStep + RingA.Num()) % RingA.Num()];
            const FVector2D& Q = RingB[(CacheB.CanonicalStart + I * CacheB.CanonicalStep + RingB.Num()) % RingB.Num()];
            if (P != Q) { return PointBefore(P, Q); }
        }
        return false;
    });
    TArray<double> SweepPrefixMaxX;
    SweepPrefixMaxX.Reserve(SweepOrder.Num());
    for (const int32 Index : SweepOrder)
    {
        const double MaxX = Caches[Index].WorldBounds.Max.X;
        SweepPrefixMaxX.Add(SweepPrefixMaxX.IsEmpty() ? MaxX : FMath::Max(SweepPrefixMaxX.Last(), MaxX));
    }

    for (int32 SweepA = 0; SweepA < SweepOrder.Num(); ++SweepA)
    {
        FPolygonCache& A = Caches[SweepOrder[SweepA]];
        for (int32 SweepB = SweepA + 1; SweepB < SweepOrder.Num(); ++SweepB)
        {
            FPolygonCache& B = Caches[SweepOrder[SweepB]];
            if (B.WorldBounds.Min.X > A.WorldBounds.Max.X + OutResult.GapInspectionCm)
            {
                break;
            }
            if (!BoundsWithin(A.WorldBounds, B.WorldBounds, OutResult.GapInspectionCm))
            {
                continue;
            }
            const bool bRelatedToRejection = Rejected.Contains(A.InputIndex) || Rejected.Contains(B.InputIndex);
            const double PairArea = BoundsWithin(A.WorldBounds, B.WorldBounds, 0.0) ? IntersectionArea(A, B) : 0.0;
            if (PairArea > AreaEpsilonCm2)
            {
                ++OutResult.OverlapPairCount;
                // Once the display budget is full, skip span construction but keep measuring
                // all pair areas and counts. Any overlap has at least one perimeter segment.
                if (OutResult.Issues.Num() >= IssueLimit)
                {
                    OutResult.bTruncated = true;
                }
                else
                {
                    AddOverlapPerimeter(A, B, PairArea, bRelatedToRejection, OutResult, IssueLimit);
                    AddOverlapPerimeter(B, A, PairArea, bRelatedToRejection, OutResult, IssueLimit);
                }
                continue; // A shallow positive-area overlap must never be labelled a gap.
            }
            bool bPairHasGap = false;
            if (OutResult.GapInspectionCm > DistanceEpsilonCm)
            {
                const FVector2D BOffset = B.Origin - A.Origin;
                for (const FEdge& EdgeA : A.Edges)
                {
                    for (const FEdge& EdgeB : B.Edges)
                    {
                        FBoundaryIssue Issue;
                        if (!FindGap(EdgeA, EdgeB, BOffset, OutResult.GapInspectionCm, Issue))
                        {
                            continue;
                        }
                        Issue.Type = EIssueType::Gap;
                        Issue.PolygonIndexA = A.InputIndex;
                        Issue.PolygonIndexB = B.InputIndex;
                        Issue.bRelatedToRejection = bRelatedToRejection;
                        bPairHasGap |= AddExposedGapSpans(Issue, A.Origin, Caches, SweepOrder, SweepPrefixMaxX, OutResult, IssueLimit);
                    }
                }
            }
            OutResult.GapPairCount += bPairHasGap ? 1 : 0;
        }
    }

    // Append rejection context after measured residuals. Finite segments remain useful even for
    // an invalid ring, but an empty/nonfinite shape yields only its rejected count/row in the UI.
    for (int32 I = 0; I < ResultPolys.Num(); ++I)
    {
        if (!Rejected.Contains(I))
        {
            continue;
        }
        const TArray<FVector2D>& Ring = ResultPolys[I];
        for (int32 PointIndex = 0; PointIndex < Ring.Num(); ++PointIndex)
        {
            const FVector2D& Start = Ring[PointIndex];
            const FVector2D& End = Ring[(PointIndex + 1) % Ring.Num()];
            if (!IsFinite(Start) || !IsFinite(End) || (End - Start).SizeSquared() <= FMath::Square(DistanceEpsilonCm))
            {
                continue;
            }
            FBoundaryIssue Issue;
            Issue.Type = EIssueType::Rejected;
            Issue.PolygonIndexA = I;
            Issue.StartA = Start;
            Issue.EndA = End;
            Issue.StartB = Start;
            Issue.EndB = End;
            Issue.bRelatedToRejection = true;
            AddIssue(OutResult, Issue, IssueLimit);
        }
    }
}
}
