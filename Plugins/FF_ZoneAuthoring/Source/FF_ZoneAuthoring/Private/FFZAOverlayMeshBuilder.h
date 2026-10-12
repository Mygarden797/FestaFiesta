// FFZAOverlayMeshBuilder.h
// 역할: 다각형을 "바닥에 깔리는 반투명 메시"로 바꾸는 계산기(오버레이 액터 전용).
//
//  - 채우기: 다각형을 삼각형으로 나눈 뒤(귀 자르기), 모든 삼각형을 같은 단계로 잘게 쪼갠다.
//            잘게 쪼갠 꼭짓점마다 지면 높이를 물어봐서 언덕/경사에도 바닥을 따라 붙는다.
//            모든 삼각형을 같은 단계로 쪼개야 이웃 삼각형 사이에 틈(T-junction)이 생기지 않는다.
//  - 외곽선: 각 변을 따라 폭 있는 띠(리본)를 만든다.
//  - 양면: 머티리얼이 단면이어도 보이도록 삼각형을 앞/뒤 두 번 넣는다.
//
// 높이 함수(HeightAt)만 바꿔 끼우면 되므로 월드 없이도 테스트할 수 있다.
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

struct FFFZAOverlayMeshBuffers
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> Colors;

    void Reset()
    {
        Vertices.Reset();
        Triangles.Reset();
        Normals.Reset();
        UV0.Reset();
        Colors.Reset();
    }

    int32 NumTriangles() const { return Triangles.Num() / 3; }
};

class FFFZAOverlayMeshBuilder
{
public:
    using FHeightFunction = TFunction<double(const FVector2D&)>;

    FFFZAOverlayMeshBuilder(FHeightFunction InHeightAt, double InMaxEdgeCm, double InLiftCm, int32 InMaxTriangles);

    // 다각형 내부를 채운다. 삼각형 예산을 넘으면 쪼개는 단계를 낮춘다. 삼각분할 실패 시 false.
    bool AddFilledPolygon(const TArray<FVector2D>& Polygon, const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out);

    // 다각형 외곽을 폭 WidthCm 띠로 그린다.
    void AddOutline(const TArray<FVector2D>& Polygon, double WidthCm, const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out);

    // 이번 빌드에서 삼각형 예산 때문에 품질을 낮춘 적이 있으면 true.
    bool WasBudgetLimited() const { return bBudgetLimited; }

private:
    int32 AddVertex(const FVector2D& Point, const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out,
        TMap<FIntPoint, int32>& Cache);
    void AddTriangleBothSides(int32 A, int32 B, int32 C, FFFZAOverlayMeshBuffers& Out);
    void SubdivideTriangle(const FVector2D& A, const FVector2D& B, const FVector2D& C, int32 Depth,
        const FLinearColor& Color, FFFZAOverlayMeshBuffers& Out, TMap<FIntPoint, int32>& Cache);

    FHeightFunction HeightAt;
    double MaxEdgeCm = 400.0;
    double LiftCm = 5.0;
    int32 MaxTriangles = 200000;
    bool bBudgetLimited = false;
};
