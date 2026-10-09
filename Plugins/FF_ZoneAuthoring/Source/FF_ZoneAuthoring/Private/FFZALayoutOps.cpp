// FFZALayoutOps.cpp
// 역할: 경계 용접(꼭짓점 → 기준 꼭짓점/변 스냅) 구현.
#include "FFZALayoutOps.h"

#include "FFZAPolygonMath.h"

namespace FFZALayoutOps
{
int32 WeldPolygons(TArray<TArray<FVector2D>>& InOutPolygons, const double ToleranceCm, TArray<bool>& OutChangedPolygons)
{
    const int32 PolygonCount = InOutPolygons.Num();
    OutChangedPolygons.Init(false, PolygonCount);
    if (ToleranceCm <= 0.0 || PolygonCount < 2)
    {
        return 0;
    }
    const double ToleranceSquared = ToleranceCm * ToleranceCm;
    // 이미 같은 위치면 옮긴 것으로 세지 않는다(재실행해도 결과가 같도록).
    constexpr double SamePositionSquared = 1e-6;
    int32 MovedCount = 0;

    for (int32 Target = 1; Target < PolygonCount; ++Target)
    {
        TArray<FVector2D>& Polygon = InOutPolygons[Target];
        for (FVector2D& Vertex : Polygon)
        {
            // 1) 기준 꼭짓점 스냅(모서리끼리 정확히 맞추기)
            double BestDistanceSquared = ToleranceSquared;
            FVector2D BestPoint = Vertex;
            bool bFound = false;
            for (int32 Reference = 0; Reference < Target; ++Reference)
            {
                for (const FVector2D& Candidate : InOutPolygons[Reference])
                {
                    const double DistanceSquared = FVector2D::DistSquared(Vertex, Candidate);
                    if (DistanceSquared <= BestDistanceSquared)
                    {
                        BestDistanceSquared = DistanceSquared;
                        BestPoint = Candidate;
                        bFound = true;
                    }
                }
            }
            // 2) 기준 변 스냅(T자로 맞닿는 경계)
            if (!bFound)
            {
                for (int32 Reference = 0; Reference < Target; ++Reference)
                {
                    const TArray<FVector2D>& Other = InOutPolygons[Reference];
                    const int32 Count = Other.Num();
                    for (int32 I = 0, J = Count - 1; I < Count; J = I++)
                    {
                        const FVector2D Closest = FFZAPolygonMath::ClosestPointOnSegment(Vertex, Other[J], Other[I]);
                        const double DistanceSquared = FVector2D::DistSquared(Vertex, Closest);
                        if (DistanceSquared <= BestDistanceSquared)
                        {
                            BestDistanceSquared = DistanceSquared;
                            BestPoint = Closest;
                            bFound = true;
                        }
                    }
                }
            }
            if (bFound && FVector2D::DistSquared(Vertex, BestPoint) > SamePositionSquared)
            {
                Vertex = BestPoint;
                OutChangedPolygons[Target] = true;
                ++MovedCount;
            }
        }
    }
    return MovedCount;
}
}
