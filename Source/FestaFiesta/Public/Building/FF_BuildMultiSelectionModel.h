// FF_BuildMultiSelectionModel.h
// 역할: 클릭과 드래그를 구별하고 화면의 선택 사각형과 시설 표시가 겹치는지 판정한다.
// 실제 입력 처리와 자동화 검사가 같은 계산을 사용하여 선택 범위가 달라지지 않게 한다.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

namespace FFBuildMultiSelectionModel
{
	constexpr float DragThresholdPixels = 8.0f;

	// 8픽셀 이상 움직였을 때 여러 시설을 고르는 드래그로 취급한다.
	inline bool IsMarqueeDrag(const FVector2D& Start, const FVector2D& Current)
	{
		return (Current - Start).SizeSquared() >= FMath::Square(DragThresholdPixels);
	}

	// 어느 방향으로 드래그해도 왼쪽 위와 오른쪽 아래 좌표를 같은 순서로 돌려준다.
	inline FBox2D MakeScreenRect(const FVector2D& Start, const FVector2D& End)
	{
		return FBox2D(
			FVector2D(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y)),
			FVector2D(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y)));
	}

	// 시설이 사각형에 일부만 걸치거나 가장자리가 맞닿아도 선택에 포함한다.
	inline bool Intersects(const FBox2D& Marquee, const FVector2D& BoundsMin,
		const FVector2D& BoundsMax)
	{
		return BoundsMax.X >= Marquee.Min.X && BoundsMin.X <= Marquee.Max.X &&
			BoundsMax.Y >= Marquee.Min.Y && BoundsMin.Y <= Marquee.Max.Y;
	}
}
