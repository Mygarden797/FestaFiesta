// FFZAPolygonMath.cpp
// 역할: 2D 다각형 순수 계산 구현. 월드/액터를 전혀 참조하지 않는다.
#include "FFZAPolygonMath.h"

#include "Algo/Reverse.h"

namespace FFZAPolygonMath
{
namespace
{
    double Cross(const FVector2D& O, const FVector2D& A, const FVector2D& B)
    {
        return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
    }

    double DistSquared(const FVector2D& A, const FVector2D& B)
    {
        const double DX = A.X - B.X;
        const double DY = A.Y - B.Y;
        return DX * DX + DY * DY;
    }

    bool SegmentBoundsOverlap(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
        const double Tolerance)
    {
        return FMath::Min(A.X, B.X) - Tolerance <= FMath::Max(C.X, D.X) &&
            FMath::Max(A.X, B.X) + Tolerance >= FMath::Min(C.X, D.X) &&
            FMath::Min(A.Y, B.Y) - Tolerance <= FMath::Max(C.Y, D.Y) &&
            FMath::Max(A.Y, B.Y) + Tolerance >= FMath::Min(C.Y, D.Y);
    }

    // 끝점 접촉과 공선 겹침까지 교차로 본다(단순 다각형 검사용).
    bool SegmentsTouch(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FVector2D& D,
        const double Tolerance)
    {
        if (!SegmentBoundsOverlap(A, B, C, D, Tolerance))
        {
            return false;
        }
        const double D1 = Cross(A, B, C);
        const double D2 = Cross(A, B, D);
        const double D3 = Cross(C, D, A);
        const double D4 = Cross(C, D, B);
        if (((D1 > Tolerance && D2 < -Tolerance) || (D1 < -Tolerance && D2 > Tolerance)) &&
            ((D3 > Tolerance && D4 < -Tolerance) || (D3 < -Tolerance && D4 > Tolerance)))
        {
            return true;
        }
        const double TolSq = Tolerance * Tolerance;
        return DistSquared(ClosestPointOnSegment(C, A, B), C) <= TolSq ||
            DistSquared(ClosestPointOnSegment(D, A, B), D) <= TolSq ||
            DistSquared(ClosestPointOnSegment(A, C, D), A) <= TolSq ||
            DistSquared(ClosestPointOnSegment(B, C, D), B) <= TolSq;
    }

    // 삼각형 ABC(반시계) 안에 P가 있는지. 변 위는 포함한다(귀 판정을 보수적으로 만들기 위해).
    bool IsPointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        constexpr double Eps = 1e-9;
        return Cross(A, B, P) >= -Eps && Cross(B, C, P) >= -Eps && Cross(C, A, P) >= -Eps;
    }
}

double SignedArea(const TArray<FVector2D>& Polygon)
{
    const int32 Count = Polygon.Num();
    if (Count < 3)
    {
        return 0.0;
    }
    double TwiceArea = 0.0;
    for (int32 I = 0, J = Count - 1; I < Count; J = I++)
    {
        TwiceArea += Polygon[J].X * Polygon[I].Y - Polygon[I].X * Polygon[J].Y;
    }
    return TwiceArea * 0.5;
}

double Area(const TArray<FVector2D>& Polygon)
{
    return FMath::Abs(SignedArea(Polygon));
}

FVector2D ClosestPointOnSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B)
{
    const FVector2D AB = B - A;
    const double LengthSquared = AB.X * AB.X + AB.Y * AB.Y;
    if (LengthSquared <= 1e-12)
    {
        return A;
    }
    const double T = FMath::Clamp(((P.X - A.X) * AB.X + (P.Y - A.Y) * AB.Y) / LengthSquared, 0.0, 1.0);
    return FVector2D(A.X + AB.X * T, A.Y + AB.Y * T);
}

bool IsPointOnBoundary(const TArray<FVector2D>& Polygon, const FVector2D& Point, const double Tolerance)
{
    const int32 Count = Polygon.Num();
    const double TolSq = Tolerance * Tolerance;
    for (int32 I = 0, J = Count - 1; I < Count; J = I++)
    {
        if (DistSquared(ClosestPointOnSegment(Point, Polygon[J], Polygon[I]), Point) <= TolSq)
        {
            return true;
        }
    }
    return false;
}

bool IsPointInside(const TArray<FVector2D>& Polygon, const FVector2D& Point, const double EdgeTolerance)
{
    const int32 Count = Polygon.Num();
    if (Count < 3)
    {
        return false;
    }
    if (EdgeTolerance > 0.0 && IsPointOnBoundary(Polygon, Point, EdgeTolerance))
    {
        return true;
    }
    bool bInside = false;
    for (int32 I = 0, J = Count - 1; I < Count; J = I++)
    {
        const FVector2D& A = Polygon[J];
        const FVector2D& B = Polygon[I];
        if ((B.Y > Point.Y) != (A.Y > Point.Y))
        {
            const double CrossX = B.X + (Point.Y - B.Y) * (A.X - B.X) / (A.Y - B.Y);
            if (Point.X < CrossX)
            {
                bInside = !bInside;
            }
        }
    }
    return bInside;
}

bool IsSimplePolygon(const TArray<FVector2D>& Polygon, const double Tolerance)
{
    const int32 Count = Polygon.Num();
    if (Count < 3)
    {
        return false;
    }
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector2D& A = Polygon[I];
        const FVector2D& B = Polygon[(I + 1) % Count];
        if (A.ContainsNaN() || DistSquared(A, B) <= Tolerance * Tolerance)
        {
            return false;
        }
        for (int32 J = I + 1; J < Count; ++J)
        {
            const bool bAdjacent = J == I + 1 || (I == 0 && J == Count - 1);
            if (bAdjacent)
            {
                continue;
            }
            if (SegmentsTouch(A, B, Polygon[J], Polygon[(J + 1) % Count], Tolerance))
            {
                return false;
            }
        }
    }
    return Area(Polygon) > Tolerance;
}

void ClipToConvex(const TArray<FVector2D>& Subject, const TArray<FVector2D>& ConvexClip, TArray<FVector2D>& OutResult)
{
    OutResult.Reset();
    if (Subject.Num() < 3 || ConvexClip.Num() < 3)
    {
        return;
    }

    // 클립 다각형을 반시계로 맞춘다. 반시계에서 "안쪽"은 각 변의 왼쪽(Cross >= 0)이다.
    TArray<FVector2D> Clip = ConvexClip;
    if (SignedArea(Clip) < 0.0)
    {
        Algo::Reverse(Clip);
    }

    TArray<FVector2D> Input;
    OutResult = Subject;
    const int32 ClipCount = Clip.Num();
    for (int32 EdgeIndex = 0; EdgeIndex < ClipCount && OutResult.Num() > 0; ++EdgeIndex)
    {
        const FVector2D& EdgeStart = Clip[EdgeIndex];
        const FVector2D& EdgeEnd = Clip[(EdgeIndex + 1) % ClipCount];
        Input = OutResult;
        OutResult.Reset();

        const int32 InputCount = Input.Num();
        for (int32 I = 0; I < InputCount; ++I)
        {
            const FVector2D& Current = Input[I];
            const FVector2D& Previous = Input[(I + InputCount - 1) % InputCount];
            const double CurrentSide = Cross(EdgeStart, EdgeEnd, Current);
            const double PreviousSide = Cross(EdgeStart, EdgeEnd, Previous);
            const bool bCurrentInside = CurrentSide >= 0.0;
            const bool bPreviousInside = PreviousSide >= 0.0;

            if (bCurrentInside != bPreviousInside)
            {
                // 두 점이 클립 변의 반대편에 있으므로 분모는 0이 아니다.
                const double T = PreviousSide / (PreviousSide - CurrentSide);
                OutResult.Add(FVector2D(
                    Previous.X + (Current.X - Previous.X) * T,
                    Previous.Y + (Current.Y - Previous.Y) * T));
            }
            if (bCurrentInside)
            {
                OutResult.Add(Current);
            }
        }
    }
}

double IntersectionAreaWithConvex(const TArray<FVector2D>& Polygon, const TArray<FVector2D>& Convex)
{
    TArray<FVector2D> Clipped;
    ClipToConvex(Polygon, Convex, Clipped);
    return Area(Clipped);
}

bool Triangulate(const TArray<FVector2D>& Polygon, TArray<int32>& OutIndices)
{
    OutIndices.Reset();
    const int32 Count = Polygon.Num();
    if (Count < 3)
    {
        return false;
    }

    // 원래 인덱스를 유지한 채 반시계 순서의 작업 목록을 만든다.
    TArray<int32> Remaining;
    Remaining.Reserve(Count);
    if (SignedArea(Polygon) >= 0.0)
    {
        for (int32 I = 0; I < Count; ++I)
        {
            Remaining.Add(I);
        }
    }
    else
    {
        for (int32 I = Count - 1; I >= 0; --I)
        {
            Remaining.Add(I);
        }
    }

    OutIndices.Reserve((Count - 2) * 3);
    // 매 반복마다 귀 하나를 잘라낸다. 귀를 못 찾고 한 바퀴 돌면 실패로 끝낸다(무한 루프 방지).
    int32 Guard = 0;
    const int32 MaxGuard = Count * Count + 8;
    int32 Index = 0;
    while (Remaining.Num() > 3 && Guard++ < MaxGuard)
    {
        const int32 N = Remaining.Num();
        const int32 PrevSlot = (Index + N - 1) % N;
        const int32 NextSlot = (Index + 1) % N;
        const FVector2D& A = Polygon[Remaining[PrevSlot]];
        const FVector2D& B = Polygon[Remaining[Index]];
        const FVector2D& C = Polygon[Remaining[NextSlot]];

        const double Turn = Cross(A, B, C);
        bool bIsEar = Turn > 1e-9;
        if (bIsEar)
        {
            for (int32 Slot = 0; Slot < N; ++Slot)
            {
                if (Slot == PrevSlot || Slot == Index || Slot == NextSlot)
                {
                    continue;
                }
                const FVector2D& P = Polygon[Remaining[Slot]];
                // 같은 위치에 겹친 점은 귀를 막지 않는다.
                if (DistSquared(P, A) < 1e-12 || DistSquared(P, B) < 1e-12 || DistSquared(P, C) < 1e-12)
                {
                    continue;
                }
                if (IsPointInTriangle(P, A, B, C))
                {
                    bIsEar = false;
                    break;
                }
            }
        }
        else if (FMath::Abs(Turn) <= 1e-9)
        {
            // 일직선 위의 가운데 점은 삼각형 없이 제거한다.
            Remaining.RemoveAt(Index);
            Index = Index % Remaining.Num();
            Guard = 0;
            continue;
        }

        if (bIsEar)
        {
            OutIndices.Add(Remaining[PrevSlot]);
            OutIndices.Add(Remaining[Index]);
            OutIndices.Add(Remaining[NextSlot]);
            Remaining.RemoveAt(Index);
            Index = Index % Remaining.Num();
            Guard = 0;
        }
        else
        {
            Index = (Index + 1) % N;
        }
    }

    if (Remaining.Num() == 3)
    {
        const FVector2D& A = Polygon[Remaining[0]];
        const FVector2D& B = Polygon[Remaining[1]];
        const FVector2D& C = Polygon[Remaining[2]];
        if (FMath::Abs(Cross(A, B, C)) > 1e-9)
        {
            OutIndices.Add(Remaining[0]);
            OutIndices.Add(Remaining[1]);
            OutIndices.Add(Remaining[2]);
        }
        return true;
    }
    return false;
}

void ScanlineCrossings(const TArray<FVector2D>& Polygon, const double Y, TArray<double>& OutCrossings)
{
    OutCrossings.Reset();
    const int32 Count = Polygon.Num();
    for (int32 I = 0, J = Count - 1; I < Count; J = I++)
    {
        const FVector2D& A = Polygon[J];
        const FVector2D& B = Polygon[I];
        // 반열림 규칙(아래 끝 포함, 위 끝 제외)으로 꼭짓점을 두 번 세지 않는다.
        if ((A.Y <= Y && B.Y > Y) || (B.Y <= Y && A.Y > Y))
        {
            OutCrossings.Add(A.X + (Y - A.Y) * (B.X - A.X) / (B.Y - A.Y));
        }
    }
    OutCrossings.Sort();
}

FBox2D GetBounds(const TArray<FVector2D>& Polygon)
{
    FBox2D Bounds(ForceInit);
    for (const FVector2D& Point : Polygon)
    {
        Bounds += Point;
    }
    return Bounds;
}
}
