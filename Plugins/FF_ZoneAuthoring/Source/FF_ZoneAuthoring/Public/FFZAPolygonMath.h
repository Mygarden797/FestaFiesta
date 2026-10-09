// FFZAPolygonMath.h
// 역할: Zone/구획/구역 경계(2D 다각형)를 다루는 순수 계산 함수 모음.
//
// - 월드 상태를 읽거나 바꾸지 않는다. 입력 배열만 보고 결과를 돌려준다(테스트하기 쉽게).
// - 좌표 단위는 cm(1 Unreal Unit)이다. FF_ZoneSystem의 1 cm 격자 계약과 같다.
// - 다각형은 "닫힌 꼭짓점 목록"이다. 마지막 점과 첫 점을 다시 넣지 않는다.
// - 시계/반시계 방향 어느 쪽이든 받는다. 필요한 함수는 내부에서 방향을 정규화한다.
#pragma once

#include "CoreMinimal.h"

namespace FFZAPolygonMath
{
    // 경계 위의 점을 "안쪽"으로 인정하는 폭(cm). FF_ZoneSystem Zone 경계 허용치(0.1 cm)와 맞춘다.
    inline constexpr double EdgeToleranceCm = 0.1;

    // 신발끈 공식 면적(cm²). 반시계 방향이면 양수, 시계 방향이면 음수.
    FF_ZONEAUTHORING_API double SignedArea(const TArray<FVector2D>& Polygon);

    // 절대 면적(cm²). 점이 3개 미만이면 0.
    FF_ZONEAUTHORING_API double Area(const TArray<FVector2D>& Polygon);

    // 점 P에서 선분 AB 위의 가장 가까운 점.
    FF_ZONEAUTHORING_API FVector2D ClosestPointOnSegment(const FVector2D& P, const FVector2D& A, const FVector2D& B);

    // 점이 다각형 안 또는 경계 EdgeTolerance 이내이면 true(경계 포함 판정).
    FF_ZONEAUTHORING_API bool IsPointInside(const TArray<FVector2D>& Polygon, const FVector2D& Point,
        double EdgeTolerance = EdgeToleranceCm);

    // 점이 다각형의 어느 변에서 Tolerance 이내에 있으면 true.
    FF_ZONEAUTHORING_API bool IsPointOnBoundary(const TArray<FVector2D>& Polygon, const FVector2D& Point,
        double Tolerance = EdgeToleranceCm);

    // 자기 교차·중복 점·면적 0이 없는 단순 다각형인지 검사한다. 비용은 꼭짓점 수의 제곱.
    FF_ZONEAUTHORING_API bool IsSimplePolygon(const TArray<FVector2D>& Polygon, double Tolerance = 0.01);

    // Subject 다각형(오목 가능)을 볼록 다각형 Clip으로 잘라 OutResult에 담는다(Sutherland–Hodgman).
    // 결과가 여러 조각이면 폭 0인 연결 변이 생길 수 있으나 면적 계산은 정확하다.
    FF_ZONEAUTHORING_API void ClipToConvex(const TArray<FVector2D>& Subject, const TArray<FVector2D>& ConvexClip,
        TArray<FVector2D>& OutResult);

    // 다각형과 볼록 다각형(예: 회전된 시설 사각형 4꼭짓점)이 실제로 겹치는 면적(cm²).
    // 변이나 꼭짓점만 닿으면 0이다. FF_ZoneSystem의 "양의 면적 겹침" 규칙과 같다.
    FF_ZONEAUTHORING_API double IntersectionAreaWithConvex(const TArray<FVector2D>& Polygon,
        const TArray<FVector2D>& Convex);

    // 귀 자르기(Ear clipping) 삼각분할. OutIndices에 3개씩 꼭짓점 인덱스를 넣는다(반시계 방향).
    // 단순 다각형이 아니면 false를 반환하고 OutIndices는 비어 있을 수 있다.
    FF_ZONEAUTHORING_API bool Triangulate(const TArray<FVector2D>& Polygon, TArray<int32>& OutIndices);

    // 수평선 y = Y와 다각형 변의 교차 X 좌표를 오름차순으로 반환한다(짝수 개, 짝/홀 채우기 규칙).
    // 셀 래스터화에 쓰인다.
    FF_ZONEAUTHORING_API void ScanlineCrossings(const TArray<FVector2D>& Polygon, double Y, TArray<double>& OutCrossings);

    // 다각형의 축 정렬 경계 상자. 점이 없으면 무효 상자.
    FF_ZONEAUTHORING_API FBox2D GetBounds(const TArray<FVector2D>& Polygon);
}
