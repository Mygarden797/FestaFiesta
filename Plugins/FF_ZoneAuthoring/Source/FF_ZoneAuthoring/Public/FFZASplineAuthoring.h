// FFZASplineAuthoring.h
// 역할: "경계선 Spline + 액터" 조합을 디자이너 친화적으로 다루는 공용 도우미.
//
// 핵심 아이디어: 디자이너가 Scale/회전을 마음대로 바꿔도, 화면에 보이는 경계 모양은 그대로 둔 채
// 액터 Transform만 규칙(Scale 1, Yaw 90도 단위, 1 cm 원점)에 맞게 되돌린다.
// 즉 "Scale을 점 좌표에 구워 넣는다(bake)". 그래서 FF_ZoneSystem 검증 규칙을 사람이 외울 필요가 없다.
#pragma once

#include "CoreMinimal.h"

class AActor;
class USplineComponent;

namespace FFZASplineAuthoring
{
    struct FNormalizeOptions
    {
        bool bResetScale = true;          // Scale을 (1,1,1)로 되돌리고 모양은 점 좌표로 보존
        bool bClearPitchRoll = true;      // Pitch/Roll을 0으로
        bool bSnapYawTo90 = true;         // Yaw를 0/90/180/270 중 가장 가까운 값으로
        bool bSnapOriginToWholeCm = true; // X/Y 원점을 정수 cm로(FF_ZoneSystem 1 cm 격자)
    };

    // 규칙을 이미 만족하면 아무것도 하지 않고 false. 바꿨으면 true.
    // 바꿀 때는 각 Spline 점의 월드 위치를 그대로 유지한다(보이는 경계 불변).
    FF_ZONEAUTHORING_API bool NormalizeActorTransformPreservingSpline(
        AActor& Actor, USplineComponent& Spline, const FNormalizeOptions& Options);

    // 액터 Transform이 규칙을 만족하는지만 검사한다(변경 없음).
    FF_ZONEAUTHORING_API bool IsActorTransformNormalized(const AActor& Actor, const FNormalizeOptions& Options);

    // Spline을 닫힌 직선 다각형으로 맞춘다(모든 점 Linear, Closed Loop). 바꿨으면 true.
    FF_ZONEAUTHORING_API bool EnsureClosedLinear(USplineComponent& Spline);

    // Spline 점들의 월드 XY 좌표(cm).
    FF_ZONEAUTHORING_API void GetWorldPolygon(const USplineComponent& Spline, TArray<FVector2D>& OutPolygon);

    // 월드 XY 좌표 목록으로 Spline 점을 다시 만든다. Z는 액터 높이를 사용한다.
    FF_ZONEAUTHORING_API void SetWorldPolygon(USplineComponent& Spline, const TArray<FVector2D>& WorldPolygon, double WorldZ);

    // 액터 원점을 왼쪽 아래로 하는 직사각형(SizeCm.X × SizeCm.Y)으로 경계를 만든다.
    // Undo가 필요하면 호출 전에 Spline.Modify()를 부른다(생성자에서도 쓰이므로 내부에서 부르지 않음).
    FF_ZONEAUTHORING_API void SetLocalRectangle(USplineComponent& Spline, const FVector2D& SizeCm);

    // 에디터 뷰포트에서 보이는 Spline 선 색(선택/비선택). 게임 빌드에서는 아무 일도 하지 않는다.
    FF_ZONEAUTHORING_API void SetEditorColor(USplineComponent& Spline, const FLinearColor& Color);
}
