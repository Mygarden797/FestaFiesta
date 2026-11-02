#include "FFZAGapAreaRepair.h"

#include "FFZABoundaryNormalize.h"
#include "FFZAPolygonMath.h"
#include "FFZAWeldDiagnostics.h"
#include "Algo/Reverse.h"
#include "Curve/PolygonIntersectionUtils.h"

namespace FFZAGapAreaRepair
{
namespace
{
    constexpr double PositionEpsilon = 1.e-4;
    constexpr double PositionEpsilonSquared = PositionEpsilon * PositionEpsilon;
    constexpr double AreaEpsilon = 1.e-6;
    constexpr int32 MaximumPoints = 262144;
    constexpr int32 MaximumIssues = 131072;
    constexpr int32 MaximumBooleanOperations = 32768;
    constexpr int32 MaximumPasses = 32;
    constexpr int64 MaximumCoordinatedPointVisits = 4194304;

    double Dot(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
    double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
    bool PointBefore(const FVector2D& A, const FVector2D& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }
    bool SamePoint(const FVector2D& A, const FVector2D& B)
    {
        return FVector2D::DistSquared(A, B) <= PositionEpsilonSquared;
    }
    double SignedArea(const TArray<FVector2D>& Ring)
    {
        double Twice = 0.0;
        for (int32 I = 1; I + 1 < Ring.Num(); ++I)
        {
            Twice += Cross(Ring[I] - Ring[0], Ring[I + 1] - Ring[0]);
        }
        return Twice * 0.5;
    }

    void RemoveZeroAreaRetraces(TArray<FVector2D>& Ring)
    {
        constexpr double DoublePrecision = 2.2204460492503131e-16;
        bool bRemoved = true;
        while (bRemoved && Ring.Num() > 3)
        {
            bRemoved = false;
            for (int32 Point = Ring.Num() - 1; Point >= 0 && Ring.Num() > 3; --Point)
            {
                const FVector2D& A = Ring[(Point + Ring.Num() - 1) % Ring.Num()];
                const FVector2D& B = Ring[Point];
                const FVector2D& C = Ring[(Point + 1) % Ring.Num()];
                const FVector2D AB = B - A, BC = C - B;
                if (Dot(AB, BC) >= 0.0) { continue; }
                const double ProductScale = FMath::Abs(AB.X * BC.Y) + FMath::Abs(AB.Y * BC.X);
                const double CoordinateScale = FMath::Max(FMath::Max(A.GetAbsMax(), B.GetAbsMax()), C.GetAbsMax());
                const double Roundoff = DoublePrecision *
                    (64.0 * ProductScale + 8.0 * CoordinateScale * (AB.Size() + BC.Size()));
                const double AreaTolerance = FMath::Max(2.0 * AreaEpsilon, Roundoff);
                // Clipper's integer rescaling can emit a long, zero-width A->B->A spur.
                // Drop only an opposite-direction collinear walk at numerical precision;
                // ordinary forward shared-edge nodes and finite-area features are retained.
                if (FMath::Abs(Cross(AB, BC)) <= AreaTolerance)
                {
                    Ring.RemoveAt(Point);
                    bRemoved = true;
                }
            }
            for (int32 Point = Ring.Num() - 1; Point >= 0 && Ring.Num() > 3; --Point)
            {
                if (SamePoint(Ring[Point], Ring[(Point + 1) % Ring.Num()]))
                {
                    Ring.RemoveAt(Point);
                    bRemoved = true;
                }
            }
        }
    }
    bool ValidRing(const TArray<FVector2D>& Ring)
    {
        if (Ring.Num() < 3) { return false; }
        TArray<FVector2D> Local;
        Local.Reserve(Ring.Num());
        for (const FVector2D& Point : Ring)
        {
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)) { return false; }
            Local.Add(Point - Ring[0]);
        }
        const double Area = SignedArea(Local);
        return FMath::IsFinite(Area) && FMath::Abs(Area) > AreaEpsilon &&
            FFZAPolygonMath::IsSimplePolygon(Local, PositionEpsilon);
    }
    bool BoundsOverlap(const FBox2D& A, const FBox2D& B)
    {
        return A.Min.X <= B.Max.X + PositionEpsilon && A.Max.X + PositionEpsilon >= B.Min.X &&
            A.Min.Y <= B.Max.Y + PositionEpsilon && A.Max.Y + PositionEpsilon >= B.Min.Y;
    }
    struct FCanonicalRing
    {
        int32 Start = 0;
        int32 Step = 1;
        double Area = 0.0;
    };
    FCanonicalRing CanonicalRing(const TArray<FVector2D>& Ring)
    {
        FCanonicalRing Result;
        Result.Area = FMath::Abs(SignedArea(Ring));
        for (int32 I = 1; I < Ring.Num(); ++I)
        {
            if (PointBefore(Ring[I], Ring[Result.Start])) { Result.Start = I; }
        }
        if (PointBefore(Ring[(Result.Start + Ring.Num() - 1) % Ring.Num()],
            Ring[(Result.Start + 1) % Ring.Num()])) { Result.Step = -1; }
        return Result;
    }
    bool GeometryBefore(const int32 A, const int32 B, const TArray<TArray<FVector2D>>& Rings,
        const TArray<FCanonicalRing>& Canonical)
    {
        // Prefer the larger receiving shape, then compare its canonical geometry.
        if (Canonical[A].Area != Canonical[B].Area) { return Canonical[A].Area > Canonical[B].Area; }
        if (Rings[A].Num() != Rings[B].Num()) { return Rings[A].Num() < Rings[B].Num(); }
        for (int32 I = 0; I < Rings[A].Num(); ++I)
        {
            const FVector2D& P = Rings[A][(Canonical[A].Start + Canonical[A].Step * I + Rings[A].Num()) % Rings[A].Num()];
            const FVector2D& Q = Rings[B][(Canonical[B].Start + Canonical[B].Step * I + Rings[B].Num()) % Rings[B].Num()];
            if (P != Q) { return PointBefore(P, Q); }
        }
        return A < B; // Identical inputs have no positive exposed gap between themselves.
    }
    double ClosestKey(const FVector2D& Point, const TArray<FVector2D>& Ring, double& OutDistanceSquared)
    {
        double Key = 0.0;
        OutDistanceSquared = TNumericLimits<double>::Max();
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const FVector2D Delta = Ring[(Edge + 1) % Ring.Num()] - Ring[Edge];
            const double LengthSquared = Delta.SizeSquared();
            if (LengthSquared <= PositionEpsilonSquared) { continue; }
            const double T = FMath::Clamp(Dot(Point - Ring[Edge], Delta) / LengthSquared, 0.0, 1.0);
            const double Distance = FVector2D::DistSquared(Point, Ring[Edge] + Delta * T);
            if (Distance < OutDistanceSquared)
            {
                OutDistanceSquared = Distance;
                Key = Edge + T;
                if (Key >= Ring.Num()) { Key = 0.0; }
            }
        }
        return Key;
    }
    UE::Geometry::FGeneralPolygon2d LocalPolygon(const TArray<FVector2D>& Ring, const FVector2D& Origin)
    {
        TArray<FVector2D> Local;
        Local.Reserve(Ring.Num());
        for (const FVector2D& Point : Ring) { Local.Add(Point - Origin); }
        // Union combines multiple positive subjects with a nonzero fill rule. Mixing a
        // clockwise authored ring with counterclockwise Boolean output can cancel their
        // shared filled region; use one orientation for all Boolean operands.
        if (SignedArea(Local) < 0.0) { Algo::Reverse(Local); }
        return UE::Geometry::FGeneralPolygon2d(UE::Geometry::FPolygon2d(Local));
    }
    void RestoreBoundaryPoints(TArray<FVector2D>& Ring, const TArray<TArray<FVector2D>>& Current,
        const TArray<int32>& Candidates, const TArray<FVector2D>& Strip, const int32 PreferredOwner = INDEX_NONE)
    {
        for (FVector2D& Point : Ring)
        {
            bool bRestored = false;
            // Existing vertices anchor a contact. A computed taper intersection may differ
            // by a few ULPs; retaining that instead of the authored corner can disconnect
            // two subjects when Clipper converts them back to integer coordinates.
            if (Current.IsValidIndex(PreferredOwner))
            {
                for (const FVector2D& Corner : Current[PreferredOwner])
                {
                    if (SamePoint(Point, Corner)) { Point = Corner; bRestored = true; break; }
                }
            }
            if (bRestored) { continue; }
            for (const int32 Index : Candidates)
            {
                for (const FVector2D& Corner : Current[Index])
                {
                    if (SamePoint(Point, Corner)) { Point = Corner; bRestored = true; break; }
                }
                if (bRestored) { break; }
            }
            if (bRestored) { continue; }
            for (const FVector2D& Corner : Strip)
            {
                if (SamePoint(Point, Corner)) { Point = Corner; bRestored = true; break; }
            }
            if (bRestored) { continue; }
            for (const int32 Index : Candidates)
            {
                for (int32 Edge = 0; Edge < Current[Index].Num(); ++Edge)
                {
                    const FVector2D Closest = FFZAPolygonMath::ClosestPointOnSegment(Point, Current[Index][Edge],
                        Current[Index][(Edge + 1) % Current[Index].Num()]);
                    if (SamePoint(Point, Closest)) { Point = Closest; bRestored = true; break; }
                }
                if (bRestored) { break; }
            }
        }
        for (int32 I = Ring.Num() - 1; I >= 0 && Ring.Num() > 1; --I)
        {
            if (SamePoint(Ring[I], Ring[(I + 1) % Ring.Num()])) { Ring.RemoveAt(I); }
        }
        RemoveZeroAreaRetraces(Ring);
    }

    bool NodeOwnerContacts(const TArray<FVector2D>& Owner,
        const TArray<UE::Geometry::FGeneralPolygon2d>& Unoccupied, const FVector2D& Origin,
        int64& InOutPointVisits, TArray<FVector2D>& OutOwner)
    {
        TArray<FVector2D> Contacts;
        for (const UE::Geometry::FGeneralPolygon2d& Polygon : Unoccupied)
        {
            for (const FVector2D& Point : Polygon.GetOuter().GetVertices()) { Contacts.Add(Point + Origin); }
            for (const UE::Geometry::FPolygon2d& Hole : Polygon.GetHoles())
            {
                for (const FVector2D& Point : Hole.GetVertices()) { Contacts.Add(Point + Origin); }
            }
        }
        const int64 Visits = static_cast<int64>(Owner.Num()) * Contacts.Num();
        if (Visits > MaximumCoordinatedPointVisits - InOutPointVisits) { return false; }
        InOutPointVisits += Visits;
        struct FAreaContactCut { double T; FVector2D Point; };
        OutOwner.Reset();
        OutOwner.Reserve(Owner.Num() + Contacts.Num());
        for (int32 Edge = 0; Edge < Owner.Num(); ++Edge)
        {
            const FVector2D& A = Owner[Edge];
            const FVector2D& B = Owner[(Edge + 1) % Owner.Num()];
            const FVector2D Direction = B - A;
            const double LengthSquared = Direction.SizeSquared();
            TArray<FAreaContactCut> Cuts;
            for (const FVector2D& Point : Contacts)
            {
                if (SamePoint(Point, A) || SamePoint(Point, B)) { continue; }
                const double T = Dot(Point - A, Direction) / LengthSquared;
                if (T > 0.0 && T < 1.0 && SamePoint(Point, A + Direction * T)) { Cuts.Add({T, Point}); }
            }
            Cuts.Sort([](const FAreaContactCut& A, const FAreaContactCut& B)
            {
                return A.T != B.T ? A.T < B.T : PointBefore(A.Point, B.Point);
            });
            OutOwner.Add(A);
            for (const FAreaContactCut& Cut : Cuts)
            {
                if (!SamePoint(Cut.Point, OutOwner.Last())) { OutOwner.Add(Cut.Point); }
                if (OutOwner.Num() > MaximumPoints) { return false; }
            }
        }
        return true;
    }
    double GeneralArea(const TArray<UE::Geometry::FGeneralPolygon2d>& Polygons)
    {
        double Area = 0.0;
        for (const UE::Geometry::FGeneralPolygon2d& Polygon : Polygons)
        {
            Area += FMath::Abs(Polygon.SignedArea());
        }
        return Area;
    }

    void RemoveAdjacentDuplicates(TArray<FVector2D>& Ring)
    {
        for (int32 I = Ring.Num() - 1; I >= 0 && Ring.Num() > 1; --I)
        {
            if (SamePoint(Ring[I], Ring[(I + 1) % Ring.Num()])) { Ring.RemoveAt(I); }
        }
    }

    bool OnSupportingSegment(const TArray<FVector2D>& Ring, const FVector2D& SpanStart,
        const FVector2D& SpanEnd, const FVector2D& Contact)
    {
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const FVector2D& A = Ring[Edge];
            const FVector2D& B = Ring[(Edge + 1) % Ring.Num()];
            if (FVector2D::DistSquared(SpanStart, FFZAPolygonMath::ClosestPointOnSegment(SpanStart, A, B)) <= PositionEpsilonSquared &&
                FVector2D::DistSquared(SpanEnd, FFZAPolygonMath::ClosestPointOnSegment(SpanEnd, A, B)) <= PositionEpsilonSquared &&
                FVector2D::DistSquared(Contact, FFZAPolygonMath::ClosestPointOnSegment(Contact, A, B)) <= PositionEpsilonSquared)
            {
                return true;
            }
        }
        return false;
    }

    void RestoreTaperedContact(FFZAWeldDiagnostics::FBoundaryIssue& Gap,
        const TArray<TArray<FVector2D>>& Current)
    {
        const FVector2D DirectionA = Gap.EndA - Gap.StartA;
        const FVector2D DirectionB = Gap.EndB - Gap.StartB;
        const double Denominator = Cross(DirectionA, DirectionB);
        const double Scale = FMath::Sqrt(DirectionA.SizeSquared() * DirectionB.SizeSquared());
        if (!FMath::IsFinite(Scale) || Scale <= PositionEpsilonSquared ||
            FMath::Abs(Denominator) <= Scale * 1.e-12) { return; }
        const double T = Cross(Gap.StartB - Gap.StartA, DirectionB) / Denominator;
        const FVector2D Contact = Gap.StartA + DirectionA * T;
        if (!FMath::IsFinite(Contact.X) || !FMath::IsFinite(Contact.Y)) { return; }
        constexpr double ContactToleranceSquared = 0.01 * 0.01;
        // Diagnostics starts a taper at positive width 1e-4, creating a microscopic fourth
        // corner. Recover its actual line intersection, rather than passing that sliver
        // through sequential Boolean rescalings and acquiring short, crossing output edges.
        const bool bExistingContact = OnSupportingSegment(Current[Gap.PolygonIndexA], Gap.StartA, Gap.EndA, Contact) &&
            OnSupportingSegment(Current[Gap.PolygonIndexB], Gap.StartB, Gap.EndB, Contact);
        if (!bExistingContact) { return; }
        // A very shallow taper can place the true contact farther along its edge than the
        // numerical cleanup tolerance. It is still an exact existing contact when it lies
        // on both physical supporting segments; do not limit its longitudinal distance.
        if (T <= 0.0 && FVector2D::DistSquared(Gap.StartA, Gap.StartB) <= ContactToleranceSquared)
        {
            Gap.StartA = Contact; Gap.StartB = Contact;
        }
        if (T >= 1.0 && FVector2D::DistSquared(Gap.EndA, Gap.EndB) <= ContactToleranceSquared)
        {
            Gap.EndA = Contact; Gap.EndB = Contact;
        }
    }

    TArray<FVector2D> OwnerContactStrip(const TArray<FVector2D>& InputStrip,
        const TArray<FVector2D>& OwnerRing)
    {
        TArray<FVector2D> Result = InputStrip;
        if (Result.Num() != 4) { return Result; }
        constexpr double ContactToleranceSquared = 0.01 * 0.01;
        for (const TPair<int32, int32> Pair : {TPair<int32, int32>(0, 3), TPair<int32, int32>(1, 2)})
        {
            if (FVector2D::DistSquared(Result[Pair.Key], Result[Pair.Value]) > ContactToleranceSquared) { continue; }
            double FirstDistance = 0.0, SecondDistance = 0.0;
            ClosestKey(Result[Pair.Key], OwnerRing, FirstDistance);
            ClosestKey(Result[Pair.Value], OwnerRing, SecondDistance);
            const FVector2D Contact = FirstDistance < SecondDistance ? Result[Pair.Key] :
                (SecondDistance < FirstDistance ? Result[Pair.Value] :
                    (PointBefore(Result[Pair.Key], Result[Pair.Value]) ? Result[Pair.Key] : Result[Pair.Value]));
            Result[Pair.Key] = Contact; Result[Pair.Value] = Contact;
        }
        RemoveAdjacentDuplicates(Result);
        return Result;
    }

    struct FBandInterval { double Start; double End; };

    bool ClipLinearBand(const double Start, const double Delta, const double Low, const double High,
        double& InOutStart, double& InOutEnd)
    {
        if (FMath::Abs(Delta) <= 1.e-12) { return Start >= Low && Start <= High; }
        double TLow = (Low - Start) / Delta, THigh = (High - Start) / Delta;
        if (TLow > THigh) { Swap(TLow, THigh); }
        InOutStart = FMath::Max(InOutStart, TLow);
        InOutEnd = FMath::Min(InOutEnd, THigh);
        return InOutEnd >= InOutStart;
    }

    void AddDiskInterval(const FVector2D& Start, const FVector2D& Delta, const FVector2D& Center,
        const double RadiusSquared, TArray<FBandInterval>& Intervals)
    {
        const FVector2D Offset = Start - Center;
        const double A = Delta.SizeSquared();
        const double B = Dot(Offset, Delta);
        const double C = Offset.SizeSquared() - RadiusSquared;
        const double Discriminant = B * B - A * C;
        if (Discriminant < 0.0) { return; }
        const double Root = FMath::Sqrt(FMath::Max(0.0, Discriminant));
        const double Low = FMath::Max(0.0, (-B - Root) / A);
        const double High = FMath::Min(1.0, (-B + Root) / A);
        if (High >= Low) { Intervals.Add({Low, High}); }
    }

    // Exact coverage by the union of original edge capsules. Endpoint checks alone allow
    // a new long chord to cut across a distant concavity; checking every covered interval
    // bounds its entire length without arbitrary sampling or distance-dependent work.
    bool EdgeWithinOriginalBand(const FVector2D& Start, const FVector2D& End,
        const TArray<FVector2D>& Original, const double RadiusSquared)
    {
        const FVector2D Delta = End - Start;
        if (Delta.SizeSquared() <= PositionEpsilonSquared) { return false; }
        const double Radius = FMath::Sqrt(RadiusSquared);
        TArray<FBandInterval> Intervals;
        Intervals.Reserve(Original.Num() * 3);
        for (int32 Edge = 0; Edge < Original.Num(); ++Edge)
        {
            const FVector2D& A = Original[Edge];
            const FVector2D& B = Original[(Edge + 1) % Original.Num()];
            const FVector2D EdgeDelta = B - A;
            const double Length = EdgeDelta.Size();
            if (Length <= PositionEpsilon) { continue; }
            const FVector2D Axis = EdgeDelta / Length;
            const FVector2D Normal(-Axis.Y, Axis.X);
            double Low = 0.0, High = 1.0;
            if (ClipLinearBand(Dot(Start - A, Axis), Dot(Delta, Axis), 0.0, Length, Low, High) &&
                ClipLinearBand(Dot(Start - A, Normal), Dot(Delta, Normal), -Radius, Radius, Low, High))
            {
                Intervals.Add({Low, High});
            }
            AddDiskInterval(Start, Delta, A, RadiusSquared, Intervals);
            AddDiskInterval(Start, Delta, B, RadiusSquared, Intervals);
        }
        Intervals.Sort([](const FBandInterval& A, const FBandInterval& B)
        {
            return A.Start != B.Start ? A.Start < B.Start : A.End < B.End;
        });
        double CoveredEnd = 0.0;
        for (const FBandInterval& Interval : Intervals)
        {
            if (Interval.Start > CoveredEnd + 1.e-10) { return false; }
            CoveredEnd = FMath::Max(CoveredEnd, Interval.End);
            if (CoveredEnd >= 1.0 - 1.e-10) { return true; }
        }
        return false;
    }

    bool NormalizeEpoch(FResult& InOutResult, const TArray<TArray<FVector2D>>& Authored,
        const double RadiusSquared, TArray<FBox2D>& InOutBounds, int64& OutTotalPoints,
        bool& OutChanged)
    {
        TArray<TArray<FVector2D>> Staged = InOutResult.Polygons;
        TArray<TArray<double>> StagedKeys = InOutResult.SourcePointKeys;
        FString Error;
        if (!FFZABoundaryNormalize::Normalize(Staged, StagedKeys, Authored, 0.01, Error))
        {
            InOutResult.FailureReason = MoveTemp(Error);
            InOutResult.bBlocked = InOutResult.FailureReason.Contains(TEXT("한도"));
            return false;
        }
        int64 Total = 0;
        for (int32 Polygon = 0; Polygon < Staged.Num(); ++Polygon)
        {
            Total += Staged[Polygon].Num();
            if (Total > MaximumPoints)
            {
                InOutResult.bBlocked = true;
                InOutResult.FailureReason = TEXT("틈 면적 복구 경계 정리 후 꼭짓점 처리 한도를 넘었습니다."); return false;
            }
            for (int32 Point = 0; Point < Staged[Polygon].Num(); ++Point)
            {
                if (!EdgeWithinOriginalBand(Staged[Polygon][Point],
                    Staged[Polygon][(Point + 1) % Staged[Polygon].Num()], Authored[Polygon], RadiusSquared))
                {
                    InOutResult.FailureReason = TEXT("공통 접점 정리 후 경계가 원본 복구 거리를 넘습니다."); return false;
                }
            }
        }
        OutChanged = Staged != InOutResult.Polygons;
        InOutResult.Polygons = MoveTemp(Staged);
        InOutResult.SourcePointKeys = MoveTemp(StagedKeys);
        OutTotalPoints = Total;
        for (int32 Polygon = 0; Polygon < InOutResult.Polygons.Num(); ++Polygon)
        {
            InOutBounds[Polygon] = FFZAPolygonMath::GetBounds(InOutResult.Polygons[Polygon]);
        }
        return true;
    }

    // The strip is clipped against every occupied Zone before union. Sequential processing
    // cannot consume an earlier fill or create a new overlap in a third Zone.
    bool TryOwner(const int32 Owner, const TArray<FVector2D>& InputStrip,
        const TArray<TArray<FVector2D>>& Current, const TArray<FBox2D>& Bounds,
        const TArray<TArray<double>>& CurrentKeys,
        const TArray<TArray<FVector2D>>& Authored, const TArray<int32>& GeometryOrder,
        const double RadiusSquared, int32& InOutOperations, int64& InOutCoordinatedPointVisits, TArray<FVector2D>& OutRing,
        TArray<double>& OutKeys, bool& OutBudget, FString* OutRejectedReason = nullptr,
        FResult* OutCoordinatedTrial = nullptr)
    {
        const auto Reject = [&](const FString& Reason)
        {
            if (OutRejectedReason) { *OutRejectedReason = Reason; }
            return false;
        };
        const TArray<FVector2D> Strip = OwnerContactStrip(InputStrip, Current[Owner]);
        if (!ValidRing(Strip)) { return Reject(TEXT("틈의 공통 접점 경계를 구성할 수 없습니다.")); }
        const FVector2D Origin = Strip[0];
        const FBox2D StripBounds = FFZAPolygonMath::GetBounds(Strip);
        TArray<int32> Candidates;
        TArray<UE::Geometry::FGeneralPolygon2d> Negative;
        for (const int32 Other : GeometryOrder)
        {
            if (BoundsOverlap(StripBounds, Bounds[Other]))
            {
                Candidates.Add(Other);
                if (Other != Owner) { Negative.Add(LocalPolygon(Current[Other], Origin)); }
            }
        }
        TArray<UE::Geometry::FGeneralPolygon2d> Positive;
        Positive.Add(LocalPolygon(Strip, Origin));
        TArray<UE::Geometry::FGeneralPolygon2d> Unoccupied;
        if (++InOutOperations > MaximumBooleanOperations) { OutBudget = true; return false; }
        if (!UE::Geometry::PolygonsDifference(Positive, Negative, Unoccupied) || GeneralArea(Unoccupied) <= AreaEpsilon)
        {
            return Reject(TEXT("현재 틈의 비어 있는 면적을 계산할 수 없습니다."));
        }
        // Difference and union each rescale coordinates to a different integer range.
        // Unpacking the first result can move a shared contact by one floating-point ULP;
        // a second union then sees two disconnected pieces. Recover exact current/strip
        // contacts BEFORE that union, retaining every hole of the unoccupied region.
        for (UE::Geometry::FGeneralPolygon2d& Polygon : Unoccupied)
        {
            TArray<FVector2D> Outer;
            for (const FVector2D& Point : Polygon.GetOuter().GetVertices()) { Outer.Add(Point + Origin); }
            RestoreBoundaryPoints(Outer, Current, Candidates, Strip, Owner);
            if (!ValidRing(Outer)) { return Reject(TEXT("비어 있는 틈 경계의 접점을 복원할 수 없습니다.")); }
            UE::Geometry::FGeneralPolygon2d Restored = LocalPolygon(Outer, Origin);
            for (const UE::Geometry::FPolygon2d& OriginalHole : Polygon.GetHoles())
            {
                TArray<FVector2D> Hole;
                for (const FVector2D& Point : OriginalHole.GetVertices()) { Hole.Add(Point + Origin); }
                RestoreBoundaryPoints(Hole, Current, Candidates, Strip, Owner);
                if (!ValidRing(Hole)) { return Reject(TEXT("다른 Zone이 차지한 틈의 내부 경계를 복원할 수 없습니다.")); }
                for (FVector2D& Point : Hole) { Point -= Origin; }
                if (SignedArea(Hole) > 0.0) { Algo::Reverse(Hole); }
                if (!Restored.AddHole(UE::Geometry::FPolygon2d(Hole), false, true))
                {
                    return Reject(TEXT("다른 Zone이 차지한 틈의 내부 경계를 유지할 수 없습니다."));
                }
            }
            Polygon = MoveTemp(Restored);
        }
        TArray<FVector2D> NodedOwner;
        if (!NodeOwnerContacts(Current[Owner], Unoccupied, Origin, InOutCoordinatedPointVisits, NodedOwner))
        {
            OutBudget = true;
            return Reject(TEXT("틈 경계 공통 접점 삽입의 누적 처리 한도를 넘었습니다."));
        }
        // Identical contact XY must be a vertex in BOTH inputs. A point geometrically on
        // an unsplit long edge may pack one integer off that edge, separating the union.
        Unoccupied.Add(LocalPolygon(NodedOwner, Origin));
        TArray<UE::Geometry::FGeneralPolygon2d> Union;
        if (++InOutOperations > MaximumBooleanOperations) { OutBudget = true; return false; }
        const bool bUnionBuilt = UE::Geometry::PolygonsUnion(Unoccupied, Union, false);
        if (!bUnionBuilt || Union.Num() != 1 || Union[0].HasHoles() ||
            GeneralArea(Union) <= FMath::Abs(SignedArea(Current[Owner])) + AreaEpsilon)
        {
            return Reject(FString::Printf(TEXT("틈을 합치면 Zone 경계가 %d개로 분리되거나 내부 구멍이 생깁니다."), Union.Num()));
        }
        OutRing.Reset();
        OutRing.Reserve(Union[0].GetOuter().VertexCount());
        for (const FVector2D& LocalPoint : Union[0].GetOuter().GetVertices()) { OutRing.Add(LocalPoint + Origin); }
        RestoreBoundaryPoints(OutRing, Current, Candidates, Strip, Owner);
        if (SignedArea(OutRing) * SignedArea(Authored[Owner]) < 0.0) { Algo::Reverse(OutRing); }
        OutKeys.Reset();
        OutKeys.Reserve(OutRing.Num());
        for (const FVector2D& Point : OutRing)
        {
            double DistanceSquared = 0.0;
            const double Key = ClosestKey(Point, Authored[Owner], DistanceSquared);
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y) || !FMath::IsFinite(Key))
            {
                return Reject(TEXT("틈을 합친 경계의 좌표 또는 원본 높이 키가 유효하지 않습니다."));
            }
            OutKeys.Add(Key);
        }
        if (!ValidRing(OutRing))
        {
            if (!OutCoordinatedTrial)
            {
                return Reject(TEXT("틈을 합친 경계가 자기교차하거나 너무 짧은 변을 만듭니다."));
            }
            int64 TrialVisits = OutRing.Num() - Current[Owner].Num();
            for (const TArray<FVector2D>& Ring : Current) { TrialVisits += Ring.Num(); }
            for (const TArray<FVector2D>& Ring : Authored) { TrialVisits += Ring.Num(); }
            if (TrialVisits > MaximumCoordinatedPointVisits - InOutCoordinatedPointVisits)
            {
                OutBudget = true;
                return Reject(TEXT("틈 후보의 전체 경계 정리 누적 처리 한도를 넘었습니다."));
            }
            InOutCoordinatedPointVisits += TrialVisits;
            // A single valid Boolean region can contain duplicate contact nodes after
            // unpacking. Validate its actual coordinated topology after numerical cleanup;
            // never clean one owner in isolation and drop its neighbors' matching changes.
            FResult Trial;
            Trial.Polygons = Current;
            Trial.SourcePointKeys = CurrentKeys;
            Trial.Polygons[Owner] = OutRing;
            Trial.SourcePointKeys[Owner] = OutKeys;
            TArray<FBox2D> TrialBounds = Bounds;
            int64 TrialPoints = 0;
            bool bChanged = false;
            if (!NormalizeEpoch(Trial, Authored, RadiusSquared, TrialBounds, TrialPoints, bChanged))
            {
                OutBudget = Trial.bBlocked;
                return Reject(MoveTemp(Trial.FailureReason));
            }
            FFZAWeldDiagnostics::FResult TrialDiagnostics;
            FFZAWeldDiagnostics::Analyze(Trial.Polygons, {}, 0.0, TrialDiagnostics, 0);
            if (TrialDiagnostics.OverlapPairCount != 0)
            {
                return Reject(TEXT("틈 후보의 공통 접점 정리 결과가 다른 Zone과 겹칩니다."));
            }
            if (Trial.Polygons == Current)
            {
                return Reject(TEXT("틈 후보의 공통 접점 정리 후 경계 변화가 없습니다."));
            }
            OutRing = Trial.Polygons[Owner]; OutKeys = Trial.SourcePointKeys[Owner];
            *OutCoordinatedTrial = MoveTemp(Trial);
            return true;
        }
        for (int32 PointIndex = 0; PointIndex < OutRing.Num(); ++PointIndex)
        {
            const FVector2D& Point = OutRing[PointIndex];
            double DistanceSquared = 0.0;
            ClosestKey(Point, Authored[Owner], DistanceSquared);
            if (DistanceSquared > RadiusSquared ||
                !EdgeWithinOriginalBand(Point, OutRing[(PointIndex + 1) % OutRing.Num()], Authored[Owner], RadiusSquared))
            {
                return Reject(FString::Printf(TEXT("새 경계가 원본 복구 거리 %.2fcm를 넘습니다. 점(%.6f, %.6f), 거리 %.6fcm."),
                    FMath::Sqrt(RadiusSquared), Point.X, Point.Y, FMath::Sqrt(DistanceSquared)));
            }
        }
        return true;
    }
}

static bool BuildStrategy(const TArray<TArray<FVector2D>>& AuthoredPolygons, const double RepairRadiusCm,
    const bool bBalanced, FResult& OutResult)
{
    OutResult = FResult();
    OutResult.Polygons = AuthoredPolygons;
    OutResult.SourcePointKeys.SetNum(AuthoredPolygons.Num());
    if (!FMath::IsFinite(RepairRadiusCm) || RepairRadiusCm <= PositionEpsilon || RepairRadiusCm >= 1.e154)
    {
        OutResult.FailureReason = TEXT("틈 복구 거리가 유효하지 않습니다."); return false;
    }
    int64 TotalPoints = 0;
    for (const TArray<FVector2D>& Ring : AuthoredPolygons) { TotalPoints += Ring.Num(); }
    if (TotalPoints > MaximumPoints)
    {
        OutResult.bBlocked = true;
        OutResult.FailureReason = TEXT("틈 면적 복구의 전체 꼭짓점 처리 한도를 넘었습니다."); return true;
    }
    TArray<FCanonicalRing> Canonical;
    TArray<int32> GeometryOrder;
    TArray<FBox2D> Bounds;
    Canonical.SetNum(AuthoredPolygons.Num());
    Bounds.SetNum(AuthoredPolygons.Num());
    for (int32 Index = 0; Index < AuthoredPolygons.Num(); ++Index)
    {
        if (!ValidRing(AuthoredPolygons[Index]))
        {
            OutResult.FailureReason = FString::Printf(TEXT("Zone %d: 틈 복구 원본 경계가 유효하지 않습니다."), Index + 1); return false;
        }
        Canonical[Index] = CanonicalRing(AuthoredPolygons[Index]);
        Bounds[Index] = FFZAPolygonMath::GetBounds(AuthoredPolygons[Index]);
        GeometryOrder.Add(Index);
        OutResult.SourcePointKeys[Index].Reserve(AuthoredPolygons[Index].Num());
        for (int32 Point = 0; Point < AuthoredPolygons[Index].Num(); ++Point)
        {
            OutResult.SourcePointKeys[Index].Add(static_cast<double>(Point));
        }
    }
    GeometryOrder.Sort([&](const int32 A, const int32 B) { return GeometryBefore(A, B, AuthoredPolygons, Canonical); });
    TArray<int32> GeometryRank;
    GeometryRank.SetNumUninitialized(GeometryOrder.Num());
    for (int32 Rank = 0; Rank < GeometryOrder.Num(); ++Rank) { GeometryRank[GeometryOrder[Rank]] = Rank; }
    const double RadiusSquared = FMath::Square(RepairRadiusCm + PositionEpsilon);
    int32 Operations = 0;
    int64 CoordinatedPointVisits = 0;
    for (int32 Pass = 0; Pass < MaximumPasses; ++Pass)
    {
        FFZAWeldDiagnostics::FResult Diagnostics;
        FFZAWeldDiagnostics::Analyze(OutResult.Polygons, {}, RepairRadiusCm, Diagnostics, MaximumIssues);
        if (Diagnostics.bTruncated)
        {
            OutResult.bBlocked = true;
            OutResult.FailureReason = TEXT("틈 면적 복구의 전체 경계 구간 처리 한도를 넘었습니다."); return true;
        }
        if (Diagnostics.GapPairCount == 0) { OutResult.bComplete = true; return true; }
        TArray<FFZAWeldDiagnostics::FBoundaryIssue> Gaps;
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Diagnostics.Issues)
        {
            if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Gap)
            {
                FFZAWeldDiagnostics::FBoundaryIssue& Gap = Gaps.Add_GetRef(Issue);
                RestoreTaperedContact(Gap, OutResult.Polygons);
            }
        }
        Gaps.Sort([&](const FFZAWeldDiagnostics::FBoundaryIssue& A, const FFZAWeldDiagnostics::FBoundaryIssue& B)
        {
            const int32 AFirst = FMath::Min(GeometryRank[A.PolygonIndexA], GeometryRank[A.PolygonIndexB]);
            const int32 BFirst = FMath::Min(GeometryRank[B.PolygonIndexA], GeometryRank[B.PolygonIndexB]);
            if (AFirst != BFirst) { return AFirst < BFirst; }
            const int32 ASecond = FMath::Max(GeometryRank[A.PolygonIndexA], GeometryRank[A.PolygonIndexB]);
            const int32 BSecond = FMath::Max(GeometryRank[B.PolygonIndexA], GeometryRank[B.PolygonIndexB]);
            if (ASecond != BSecond) { return ASecond < BSecond; }
            if (A.StartA != B.StartA) { return PointBefore(A.StartA, B.StartA); }
            return PointBefore(A.EndA, B.EndA);
        });
        bool bProgress = false;
        FString LastRejectedReason;
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Gap : Gaps)
        {
            TArray<FVector2D> Strip = { Gap.StartA, Gap.EndA, Gap.EndB, Gap.StartB };
            RemoveAdjacentDuplicates(Strip);
            if (!ValidRing(Strip)) { continue; }

            // Prefer a common middle seam: both Zones gain disjoint halves while retaining
            // their authored corners. This handles delicate tongues without the corner
            // collapse caused by moving all incident edges at once. Commit the pair only
            // when both independently validated unions succeed.
            const FVector2D MiddleStart = Gap.StartA + (Gap.StartB - Gap.StartA) * 0.5;
            const FVector2D MiddleEnd = Gap.EndA + (Gap.EndB - Gap.EndA) * 0.5;
            TArray<FVector2D> HalfA = {Gap.StartA, Gap.EndA, MiddleEnd, MiddleStart};
            TArray<FVector2D> HalfB = {MiddleStart, MiddleEnd, Gap.EndB, Gap.StartB};
            RemoveAdjacentDuplicates(HalfA);
            RemoveAdjacentDuplicates(HalfB);
            TArray<FVector2D> RepairedA, RepairedB;
            TArray<double> KeysA, KeysB;
            bool bPairBudget = false;
            if (bBalanced && ValidRing(HalfA) && ValidRing(HalfB) &&
                TryOwner(Gap.PolygonIndexA, HalfA, OutResult.Polygons, Bounds, OutResult.SourcePointKeys, AuthoredPolygons, GeometryOrder,
                    RadiusSquared, Operations, CoordinatedPointVisits, RepairedA, KeysA, bPairBudget) &&
                TryOwner(Gap.PolygonIndexB, HalfB, OutResult.Polygons, Bounds, OutResult.SourcePointKeys, AuthoredPolygons, GeometryOrder,
                    RadiusSquared, Operations, CoordinatedPointVisits, RepairedB, KeysB, bPairBudget))
            {
                TotalPoints += RepairedA.Num() + RepairedB.Num() - OutResult.Polygons[Gap.PolygonIndexA].Num() -
                    OutResult.Polygons[Gap.PolygonIndexB].Num();
                if (TotalPoints > MaximumPoints)
                {
                    OutResult.bBlocked = true;
                    OutResult.FailureReason = TEXT("틈 면적 복구 결과의 꼭짓점 처리 한도를 넘었습니다."); return true;
                }
                OutResult.Polygons[Gap.PolygonIndexA] = MoveTemp(RepairedA);
                OutResult.Polygons[Gap.PolygonIndexB] = MoveTemp(RepairedB);
                OutResult.SourcePointKeys[Gap.PolygonIndexA] = MoveTemp(KeysA);
                OutResult.SourcePointKeys[Gap.PolygonIndexB] = MoveTemp(KeysB);
                Bounds[Gap.PolygonIndexA] = FFZAPolygonMath::GetBounds(OutResult.Polygons[Gap.PolygonIndexA]);
                Bounds[Gap.PolygonIndexB] = FFZAPolygonMath::GetBounds(OutResult.Polygons[Gap.PolygonIndexB]);
                bProgress = true;
                continue;
            }
            if (bPairBudget)
            {
                OutResult.bBlocked = true;
                OutResult.FailureReason = TEXT("틈 면적 복구의 폴리곤 연산 처리 한도를 넘었습니다."); return true;
            }
            const int32 First = GeometryRank[Gap.PolygonIndexA] < GeometryRank[Gap.PolygonIndexB] ?
                Gap.PolygonIndexA : Gap.PolygonIndexB;
            const int32 Owners[2] = {First, First == Gap.PolygonIndexA ? Gap.PolygonIndexB : Gap.PolygonIndexA};
            for (const int32 Owner : Owners)
            {
                TArray<FVector2D> Repaired;
                TArray<double> Keys;
                FResult CoordinatedTrial;
                bool bBudget = false;
                if (TryOwner(Owner, Strip, OutResult.Polygons, Bounds, OutResult.SourcePointKeys, AuthoredPolygons, GeometryOrder,
                    RadiusSquared, Operations, CoordinatedPointVisits, Repaired, Keys, bBudget, &LastRejectedReason, &CoordinatedTrial))
                {
                    if (!CoordinatedTrial.Polygons.IsEmpty())
                    {
                        OutResult.Polygons = MoveTemp(CoordinatedTrial.Polygons);
                        OutResult.SourcePointKeys = MoveTemp(CoordinatedTrial.SourcePointKeys);
                        TotalPoints = 0;
                        for (int32 Polygon = 0; Polygon < OutResult.Polygons.Num(); ++Polygon)
                        {
                            TotalPoints += OutResult.Polygons[Polygon].Num();
                            Bounds[Polygon] = FFZAPolygonMath::GetBounds(OutResult.Polygons[Polygon]);
                        }
                        bProgress = true;
                        break;
                    }
                    TotalPoints += Repaired.Num() - OutResult.Polygons[Owner].Num();
                    if (TotalPoints > MaximumPoints)
                    {
                        OutResult.bBlocked = true;
                        OutResult.FailureReason = TEXT("틈 면적 복구 결과의 꼭짓점 처리 한도를 넘었습니다."); return true;
                    }
                    OutResult.Polygons[Owner] = MoveTemp(Repaired);
                    OutResult.SourcePointKeys[Owner] = MoveTemp(Keys);
                    Bounds[Owner] = FFZAPolygonMath::GetBounds(OutResult.Polygons[Owner]);
                    bProgress = true;
                    break;
                }
                if (bBudget)
                {
                    OutResult.bBlocked = true;
                    OutResult.FailureReason = TEXT("틈 면적 복구의 폴리곤 연산 처리 한도를 넘었습니다."); return true;
                }
            }
        }
        // Every accepted Boolean pass may introduce sub-tolerance contact nodes. Repair
        // their common XY across the WHOLE snapshot before the next Boolean operation;
        // otherwise a later rescaling can shorten inherited edges below strict validity
        // and prevent an unrelated, perfectly valid large gap from being filled.
        bool bNormalizedChange = false;
        if (!NormalizeEpoch(OutResult, AuthoredPolygons, RadiusSquared, Bounds, TotalPoints, bNormalizedChange))
        {
            return true;
        }
        bProgress |= bNormalizedChange;
        if (!bProgress)
        {
            OutResult.FailureReason = LastRejectedReason.IsEmpty() ?
                TEXT("남은 틈을 메우면 Zone이 분리되거나 내부 구멍이 생기거나 복구 거리를 넘습니다.") : LastRejectedReason;
            return true;
        }
    }
    FFZAWeldDiagnostics::FResult FinalDiagnostics;
    FFZAWeldDiagnostics::Analyze(OutResult.Polygons, {}, RepairRadiusCm, FinalDiagnostics, MaximumIssues);
    OutResult.bComplete = FinalDiagnostics.GapPairCount == 0 && !FinalDiagnostics.bTruncated;
    if (!OutResult.bComplete)
    {
        OutResult.bBlocked = true;
        OutResult.FailureReason = TEXT("틈 면적 복구 반복 처리 한도에 도달했습니다.");
    }
    return true;
}

static bool BuildOrdered(const TArray<TArray<FVector2D>>& AuthoredPolygons, const double RepairRadiusCm, FResult& OutResult)
{
    if (!BuildStrategy(AuthoredPolygons, RepairRadiusCm, true, OutResult)) { return false; }
    if (OutResult.bComplete || OutResult.bBlocked) { return true; }

    // Concave facing boundaries can turn repeated midpoint additions into stair-shaped
    // pockets. Try the same complete repair from the unchanged input using whole-strip
    // ownership instead. Starting over avoids accumulating a partial strategy's drift.
    FResult Alternative;
    if (!BuildStrategy(AuthoredPolygons, RepairRadiusCm, false, Alternative)) { return true; }
    if (Alternative.bComplete)
    {
        OutResult = MoveTemp(Alternative);
    }
    else if (!Alternative.bBlocked)
    {
        FFZAWeldDiagnostics::FResult FirstDiagnostics, AlternativeDiagnostics;
        FFZAWeldDiagnostics::Analyze(OutResult.Polygons, {}, RepairRadiusCm, FirstDiagnostics, 0);
        FFZAWeldDiagnostics::Analyze(Alternative.Polygons, {}, RepairRadiusCm, AlternativeDiagnostics, 0);
        if (AlternativeDiagnostics.GapPairCount < FirstDiagnostics.GapPairCount)
        {
            OutResult = MoveTemp(Alternative);
        }
    }
    return true;
}

bool Build(const TArray<TArray<FVector2D>>& AuthoredPolygons, const double RepairRadiusCm, FResult& OutResult)
{
    OutResult = FResult();
    OutResult.Polygons = AuthoredPolygons;
    if (!FMath::IsFinite(RepairRadiusCm) || RepairRadiusCm <= PositionEpsilon || RepairRadiusCm >= 1.e154)
    {
        OutResult.FailureReason = TEXT("틈 복구 거리가 유효하지 않습니다."); return false;
    }
    int64 TotalPoints = 0;
    for (const TArray<FVector2D>& Ring : AuthoredPolygons) { TotalPoints += Ring.Num(); }
    if (TotalPoints > MaximumPoints)
    {
        OutResult.bBlocked = true;
        OutResult.FailureReason = TEXT("틈 면적 복구의 전체 꼭짓점 처리 한도를 넘었습니다."); return true;
    }
    // Canonicalize the calculation, not the authored data. Diagnostic projection axes and
    // Boolean traversal must see the same inputs when collection/spline order changes.
    TArray<FCanonicalRing> SourceOrder, Canonical;
    TArray<TArray<FVector2D>> CanonicalRings;
    TArray<int32> Indices;
    SourceOrder.SetNum(AuthoredPolygons.Num());
    Canonical.SetNum(AuthoredPolygons.Num());
    CanonicalRings.SetNum(AuthoredPolygons.Num());
    for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
    {
        const TArray<FVector2D>& Source = AuthoredPolygons[Polygon];
        if (!ValidRing(Source))
        {
            OutResult.FailureReason = FString::Printf(TEXT("Zone %d: 틈 복구 원본 경계가 유효하지 않습니다."), Polygon + 1);
            return false;
        }
        SourceOrder[Polygon] = CanonicalRing(Source);
        const FCanonicalRing& Order = SourceOrder[Polygon];
        for (int32 Point = 0; Point < Source.Num(); ++Point)
        {
            CanonicalRings[Polygon].Add(Source[(Order.Start + Order.Step * Point + Source.Num()) % Source.Num()]);
        }
        Canonical[Polygon] = CanonicalRing(CanonicalRings[Polygon]);
        Indices.Add(Polygon);
    }
    Indices.Sort([&](const int32 A, const int32 B) { return GeometryBefore(A, B, CanonicalRings, Canonical); });
    TArray<TArray<FVector2D>> OrderedInput;
    OrderedInput.Reserve(Indices.Num());
    for (const int32 Polygon : Indices) { OrderedInput.Add(MoveTemp(CanonicalRings[Polygon])); }
    FResult OrderedResult;
    const bool bBuilt = BuildOrdered(OrderedInput, RepairRadiusCm, OrderedResult);
    OutResult.bComplete = OrderedResult.bComplete;
    OutResult.bBlocked = OrderedResult.bBlocked;
    OutResult.FailureReason = MoveTemp(OrderedResult.FailureReason);
    OutResult.SourcePointKeys.SetNum(AuthoredPolygons.Num());
    for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
    {
        const int32 Polygon = Indices[Slot];
        const int32 Count = AuthoredPolygons[Polygon].Num();
        TArray<double>& Keys = OutResult.SourcePointKeys[Polygon];
        for (int32 Point = 0; Point < Count; ++Point) { Keys.Add(static_cast<double>(Point)); }
        if (!OrderedResult.Polygons.IsValidIndex(Slot) || !OrderedResult.SourcePointKeys.IsValidIndex(Slot) ||
            OrderedResult.SourcePointKeys[Slot].Num() != OrderedResult.Polygons[Slot].Num()) { continue; }
        if (OrderedResult.Polygons[Slot] == OrderedInput[Slot]) { continue; } // Exact no-op point order/keys.
        const FCanonicalRing& Order = SourceOrder[Polygon];
        TArray<double> MappedKeys;
        MappedKeys.Reserve(OrderedResult.SourcePointKeys[Slot].Num());
        for (const double Key : OrderedResult.SourcePointKeys[Slot])
        {
            if (!FMath::IsFinite(Key) || Key < 0.0 || Key >= Count)
            {
                OutResult.bComplete = false;
                OutResult.bBlocked = true;
                OutResult.FailureReason = FString::Printf(TEXT("Zone %d: 정렬된 복구 결과의 원본 높이 키가 유효하지 않습니다."), Polygon + 1);
                return true;
            }
            double Mapped = FMath::Fmod(Order.Start + Order.Step * Key, static_cast<double>(Count));
            if (Mapped < 0.0) { Mapped += Count; }
            MappedKeys.Add(Mapped >= Count ? 0.0 : Mapped);
        }
        OutResult.Polygons[Polygon] = MoveTemp(OrderedResult.Polygons[Slot]);
        Keys = MoveTemp(MappedKeys);
        if (Order.Step < 0)
        {
            Algo::Reverse(OutResult.Polygons[Polygon]);
            Algo::Reverse(Keys);
        }
    }
    for (int32 Slot = 0; Slot < Indices.Num(); ++Slot)
    {
        const FString Prefix = FString::Printf(TEXT("Zone %d:"), Slot + 1);
        if (OutResult.FailureReason.StartsWith(Prefix))
        {
            OutResult.FailureReason = FString::Printf(TEXT("Zone %d:"), Indices[Slot] + 1) + OutResult.FailureReason.RightChop(Prefix.Len());
            break;
        }
    }
    return bBuilt;
}

bool ValidateRepairBand(const TArray<TArray<FVector2D>>& AuthoredPolygons,
    const TArray<TArray<FVector2D>>& RepairedPolygons, const double RepairRadiusCm, FString& OutError)
{
    OutError.Reset();
    if (AuthoredPolygons.Num() != RepairedPolygons.Num() || !FMath::IsFinite(RepairRadiusCm) ||
        RepairRadiusCm <= PositionEpsilon || RepairRadiusCm >= 1.e154)
    {
        OutError = TEXT("원본 경계와 복구 결과의 수 또는 복구 거리가 유효하지 않습니다."); return false;
    }
    int64 OriginalPoints = 0, RepairedPoints = 0;
    for (const TArray<FVector2D>& Ring : AuthoredPolygons) { OriginalPoints += Ring.Num(); }
    for (const TArray<FVector2D>& Ring : RepairedPolygons) { RepairedPoints += Ring.Num(); }
    if (OriginalPoints > MaximumPoints || RepairedPoints > MaximumPoints)
    {
        OutError = TEXT("원본 경계 복구 거리 검사의 꼭짓점 처리 한도를 넘었습니다."); return false;
    }
    const double RadiusSquared = FMath::Square(RepairRadiusCm + PositionEpsilon);
    for (int32 Polygon = 0; Polygon < RepairedPolygons.Num(); ++Polygon)
    {
        if (!ValidRing(AuthoredPolygons[Polygon]) || !ValidRing(RepairedPolygons[Polygon]))
        {
            OutError = FString::Printf(TEXT("Zone %d: 원본 복구 거리를 검사할 경계가 유효하지 않습니다."), Polygon + 1); return false;
        }
        for (int32 Edge = 0; Edge < RepairedPolygons[Polygon].Num(); ++Edge)
        {
            if (!EdgeWithinOriginalBand(RepairedPolygons[Polygon][Edge],
                RepairedPolygons[Polygon][(Edge + 1) % RepairedPolygons[Polygon].Num()], AuthoredPolygons[Polygon], RadiusSquared))
            {
                OutError = FString::Printf(TEXT("Zone %d: 공통 접점 정리 후 경계가 원본 복구 거리 %.2fcm를 넘습니다."),
                    Polygon + 1, RepairRadiusCm); return false;
            }
        }
    }
    return true;
}
}
