// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Atlas/FF_AtlasScreenGeo.h"

namespace
{
	/** 크기가 이보다 작으면 없는 것으로 본다. 첫 프레임에 레이아웃이 0 으로 들어온다. */
	constexpr double AtlasMinDrawableSize = 1.e-3;
}

FFF_AtlasImageRect FFF_AtlasScreenGeo::ComputeImageRect(
	const FVector2D& WidgetSize,
	const FVector2D& ImageSize,
	EFF_AtlasImageFit Fit)
{
	FFF_AtlasImageRect Result;

	// 위젯이든 이미지든 크기가 없으면 그릴 사각형이 없다. 0 으로 나누지 않는다.
	if (WidgetSize.X <= AtlasMinDrawableSize || WidgetSize.Y <= AtlasMinDrawableSize
		|| ImageSize.X <= AtlasMinDrawableSize || ImageSize.Y <= AtlasMinDrawableSize)
	{
		return Result;
	}

	if (Fit == EFF_AtlasImageFit::Stretch)
	{
		// 비율을 버리고 꽉 채운다. 여백이 없으므로 Offset 은 0 이다.
		Result.Offset = FVector2D::ZeroVector;
		Result.Size = WidgetSize;
		return Result;
	}

	// 각 축을 채우는 데 필요한 배율. 작은 쪽을 쓰면 전부 보이고(Contain),
	// 큰 쪽을 쓰면 덮인다(Cover).
	const double ScaleX = WidgetSize.X / ImageSize.X;
	const double ScaleY = WidgetSize.Y / ImageSize.Y;

	const double Scale = (Fit == EFF_AtlasImageFit::Contain)
		? FMath::Min(ScaleX, ScaleY)
		: FMath::Max(ScaleX, ScaleY);

	Result.Size = ImageSize * Scale;

	// 남거나 넘치는 만큼을 절반씩 나눠 가운데에 둔다. Cover 면 이 값이 음수다 —
	// 사각형이 위젯 밖으로 나가 잘린다는 뜻이고, 핀 계산은 그래도 맞다.
	Result.Offset = (WidgetSize - Result.Size) * 0.5;

	return Result;
}

FVector2D FFF_AtlasScreenGeo::NormalizedToWidgetLocal(
	const FVector2D& Normalized,
	const FFF_AtlasImageRect& ImageRect)
{
	// 위젯 전체가 아니라 **그려진 사각형**을 기준으로 한다. 여백만큼 밀리는 것을 막는 지점이다.
	return ImageRect.Offset + Normalized * ImageRect.Size;
}

FVector2D FFF_AtlasScreenGeo::WidgetLocalToNormalized(
	const FVector2D& WidgetLocal,
	const FFF_AtlasImageRect& ImageRect)
{
	// 그려진 사각형이 없으면 되돌릴 기준이 없다.
	if (ImageRect.Size.X <= AtlasMinDrawableSize || ImageRect.Size.Y <= AtlasMinDrawableSize)
	{
		return FVector2D::ZeroVector;
	}

	// 클램프하지 않는다 — 사각형 밖을 눌렀다는 사실을 호출부가 알아야 한다.
	return (WidgetLocal - ImageRect.Offset) / ImageRect.Size;
}

int32 FFF_AtlasScreenGeo::FindDirectionalPinIndex(
	const TArray<FVector2D>& PinPositions,
	int32 CurrentIndex,
	const FVector2D& ScreenDirection)
{
	if (!PinPositions.IsValidIndex(CurrentIndex) || PinPositions.Num() < 2
		|| ScreenDirection.IsNearlyZero())
	{
		return INDEX_NONE;
	}

	const FVector2D Direction = ScreenDirection.GetSafeNormal();
	const FVector2D Current = PinPositions[CurrentIndex];

	double BestScore = TNumericLimits<double>::Max();
	int32 BestIndex = INDEX_NONE;

	for (int32 CandidateIndex = 0; CandidateIndex < PinPositions.Num(); ++CandidateIndex)
	{
		if (CandidateIndex == CurrentIndex)
		{
			continue;
		}

		const FVector2D Delta = PinPositions[CandidateIndex] - Current;
		const double DistanceSquared = Delta.SizeSquared();
		if (DistanceSquared <= UE_DOUBLE_SMALL_NUMBER)
		{
			// 같은 자리에 겹친 핀은 방향 입력만으로 구분할 수 없다.
			continue;
		}

		const double ForwardDistance = FVector2D::DotProduct(Delta, Direction);
		if (ForwardDistance <= UE_DOUBLE_SMALL_NUMBER)
		{
			// 입력 방향의 뒤쪽 또는 정확히 옆에 있는 후보는 제외한다.
			continue;
		}

		// 거리만 보면 가까운 대각선 핀이 항상 이기고, 각도만 보면 아주 먼 핀이 이긴다.
		// 거리 제곱을 전방 투영으로 나누면 두 성질을 함께 반영할 수 있다.
		const double Score = DistanceSquared / ForwardDistance;
		if (Score < BestScore)
		{
			BestScore = Score;
			BestIndex = CandidateIndex;
		}
	}

	return BestIndex;
}

// --- 드래그 추적 ---

void FFF_AtlasDragTracker::Press(const FVector2D& Position)
{
	PressPosition = Position;
	LastPosition = Position;
	bPressed = true;
	bDragging = false;
}

FVector2D FFF_AtlasDragTracker::AddMove(const FVector2D& Position)
{
	if (!bPressed)
	{
		// 누르지 않은 채 움직이는 것은 호버다. 회전시키지 않는다.
		return FVector2D::ZeroVector;
	}

	if (!bDragging)
	{
		// 누른 자리에서 얼마나 벗어났나. **축별이 아니라 총 거리**다 —
		// 축별로 재면 대각선으로 천천히 끌 때 어느 축도 임계를 넘지 못한다.
		const double Travel = (Position - PressPosition).Size();
		if (Travel <= static_cast<double>(DragThresholdPixels))
		{
			// 아직 클릭일 수 있다. 이 구간의 이동은 버린다 (데드존).
			LastPosition = Position;
			return FVector2D::ZeroVector;
		}

		// 여기서 드래그로 승격된다. 쌓인 이동을 한꺼번에 적용하지 않는다 —
		// 그러면 지구가 임계 거리만큼 툭 튄다.
		bDragging = true;
		LastPosition = Position;
		return FVector2D::ZeroVector;
	}

	const FVector2D Delta = Position - LastPosition;
	LastPosition = Position;
	return Delta;
}

bool FFF_AtlasDragTracker::Release()
{
	// 누른 적이 없으면 클릭이 아니다. 위젯 밖에서 누르고 안에서 뗀 경우가 클릭이 되면 안 된다.
	const bool bWasClick = bPressed && !bDragging;

	Reset();
	return bWasClick;
}

void FFF_AtlasDragTracker::Reset()
{
	// 해제는 조건부, 정리는 무조건이다.
	PressPosition = FVector2D::ZeroVector;
	LastPosition = FVector2D::ZeroVector;
	bPressed = false;
	bDragging = false;
}
