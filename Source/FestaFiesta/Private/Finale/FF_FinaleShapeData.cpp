// Copyright Epic Games, Inc. All Rights Reserved.

#include "Finale/FF_FinaleShapeData.h"
#include "Finale/FF_FinaleShowTypes.h"

void UFF_FinaleShapeData::SamplePoints(const int32 Count, TArray<FVector2D>& OutPoints) const
{
	OutPoints.Reset();

	if (!IsValidShape() || Count <= 0)
	{
		return;
	}

	/* 구간마다 길이를 재서 누적한다. 이 표가 있어야 "전체의 30% 지점" 을 찾을 수 있다.

	   닫힌 도형은 마지막 점에서 첫 점으로 돌아오는 구간을 하나 더 센다.
	*/
	const int32 PointCount = Points.Num();
	const int32 SegmentCount = bClosed ? PointCount : PointCount - 1;

	TArray<float> Cumulative;
	Cumulative.Reserve(SegmentCount + 1);
	Cumulative.Add(0.f);

	float Total = 0.f;
	for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
	{
		const FVector2D& From = Points[Segment];
		const FVector2D& To = Points[(Segment + 1) % PointCount];

		Total += FVector2D::Distance(From, To);
		Cumulative.Add(Total);
	}

	if (Total <= KINDA_SMALL_NUMBER)
	{
		// 점이 전부 같은 자리다. 선이 아니므로 그릴 것이 없다.
		return;
	}

	OutPoints.Reserve(Count);

	/* 닫힌 도형은 Count 로 나눈다. 열린 도형은 Count - 1 로 나눈다.

	   닫힌 쪽을 Count - 1 로 나누면 마지막 점이 첫 점 자리에 겹쳐서, 한 자리에
	   두 발이 터지고 반대편은 한 발 비어 보인다.
	*/
	const float Step = bClosed
		? Total / static_cast<float>(Count)
		: (Count == 1 ? 0.f : Total / static_cast<float>(Count - 1));

	int32 Segment = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Target = FMath::Min(Step * static_cast<float>(Index), Total);

		// 목표 거리를 품는 구간까지 걸어간다. 인덱스가 되돌아가지 않으므로 전체가 O(n)이다.
		while (Segment < SegmentCount - 1 && Cumulative[Segment + 1] < Target)
		{
			++Segment;
		}

		const float SegmentStart = Cumulative[Segment];
		const float SegmentLength = Cumulative[Segment + 1] - SegmentStart;

		const float Alpha = (SegmentLength > KINDA_SMALL_NUMBER)
			? (Target - SegmentStart) / SegmentLength
			: 0.f;

		const FVector2D& From = Points[Segment];
		const FVector2D& To = Points[(Segment + 1) % PointCount];

		OutPoints.Add(FMath::Lerp(From, To, Alpha));
	}
}

void UFF_FinaleShapeData::SetFromRawPoints(const TArray<FVector2D>& RawPoints, const bool bInClosed)
{
	Points.Reset();
	bClosed = bInClosed;

	if (RawPoints.Num() < 2)
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 점이 %d 개뿐이라 모양으로 굽지 않는다. 최소 2개가 필요하다."),
			RawPoints.Num());
		return;
	}

	// 경계 상자를 잡는다.
	FVector2D Min = RawPoints[0];
	FVector2D Max = RawPoints[0];

	for (const FVector2D& Point : RawPoints)
	{
		Min.X = FMath::Min(Min.X, Point.X);
		Min.Y = FMath::Min(Min.Y, Point.Y);
		Max.X = FMath::Max(Max.X, Point.X);
		Max.Y = FMath::Max(Max.Y, Point.Y);
	}

	const FVector2D Size = Max - Min;
	const FVector2D Center = (Max + Min) * 0.5f;

	/* 긴 쪽을 1 로 맞춘다. 축마다 따로 맞추면 모양이 찌그러진다.

	   그래서 결과가 항상 -0.5 ~ +0.5 를 꽉 채우지는 않는다. 세로로 긴 하트는
	   가로가 그보다 좁게 남는데, 그게 맞다 — ExtentCm 은 "이 모양이 차지하는
	   가장 긴 변" 이라는 뜻이 된다.
	*/
	const float Longest = FMath::Max(Size.X, Size.Y);

	if (Longest <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogFestaFiestaFinale, Warning,
			TEXT("[피날레] 점이 전부 같은 자리라 모양으로 굽지 않는다."));
		return;
	}

	const float Scale = 1.f / Longest;

	Points.Reserve(RawPoints.Num());
	for (const FVector2D& Point : RawPoints)
	{
		Points.Add((Point - Center) * Scale);
	}

	UE_LOG(LogFestaFiestaFinale, Log,
		TEXT("[피날레] 모양을 구웠다. 점 %d개, %s, 원본 크기 %.0f x %.0f"),
		Points.Num(), bClosed ? TEXT("닫힘") : TEXT("열림"), Size.X, Size.Y);
}
