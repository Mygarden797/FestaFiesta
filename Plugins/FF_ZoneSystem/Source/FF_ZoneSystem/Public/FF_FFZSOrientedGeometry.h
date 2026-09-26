// FF_FFZSOrientedGeometry.h
// 역할: 자유 각도로 회전한 사각형의 꼭짓점, 포함, 교차를 계산한다. 배치 판정과 표시가 함께 사용한다.
#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"

struct FFFZSCellRect;
struct FFFZSOrientedRect;

/**
 * 시설 사각형을 원하는 각도로 회전시키는 순수 계산이다. 메모리 할당이나 월드 변경은 없다.
 * 사각형의 최솟값은 포함하고 최댓값은 제외한다. (0, 0) 칸의 중심을 기준으로 회전한 뒤
 * OriginCell만큼 옮기므로 90도 단위 회전은 기존 정수 격자 결과와 정확히 일치한다.
 */
namespace FFZSOrientedGeometry
{
    static constexpr int32 CornerCount = 4;
    using FCornerArray = TStaticArray<FVector2D, CornerCount>;

    /** 시설 기준 사각형에 회전 각도와 설치 위치를 적용한다. */
    FF_ZONESYSTEM_API FFFZSOrientedRect MakeFromLocalRect(
        const FFFZSCellRect& LocalRect,
        FIntPoint OriginCell,
        float YawDegrees);

    /** 왼쪽 아래, 오른쪽 아래, 오른쪽 위, 왼쪽 위 순서로 꼭짓점을 반환한다. */
    FF_ZONESYSTEM_API FCornerArray GetCorners(const FFFZSOrientedRect& Rect);

    /** 회전된 사각형을 감싸는 수평·수직 경계를 구한다. 입력이 잘못되면 무효 상자를 반환한다. */
    FF_ZONESYSTEM_API FBox2D GetAabb(const FFFZSOrientedRect& Rect);

    /**
     * 회전된 사각형이 빠져나오지 않도록 바깥쪽 정수 격자로 경계를 반올림한다.
     * 90도 단위 회전은 기존 RotateRect + OriginCell 결과와 같다.
     * 잘못된 입력이나 정수 범위 초과는 무효 사각형으로 반환한다.
     */
    FF_ZONESYSTEM_API FFFZSCellRect ToConservativeCellRect(const FFFZSOrientedRect& Rect);

    /**
     * 각 사각형의 축에서 분리되는지 검사해 실제 면적이 겹치는지를 판정한다.
     * 변이나 꼭짓점만 닿는 경우는 기존 격자 규칙처럼 겹침으로 보지 않는다.
     */
    FF_ZONESYSTEM_API bool IntersectsPositiveArea(
        const FFFZSOrientedRect& A,
        const FFFZSOrientedRect& B);
}
