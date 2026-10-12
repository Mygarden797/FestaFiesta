// FFZALayoutOps.h
// 역할: 여러 Zone 경계를 한꺼번에 다듬는 순수 계산(레이아웃 매니저 버튼이 사용).
#pragma once

#include "CoreMinimal.h"

namespace FFZALayoutOps
{
    // 경계 "용접": 서로 거의 맞닿은 Zone 경계의 미세한 틈/겹침을 없앤다.
    //
    // 앞쪽(인덱스가 작은) 다각형은 기준으로 두고, 뒤쪽 다각형의 꼭짓점을 ToleranceCm 안에 있는
    //  1) 앞쪽 다각형의 꼭짓점이 있으면 그 꼭짓점으로,
    //  2) 없으면 앞쪽 다각형의 변 위 가장 가까운 점으로
    // 옮긴다. 기준을 한 방향으로만 두므로 두 Zone이 서로를 향해 움직여 다시 어긋나는 일이 없다.
    // 반환값: 옮긴 꼭짓점 수. OutChangedPolygons[i]가 true면 i번 다각형이 바뀌었다.
    FF_ZONEAUTHORING_API int32 WeldPolygons(TArray<TArray<FVector2D>>& InOutPolygons, double ToleranceCm,
        TArray<bool>& OutChangedPolygons);
}
