// FF_BuildMultiSelectionModel.h
// Screen-space marquee rules shared by world input and deterministic tests.

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

namespace FFBuildMultiSelectionModel
{
	constexpr float DragThresholdPixels = 8.0f;

	inline bool IsMarqueeDrag(const FVector2D& Start, const FVector2D& Current)
	{
		return (Current - Start).SizeSquared() >= FMath::Square(DragThresholdPixels);
	}

	inline FBox2D MakeScreenRect(const FVector2D& Start, const FVector2D& End)
	{
		return FBox2D(
			FVector2D(FMath::Min(Start.X, End.X), FMath::Min(Start.Y, End.Y)),
			FVector2D(FMath::Max(Start.X, End.X), FMath::Max(Start.Y, End.Y)));
	}

	inline bool Intersects(const FBox2D& Marquee, const FVector2D& BoundsMin,
		const FVector2D& BoundsMax)
	{
		return BoundsMax.X >= Marquee.Min.X && BoundsMin.X <= Marquee.Max.X &&
			BoundsMax.Y >= Marquee.Min.Y && BoundsMin.Y <= Marquee.Max.Y;
	}
}
