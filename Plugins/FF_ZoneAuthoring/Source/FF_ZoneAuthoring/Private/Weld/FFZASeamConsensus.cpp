#include "FFZASeamConsensus.h"
#include "FFZAPolygonMath.h"
#include "FFZAWeldDiagnostics.h"

namespace FFZASeamConsensus
{
namespace
{
    constexpr double Epsilon = 1.e-4;
    constexpr double EpsilonSquared = Epsilon * Epsilon;
    constexpr int32 MaximumPasses = 32;
    constexpr int32 MaximumIssues = 131072;
    constexpr int32 MaximumNodes = 262144;
    constexpr int32 MaximumLinks = MaximumNodes * 8;
    constexpr int32 MaximumCutPropagationPasses = 16;

    double Dot(const FVector2D& A, const FVector2D& B) { return A.X * B.X + A.Y * B.Y; }
    double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }
    bool PointBefore(const FVector2D& A, const FVector2D& B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; }
    bool SamePoint(const FVector2D& A, const FVector2D& B) { return FVector2D::DistSquared(A, B) <= EpsilonSquared; }

    struct FBounds
    {
        FVector2D Min, Max;
        FBounds(const FVector2D& A, const FVector2D& B)
            : Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y)) {}
        explicit FBounds(const TArray<FVector2D>& Ring) : FBounds(Ring[0], Ring[0])
        {
            for (const FVector2D& Point : Ring)
            {
                Min.X = FMath::Min(Min.X, Point.X); Min.Y = FMath::Min(Min.Y, Point.Y);
                Max.X = FMath::Max(Max.X, Point.X); Max.Y = FMath::Max(Max.Y, Point.Y);
            }
        }
        bool Touches(const FBounds& Other, const double Padding = Epsilon) const
        {
            return Min.X - Padding <= Other.Max.X && Max.X + Padding >= Other.Min.X &&
                Min.Y - Padding <= Other.Max.Y && Max.Y + Padding >= Other.Min.Y;
        }
    };

    double SignedArea(const TArray<FVector2D>& Ring)
    {
        double TwiceArea = 0.0;
        for (int32 Point = 1; Point + 1 < Ring.Num(); ++Point)
        {
            TwiceArea += Cross(Ring[Point] - Ring[0], Ring[Point + 1] - Ring[0]);
        }
        return TwiceArea * 0.5;
    }

    bool ValidRing(const TArray<FVector2D>& Ring, double& OutArea)
    {
        if (Ring.Num() < 3) { return false; }
        TArray<FVector2D> Local;
        Local.Reserve(Ring.Num());
        for (int32 Point = 0; Point < Ring.Num(); ++Point)
        {
            if (!FMath::IsFinite(Ring[Point].X) || !FMath::IsFinite(Ring[Point].Y)) { return false; }
            Local.Add(Ring[Point] - Ring[0]);
            const FVector2D Previous = Ring[Point] - Ring[(Point + Ring.Num() - 1) % Ring.Num()];
            const FVector2D Next = Ring[(Point + 1) % Ring.Num()] - Ring[Point];
            if (Dot(Previous, Next) < 0.0 && FMath::Abs(Cross(Previous, Next)) <= Epsilon * (Previous.Size() + Next.Size())) { return false; }
        }
        OutArea = SignedArea(Local);
        return FMath::IsFinite(OutArea) && FMath::Abs(OutArea) > EpsilonSquared && FFZAPolygonMath::IsSimplePolygon(Local, Epsilon);
    }

    struct FCanonical
    {
        int32 Start = 0, Step = 1;
        explicit FCanonical(const TArray<FVector2D>& Ring)
        {
            for (int32 Point = 1; Point < Ring.Num(); ++Point) { if (PointBefore(Ring[Point], Ring[Start])) { Start = Point; } }
            if (PointBefore(Ring[(Start + Ring.Num() - 1) % Ring.Num()], Ring[(Start + 1) % Ring.Num()])) { Step = -1; }
        }
    };

    TArray<int32> GeometryOrder(const TArray<TArray<FVector2D>>& Rings)
    {
        TArray<int32> Order;
        TArray<FCanonical> Canonical;
        TArray<FBounds> Bounds;
        Order.Reserve(Rings.Num()); Canonical.Reserve(Rings.Num()); Bounds.Reserve(Rings.Num());
        for (int32 Polygon = 0; Polygon < Rings.Num(); ++Polygon)
        {
            Order.Add(Polygon); Canonical.Emplace(Rings[Polygon]); Bounds.Emplace(Rings[Polygon]);
        }
        Order.Sort([&](const int32 A, const int32 B)
        {
            if (Bounds[A].Min != Bounds[B].Min) { return PointBefore(Bounds[A].Min, Bounds[B].Min); }
            if (Bounds[A].Max != Bounds[B].Max) { return PointBefore(Bounds[A].Max, Bounds[B].Max); }
            if (Rings[A].Num() != Rings[B].Num()) { return Rings[A].Num() < Rings[B].Num(); }
            for (int32 Point = 0; Point < Rings[A].Num(); ++Point)
            {
                const FVector2D& P = Rings[A][(Canonical[A].Start + Point * Canonical[A].Step + Rings[A].Num()) % Rings[A].Num()];
                const FVector2D& Q = Rings[B][(Canonical[B].Start + Point * Canonical[B].Step + Rings[B].Num()) % Rings[B].Num()];
                if (P != Q) { return PointBefore(P, Q); }
            }
            return false;
        });
        return Order;
    }

    void InspectCanonical(const TArray<TArray<FVector2D>>& Rings, const double Radius,
        FFZAWeldDiagnostics::FResult& OutDiagnostics, const int32 IssueBudget)
    {
        // The inspector uses its first ring's axis for projections and caller indices for
        // equal-min-X sweep ties. Canonicalize the array as well as solver constraints so these
        // asymmetric choices never depend on actor IDs, input order, or the current selection.
        const TArray<int32> Order = GeometryOrder(Rings);
        TArray<TArray<FVector2D>> OrderedRings;
        OrderedRings.Reserve(Rings.Num());
        for (const int32 Polygon : Order) { OrderedRings.Add(Rings[Polygon]); }
        FFZAWeldDiagnostics::Analyze(OrderedRings, {}, Radius, OutDiagnostics, IssueBudget);
        for (FFZAWeldDiagnostics::FBoundaryIssue& Issue : OutDiagnostics.Issues)
        {
            Issue.PolygonIndexA = Order[Issue.PolygonIndexA];
            if (Issue.PolygonIndexB != INDEX_NONE) { Issue.PolygonIndexB = Order[Issue.PolygonIndexB]; }
        }
    }

    struct FPolygonSets
    {
        TArray<int32> Parents;
        explicit FPolygonSets(const int32 Count)
        {
            Parents.Reserve(Count); for (int32 Polygon = 0; Polygon < Count; ++Polygon) { Parents.Add(Polygon); }
        }
        int32 Find(int32 Index)
        {
            while (Parents[Index] != Index) { Parents[Index] = Parents[Parents[Index]]; Index = Parents[Index]; }
            return Index;
        }
        void Join(const int32 A, const int32 B)
        {
            const int32 RootA = Find(A), RootB = Find(B); if (RootA != RootB) { Parents[RootB] = RootA; }
        }
        void Groups(const TArray<int32>& Order, TArray<FFZALayoutOps::FWeldGroup>& OutGroups)
        {
            OutGroups.Reset();
            TMap<int32, int32> GroupByRoot;
            for (const int32 Polygon : Order)
            {
                const int32 Root = Find(Polygon);
                int32* Group = GroupByRoot.Find(Root);
                if (!Group) { const int32 Index = OutGroups.AddDefaulted(); GroupByRoot.Add(Root, Index); Group = GroupByRoot.Find(Root); }
                OutGroups[*Group].PolygonIndices.Add(Polygon);
            }
        }
    };

    double NormalizeKey(const double Key, const int32 Count)
    {
        double Result = FMath::Fmod(Key, static_cast<double>(Count));
        if (Result < 0.0) { Result += Count; }
        return Result;
    }

    FVector2D AuthoredPoint(const TArray<FVector2D>& Ring, const double Key)
    {
        const int32 Edge = FMath::FloorToInt(Key);
        return FMath::Lerp(Ring[Edge], Ring[(Edge + 1) % Ring.Num()], Key - Edge);
    }

    double NearestKey(const TArray<FVector2D>& Authored, const FVector2D& Point)
    {
        double BestDistance = TNumericLimits<double>::Max(), BestKey = 0.0;
        for (int32 Edge = 0; Edge < Authored.Num(); ++Edge)
        {
            const FVector2D A = Authored[Edge], Direction = Authored[(Edge + 1) % Authored.Num()] - A;
            const double T = FMath::Clamp(Dot(Point - A, Direction) / Direction.SizeSquared(), 0.0, 1.0);
            const double Distance = FVector2D::DistSquared(Point, A + Direction * T);
            if (Distance < BestDistance) { BestDistance = Distance; BestKey = NormalizeKey(Edge + T, Authored.Num()); }
        }
        return BestKey;
    }

    bool ParameterOnEdge(const double Key, const int32 Edge, const int32 Count, double& OutT)
    {
        OutT = Key - Edge;
        if (OutT < -1.e-9) { OutT += Count; }
        if (OutT < -1.e-9 || OutT > 1.0 + 1.e-9) { return false; }
        OutT = FMath::Clamp(OutT, 0.0, 1.0); return true;
    }

    double SplitKey(const TArray<FVector2D>& Authored, const TArray<FVector2D>& Current,
        const TArray<double>& Keys, const int32 Edge, const double T)
    {
        if (T == 0.0) { return Keys[Edge]; }
        if (T == 1.0) { return Keys[(Edge + 1) % Keys.Num()]; }
        const double Start = Keys[Edge], End = Keys[(Edge + 1) % Keys.Num()];
        const int32 FirstEdge = FMath::FloorToInt(Start);
        for (const int32 Candidate : {FirstEdge, (FirstEdge + Authored.Num() - 1) % Authored.Num()})
        {
            double StartT = 0.0, EndT = 0.0;
            if (ParameterOnEdge(Start, Candidate, Authored.Num(), StartT) && ParameterOnEdge(End, Candidate, Authored.Num(), EndT))
            {
                return NormalizeKey(Candidate + FMath::Lerp(StartT, EndT, T), Authored.Num());
            }
        }
        return NearestKey(Authored, FMath::Lerp(Current[Edge], Current[(Edge + 1) % Current.Num()], T));
    }

    struct FCut { double T; int32 Node; };
    struct FNode
    {
        int32 Polygon, Parent, Count = 1, Head, Tail, Next = INDEX_NONE;
        FVector2D Source;
        double Key;
        FNode(const int32 InPolygon, const FVector2D& InSource, const double InKey, const int32 Index)
            : Polygon(InPolygon), Parent(Index), Head(Index), Tail(Index), Source(InSource), Key(InKey) {}
    };
    struct FMatch
    {
        int32 PolygonA, EdgeA, PolygonB, EdgeB;
        double StartA, EndA, StartB, EndB;
    };
    struct FLink { int32 A, B; };
    struct FSeamLine { FVector2D Start, End; };
    using FPolygonCuts = TArray<TArray<FCut>>;

    int32 FindNode(TArray<FNode>& Nodes, int32 Node)
    {
        while (Nodes[Node].Parent != Node) { Nodes[Node].Parent = Nodes[Nodes[Node].Parent].Parent; Node = Nodes[Node].Parent; }
        return Node;
    }

    bool CompatibleZoneNodes(const TArray<FNode>& Nodes, const int32 RootA, const int32 RootB,
        const int32 Polygon, const int32 AuthoredCount)
    {
        int32 OriginalCorner = INDEX_NONE;
        double FirstKey = -1.0;
        for (const int32 Root : {RootA, RootB})
        {
            for (int32 Member = Nodes[Root].Head; Member != INDEX_NONE; Member = Nodes[Member].Next)
            {
                if (Nodes[Member].Polygon != Polygon) { continue; }
                const double Key = Nodes[Member].Key;
                if (FirstKey < 0.0) { FirstKey = Key; }
                if (Key == static_cast<double>(FMath::FloorToInt(Key)))
                {
                    const int32 Corner = FMath::FloorToInt(Key);
                    if (OriginalCorner != INDEX_NONE && OriginalCorner != Corner) { return false; }
                    OriginalCorner = Corner;
                }
            }
        }
        const int32 Edge = OriginalCorner != INDEX_NONE ? OriginalCorner : FMath::FloorToInt(FirstKey);
        for (const int32 Root : {RootA, RootB})
        {
            for (int32 Member = Nodes[Root].Head; Member != INDEX_NONE; Member = Nodes[Member].Next)
            {
                if (Nodes[Member].Polygon != Polygon) { continue; }
                double T = 0.0;
                if (!ParameterOnEdge(Nodes[Member].Key, Edge, AuthoredCount, T) &&
                    !(OriginalCorner != INDEX_NONE && ParameterOnEdge(Nodes[Member].Key,
                        (Edge + AuthoredCount - 1) % AuthoredCount, AuthoredCount, T))) { return false; }
            }
        }
        return true;
    }

    bool JoinNodes(TArray<FNode>& Nodes, const int32 A, const int32 B, const TArray<TArray<FVector2D>>& Authored)
    {
        int32 RootA = FindNode(Nodes, A), RootB = FindNode(Nodes, B);
        if (RootA == RootB) { return true; }
        // Distinct authored corners must not join indirectly and collapse a narrow Zone. Cuts
        // introduced on ONE authored edge may coincide at a T junction; a single retained corner
        // may also absorb synthetic cuts on its incident edges. They emit one averaged source key.
        for (int32 MemberA = Nodes[RootA].Head; MemberA != INDEX_NONE; MemberA = Nodes[MemberA].Next)
        {
            for (int32 MemberB = Nodes[RootB].Head; MemberB != INDEX_NONE; MemberB = Nodes[MemberB].Next)
            {
                if (Nodes[MemberA].Polygon == Nodes[MemberB].Polygon &&
                    !CompatibleZoneNodes(Nodes, RootA, RootB, Nodes[MemberA].Polygon,
                        Authored[Nodes[MemberA].Polygon].Num())) { return false; }
            }
        }
        if (Nodes[RootA].Count < Nodes[RootB].Count) { Swap(RootA, RootB); }
        Nodes[RootB].Parent = RootA;
        Nodes[Nodes[RootA].Tail].Next = Nodes[RootB].Head;
        Nodes[RootA].Tail = Nodes[RootB].Tail;
        Nodes[RootA].Count += Nodes[RootB].Count;
        return true;
    }

    double KeyForCluster(const TArray<FNode>& Nodes, const int32 Root, const int32 Polygon, const int32 AuthoredCount)
    {
        double First = -1.0;
        for (int32 Member = Nodes[Root].Head; Member != INDEX_NONE; Member = Nodes[Member].Next)
        {
            if (Nodes[Member].Polygon != Polygon) { continue; }
            const double Key = Nodes[Member].Key;
            if (Key == static_cast<double>(FMath::FloorToInt(Key))) { return Key; }
            if (First < 0.0) { First = Key; }
        }
        const int32 Edge = FMath::FloorToInt(First);
        double Total = 0.0; int32 Count = 0;
        for (int32 Member = Nodes[Root].Head; Member != INDEX_NONE; Member = Nodes[Member].Next)
        {
            if (Nodes[Member].Polygon != Polygon) { continue; }
            double T = 0.0; ParameterOnEdge(Nodes[Member].Key, Edge, AuthoredCount, T);
            Total += T; ++Count;
        }
        return NormalizeKey(Edge + Total / Count, AuthoredCount);
    }

    bool EdgeForSpan(const TArray<FVector2D>& Ring, const FVector2D& Start, const FVector2D& End,
        int32& OutEdge, double& OutStart, double& OutEnd)
    {
        double Best = TNumericLimits<double>::Max();
        OutEdge = INDEX_NONE;
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const FVector2D A = Ring[Edge], Direction = Ring[(Edge + 1) % Ring.Num()] - A;
            const double LengthSquared = Direction.SizeSquared();
            const double T0 = Dot(Start - A, Direction) / LengthSquared, T1 = Dot(End - A, Direction) / LengthSquared;
            const double ParameterEpsilon = Epsilon / FMath::Sqrt(LengthSquared);
            if (T0 < -ParameterEpsilon || T0 > 1.0 + ParameterEpsilon || T1 < -ParameterEpsilon || T1 > 1.0 + ParameterEpsilon) { continue; }
            const double Error = FMath::Max(FVector2D::DistSquared(Start, A + Direction * T0), FVector2D::DistSquared(End, A + Direction * T1));
            if (Error <= EpsilonSquared * 16.0 && Error < Best)
            {
                Best = Error; OutEdge = Edge; OutStart = FMath::Clamp(T0, 0.0, 1.0); OutEnd = FMath::Clamp(T1, 0.0, 1.0);
            }
        }
        return OutEdge != INDEX_NONE;
    }

    void ExactSharedMatches(const TArray<TArray<FVector2D>>& Rings, const TArray<double>& Areas,
        TArray<FMatch>& OutMatches, FPolygonSets& Components)
    {
        TArray<FBounds> Bounds;
        TArray<TArray<FBounds>> EdgeBounds;
        TArray<int32> Sweep;
        Bounds.Reserve(Rings.Num()); EdgeBounds.SetNum(Rings.Num()); Sweep.Reserve(Rings.Num());
        for (int32 Polygon = 0; Polygon < Rings.Num(); ++Polygon)
        {
            Bounds.Emplace(Rings[Polygon]); Sweep.Add(Polygon); EdgeBounds[Polygon].Reserve(Rings[Polygon].Num());
            for (int32 Edge = 0; Edge < Rings[Polygon].Num(); ++Edge) { EdgeBounds[Polygon].Emplace(Rings[Polygon][Edge], Rings[Polygon][(Edge + 1) % Rings[Polygon].Num()]); }
        }
        Sweep.Sort([&](const int32 A, const int32 B) { return Bounds[A].Min.X < Bounds[B].Min.X; });
        for (int32 First = 0; First < Sweep.Num(); ++First)
        {
            const int32 A = Sweep[First];
            for (int32 Second = First + 1; Second < Sweep.Num(); ++Second)
            {
                const int32 B = Sweep[Second];
                if (Bounds[B].Min.X > Bounds[A].Max.X + Epsilon) { break; }
                if (!Bounds[A].Touches(Bounds[B])) { continue; }
                for (int32 EdgeA = 0; EdgeA < Rings[A].Num(); ++EdgeA)
                {
                    if (!EdgeBounds[A][EdgeA].Touches(Bounds[B])) { continue; }
                    const FVector2D P = Rings[A][EdgeA], Direction = Rings[A][(EdgeA + 1) % Rings[A].Num()] - P;
                    const double Length = Direction.Size();
                    for (int32 EdgeB = 0; EdgeB < Rings[B].Num(); ++EdgeB)
                    {
                        if (!EdgeBounds[A][EdgeA].Touches(EdgeBounds[B][EdgeB])) { continue; }
                        const FVector2D R = Rings[B][EdgeB], Other = Rings[B][(EdgeB + 1) % Rings[B].Num()] - R;
                        const double OtherLength = Other.Size();
                        if (Dot(Direction, Other) * Areas[A] * Areas[B] >= 0.0 ||
                            FMath::Abs(Cross(Direction / Length, R - P)) > Epsilon ||
                            FMath::Abs(Cross(Direction / Length, R + Other - P)) > Epsilon) { continue; }
                        const double U0 = Dot(R - P, Direction) / (Length * Length), U1 = Dot(R + Other - P, Direction) / (Length * Length);
                        const double Start = FMath::Max(0.0, FMath::Min(U0, U1)), End = FMath::Min(1.0, FMath::Max(U0, U1));
                        if ((End - Start) * Length <= Epsilon || OtherLength <= Epsilon) { continue; }
                        const double StartB = FMath::Clamp(Dot(P + Direction * Start - R, Other) / Other.SizeSquared(), 0.0, 1.0);
                        const double EndB = FMath::Clamp(Dot(P + Direction * End - R, Other) / Other.SizeSquared(), 0.0, 1.0);
                        OutMatches.Add({A, EdgeA, B, EdgeB, Start, End, StartB, EndB}); Components.Join(A, B);
                    }
                }
            }
        }
    }

    bool ConsensusPosition(TArray<FNode>& Nodes, const int32 Root, TArray<FSeamLine>& Lines,
        const TArray<TArray<FVector2D>>& MovementAnchors, const double Radius, FVector2D& OutPosition)
    {
        TArray<FVector2D> Sources, Centers;
        Sources.Reserve(Nodes[Root].Count); Centers.Reserve(Nodes[Root].Count);
        for (int32 Member = Nodes[Root].Head; Member != INDEX_NONE; Member = Nodes[Member].Next)
        {
            Sources.Add(Nodes[Member].Source);
            Centers.Add(AuthoredPoint(MovementAnchors[Nodes[Member].Polygon], Nodes[Member].Key));
        }
        Sources.Sort(PointBefore); Centers.Sort(PointBefore);
        const FVector2D Origin = Sources[0];
        FVector2D Delta = FVector2D::ZeroVector;
        for (const FVector2D& Source : Sources) { Delta += Source - Origin; }
        OutPosition = Origin + Delta / Sources.Num();
        // Incident midpoint lines define a common junction without counting a long Zone twice.
        // In full rank, intersect the least-squares normals (T: x=midline AND y=midline). Parallel
        // seams project the centroid onto their common midline. All arithmetic is translated.
        Lines.Sort([](const FSeamLine& A, const FSeamLine& B)
        {
            return A.Start != B.Start ? PointBefore(A.Start, B.Start) : PointBefore(A.End, B.End);
        });
        double XX = 0.0, XY = 0.0, YY = 0.0;
        FVector2D RightHandSide = FVector2D::ZeroVector;
        for (const FSeamLine& Line : Lines)
        {
            const FVector2D Direction = Line.End - Line.Start;
            const double Length = Direction.Size();
            if (Length <= Epsilon) { continue; }
            const FVector2D Normal(-Direction.Y / Length, Direction.X / Length);
            const double Distance = Dot(Normal, Line.Start - OutPosition);
            XX += Normal.X * Normal.X; XY += Normal.X * Normal.Y; YY += Normal.Y * Normal.Y;
            RightHandSide += Normal * Distance;
        }
        const double Trace = XX + YY;
        const double Determinant = XX * YY - XY * XY;
        if (Trace > 0.0)
        {
            FVector2D Adjustment = RightHandSide / Trace;
            if (Determinant > 1.e-8 * Trace * Trace)
            {
                Adjustment.X = (YY * RightHandSide.X - XY * RightHandSide.Y) / Determinant;
                Adjustment.Y = (XX * RightHandSide.Y - XY * RightHandSide.X) / Determinant;
            }
            const FVector2D Candidate = OutPosition + Adjustment;
            if (FMath::IsFinite(Candidate.X) && FMath::IsFinite(Candidate.Y)) { OutPosition = Candidate; }
        }
        // Multiway means must lie in every member's ORIGINAL displacement disk. Stable cyclic
        // projection handles feasible off-centre intersections without independent side clamping.
        const double RadiusSquared = FMath::Square(Radius + Epsilon);
        for (int32 Iteration = 0; Iteration < 64; ++Iteration)
        {
            bool bInside = true;
            for (const FVector2D& Center : Centers)
            {
                const FVector2D Offset = OutPosition - Center;
                const double DistanceSquared = Offset.SizeSquared();
                if (DistanceSquared > RadiusSquared)
                {
                    bInside = false; OutPosition = Center + Offset * (Radius / FMath::Sqrt(DistanceSquared));
                }
            }
            if (bInside) { return FMath::IsFinite(OutPosition.X) && FMath::IsFinite(OutPosition.Y); }
        }
        return false;
    }

    bool BuildPass(const TArray<TArray<FVector2D>>& Authored,
        const TArray<TArray<FVector2D>>& MovementAnchors, const double Radius,
        const FFZAWeldDiagnostics::FResult& Diagnostics, FPolygonSets& Components,
        FResult& Result, bool& OutChanged)
    {
        const TArray<TArray<FVector2D>>& Current = Result.Polygons;
        TArray<FMatch> Matches;
        Matches.Reserve(Diagnostics.Issues.Num());
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Diagnostics.Issues)
        {
            if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Overlap)
            {
                Components.Join(Issue.PolygonIndexA, Issue.PolygonIndexB); continue;
            }
            if (Issue.Type != FFZAWeldDiagnostics::EIssueType::Gap) { continue; }
            FMatch Match{Issue.PolygonIndexA, INDEX_NONE, Issue.PolygonIndexB, INDEX_NONE, 0.0, 0.0, 0.0, 0.0};
            if (!EdgeForSpan(Current[Match.PolygonA], Issue.StartA, Issue.EndA, Match.EdgeA, Match.StartA, Match.EndA) ||
                !EdgeForSpan(Current[Match.PolygonB], Issue.StartB, Issue.EndB, Match.EdgeB, Match.StartB, Match.EndB))
            {
                Result.FailureReason = TEXT("검출한 틈 구간을 원본 경계에 정확히 연결할 수 없습니다.");
                Result.RejectedPolygonIndices.AddUnique(Match.PolygonA); Result.RejectedPolygonIndices.AddUnique(Match.PolygonB); return false;
            }
            Matches.Add(Match); Components.Join(Match.PolygonA, Match.PolygonB);
        }
        TArray<double> Areas;
        Areas.Reserve(Current.Num()); for (const TArray<FVector2D>& Ring : Current) { Areas.Add(SignedArea(Ring)); }
        ExactSharedMatches(Current, Areas, Matches, Components);
        if (Matches.IsEmpty()) { OutChanged = false; return true; }
        if (Matches.Num() > MaximumIssues)
        {
            Result.bBlocked = true; Result.FailureReason = TEXT("공유 경계 구간 수가 자동 복구 처리 한도를 넘습니다."); return false;
        }

        TArray<FNode> Nodes;
        TArray<FPolygonCuts> Cuts;
        TArray<TArray<int32>> OriginalNodes;
        Cuts.SetNum(Current.Num()); OriginalNodes.SetNum(Current.Num());
        int64 CurrentPointCount = 0;
        for (const TArray<FVector2D>& Ring : Current) { CurrentPointCount += Ring.Num(); }
        if (CurrentPointCount > MaximumNodes)
        {
            Result.bBlocked = true; Result.FailureReason = TEXT("경계 점 수가 자동 복구 처리 한도를 넘습니다."); return false;
        }
        Nodes.Reserve(FMath::Min(MaximumNodes, static_cast<int32>(CurrentPointCount) + Matches.Num() * 4));
        for (int32 Polygon = 0; Polygon < Current.Num(); ++Polygon)
        {
            Cuts[Polygon].SetNum(Current[Polygon].Num()); OriginalNodes[Polygon].Reserve(Current[Polygon].Num());
            for (int32 Point = 0; Point < Current[Polygon].Num(); ++Point)
            {
                const int32 Node = Nodes.Num(); OriginalNodes[Polygon].Add(Node);
                Nodes.Emplace(Polygon, Current[Polygon][Point], Result.SourcePointKeys[Polygon][Point], Node);
            }
        }
        for (int32 Polygon = 0; Polygon < Current.Num(); ++Polygon)
        {
            for (int32 Edge = 0; Edge < Current[Polygon].Num(); ++Edge)
            {
                Cuts[Polygon][Edge].Add({0.0, OriginalNodes[Polygon][Edge]});
                Cuts[Polygon][Edge].Add({1.0, OriginalNodes[Polygon][(Edge + 1) % Current[Polygon].Num()]});
            }
        }
        bool bBudgetExceeded = false;
        const auto CutNode = [&](const int32 Polygon, const int32 Edge, const double Parameter) -> int32
        {
            TArray<FCut>& EdgeCuts = Cuts[Polygon][Edge];
            const double T = FMath::Clamp(Parameter, 0.0, 1.0);
            const double ParameterEpsilon = Epsilon / (Current[Polygon][(Edge + 1) % Current[Polygon].Num()] - Current[Polygon][Edge]).Size();
            for (const FCut& Cut : EdgeCuts) { if (FMath::Abs(Cut.T - T) <= ParameterEpsilon) { return Cut.Node; } }
            if (Nodes.Num() >= MaximumNodes) { bBudgetExceeded = true; return INDEX_NONE; }
            const int32 Node = Nodes.Num();
            Nodes.Emplace(Polygon, FMath::Lerp(Current[Polygon][Edge], Current[Polygon][(Edge + 1) % Current[Polygon].Num()], T),
                SplitKey(Authored[Polygon], Current[Polygon], Result.SourcePointKeys[Polygon], Edge, T), Node);
            EdgeCuts.Add({T, Node}); return Node;
        };
        TArray<FLink> Links;
        Links.Reserve(Matches.Num() * 2);
        for (const FMatch& Match : Matches)
        {
            Links.Add({CutNode(Match.PolygonA, Match.EdgeA, Match.StartA), CutNode(Match.PolygonB, Match.EdgeB, Match.StartB)});
            Links.Add({CutNode(Match.PolygonA, Match.EdgeA, Match.EndA), CutNode(Match.PolygonB, Match.EdgeB, Match.EndB)});
        }
        bool bCutsStable = false;
        for (int32 Iteration = 0; Iteration < MaximumCutPropagationPasses && !bBudgetExceeded; ++Iteration)
        {
            const int32 BeforeCount = Nodes.Num();
            for (const FMatch& Match : Matches)
            {
                if (bBudgetExceeded) { break; }
                const auto Propagate = [&](const int32 SourcePolygon, const int32 SourceEdge, const double Start, const double End,
                    const int32 TargetPolygon, const int32 TargetEdge, const double TargetStart, const double TargetEnd)
                {
                    if (bBudgetExceeded) { return; }
                    const TArray<FCut>& SourceCuts = Cuts[SourcePolygon][SourceEdge];
                    const double ParameterEpsilon = Epsilon / (Current[SourcePolygon][(SourceEdge + 1) % Current[SourcePolygon].Num()] - Current[SourcePolygon][SourceEdge]).Size();
                    for (const FCut& Cut : SourceCuts)
                    {
                        if (Cut.T < FMath::Min(Start, End) - ParameterEpsilon || Cut.T > FMath::Max(Start, End) + ParameterEpsilon) { continue; }
                        const double Alpha = FMath::Clamp((Cut.T - Start) / (End - Start), 0.0, 1.0);
                        const int32 TargetNode = CutNode(TargetPolygon, TargetEdge, FMath::Lerp(TargetStart, TargetEnd, Alpha));
                        if (Links.Num() >= MaximumLinks) { bBudgetExceeded = true; return; }
                        Links.Add({Cut.Node, TargetNode});
                    }
                };
                Propagate(Match.PolygonA, Match.EdgeA, Match.StartA, Match.EndA, Match.PolygonB, Match.EdgeB, Match.StartB, Match.EndB);
                Propagate(Match.PolygonB, Match.EdgeB, Match.StartB, Match.EndB, Match.PolygonA, Match.EdgeA, Match.StartA, Match.EndA);
            }
            if (Nodes.Num() == BeforeCount) { bCutsStable = true; break; }
        }
        if (bBudgetExceeded || !bCutsStable)
        {
            Result.bBlocked = true;
            Result.FailureReason = TEXT("공유 경계 점 전파가 처리 한도 안에서 안정되지 않았습니다."); return false;
        }
        const TArray<int32> Order = GeometryOrder(Current);
        TArray<int32> Ranks; Ranks.SetNumUninitialized(Current.Num());
        for (int32 Rank = 0; Rank < Order.Num(); ++Rank) { Ranks[Order[Rank]] = Rank; }
        const auto NodeBefore = [&](const int32 A, const int32 B)
        {
            return Nodes[A].Source != Nodes[B].Source ? PointBefore(Nodes[A].Source, Nodes[B].Source) : Ranks[Nodes[A].Polygon] < Ranks[Nodes[B].Polygon];
        };
        for (FLink& Link : Links) { if (NodeBefore(Link.B, Link.A)) { Swap(Link.A, Link.B); } }
        Links.Sort([&](const FLink& A, const FLink& B) { return A.A != B.A ? NodeBefore(A.A, B.A) : NodeBefore(A.B, B.B); });
        for (const FLink& Link : Links) { JoinNodes(Nodes, Link.A, Link.B, Authored); }
        TArray<TArray<FSeamLine>> LinesByRoot; LinesByRoot.SetNum(Nodes.Num());
        for (const FMatch& Match : Matches)
        {
            const FVector2D A = Current[Match.PolygonA][Match.EdgeA];
            const FVector2D AD = Current[Match.PolygonA][(Match.EdgeA + 1) % Current[Match.PolygonA].Num()] - A;
            const FVector2D B = Current[Match.PolygonB][Match.EdgeB];
            const FVector2D BD = Current[Match.PolygonB][(Match.EdgeB + 1) % Current[Match.PolygonB].Num()] - B;
            const FVector2D StartA = A + AD * Match.StartA, StartB = B + BD * Match.StartB;
            const FVector2D EndA = A + AD * Match.EndA, EndB = B + BD * Match.EndB;
            FSeamLine Line{StartA + (StartB - StartA) * 0.5, EndA + (EndB - EndA) * 0.5};
            if (PointBefore(Line.End, Line.Start)) { Swap(Line.Start, Line.End); }
            TSet<int32> Roots;
            const double ParameterEpsilon = Epsilon / AD.Size();
            for (const FCut& Cut : Cuts[Match.PolygonA][Match.EdgeA])
            {
                if (Cut.T >= FMath::Min(Match.StartA, Match.EndA) - ParameterEpsilon &&
                    Cut.T <= FMath::Max(Match.StartA, Match.EndA) + ParameterEpsilon)
                {
                    Roots.Add(FindNode(Nodes, Cut.Node));
                }
            }
            for (const int32 Root : Roots) { LinesByRoot[Root].Add(Line); }
        }
        TArray<FVector2D> Positions; Positions.SetNumUninitialized(Nodes.Num());
        for (int32 Node = 0; Node < Nodes.Num(); ++Node)
        {
            if (FindNode(Nodes, Node) == Node && !ConsensusPosition(Nodes, Node, LinesByRoot[Node], MovementAnchors, Radius, Positions[Node]))
            {
                for (int32 Member = Nodes[Node].Head; Member != INDEX_NONE; Member = Nodes[Member].Next) { Result.RejectedPolygonIndices.AddUnique(Nodes[Member].Polygon); }
                Result.FailureReason = TEXT("공유 접점을 만들면 원본 경계의 최대 이동 범위를 넘습니다."); return false;
            }
        }
        TArray<TArray<FVector2D>> Proposed;
        TArray<TArray<double>> ProposedKeys;
        Proposed.SetNum(Current.Num()); ProposedKeys.SetNum(Current.Num());
        OutChanged = false;
        for (int32 Polygon = 0; Polygon < Current.Num(); ++Polygon)
        {
            Proposed[Polygon].Reserve(Current[Polygon].Num()); ProposedKeys[Polygon].Reserve(Current[Polygon].Num());
            TSet<int32> EmittedClusters;
            for (int32 Edge = 0; Edge < Current[Polygon].Num(); ++Edge)
            {
                TArray<FCut>& EdgeCuts = Cuts[Polygon][Edge];
                EdgeCuts.Sort([](const FCut& A, const FCut& B) { return A.T < B.T; });
                for (const FCut& Cut : EdgeCuts)
                {
                    if (Cut.T == 1.0) { continue; }
                    const int32 Root = FindNode(Nodes, Cut.Node);
                    if (EmittedClusters.Contains(Root)) { continue; }
                    EmittedClusters.Add(Root);
                    Proposed[Polygon].Add(Positions[Root]);
                    ProposedKeys[Polygon].Add(KeyForCluster(Nodes, Root, Polygon, Authored[Polygon].Num()));
                }
            }
            double Area = 0.0;
            if (!ValidRing(Proposed[Polygon], Area) || (Area > 0.0) != (Areas[Polygon] > 0.0))
            {
                Result.RejectedPolygonIndices.AddUnique(Polygon);
                Result.FailureReason = TEXT("동시 용접이 Zone을 붕괴시키거나 자기교차를 만듭니다. 전체 적용을 중단했습니다."); return false;
            }
            OutChanged |= Proposed[Polygon] != Current[Polygon];
        }
        Result.Polygons = MoveTemp(Proposed); Result.SourcePointKeys = MoveTemp(ProposedKeys);
        ++Result.PassCount; return true;
    }
}

bool Build(const TArray<TArray<FVector2D>>& AuthoredPolygons, const double RepairRadiusCm, FResult& OutResult,
    const TArray<TArray<FVector2D>>* MovementOrigins)
{
    OutResult = FResult(); OutResult.Polygons = AuthoredPolygons;
    OutResult.SourcePointKeys.SetNum(AuthoredPolygons.Num());
    if (AuthoredPolygons.IsEmpty() || !FMath::IsFinite(RepairRadiusCm) || RepairRadiusCm <= 0.0 ||
        !FMath::IsFinite(FMath::Square(RepairRadiusCm + Epsilon)))
    {
        OutResult.FailureReason = TEXT("유효한 Zone 경계와 유한한 양수 복구 범위가 필요합니다."); return false;
    }
    int64 PointCount = 0;
    for (const TArray<FVector2D>& Ring : AuthoredPolygons) { PointCount += Ring.Num(); }
    if (PointCount > MaximumNodes)
    {
        OutResult.bBlocked = true;
        OutResult.FailureReason = TEXT("전체 경계 점 수가 동시 자동 용접 처리 한도를 넘습니다."); return true;
    }
    const TArray<TArray<FVector2D>>& MovementAnchors = MovementOrigins ? *MovementOrigins : AuthoredPolygons;
    if (MovementAnchors.Num() != AuthoredPolygons.Num())
    {
        OutResult.FailureReason = TEXT("이동 기준점의 Zone 수가 계산할 경계와 일치하지 않습니다."); return false;
    }
    const double MovementRadiusSquared = FMath::Square(RepairRadiusCm + Epsilon);
    for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
    {
        if (MovementAnchors[Polygon].Num() != AuthoredPolygons[Polygon].Num())
        {
            OutResult.FailureReason = TEXT("이동 기준점 수가 해당 Zone 경계의 점 수와 일치하지 않습니다."); return false;
        }
        for (int32 Point = 0; Point < MovementAnchors[Polygon].Num(); ++Point)
        {
            const FVector2D& Anchor = MovementAnchors[Polygon][Point];
            if (!FMath::IsFinite(Anchor.X) || !FMath::IsFinite(Anchor.Y))
            {
                OutResult.FailureReason = TEXT("이동 기준점은 유한한 XY 좌표여야 합니다."); return false;
            }
            if (MovementOrigins && FVector2D::DistSquared(AuthoredPolygons[Polygon][Point], Anchor) > MovementRadiusSquared)
            {
                OutResult.RejectedPolygonIndices.AddUnique(Polygon);
            }
        }
    }
    if (!OutResult.RejectedPolygonIndices.IsEmpty())
    {
        OutResult.FailureReason = TEXT("초기 경계점이 누적 이동 기준점의 허용 범위를 이미 넘습니다."); return false;
    }
    for (int32 Polygon = 0; Polygon < AuthoredPolygons.Num(); ++Polygon)
    {
        double Area = 0.0;
        if (!ValidRing(AuthoredPolygons[Polygon], Area)) { OutResult.RejectedPolygonIndices.Add(Polygon); }
        OutResult.SourcePointKeys[Polygon].Reserve(AuthoredPolygons[Polygon].Num());
        for (int32 Point = 0; Point < AuthoredPolygons[Polygon].Num(); ++Point) { OutResult.SourcePointKeys[Polygon].Add(static_cast<double>(Point)); }
    }
    if (!OutResult.RejectedPolygonIndices.IsEmpty())
    {
        OutResult.FailureReason = TEXT("유효하지 않은 Zone 경계가 있어 동시 자동 용접을 계산할 수 없습니다."); return false;
    }
    const TArray<int32> Order = GeometryOrder(AuthoredPolygons);
    FPolygonSets Components(AuthoredPolygons.Num());
    for (int32 Iteration = 0; Iteration < MaximumPasses; ++Iteration)
    {
        FFZAWeldDiagnostics::FResult Diagnostics;
        InspectCanonical(OutResult.Polygons, RepairRadiusCm, Diagnostics, MaximumIssues);
        if (Diagnostics.bTruncated)
        {
            OutResult.bBlocked = true;
            OutResult.FailureReason = TEXT("전체 진단 구간 수가 동시 자동 용접 처리 한도를 넘습니다."); break;
        }
        bool bChanged = false;
        if (!BuildPass(AuthoredPolygons, MovementAnchors, RepairRadiusCm, Diagnostics, Components, OutResult, bChanged)) { break; }
        FFZAWeldDiagnostics::FResult After;
        InspectCanonical(OutResult.Polygons, RepairRadiusCm, After, 0);
        if (After.GapPairCount == 0) { OutResult.bComplete = true; break; }
        if (!bChanged)
        {
            OutResult.FailureReason = TEXT("남은 틈을 줄이는 유효한 공유 경계를 만들 수 없습니다."); break;
        }
    }
    if (!OutResult.bComplete && OutResult.FailureReason.IsEmpty())
    {
        OutResult.FailureReason = TEXT("반복 처리 한도 안에서 모든 틈을 제거하지 못했습니다.");
    }
    Components.Groups(Order, OutResult.Groups);
    return true;
}
}
