// FFZAOverlayMeshBuilder.cpp
// 역할: 다각형 채우기/외곽선 메시 생성(지면 높이 추종, 균일 분할, 양면).
#include "FFZAOverlayMeshBuilder.h"

#include "FFZAPolygonMath.h"

namespace
{
    constexpr int32 MaxSubdivisionDepth = 10;

    // 0.1 cm 단위로 반올림한 키. 같은 위치의 꼭짓점을 공유해 높이 조회와 틈을 줄인다.
    FIntPoint MakeVertexKey(const FVector2D& Point)
    {
        return FIntPoint(static_cast<int32>(FMath::RoundToDouble(Point.X * 10.0)),
            static_cast<int32>(FMath::RoundToDouble(Point.Y * 10.0)));
    }

    double EdgeLength(const FVector2D& A, const FVector2D& B)
    {
        return FMath::Sqrt((A.X - B.X) * (A.X - B.X) + (A.Y - B.Y) * (A.Y - B.Y));
    }
}

FFFZAOverlayMeshBuilder::FFFZAOverlayMeshBuilder(FHeightFunction InHeightAt, const double InMaxEdgeCm,
    const double InLiftCm, const int32 InMaxTriangles)
    : HeightAt(MoveTemp(InHeightAt))
    , MaxEdgeCm(FMath::Max(10.0, InMaxEdgeCm))
    , LiftCm(InLiftCm)
    , MaxTriangles(FMath::Max(16, InMaxTriangles))
{
}

int32 FFFZAOverlayMeshBuilder::AddVertex(const FVector2D& Point, const FLinearColor& Color,
    FFFZAOverlayMeshBuffers& Out, TMap<FIntPoint, int32>& Cache)
{
    const FIntPoint Key = MakeVertexKey(Point);
    if (const int32* Existing = Cache.Find(Key))
    {
        return *Existing;
    }
    const double Z = (HeightAt ? HeightAt(Point) : 0.0) + LiftCm;
    const int32 Index = Out.Vertices.Num();
    Out.Vertices.Add(FVector(Point.X, Point.Y, Z));
    Out.Normals.Add(FVector(0.0, 0.0, 1.0));
    Out.UV0.Add(FVector2D(Point.X / 100.0, Point.Y / 100.0));
    Out.Colors.Add(Color);
    Cache.Add(Key, Index);
    return Index;
}

void FFFZAOverlayMeshBuilder::AddTriangleBothSides(const int32 A, const int32 B, const int32 C, FFFZAOverlayMeshBuffers& Out)
{
    Out.Triangles.Add(A);
    Out.Triangles.Add(B);
    Out.Triangles.Add(C);
    Out.Triangles.Add(A);
    Out.Triangles.Add(C);
    Out.Triangles.Add(B);
}

void FFFZAOverlayMeshBuilder::SubdivideTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C,
    const int32 Depth, const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out, TMap<FIntPoint, int32>& Cache)
{
    if (Depth <= 0)
    {
        AddTriangleBothSides(AddVertex(A, Color, Out, Cache), AddVertex(B, Color, Out, Cache),
            AddVertex(C, Color, Out, Cache), Out);
        return;
    }
    const FVector2D AB = (A + B) * 0.5;
    const FVector2D BC = (B + C) * 0.5;
    const FVector2D CA = (C + A) * 0.5;
    SubdivideTriangle(A, AB, CA, Depth - 1, Color, Out, Cache);
    SubdivideTriangle(AB, B, BC, Depth - 1, Color, Out, Cache);
    SubdivideTriangle(CA, BC, C, Depth - 1, Color, Out, Cache);
    SubdivideTriangle(AB, BC, CA, Depth - 1, Color, Out, Cache);
}

bool FFFZAOverlayMeshBuilder::AddFilledPolygon(const TArray<FVector2D>& Polygon, const FLinearColor& Color,
    FFFZAOverlayMeshBuffers& Out)
{
    TArray<int32> Indices;
    if (!FFZAPolygonMath::Triangulate(Polygon, Indices) || Indices.IsEmpty())
    {
        return false;
    }
    const int32 BaseTriangles = Indices.Num() / 3;

    // 가장 긴 변이 MaxEdgeCm 이하가 되도록 필요한 분할 단계를 구한다(모든 삼각형 공통).
    double LongestEdge = 0.0;
    for (int32 I = 0; I < Indices.Num(); I += 3)
    {
        const FVector2D& A = Polygon[Indices[I]];
        const FVector2D& B = Polygon[Indices[I + 1]];
        const FVector2D& C = Polygon[Indices[I + 2]];
        LongestEdge = FMath::Max(LongestEdge, FMath::Max(EdgeLength(A, B), FMath::Max(EdgeLength(B, C), EdgeLength(C, A))));
    }
    int32 Depth = 0;
    while (Depth < MaxSubdivisionDepth && LongestEdge / static_cast<double>(1 << Depth) > MaxEdgeCm)
    {
        ++Depth;
    }

    // 남은 예산 안에 들어올 때까지 단계를 낮춘다(분할 1단계 = 삼각형 4배).
    const int32 RemainingBudget = FMath::Max(0, MaxTriangles - Out.NumTriangles() / 2);
    while (Depth > 0 && static_cast<int64>(BaseTriangles) * (static_cast<int64>(1) << (2 * Depth)) > RemainingBudget)
    {
        --Depth;
        bBudgetLimited = true;
    }
    if (BaseTriangles > RemainingBudget)
    {
        bBudgetLimited = true;
        return false;
    }

    TMap<FIntPoint, int32> Cache;
    for (int32 I = 0; I < Indices.Num(); I += 3)
    {
        SubdivideTriangle(Polygon[Indices[I]], Polygon[Indices[I + 1]], Polygon[Indices[I + 2]], Depth, Color, Out, Cache);
    }
    return true;
}

void FFFZAOverlayMeshBuilder::AddOutline(const TArray<FVector2D>& Polygon, const double WidthCm,
    const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out)
{
    const int32 Count = Polygon.Num();
    if (Count < 2 || WidthCm <= 0.0)
    {
        return;
    }
    const double HalfWidth = WidthCm * 0.5;
    TMap<FIntPoint, int32> Cache;
    for (int32 I = 0; I < Count; ++I)
    {
        const FVector2D& Start = Polygon[I];
        const FVector2D& End = Polygon[(I + 1) % Count];
        const double Length = EdgeLength(Start, End);
        if (Length <= UE_KINDA_SMALL_NUMBER)
        {
            continue;
        }
        const FVector2D Direction((End.X - Start.X) / Length, (End.Y - Start.Y) / Length);
        const FVector2D Side(-Direction.Y * HalfWidth, Direction.X * HalfWidth);
        const int32 Segments = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble(Length / MaxEdgeCm)), 1, 4096);
        if (Out.NumTriangles() / 2 + Segments * 2 > MaxTriangles)
        {
            bBudgetLimited = true;
            return;
        }
        for (int32 S = 0; S < Segments; ++S)
        {
            const double T0 = static_cast<double>(S) / Segments;
            const double T1 = static_cast<double>(S + 1) / Segments;
            const FVector2D P0(Start.X + (End.X - Start.X) * T0, Start.Y + (End.Y - Start.Y) * T0);
            const FVector2D P1(Start.X + (End.X - Start.X) * T1, Start.Y + (End.Y - Start.Y) * T1);
            const int32 A = AddVertex(P0 - Side, Color, Out, Cache);
            const int32 B = AddVertex(P1 - Side, Color, Out, Cache);
            const int32 C = AddVertex(P1 + Side, Color, Out, Cache);
            const int32 D = AddVertex(P0 + Side, Color, Out, Cache);
            AddTriangleBothSides(A, B, C, Out);
            AddTriangleBothSides(A, C, D, Out);
        }
    }
}
