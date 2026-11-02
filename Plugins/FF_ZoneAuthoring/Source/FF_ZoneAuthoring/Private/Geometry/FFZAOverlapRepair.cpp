#include "FFZAOverlapRepair.h"

#include "FFZAPolygonMath.h"
#include "Algo/Reverse.h"
#include "Curve/PolygonIntersectionUtils.h"

namespace FFZAOverlapRepair
{
namespace
{
    constexpr double DistanceEpsilonCm = 1.e-4;
    constexpr double DistanceEpsilonSquared = DistanceEpsilonCm * DistanceEpsilonCm;
    constexpr double AreaEpsilonCm2 = 1.e-6;

    double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
    double Dot(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
    bool SamePoint(const FVector2D& A, const FVector2D& B)
    {
        return FVector2D::DistSquared(A, B) <= DistanceEpsilonSquared;
    }

    double LocalSignedArea(const TArray<FVector2D>& Ring)
    {
        double TwiceArea = 0.0;
        for (int32 I = 1; I + 1 < Ring.Num(); ++I)
        {
            TwiceArea += Cross(Ring[I] - Ring[0], Ring[I + 1] - Ring[0]);
        }
        return TwiceArea * 0.5;
    }

    struct FRingCache
    {
        FBox2D Bounds = FBox2D(ForceInit);
        double SignedArea = 0.0;
        int32 CanonicalStart = 0;
        int32 CanonicalDirection = 1;
    };

    bool CacheRing(const TArray<FVector2D>& Ring, FRingCache& OutCache)
    {
        if (Ring.Num() < 3) { return false; }
        TArray<FVector2D> Local;
        Local.Reserve(Ring.Num());
        for (int32 I = 0; I < Ring.Num(); ++I)
        {
            const FVector2D& Point = Ring[I];
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)) { return false; }
            Local.Add(Point - Ring[0]);
            OutCache.Bounds += Point;
            const FVector2D& Start = Ring[OutCache.CanonicalStart];
            if (Point.X < Start.X || (Point.X == Start.X && Point.Y < Start.Y))
            {
                OutCache.CanonicalStart = I;
            }
            const FVector2D Previous = Point - Ring[(I + Ring.Num() - 1) % Ring.Num()];
            const FVector2D Next = Ring[(I + 1) % Ring.Num()] - Point;
            if (Dot(Previous, Next) < 0.0 &&
                FMath::Abs(Cross(Previous, Next)) <= DistanceEpsilonCm * (Previous.Size() + Next.Size()))
            {
                return false;
            }
        }
        OutCache.SignedArea = LocalSignedArea(Local);
        if (!FMath::IsFinite(OutCache.SignedArea) || FMath::Abs(OutCache.SignedArea) <= AreaEpsilonCm2 ||
            !FFZAPolygonMath::IsSimplePolygon(Local, DistanceEpsilonCm))
        {
            return false;
        }
        const FVector2D& Next = Ring[(OutCache.CanonicalStart + 1) % Ring.Num()];
        const FVector2D& Previous = Ring[(OutCache.CanonicalStart + Ring.Num() - 1) % Ring.Num()];
        OutCache.CanonicalDirection = Next.X < Previous.X || (Next.X == Previous.X && Next.Y < Previous.Y) ? 1 : -1;
        return true;
    }

    bool BoundsOverlap(const FBox2D& A, const FBox2D& B, const double Padding = 0.0)
    {
        return A.Min.X <= B.Max.X + Padding && A.Max.X + Padding >= B.Min.X &&
            A.Min.Y <= B.Max.Y + Padding && A.Max.Y + Padding >= B.Min.Y;
    }

    // Geometry is the tie-break, never caller order or authored Zone IDs.
    bool GeometryBefore(const int32 A, const int32 B, const TArray<TArray<FVector2D>>& Rings,
        const TArray<FRingCache>& Cache)
    {
        const double AreaA = FMath::Abs(Cache[A].SignedArea), AreaB = FMath::Abs(Cache[B].SignedArea);
        if (AreaA != AreaB) { return AreaA > AreaB; }
        const TArray<FVector2D>& RingA = Rings[A];
        const TArray<FVector2D>& RingB = Rings[B];
        if (RingA.Num() != RingB.Num()) { return RingA.Num() < RingB.Num(); }
        for (int32 I = 0; I < RingA.Num(); ++I)
        {
            const FVector2D& P = RingA[(Cache[A].CanonicalStart + Cache[A].CanonicalDirection * I + RingA.Num()) % RingA.Num()];
            const FVector2D& Q = RingB[(Cache[B].CanonicalStart + Cache[B].CanonicalDirection * I + RingB.Num()) % RingB.Num()];
            if (P.X != Q.X) { return P.X < Q.X; }
            if (P.Y != Q.Y) { return P.Y < Q.Y; }
        }
        return A < B; // Indistinguishable duplicate rings will be rejected as empty ownership.
    }

    void BuildCandidates(const TArray<FRingCache>& Cache, TArray<TArray<int32>>& OutCandidates)
    {
        OutCandidates.SetNum(Cache.Num());
        TArray<int32> Sweep;
        Sweep.Reserve(Cache.Num());
        for (int32 I = 0; I < Cache.Num(); ++I) { Sweep.Add(I); }
        Sweep.Sort([&Cache](const int32 A, const int32 B)
        {
            return Cache[A].Bounds.Min.X == Cache[B].Bounds.Min.X ? A < B : Cache[A].Bounds.Min.X < Cache[B].Bounds.Min.X;
        });
        for (int32 I = 0; I < Sweep.Num(); ++I)
        {
            const int32 A = Sweep[I];
            for (int32 J = I + 1; J < Sweep.Num(); ++J)
            {
                const int32 B = Sweep[J];
                if (Cache[B].Bounds.Min.X > Cache[A].Bounds.Max.X + DistanceEpsilonCm) { break; }
                if (BoundsOverlap(Cache[A].Bounds, Cache[B].Bounds, DistanceEpsilonCm))
                {
                    OutCandidates[A].Add(B); OutCandidates[B].Add(A);
                }
            }
        }
    }

    UE::Geometry::FGeneralPolygon2d ToGeneralPolygon(const TArray<FVector2D>& Ring, const FVector2D& Origin)
    {
        TArray<FVector2D> Local;
        Local.Reserve(Ring.Num());
        for (const FVector2D& Point : Ring) { Local.Add(Point - Origin); }
        // The engine Boolean arrays use a nonzero fill rule. Give every occupied outer
        // contour the same winding so differently authored Zones cannot cancel a clip.
        // The repaired target regains its authored winding after the Boolean operation.
        if (LocalSignedArea(Local) < 0.0) { Algo::Reverse(Local); }
        int32 Start = 0;
        for (int32 Point = 1; Point < Local.Num(); ++Point)
        {
            if (Local[Point].X < Local[Start].X ||
                (Local[Point].X == Local[Start].X && Local[Point].Y < Local[Start].Y)) { Start = Point; }
        }
        // Identical occupied contours must reach Clipper with identical traversal starts.
        // Otherwise direction/start changes can retain different redundant seam nodes.
        if (Start != 0)
        {
            TArray<FVector2D> Canonical;
            Canonical.Reserve(Local.Num());
            for (int32 Point = 0; Point < Local.Num(); ++Point) { Canonical.Add(Local[(Start + Point) % Local.Num()]); }
            Local = MoveTemp(Canonical);
        }
        return UE::Geometry::FGeneralPolygon2d(UE::Geometry::FPolygon2d(Local));
    }

    double NormalizeKey(const double Key, const int32 OriginalCount)
    {
        double Result = FMath::Fmod(Key, static_cast<double>(OriginalCount));
        if (Result < 0.0) { Result += OriginalCount; }
        return Result >= OriginalCount ? 0.0 : Result;
    }

    double NearestAuthoredKey(const FVector2D& Point, const TArray<FVector2D>& Authored)
    {
        double BestDistance = TNumericLimits<double>::Max();
        double BestKey = 0.0;
        for (int32 Edge = 0; Edge < Authored.Num(); ++Edge)
        {
            const FVector2D Direction = Authored[(Edge + 1) % Authored.Num()] - Authored[Edge];
            const double T = FMath::Clamp(Dot(Point - Authored[Edge], Direction) / Direction.SizeSquared(), 0.0, 1.0);
            const double Distance = FVector2D::DistSquared(Point, Authored[Edge] + Direction * T);
            if (Distance < BestDistance)
            {
                BestDistance = Distance; BestKey = NormalizeKey(Edge + T, Authored.Num());
            }
        }
        return BestKey;
    }

    bool ParameterOnAuthoredEdge(const double Key, const int32 Edge, const int32 OriginalCount, double& OutT)
    {
        OutT = Key - Edge;
        if (OutT < -1.e-9) { OutT += OriginalCount; }
        if (OutT < -1.e-9 || OutT > 1.0 + 1.e-9) { return false; }
        OutT = FMath::Clamp(OutT, 0.0, 1.0);
        return true;
    }

    double InterpolateKey(const TArray<FVector2D>& Ring, const TArray<double>& Keys,
        const int32 Edge, const double T, const TArray<FVector2D>& Authored)
    {
        const double StartKey = Keys[Edge], EndKey = Keys[(Edge + 1) % Keys.Num()];
        const int32 StartEdge = FMath::FloorToInt(StartKey);
        // A retained corner belongs to both adjacent original edges. Consider the previous
        // edge explicitly, including the closing edge ending at key 0. Decreasing keys on
        // that SAME authored edge interpolate in reverse instead of wrapping the whole ring.
        const int32 CandidateEdges[2] = {StartEdge, (StartEdge + Authored.Num() - 1) % Authored.Num()};
        for (const int32 Candidate : CandidateEdges)
        {
            double StartT = 0.0, EndT = 0.0;
            if (ParameterOnAuthoredEdge(StartKey, Candidate, Authored.Num(), StartT) &&
                ParameterOnAuthoredEdge(EndKey, Candidate, Authored.Num(), EndT))
            {
                return NormalizeKey(Candidate + FMath::Lerp(StartT, EndT, T), Authored.Num());
            }
        }
        // Boolean seams may join different original edges. Perimeter-key interpolation has
        // no geometric meaning on such a seam; map its current point onto the authored ring.
        return NearestAuthoredKey(FMath::Lerp(Ring[Edge], Ring[(Edge + 1) % Ring.Num()], T), Authored);
    }

    FVector2D AuthoredPoint(const TArray<FVector2D>& Authored, const double Key)
    {
        const int32 Edge = FMath::FloorToInt(Key);
        return FMath::Lerp(Authored[Edge], Authored[(Edge + 1) % Authored.Num()], Key - Edge);
    }

    double BoundaryDistanceSquared(const FVector2D& Point, const TArray<FVector2D>& Ring)
    {
        double Result = TNumericLimits<double>::Max();
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            Result = FMath::Min(Result, FVector2D::DistSquared(Point,
                FFZAPolygonMath::ClosestPointOnSegment(Point, Ring[Edge], Ring[(Edge + 1) % Ring.Num()])));
        }
        return Result;
    }

    // Exact retained vertices keep their authored key. New boolean intersections interpolate
    // through the current edge's keys, composing earlier gap repair into the ORIGINAL ring.
    double KeyForPoint(const FVector2D& Point, const TArray<FVector2D>& Ring,
        const TArray<double>& Keys, const TArray<FVector2D>& Authored)
    {
        for (int32 I = 0; I < Ring.Num(); ++I)
        {
            if (SamePoint(Point, Ring[I])) { return Keys[I]; }
        }
        double BestDistance = TNumericLimits<double>::Max();
        double BestKey = 0.0;
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const FVector2D Direction = Ring[(Edge + 1) % Ring.Num()] - Ring[Edge];
            const double T = FMath::Clamp(Dot(Point - Ring[Edge], Direction) / Direction.SizeSquared(), 0.0, 1.0);
            const double Distance = FVector2D::DistSquared(Point, Ring[Edge] + Direction * T);
            if (Distance < BestDistance)
            {
                BestDistance = Distance; BestKey = InterpolateKey(Ring, Keys, Edge, T, Authored);
            }
        }
        return BestKey;
    }

    void RestoreExactPoints(TArray<FVector2D>& Result, const TArray<FVector2D>& Target,
        const TArray<TArray<FVector2D>>& Rings, const TArray<int32>& Owners)
    {
        for (FVector2D& Point : Result)
        {
            bool bExact = false;
            // Clipper uses integer conversion. Recover owner corners/projections so paired
            // seam coordinates are shared exactly rather than merely numerically close.
            for (const int32 Owner : Owners)
            {
                for (const FVector2D& Corner : Rings[Owner])
                {
                    if (SamePoint(Point, Corner)) { Point = Corner; bExact = true; break; }
                }
                if (bExact) { break; }
                for (int32 Edge = 0; Edge < Rings[Owner].Num(); ++Edge)
                {
                    const FVector2D Closest = FFZAPolygonMath::ClosestPointOnSegment(Point,
                        Rings[Owner][Edge], Rings[Owner][(Edge + 1) % Rings[Owner].Num()]);
                    if (SamePoint(Point, Closest)) { Point = Closest; bExact = true; break; }
                }
                if (bExact) { break; }
            }
            if (bExact) { continue; }
            for (const FVector2D& Existing : Target)
            {
                if (SamePoint(Point, Existing)) { Point = Existing; break; }
            }
        }
        for (int32 I = Result.Num() - 1; I >= 0 && Result.Num() > 1; --I)
        {
            if (SamePoint(Result[I], Result[(I + 1) % Result.Num()])) { Result.RemoveAt(I); }
        }
    }

    bool WithinAuthoredBound(const TArray<FVector2D>& Ring, const TArray<double>& Keys,
        const TArray<FVector2D>& Authored, const double BoundSquared)
    {
        for (int32 I = 0; I < Ring.Num(); ++I)
        {
            if (!FMath::IsFinite(Keys[I]) || Keys[I] < 0.0 || Keys[I] >= Authored.Num() ||
                FVector2D::DistSquared(Ring[I], AuthoredPoint(Authored, Keys[I])) > BoundSquared)
            {
                return false;
            }
        }
        // Clipping can remove corners. The surviving Zone boundary must still stay within
        // the same repair bound of those authored corners; narrow output points alone do not
        // justify deleting a distant authored part.
        for (const FVector2D& Point : Authored)
        {
            if (BoundaryDistanceSquared(Point, Ring) > BoundSquared) { return false; }
        }
        return true;
    }

    struct FSeamNode { double T; FVector2D Point; };

    void NodeRing(const int32 Index, const TArray<TArray<FVector2D>>& Rings,
        const TArray<TArray<double>>& Keys, const TArray<int32>& Candidates, const TArray<FVector2D>& Authored,
        TArray<FVector2D>& OutRing, TArray<double>& OutKeys)
    {
        const TArray<FVector2D>& Ring = Rings[Index];
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const FVector2D& A = Ring[Edge];
            const FVector2D& B = Ring[(Edge + 1) % Ring.Num()];
            const FVector2D Direction = B - A;
            const double LengthSquared = Direction.SizeSquared();
            const FBox2D EdgeBounds(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)),
                FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)));
            TArray<FSeamNode> Nodes;
            for (const int32 Other : Candidates)
            {
                for (const FVector2D& Point : Rings[Other])
                {
                    if (Point.X < EdgeBounds.Min.X - DistanceEpsilonCm || Point.X > EdgeBounds.Max.X + DistanceEpsilonCm ||
                        Point.Y < EdgeBounds.Min.Y - DistanceEpsilonCm || Point.Y > EdgeBounds.Max.Y + DistanceEpsilonCm ||
                        SamePoint(Point, A) || SamePoint(Point, B)) { continue; }
                    const double T = Dot(Point - A, Direction) / LengthSquared;
                    if (T > 0.0 && T < 1.0 && SamePoint(Point, A + Direction * T)) { Nodes.Add({T, Point}); }
                }
            }
            Nodes.Sort([](const FSeamNode& Left, const FSeamNode& Right)
            {
                if (Left.T != Right.T) { return Left.T < Right.T; }
                if (Left.Point.X != Right.Point.X) { return Left.Point.X < Right.Point.X; }
                return Left.Point.Y < Right.Point.Y;
            });
            OutRing.Add(A); OutKeys.Add(Keys[Index][Edge]);
            for (const FSeamNode& Node : Nodes)
            {
                if (!SamePoint(Node.Point, OutRing.Last()))
                {
                    OutRing.Add(Node.Point); OutKeys.Add(InterpolateKey(Ring, Keys[Index], Edge, Node.T, Authored));
                }
            }
        }
    }
}

bool Repair(TArray<TArray<FVector2D>>& InOutPolygons, const int32 PreferredReferencePolygonIndex,
    const TArray<FFZALayoutOps::FWeldGroup>& Groups, TArray<TArray<double>>& InOutSourcePointKeys,
    const TArray<TArray<FVector2D>>& AuthoredPolygons, const double MaxAuthoredMoveCm, FString& OutError)
{
    OutError.Reset();
    const int32 Count = InOutPolygons.Num();
    if (!InOutPolygons.IsValidIndex(PreferredReferencePolygonIndex) || AuthoredPolygons.Num() != Count ||
        InOutSourcePointKeys.Num() != Count || !FMath::IsFinite(MaxAuthoredMoveCm) || MaxAuthoredMoveCm < 0.0)
    {
        OutError = TEXT("선택 Zone의 기준, 원본 경계 또는 이동 한도가 유효하지 않습니다."); return false;
    }

    TArray<FRingCache> Cache;
    Cache.SetNum(Count);
    // Large finite sentinel means unrestricted Boolean trimming, without overflowing the
    // squared comparison. Geometry/key finiteness and topology checks remain mandatory.
    const double BoundSquared = MaxAuthoredMoveCm < 1.e154 ?
        FMath::Square(MaxAuthoredMoveCm + DistanceEpsilonCm) : TNumericLimits<double>::Max();
    for (int32 I = 0; I < Count; ++I)
    {
        FRingCache AuthoredCache;
        if (!CacheRing(InOutPolygons[I], Cache[I]) || !CacheRing(AuthoredPolygons[I], AuthoredCache) ||
            InOutSourcePointKeys[I].Num() != InOutPolygons[I].Num() ||
            !WithinAuthoredBound(InOutPolygons[I], InOutSourcePointKeys[I], AuthoredPolygons[I], BoundSquared))
        {
            OutError = FString::Printf(TEXT("Zone %d: 경계/원본 연결 키가 유효하지 않거나 허용 이동 범위를 넘습니다."), I + 1);
            return false;
        }
    }

    TArray<int32> Order;
    Order.Reserve(Count);
    Order.Add(PreferredReferencePolygonIndex);
    TArray<bool> IsReference;
    IsReference.Init(false, Count);
    IsReference[PreferredReferencePolygonIndex] = true;
    TArray<int32> References;
    for (const FFZALayoutOps::FWeldGroup& Group : Groups)
    {
        if (InOutPolygons.IsValidIndex(Group.ReferencePolygonIndex) && !IsReference[Group.ReferencePolygonIndex])
        {
            References.Add(Group.ReferencePolygonIndex); IsReference[Group.ReferencePolygonIndex] = true;
        }
    }
    References.Sort([&](const int32 A, const int32 B) { return GeometryBefore(A, B, InOutPolygons, Cache); });
    Order.Append(References);
    TArray<int32> Members;
    for (int32 I = 0; I < Count; ++I) { if (!IsReference[I]) { Members.Add(I); } }
    Members.Sort([&](const int32 A, const int32 B) { return GeometryBefore(A, B, InOutPolygons, Cache); });
    Order.Append(Members);
    TArray<int32> OrderIndex;
    OrderIndex.SetNumUninitialized(Count);
    for (int32 I = 0; I < Count; ++I) { OrderIndex[Order[I]] = I; }

    TArray<TArray<int32>> Candidates;
    BuildCandidates(Cache, Candidates);
    TArray<TArray<FVector2D>> Staged = InOutPolygons;
    TArray<TArray<double>> StagedKeys = InOutSourcePointKeys;
    for (const int32 Target : Order)
    {
        TArray<int32> Owners;
        for (const int32 Other : Candidates[Target])
        {
            if (OrderIndex[Other] < OrderIndex[Target]) { Owners.Add(Other); }
        }
        if (Owners.IsEmpty()) { continue; }
        Owners.Sort([&OrderIndex](const int32 A, const int32 B) { return OrderIndex[A] < OrderIndex[B]; });
        // A cyclic spline start must not change the floating-point translation used by
        // either Boolean rescaling. The cached minimum corner belongs to this unchanged
        // target input; source keys and authored ring order remain in their own arrays.
        const FVector2D Origin = Staged[Target][Cache[Target].CanonicalStart];
        TArray<UE::Geometry::FGeneralPolygon2d> Positive;
        TArray<UE::Geometry::FGeneralPolygon2d> Negative;
        Positive.Add(ToGeneralPolygon(Staged[Target], Origin));
        Negative.Reserve(Owners.Num());
        for (const int32 Owner : Owners) { Negative.Add(ToGeneralPolygon(Staged[Owner], Origin)); }
        TArray<UE::Geometry::FGeneralPolygon2d> Difference;
        if (!UE::Geometry::PolygonsDifference(Positive, Negative, Difference))
        {
            OutError = FString::Printf(TEXT("Zone %d: 겹침 제거 계산에 실패했습니다."), Target + 1); return false;
        }
        double ResultArea = 0.0;
        for (const UE::Geometry::FGeneralPolygon2d& Polygon : Difference) { ResultArea += FMath::Abs(Polygon.SignedArea()); }
        // Merely shared edges/corners do not require boolean normalization of the ring.
        if (FMath::Abs(Cache[Target].SignedArea) - ResultArea <= AreaEpsilonCm2) { continue; }
        if (Difference.Num() != 1 || Difference[0].HasHoles())
        {
            OutError = FString::Printf(TEXT("Zone %d: 겹침을 제거하면 Zone이 사라지거나 분리/내부 구멍이 생깁니다. 기준이나 경계를 조정해야 합니다."), Target + 1);
            return false;
        }
        TArray<FVector2D> Repaired;
        Repaired.Reserve(Difference[0].GetOuter().VertexCount());
        for (const FVector2D& Point : Difference[0].GetOuter().GetVertices()) { Repaired.Add(Point + Origin); }
        RestoreExactPoints(Repaired, Staged[Target], Staged, Owners);
        if (LocalSignedArea(Repaired) * Cache[Target].SignedArea < 0.0) { Algo::Reverse(Repaired); }
        TArray<double> RepairedKeys;
        RepairedKeys.Reserve(Repaired.Num());
        for (const FVector2D& Point : Repaired)
        {
            RepairedKeys.Add(KeyForPoint(Point, Staged[Target], StagedKeys[Target], AuthoredPolygons[Target]));
        }
        FRingCache RepairedCache;
        if (!CacheRing(Repaired, RepairedCache) || !WithinAuthoredBound(Repaired, RepairedKeys, AuthoredPolygons[Target], BoundSquared))
        {
            OutError = FString::Printf(TEXT("Zone %d: 겹침 제거가 유효한 단일 경계 또는 허용 이동 범위를 유지하지 못합니다."), Target + 1);
            return false;
        }
        Staged[Target] = MoveTemp(Repaired); StagedKeys[Target] = MoveTemp(RepairedKeys);
    }

    // Clipped ownership seams share all corner nodes on both sides. Read only staged geometry
    // during this pass so traversal order cannot add progressively different nodes.
    TArray<TArray<FVector2D>> Noded;
    TArray<TArray<double>> NodedKeys;
    Noded.SetNum(Count); NodedKeys.SetNum(Count);
    for (int32 I = 0; I < Count; ++I)
    {
        NodeRing(I, Staged, StagedKeys, Candidates[I], AuthoredPolygons[I], Noded[I], NodedKeys[I]);
        // Unchanged staged rings already passed validation. Revalidate only rings gaining
        // shared nodes, rather than paying the quadratic simplicity check for every Zone.
        FRingCache NodedCache;
        if (Noded[I].Num() != Staged[I].Num() &&
            (!CacheRing(Noded[I], NodedCache) || !WithinAuthoredBound(Noded[I], NodedKeys[I], AuthoredPolygons[I], BoundSquared)))
        {
            OutError = FString::Printf(TEXT("Zone %d: 공유 경계 점 삽입 결과가 유효하지 않습니다."), I + 1); return false;
        }
    }
    InOutPolygons = MoveTemp(Noded); InOutSourcePointKeys = MoveTemp(NodedKeys);
    return true;
}
}
