#include "FFZABoundaryNormalize.h"

#include "FFZAPolygonMath.h"

namespace FFZABoundaryNormalize
{
namespace
{
    constexpr int32 MaximumPoints = 262144;
    constexpr int64 MaximumComparisons = 2097152;

    double NormalizeSignedArea(const TArray<FVector2D>& Ring)
    {
        double TwiceArea = 0.0;
        for (int32 I = 1; I + 1 < Ring.Num(); ++I)
        {
            const FVector2D A = Ring[I] - Ring[0];
            const FVector2D B = Ring[I + 1] - Ring[0];
            TwiceArea += A.X * B.Y - A.Y * B.X;
        }
        return TwiceArea * 0.5;
    }

    bool NormalizePointBefore(const FVector2D& A, const FVector2D& B)
    {
        return A.X != B.X ? A.X < B.X : A.Y < B.Y;
    }

    FVector2D NormalizeAuthoredPoint(const TArray<FVector2D>& Original, const double Key)
    {
        const int32 Edge = FMath::FloorToInt(Key);
        return FMath::Lerp(Original[Edge], Original[(Edge + 1) % Original.Num()], Key - Edge);
    }

    struct FNormalizeNode
    {
        FVector2D Point;
        FVector2D Source;
        double Key = 0.0;
        int32 Polygon = INDEX_NONE;
        int32 Cluster = INDEX_NONE;
        bool bOriginalCorner = false;
        bool bUnchangedCorner = false;
    };

    struct FNormalizeCluster
    {
        FBox2D Bounds = FBox2D(ForceInit);
        FVector2D Representative = FVector2D::ZeroVector;
        TArray<int32> Members;
        TSet<int32> Polygons;
        TMap<int32, FBox2D> OriginalSourceBounds;
        int64 SeedYCell = 0;
        bool bRepresentativeIsOriginalCorner = false;
        bool bRepresentativeIsUnchangedCorner = false;
    };

    bool NormalizeRepresentativeBefore(const FNormalizeNode& Node, const FNormalizeCluster& Cluster)
    {
        if (Node.bUnchangedCorner != Cluster.bRepresentativeIsUnchangedCorner) { return Node.bUnchangedCorner; }
        if (Node.bOriginalCorner != Cluster.bRepresentativeIsOriginalCorner) { return Node.bOriginalCorner; }
        return NormalizePointBefore(Node.Point, Cluster.Representative);
    }

    bool NormalizeKeyBefore(const FNormalizeNode& A, const FNormalizeNode& B, const FVector2D& Representative)
    {
        // Retained corners keep exact integer keys and their own original Z. Otherwise take
        // the nearest current source node, using authored geometry rather than ring index
        // as a deterministic tie-break when the spline direction/start changes.
        if (A.bOriginalCorner != B.bOriginalCorner) { return A.bOriginalCorner; }
        const double DistanceA = FVector2D::DistSquared(A.Point, Representative);
        const double DistanceB = FVector2D::DistSquared(B.Point, Representative);
        if (DistanceA != DistanceB) { return DistanceA < DistanceB; }
        if (A.Source != B.Source) { return NormalizePointBefore(A.Source, B.Source); }
        if (A.Point != B.Point) { return NormalizePointBefore(A.Point, B.Point); }
        return A.Key < B.Key;
    }

    bool NormalizeParameterOnOriginalEdge(const double Key, const int32 Edge, const int32 Count, double& OutT)
    {
        OutT = Key - Edge;
        if (OutT < -1.e-9) { OutT += Count; }
        if (OutT < -1.e-9 || OutT > 1.0 + 1.e-9) { return false; }
        OutT = FMath::Clamp(OutT, 0.0, 1.0); return true;
    }

    double NormalizeClosestOriginalKey(const FVector2D& Point, const TArray<FVector2D>& Original)
    {
        double BestDistance = TNumericLimits<double>::Max(), BestKey = 0.0;
        for (int32 Edge = 0; Edge < Original.Num(); ++Edge)
        {
            const FVector2D Direction = Original[(Edge + 1) % Original.Num()] - Original[Edge];
            const double LengthSquared = Direction.SizeSquared();
            if (LengthSquared <= 1.e-12) { continue; }
            const double T = FMath::Clamp(FVector2D::DotProduct(Point - Original[Edge], Direction) / LengthSquared, 0.0, 1.0);
            const FVector2D Projected = Original[Edge] + Direction * T;
            const double Distance = FVector2D::DistSquared(Point, Projected);
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                BestKey = T == 1.0 ? static_cast<double>((Edge + 1) % Original.Num()) : Edge + T;
            }
        }
        return BestKey;
    }

    double NormalizeSplitKey(const TArray<FVector2D>& Ring, const TArray<double>& Keys,
        const int32 Edge, const double T, const TArray<FVector2D>& Original)
    {
        const double A = Keys[Edge], B = Keys[(Edge + 1) % Keys.Num()];
        const int32 FirstEdge = FMath::FloorToInt(A);
        for (const int32 Candidate : {FirstEdge, (FirstEdge + Original.Num() - 1) % Original.Num()})
        {
            double Start = 0.0, End = 0.0;
            if (NormalizeParameterOnOriginalEdge(A, Candidate, Original.Num(), Start) &&
                NormalizeParameterOnOriginalEdge(B, Candidate, Original.Num(), End))
            {
                double Key = Candidate + FMath::Lerp(Start, End, T);
                if (Key >= Original.Num()) { Key = 0.0; }
                return Key;
            }
        }
        return NormalizeClosestOriginalKey(FMath::Lerp(Ring[Edge], Ring[(Edge + 1) % Ring.Num()], T), Original);
    }

    struct FNormalizeEdge
    {
        FVector2D A, B;
        FBox2D Bounds = FBox2D(ForceInit);
        int32 Polygon = INDEX_NONE;
        int32 Edge = INDEX_NONE;
        bool bOriginalEdge = false;
    };

    struct FNormalizeEdgeIndex
    {
        TArray<FNormalizeEdge> Edges;
        TArray<double> PrefixMaximumX;

        void Build(const TArray<TArray<FVector2D>>& Rings, const TArray<TArray<double>>& Keys,
            const TArray<TArray<FVector2D>>& Original)
        {
            Edges.Reset(); PrefixMaximumX.Reset();
            for (int32 Polygon = 0; Polygon < Rings.Num(); ++Polygon)
            {
                for (int32 Point = 0; Point < Rings[Polygon].Num(); ++Point)
                {
                    FNormalizeEdge Edge;
                    Edge.A = Rings[Polygon][Point]; Edge.B = Rings[Polygon][(Point + 1) % Rings[Polygon].Num()];
                    Edge.Bounds += Edge.A; Edge.Bounds += Edge.B; Edge.Polygon = Polygon; Edge.Edge = Point;
                    const double AKey = Keys[Polygon][Point], BKey = Keys[Polygon][(Point + 1) % Keys[Polygon].Num()];
                    if (Edge.A.Equals(NormalizeAuthoredPoint(Original[Polygon], AKey), 1.e-8) &&
                        Edge.B.Equals(NormalizeAuthoredPoint(Original[Polygon], BKey), 1.e-8))
                    {
                        const int32 First = FMath::FloorToInt(AKey);
                        for (const int32 Candidate : {First, (First + Original[Polygon].Num() - 1) % Original[Polygon].Num()})
                        {
                            double Start, End;
                            if (NormalizeParameterOnOriginalEdge(AKey, Candidate, Original[Polygon].Num(), Start) &&
                                NormalizeParameterOnOriginalEdge(BKey, Candidate, Original[Polygon].Num(), End))
                            {
                                Edge.bOriginalEdge = true; break;
                            }
                        }
                    }
                    Edges.Add(MoveTemp(Edge));
                }
            }
            Edges.Sort([](const FNormalizeEdge& A, const FNormalizeEdge& B)
            {
                if (A.Bounds.Min.X != B.Bounds.Min.X) { return A.Bounds.Min.X < B.Bounds.Min.X; }
                if (A.Bounds.Min.Y != B.Bounds.Min.Y) { return A.Bounds.Min.Y < B.Bounds.Min.Y; }
                if (A.Bounds.Max.X != B.Bounds.Max.X) { return A.Bounds.Max.X < B.Bounds.Max.X; }
                if (A.Bounds.Max.Y != B.Bounds.Max.Y) { return A.Bounds.Max.Y < B.Bounds.Max.Y; }
                if (A.A != B.A) { return NormalizePointBefore(A.A, B.A); }
                return NormalizePointBefore(A.B, B.B);
            });
            double MaximumX = -TNumericLimits<double>::Max();
            for (const FNormalizeEdge& Edge : Edges)
            {
                MaximumX = FMath::Max(MaximumX, Edge.Bounds.Max.X); PrefixMaximumX.Add(MaximumX);
            }
        }

        bool Query(const FVector2D& Point, const double Tolerance, int64& Comparisons,
            TArray<int32>& OutEdges) const
        {
            OutEdges.Reset();
            const double MinX = Point.X - Tolerance, MaxX = Point.X + Tolerance;
            int32 Low = 0, High = Edges.Num();
            while (Low < High)
            {
                const int32 Middle = Low + (High - Low) / 2;
                if (Edges[Middle].Bounds.Min.X <= MaxX) { Low = Middle + 1; } else { High = Middle; }
            }
            for (int32 Index = Low - 1; Index >= 0 && PrefixMaximumX[Index] >= MinX; --Index)
            {
                if (++Comparisons > MaximumComparisons) { return false; }
                const FBox2D& Bounds = Edges[Index].Bounds;
                if (Bounds.Max.X >= MinX && Bounds.Min.Y <= Point.Y + Tolerance && Bounds.Max.Y >= Point.Y - Tolerance)
                {
                    OutEdges.Add(Index);
                }
            }
            return true;
        }
    };

    bool NormalizeEdgeBefore(const FNormalizeEdge& A, const FNormalizeEdge& B)
    {
        if (A.bOriginalEdge != B.bOriginalEdge) { return A.bOriginalEdge; }
        const FVector2D& AStart = NormalizePointBefore(A.A, A.B) ? A.A : A.B;
        const FVector2D& AEnd = NormalizePointBefore(A.A, A.B) ? A.B : A.A;
        const FVector2D& BStart = NormalizePointBefore(B.A, B.B) ? B.A : B.B;
        const FVector2D& BEnd = NormalizePointBefore(B.A, B.B) ? B.B : B.A;
        if (AStart != BStart) { return NormalizePointBefore(AStart, BStart); }
        return NormalizePointBefore(AEnd, BEnd);
    }

    int32 NormalizeClusterRoot(TArray<int32>& Parents, const int32 Index)
    {
        int32 Root = Index;
        while (Parents[Root] != Root) { Root = Parents[Root]; }
        int32 Current = Index;
        while (Parents[Current] != Current)
        {
            const int32 Next = Parents[Current]; Parents[Current] = Root; Current = Next;
        }
        return Root;
    }

    bool NormalizeMergeProjectedClusters(TArray<FNormalizeCluster>& Clusters, TArray<FNormalizeNode>& Nodes,
        const double Tolerance, int64& Comparisons, FString& OutError)
    {
        const double ToleranceSquared = Tolerance * Tolerance;
        TArray<int32> Parents;
        Parents.Reserve(Clusters.Num());
        for (int32 Index = 0; Index < Clusters.Num(); ++Index) { Parents.Add(Index); }
        bool bConverged = false;
        constexpr int32 MaximumMergePasses = 16;
        for (int32 Pass = 0; Pass < MaximumMergePasses; ++Pass)
        {
            TArray<int32> Order;
            for (int32 Index = 0; Index < Clusters.Num(); ++Index)
            {
                if (NormalizeClusterRoot(Parents, Index) == Index) { Order.Add(Index); }
            }
            Order.Sort([&Clusters](const int32 A, const int32 B)
            {
                if (Clusters[A].Representative != Clusters[B].Representative)
                {
                    return NormalizePointBefore(Clusters[A].Representative, Clusters[B].Representative);
                }
                return A < B;
            });
            TArray<TPair<int32, int32>> Pairs;
            TMap<int64, TArray<int32>> ActiveY;
            TArray<int64> YCells;
            YCells.SetNum(Order.Num());
            const double OriginY = Order.IsEmpty() ? 0.0 : Clusters[Order[0]].Representative.Y;
            int32 FirstActive = 0;
            for (int32 Position = 0; Position < Order.Num(); ++Position)
            {
                const int32 Current = Order[Position];
                const FVector2D& Point = Clusters[Current].Representative;
                while (FirstActive < Position && Point.X - Clusters[Order[FirstActive]].Representative.X > Tolerance)
                {
                    if (TArray<int32>* Bucket = ActiveY.Find(YCells[FirstActive]))
                    {
                        Bucket->Remove(Order[FirstActive]);
                        if (Bucket->IsEmpty()) { ActiveY.Remove(YCells[FirstActive]); }
                    }
                    ++FirstActive;
                }
                const double CellCoordinate = (Point.Y - OriginY) / Tolerance;
                if (!FMath::IsFinite(CellCoordinate) || FMath::Abs(CellCoordinate) > 1.e15)
                {
                    OutError = TEXT("투영 후 공유 경계 병합의 좌표 범위를 넘었습니다."); return false;
                }
                const int64 YCell = FMath::FloorToInt64(CellCoordinate);
                YCells[Position] = YCell;
                for (int64 Cell = YCell - 1; Cell <= YCell + 1; ++Cell)
                {
                    const TArray<int32>* Bucket = ActiveY.Find(Cell);
                    if (!Bucket) { continue; }
                    for (const int32 Other : *Bucket)
                    {
                        if (++Comparisons > MaximumComparisons)
                        {
                            OutError = TEXT("투영 후 공유 경계 병합의 인접 점 처리 한도를 넘었습니다."); return false;
                        }
                        if (FVector2D::DistSquared(Clusters[Other].Representative, Point) <= ToleranceSquared)
                        {
                            Pairs.Add({Other, Current});
                        }
                    }
                }
                ActiveY.FindOrAdd(YCell).Add(Current);
            }
            bool bChanged = false;
            for (const TPair<int32, int32>& CandidatePair : Pairs)
            {
                    int32 A = NormalizeClusterRoot(Parents, CandidatePair.Key);
                    int32 B = NormalizeClusterRoot(Parents, CandidatePair.Value);
                    if (A == B) { continue; }
                    if (FVector2D::DistSquared(Clusters[A].Representative, Clusters[B].Representative) > ToleranceSquared) { continue; }
                    bool bCompatible = true;
                    for (const TPair<int32, FBox2D>& Pair : Clusters[B].OriginalSourceBounds)
                    {
                        if (++Comparisons > MaximumComparisons)
                        {
                            OutError = TEXT("투영 후 공유 경계 병합의 원본 점 처리 한도를 넘었습니다."); return false;
                        }
                        if (const FBox2D* Existing = Clusters[A].OriginalSourceBounds.Find(Pair.Key))
                        {
                            FBox2D Combined = *Existing; Combined += Pair.Value.Min; Combined += Pair.Value.Max;
                            if ((Combined.Max - Combined.Min).SizeSquared() > ToleranceSquared) { bCompatible = false; break; }
                        }
                    }
                    if (!bCompatible) { continue; }
                    int32 Preferred = A, Secondary = B;
                    if (Clusters[A].bRepresentativeIsUnchangedCorner != Clusters[B].bRepresentativeIsUnchangedCorner)
                    {
                        Preferred = Clusters[A].bRepresentativeIsUnchangedCorner ? A : B;
                    }
                    else if (Clusters[A].bRepresentativeIsOriginalCorner != Clusters[B].bRepresentativeIsOriginalCorner)
                    {
                        Preferred = Clusters[A].bRepresentativeIsOriginalCorner ? A : B;
                    }
                    else if (NormalizePointBefore(Clusters[B].Representative, Clusters[A].Representative)) { Preferred = B; }
                    Secondary = Preferred == A ? B : A;
                    int32 Selected = INDEX_NONE;
                    for (const int32 Candidate : {Preferred, Secondary})
                    {
                        bool bBounded = true;
                        for (const int32 Source : {A, B})
                        {
                            for (const int32 Member : Clusters[Source].Members)
                            {
                                if (++Comparisons > MaximumComparisons)
                                {
                                    OutError = TEXT("투영 후 공유 경계 병합의 초기 이동 검증 한도를 넘었습니다."); return false;
                                }
                                if (FVector2D::DistSquared(Nodes[Member].Point, Clusters[Candidate].Representative) > ToleranceSquared)
                                {
                                    bBounded = false; break;
                                }
                            }
                            if (!bBounded) { break; }
                        }
                        if (bBounded) { Selected = Candidate; break; }
                    }
                    if (Selected == INDEX_NONE) { continue; }
                    const FVector2D Representative = Clusters[Selected].Representative;
                    const bool bOriginalCorner = Clusters[Selected].bRepresentativeIsOriginalCorner;
                    const bool bUnchangedCorner = Clusters[Selected].bRepresentativeIsUnchangedCorner;
                    const int32 Root = FMath::Min(A, B), Child = FMath::Max(A, B);
                    Clusters[Root].Members.Append(Clusters[Child].Members);
                    for (const int32 Polygon : Clusters[Child].Polygons) { Clusters[Root].Polygons.Add(Polygon); }
                    for (const TPair<int32, FBox2D>& Pair : Clusters[Child].OriginalSourceBounds)
                    {
                        if (FBox2D* Existing = Clusters[Root].OriginalSourceBounds.Find(Pair.Key))
                        {
                            *Existing += Pair.Value.Min; *Existing += Pair.Value.Max;
                        }
                        else { Clusters[Root].OriginalSourceBounds.Add(Pair.Key, Pair.Value); }
                    }
                    Clusters[Root].Representative = Representative;
                    Clusters[Root].bRepresentativeIsOriginalCorner = bOriginalCorner;
                    Clusters[Root].bRepresentativeIsUnchangedCorner = bUnchangedCorner;
                    Parents[Child] = Root;
                    bChanged = true;
            }
            if (!bChanged) { bConverged = true; break; }
        }
        if (!bConverged)
        {
            OutError = TEXT("투영 후 공유 경계 병합의 반복 처리 한도에 도달했습니다."); return false;
        }
        TArray<FNormalizeCluster> Compacted;
        TArray<int32> Remapped;
        Remapped.Init(INDEX_NONE, Clusters.Num());
        for (int32 Index = 0; Index < Clusters.Num(); ++Index)
        {
            if (NormalizeClusterRoot(Parents, Index) == Index)
            {
                Remapped[Index] = Compacted.Add(MoveTemp(Clusters[Index]));
            }
        }
        for (FNormalizeNode& Node : Nodes) { Node.Cluster = Remapped[NormalizeClusterRoot(Parents, Node.Cluster)]; }
        Clusters = MoveTemp(Compacted);
        return true;
    }

    FString NormalizeTopologyFailure(const int32 Polygon, const TArray<FVector2D>& Ring, const double Tolerance)
    {
        const double ToleranceSquared = Tolerance * Tolerance;
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            const int32 Next = (Edge + 1) % Ring.Num();
            const double Length = FVector2D::Distance(Ring[Edge], Ring[Next]);
            if (Length <= Tolerance)
            {
                return FString::Printf(TEXT("Zone %d: 경계 정리 후 %d-%d번 변이 %.9gcm로 너무 짧습니다."),
                    Polygon + 1, Edge, Next, Length);
            }
        }
        for (int32 AEdge = 0; AEdge < Ring.Num(); ++AEdge)
        {
            const FVector2D& A = Ring[AEdge]; const FVector2D& B = Ring[(AEdge + 1) % Ring.Num()];
            for (int32 BEdge = AEdge + 1; BEdge < Ring.Num(); ++BEdge)
            {
                if (BEdge == AEdge + 1 || (AEdge == 0 && BEdge == Ring.Num() - 1)) { continue; }
                const FVector2D& C = Ring[BEdge]; const FVector2D& D = Ring[(BEdge + 1) % Ring.Num()];
                const double DistanceSquared = FMath::Min(FMath::Min(
                    FVector2D::DistSquared(A, FFZAPolygonMath::ClosestPointOnSegment(A, C, D)),
                    FVector2D::DistSquared(B, FFZAPolygonMath::ClosestPointOnSegment(B, C, D))), FMath::Min(
                    FVector2D::DistSquared(C, FFZAPolygonMath::ClosestPointOnSegment(C, A, B)),
                    FVector2D::DistSquared(D, FFZAPolygonMath::ClosestPointOnSegment(D, A, B))));
                const FVector2D AB = B - A, CD = D - C;
                const auto CrossPair = [](const FVector2D& U, const FVector2D& V) { return U.X * V.Y - U.Y * V.X; };
                const double C1 = CrossPair(AB, C - A), C2 = CrossPair(AB, D - A);
                const double C3 = CrossPair(CD, A - C), C4 = CrossPair(CD, B - C);
                const bool bCrossing = ((C1 > 0.0 && C2 < 0.0) || (C1 < 0.0 && C2 > 0.0)) &&
                    ((C3 > 0.0 && C4 < 0.0) || (C3 < 0.0 && C4 > 0.0));
                if (bCrossing || DistanceSquared <= ToleranceSquared)
                {
                    return FString::Printf(TEXT("Zone %d: 경계 정리 후 비인접 변 %d/%d이 교차하거나 %.9gcm 이내로 접근합니다."),
                        Polygon + 1, AEdge, BEdge, FMath::Sqrt(DistanceSquared));
                }
            }
        }
        return FString::Printf(TEXT("Zone %d: 미세 경계 정리 결과가 유효한 단일 경계/방향을 유지하지 못합니다."), Polygon + 1);
    }
}

bool Normalize(TArray<TArray<FVector2D>>& InOutPolygons, TArray<TArray<double>>& InOutSourcePointKeys,
    const TArray<TArray<FVector2D>>& AuthoredPolygons, const double ToleranceCm, FString& OutError)
{
    OutError.Reset();
    if (!FMath::IsFinite(ToleranceCm) || ToleranceCm <= 0.0 ||
        !FMath::IsFinite(ToleranceCm * ToleranceCm) ||
        InOutPolygons.Num() != AuthoredPolygons.Num() || InOutSourcePointKeys.Num() != AuthoredPolygons.Num())
    {
        OutError = TEXT("공유 경계 정리의 원본, 보간 키 또는 정리 거리가 유효하지 않습니다."); return false;
    }
    int64 TotalPoints = 0;
    int64 TotalOriginalPoints = 0;
    FVector2D Origin = FVector2D::ZeroVector;
    bool bHasOrigin = false;
    for (int32 Polygon = 0; Polygon < InOutPolygons.Num(); ++Polygon)
    {
        const TArray<FVector2D>& Ring = InOutPolygons[Polygon];
        const TArray<FVector2D>& Original = AuthoredPolygons[Polygon];
        const TArray<double>& Keys = InOutSourcePointKeys[Polygon];
        TotalPoints += Ring.Num();
        TotalOriginalPoints += Original.Num();
        if (TotalPoints > MaximumPoints || TotalOriginalPoints > MaximumPoints)
        {
            OutError = TEXT("공유 경계 정리의 꼭짓점 처리 한도를 넘었습니다."); return false;
        }
        if (Ring.Num() < 3 || Original.Num() < 3 || Keys.Num() != Ring.Num())
        {
            OutError = FString::Printf(TEXT("Zone %d: 공유 경계 정리의 꼭짓점/높이 키 수가 유효하지 않습니다."), Polygon + 1);
            return false;
        }
        for (const FVector2D& Point : Original)
        {
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
            {
                OutError = FString::Printf(TEXT("Zone %d: 원본 경계 좌표가 유효하지 않습니다."), Polygon + 1); return false;
            }
        }
        for (int32 Point = 0; Point < Ring.Num(); ++Point)
        {
            if (!FMath::IsFinite(Ring[Point].X) || !FMath::IsFinite(Ring[Point].Y) ||
                !FMath::IsFinite(Keys[Point]) || Keys[Point] < 0.0 || Keys[Point] >= Original.Num())
            {
                OutError = FString::Printf(TEXT("Zone %d: 공유 경계 좌표 또는 원본 높이 키가 유효하지 않습니다."), Polygon + 1);
                return false;
            }
            if (!bHasOrigin || NormalizePointBefore(Ring[Point], Origin)) { Origin = Ring[Point]; bHasOrigin = true; }
        }
    }
    if (!bHasOrigin) { return true; }

    const double ToleranceSquared = ToleranceCm * ToleranceCm;
    TArray<FNormalizeNode> Nodes;
    Nodes.Reserve(static_cast<int32>(TotalPoints));
    TArray<TArray<int32>> NodeIndices;
    NodeIndices.SetNum(InOutPolygons.Num());
    for (int32 Polygon = 0; Polygon < InOutPolygons.Num(); ++Polygon)
    {
        NodeIndices[Polygon].Reserve(InOutPolygons[Polygon].Num());
        for (int32 Point = 0; Point < InOutPolygons[Polygon].Num(); ++Point)
        {
            FNormalizeNode Node;
            Node.Point = InOutPolygons[Polygon][Point];
            Node.Key = InOutSourcePointKeys[Polygon][Point];
            Node.Polygon = Polygon;
            Node.bOriginalCorner = Node.Key == static_cast<double>(FMath::FloorToInt(Node.Key));
            Node.Source = NormalizeAuthoredPoint(AuthoredPolygons[Polygon], Node.Key);
            if (!FMath::IsFinite(Node.Source.X) || !FMath::IsFinite(Node.Source.Y))
            {
                OutError = FString::Printf(TEXT("Zone %d: 원본 경계 보간 좌표가 유효하지 않습니다."), Polygon + 1); return false;
            }
            Node.bUnchangedCorner = Node.bOriginalCorner && Node.Point.Equals(Node.Source, 1.e-8);
            NodeIndices[Polygon].Add(Nodes.Add(MoveTemp(Node)));
        }
    }
    TArray<int32> Order;
    Order.Reserve(Nodes.Num());
    for (int32 I = 0; I < Nodes.Num(); ++I) { Order.Add(I); }
    Order.Sort([&Nodes](const int32 A, const int32 B)
    {
        if (Nodes[A].Point != Nodes[B].Point) { return NormalizePointBefore(Nodes[A].Point, Nodes[B].Point); }
        if (Nodes[A].bOriginalCorner != Nodes[B].bOriginalCorner) { return Nodes[A].bOriginalCorner; }
        if (Nodes[A].Source != Nodes[B].Source) { return NormalizePointBefore(Nodes[A].Source, Nodes[B].Source); }
        return A < B;
    });

    TArray<FNormalizeCluster> Clusters;
    Clusters.Reserve(Nodes.Num());
    // X-sweep expires old clusters. Three neighboring Y buckets avoid a quadratic scan
    // for a vertical line of nodes; every accepted cluster has complete span <= tolerance.
    TMap<int64, TArray<int32>> ActiveY;
    int32 FirstActive = 0;
    int64 Comparisons = 0;
    for (const int32 NodeIndex : Order)
    {
        FNormalizeNode& Node = Nodes[NodeIndex];
        while (FirstActive < Clusters.Num() && Node.Point.X - Clusters[FirstActive].Bounds.Min.X > ToleranceCm)
        {
            const int64 OldCell = Clusters[FirstActive].SeedYCell;
            if (TArray<int32>* Bucket = ActiveY.Find(OldCell))
            {
                Bucket->Remove(FirstActive);
                if (Bucket->IsEmpty()) { ActiveY.Remove(OldCell); }
            }
            ++FirstActive;
        }
        const double CellCoordinate = (Node.Point.Y - Origin.Y) / ToleranceCm;
        if (!FMath::IsFinite(CellCoordinate) || FMath::Abs(CellCoordinate) > 1.e15)
        {
            OutError = TEXT("공유 경계 정리 좌표 범위를 넘었습니다."); return false;
        }
        const int64 YCell = FMath::FloorToInt64(CellCoordinate);
        int32 BestCluster = INDEX_NONE;
        for (int64 Cell = YCell - 1; Cell <= YCell + 1; ++Cell)
        {
            const TArray<int32>* Bucket = ActiveY.Find(Cell);
            if (!Bucket) { continue; }
            for (const int32 Candidate : *Bucket)
            {
                if (++Comparisons > MaximumComparisons)
                {
                    OutError = TEXT("공유 경계 정리의 인접 노드 처리 한도를 넘었습니다."); return false;
                }
                const FNormalizeCluster& Cluster = Clusters[Candidate];
                FBox2D Proposed = Cluster.Bounds; Proposed += Node.Point;
                // A complete-span bound prevents A~B~C chains from moving distant endpoints.
                if ((Proposed.Max - Proposed.Min).SizeSquared() > ToleranceSquared) { continue; }
                if (Node.bOriginalCorner)
                {
                    if (const FBox2D* Existing = Cluster.OriginalSourceBounds.Find(Node.Polygon))
                    {
                        FBox2D Sources = *Existing; Sources += Node.Source;
                        if ((Sources.Max - Sources.Min).SizeSquared() > ToleranceSquared) { continue; }
                    }
                }
                if (BestCluster == INDEX_NONE ||
                    NormalizePointBefore(Cluster.Representative, Clusters[BestCluster].Representative))
                {
                    BestCluster = Candidate;
                }
            }
        }
        if (BestCluster == INDEX_NONE)
        {
            FNormalizeCluster Cluster;
            Cluster.Bounds += Node.Point; Cluster.Representative = Node.Point;
            Cluster.bRepresentativeIsOriginalCorner = Node.bOriginalCorner;
            Cluster.bRepresentativeIsUnchangedCorner = Node.bUnchangedCorner;
            Cluster.SeedYCell = YCell;
            BestCluster = Clusters.Add(MoveTemp(Cluster));
            ActiveY.FindOrAdd(YCell).Add(BestCluster);
        }
        FNormalizeCluster& Cluster = Clusters[BestCluster];
        Cluster.Bounds += Node.Point;
        Cluster.Members.Add(NodeIndex); Cluster.Polygons.Add(Node.Polygon);
        if (Node.bOriginalCorner)
        {
            FBox2D* Sources = Cluster.OriginalSourceBounds.Find(Node.Polygon);
            if (!Sources)
            {
                FBox2D NewBounds(ForceInit); NewBounds += Node.Source;
                Cluster.OriginalSourceBounds.Add(Node.Polygon, NewBounds);
            }
            else { *Sources += Node.Source; }
        }
        if (NormalizeRepresentativeBefore(Node, Cluster))
        {
            Cluster.Representative = Node.Point; Cluster.bRepresentativeIsOriginalCorner = Node.bOriginalCorner;
            Cluster.bRepresentativeIsUnchangedCorner = Node.bUnchangedCorner;
        }
        Node.Cluster = BestCluster;
    }

    TArray<TArray<FVector2D>> Staged;
    TArray<TArray<double>> StagedKeys;
    TArray<TArray<int32>> EmittedClusters;
    Staged.SetNum(InOutPolygons.Num()); StagedKeys.SetNum(InOutPolygons.Num());
    EmittedClusters.SetNum(InOutPolygons.Num());
    TArray<FVector2D> Local;
    for (int32 Polygon = 0; Polygon < InOutPolygons.Num(); ++Polygon)
    {
        TArray<int32> Emitted;
        TMap<int32, int32> BestNodePerCluster;
        for (const int32 NodeIndex : NodeIndices[Polygon])
        {
            const FNormalizeNode& Node = Nodes[NodeIndex];
            int32* Best = BestNodePerCluster.Find(Node.Cluster);
            if (!Best) { BestNodePerCluster.Add(Node.Cluster, NodeIndex); }
            else if (NormalizeKeyBefore(Node, Nodes[*Best], Clusters[Node.Cluster].Representative)) { *Best = NodeIndex; }
            if (Emitted.IsEmpty() || Emitted.Last() != Node.Cluster) { Emitted.Add(Node.Cluster); }
        }
        if (Emitted.Num() > 1 && Emitted[0] == Emitted.Last()) { Emitted.Pop(); }
        if (Emitted.Num() < 3)
        {
            OutError = FString::Printf(TEXT("Zone %d: 미세 경계 정리 후 Zone이 사라집니다."), Polygon + 1); return false;
        }
        Staged[Polygon].Reserve(Emitted.Num()); StagedKeys[Polygon].Reserve(Emitted.Num());
        Local.Reset(Emitted.Num());
        for (const int32 Cluster : Emitted)
        {
            const FVector2D& Point = Clusters[Cluster].Representative;
            Staged[Polygon].Add(Point); StagedKeys[Polygon].Add(Nodes[BestNodePerCluster[Cluster]].Key);
        }
        EmittedClusters[Polygon] = MoveTemp(Emitted);
    }

    FNormalizeEdgeIndex EdgeIndex;
    EdgeIndex.Build(Staged, StagedKeys, AuthoredPolygons);
    TArray<int32> NearEdges;
    for (FNormalizeCluster& Cluster : Clusters)
    {
        // A retained authored corner anchors the shared coordinate. Its neighbors receive
        // that exact point during noding rather than moving the original corner's XY/Z.
        if (Cluster.bRepresentativeIsUnchangedCorner) { continue; }
        if (!EdgeIndex.Query(Cluster.Representative, ToleranceCm, Comparisons, NearEdges))
        {
            OutError = TEXT("공유 경계 정리의 인접 변 처리 한도를 넘었습니다."); return false;
        }
        int32 Best = INDEX_NONE;
        double BestDistance = TNumericLimits<double>::Max();
        FVector2D BestPoint = Cluster.Representative;
        for (const int32 Candidate : NearEdges)
        {
            const FNormalizeEdge& Edge = EdgeIndex.Edges[Candidate];
            if (Cluster.Polygons.Contains(Edge.Polygon) || FVector2D::DistSquared(Edge.A, Edge.B) <= ToleranceSquared) { continue; }
            const FVector2D Closest = FFZAPolygonMath::ClosestPointOnSegment(Cluster.Representative, Edge.A, Edge.B);
            const double Distance = FVector2D::DistSquared(Cluster.Representative, Closest);
            if (Distance > ToleranceSquared) { continue; }
            bool bBounded = true;
            for (const int32 Member : Cluster.Members)
            {
                if (++Comparisons > MaximumComparisons)
                {
                    OutError = TEXT("공유 경계 정리의 인접 점 처리 한도를 넘었습니다."); return false;
                }
                if (FVector2D::DistSquared(Nodes[Member].Point, Closest) > ToleranceSquared) { bBounded = false; break; }
            }
            if (!bBounded) { continue; }
            if (Best == INDEX_NONE ||
                (Edge.bOriginalEdge != EdgeIndex.Edges[Best].bOriginalEdge ? Edge.bOriginalEdge :
                    (Distance != BestDistance ? Distance < BestDistance : NormalizeEdgeBefore(Edge, EdgeIndex.Edges[Best]))))
            {
                Best = Candidate; BestPoint = Closest; BestDistance = Distance;
            }
        }
        Cluster.Representative = BestPoint;
    }
    if (!NormalizeMergeProjectedClusters(Clusters, Nodes, ToleranceCm, Comparisons, OutError)) { return false; }
    for (int32 Polygon = 0; Polygon < Staged.Num(); ++Polygon)
    {
        TArray<int32> Emitted;
        TMap<int32, int32> BestNodePerCluster;
        for (const int32 NodeIndex : NodeIndices[Polygon])
        {
            const FNormalizeNode& Node = Nodes[NodeIndex];
            int32* Best = BestNodePerCluster.Find(Node.Cluster);
            if (!Best) { BestNodePerCluster.Add(Node.Cluster, NodeIndex); }
            else if (NormalizeKeyBefore(Node, Nodes[*Best], Clusters[Node.Cluster].Representative)) { *Best = NodeIndex; }
            if (Emitted.IsEmpty() || Emitted.Last() != Node.Cluster) { Emitted.Add(Node.Cluster); }
        }
        if (Emitted.Num() > 1 && Emitted[0] == Emitted.Last()) { Emitted.Pop(); }
        if (Emitted.Num() < 3)
        {
            OutError = FString::Printf(TEXT("Zone %d: 투영 후 미세 경계 병합으로 Zone이 사라집니다."), Polygon + 1); return false;
        }
        Staged[Polygon].Reset(Emitted.Num()); StagedKeys[Polygon].Reset(Emitted.Num());
        for (const int32 Cluster : Emitted)
        {
            Staged[Polygon].Add(Clusters[Cluster].Representative);
            StagedKeys[Polygon].Add(Nodes[BestNodePerCluster[Cluster]].Key);
        }
        EmittedClusters[Polygon] = MoveTemp(Emitted);
    }

    // Projected points are shared with all nearby facing segments using one fixed geometry
    // snapshot. A point a few microns off a long edge must become an exact node on both
    // boundaries; independently rounding only one Zone would leave an exposed sliver.
    EdgeIndex.Build(Staged, StagedKeys, AuthoredPolygons);
    struct FNormalizeCut { double T; int32 Cluster; };
    TMap<int64, TArray<FNormalizeCut>> Cuts;
    int64 PotentialNodedPoints = 0;
    for (const TArray<FVector2D>& Ring : Staged) { PotentialNodedPoints += Ring.Num(); }
    for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
    {
        const FNormalizeCluster& Cluster = Clusters[ClusterIndex];
        const FVector2D& Point = Cluster.Representative;
        if (!EdgeIndex.Query(Point, ToleranceCm, Comparisons, NearEdges))
        {
            OutError = TEXT("공유 경계 점 삽입의 인접 변 처리 한도를 넘었습니다."); return false;
        }
        for (const int32 Candidate : NearEdges)
        {
            const FNormalizeEdge& Edge = EdgeIndex.Edges[Candidate];
            if (Cluster.Polygons.Contains(Edge.Polygon) || FVector2D::DistSquared(Point, Edge.A) <= ToleranceSquared ||
                FVector2D::DistSquared(Point, Edge.B) <= ToleranceSquared) { continue; }
            const FVector2D Direction = Edge.B - Edge.A;
            const double LengthSquared = Direction.SizeSquared();
            if (LengthSquared <= ToleranceSquared) { continue; }
            const double T = FVector2D::DotProduct(Point - Edge.A, Direction) / LengthSquared;
            if (T <= 0.0 || T >= 1.0 || FVector2D::DistSquared(Point, Edge.A + Direction * T) > ToleranceSquared) { continue; }
            if (++PotentialNodedPoints > MaximumPoints)
            {
                OutError = TEXT("공유 경계 점 삽입의 꼭짓점 처리 한도를 넘었습니다."); return false;
            }
            const int64 EdgeKey = (static_cast<int64>(Edge.Polygon) << 32) | static_cast<uint32>(Edge.Edge);
            Cuts.FindOrAdd(EdgeKey).Add({T, ClusterIndex});
        }
    }
    TArray<TArray<FVector2D>> Noded;
    TArray<TArray<double>> NodedKeys;
    Noded.SetNum(Staged.Num()); NodedKeys.SetNum(Staged.Num());
    int64 NodedCount = 0;
    for (int32 Polygon = 0; Polygon < Staged.Num(); ++Polygon)
    {
        for (int32 Edge = 0; Edge < Staged[Polygon].Num(); ++Edge)
        {
            Noded[Polygon].Add(Staged[Polygon][Edge]); NodedKeys[Polygon].Add(StagedKeys[Polygon][Edge]);
            if (++NodedCount > MaximumPoints)
            {
                OutError = TEXT("공유 경계 점 삽입의 꼭짓점 처리 한도를 넘었습니다."); return false;
            }
            const int64 EdgeKey = (static_cast<int64>(Polygon) << 32) | static_cast<uint32>(Edge);
            TArray<FNormalizeCut>* EdgeCuts = Cuts.Find(EdgeKey);
            if (!EdgeCuts) { continue; }
            EdgeCuts->Sort([&Clusters](const FNormalizeCut& A, const FNormalizeCut& B)
            {
                if (A.T != B.T) { return A.T < B.T; }
                return NormalizePointBefore(Clusters[A.Cluster].Representative, Clusters[B.Cluster].Representative);
            });
            for (const FNormalizeCut& Cut : *EdgeCuts)
            {
                const FVector2D& Point = Clusters[Cut.Cluster].Representative;
                if (FVector2D::DistSquared(Point, Noded[Polygon].Last()) <= ToleranceSquared) { continue; }
                Noded[Polygon].Add(Point);
                NodedKeys[Polygon].Add(NormalizeSplitKey(Staged[Polygon], StagedKeys[Polygon], Edge, Cut.T, AuthoredPolygons[Polygon]));
                if (++NodedCount > MaximumPoints)
                {
                    OutError = TEXT("공유 경계 점 삽입의 꼭짓점 처리 한도를 넘었습니다."); return false;
                }
            }
        }
        Local.Reset(Noded[Polygon].Num());
        for (const FVector2D& Point : Noded[Polygon]) { Local.Add(Point - Noded[Polygon][0]); }
        const double OriginalArea = NormalizeSignedArea(AuthoredPolygons[Polygon]);
        const double RepairedArea = NormalizeSignedArea(Local);
        if (!FMath::IsFinite(OriginalArea) || !FMath::IsFinite(RepairedArea) ||
            OriginalArea * RepairedArea <= 0.0 || !FFZAPolygonMath::IsSimplePolygon(Local, ToleranceCm))
        {
            OutError = NormalizeTopologyFailure(Polygon, Noded[Polygon], ToleranceCm);
            return false;
        }
    }
    InOutPolygons = MoveTemp(Noded); InOutSourcePointKeys = MoveTemp(NodedKeys);
    return true;
}
}
