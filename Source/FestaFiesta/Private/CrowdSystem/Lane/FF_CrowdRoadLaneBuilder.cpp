// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Lane/FF_CrowdRoadLaneBuilder.h"

namespace
{
	// 인접 판정 여유. 타일 피치와 측면 어긋남 모두 피치 대비 비율로 본다.
	constexpr float PitchToleranceRatio = 0.2f;

	// 타일 축이 서로 90도 배수로 맞아야 격자로 이어 붙일 수 있다.
	constexpr float AxisAlignToleranceDegrees = 5.f;

	FVector2D ToTileLocal2D(const FFF_CrowdRoadTile& Tile, const FVector& WorldCm)
	{
		const double DeltaX = WorldCm.X - Tile.CenterCm.X;
		const double DeltaY = WorldCm.Y - Tile.CenterCm.Y;
		const double Radians = FMath::DegreesToRadians(static_cast<double>(-Tile.YawDegrees));
		const double Sin = FMath::Sin(Radians);
		const double Cos = FMath::Cos(Radians);
		return FVector2D(DeltaX * Cos - DeltaY * Sin, DeltaX * Sin + DeltaY * Cos);
	}

	// 도로는 정사각 footprint(Road_Surface_*는 400x400)라서 피치를 한 값으로 본다.
	// 직사각 도로가 생기면 이 가정부터 깨진다 — 그때는 축별 반치수를 따로 봐야 한다.
	float GetTileHalfPitchCm(const FFF_CrowdRoadTile& Tile)
	{
		return FMath::Max(Tile.HalfExtentCm.X, Tile.HalfExtentCm.Y);
	}

	bool AreTilesAdjacent(const FFF_CrowdRoadTile& A, const FFF_CrowdRoadTile& B, const float MaxStepCm)
	{
		if (FMath::Abs(A.CenterCm.Z - B.CenterCm.Z) > MaxStepCm)
		{
			return false;
		}

		const float YawDelta = FMath::Abs(FRotator::NormalizeAxis(B.YawDegrees - A.YawDegrees));
		const float OffAxis = FMath::Fmod(YawDelta, 90.f);
		if (FMath::Min(OffAxis, 90.f - OffAxis) > AxisAlignToleranceDegrees)
		{
			return false;
		}

		const float PitchCm = GetTileHalfPitchCm(A) + GetTileHalfPitchCm(B);
		if (PitchCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float ToleranceCm = PitchCm * PitchToleranceRatio;
		const FVector2D Local = ToTileLocal2D(A, B.CenterCm);
		const float AbsX = static_cast<float>(FMath::Abs(Local.X));
		const float AbsY = static_cast<float>(FMath::Abs(Local.Y));

		// 대각으로 맞닿은 타일은 잇지 않는다. 모서리만 닿은 두 칸 사이로는 실제로 지나갈 수 없다.
		const bool bAlongX = FMath::Abs(AbsX - PitchCm) <= ToleranceCm && AbsY <= ToleranceCm;
		const bool bAlongY = FMath::Abs(AbsY - PitchCm) <= ToleranceCm && AbsX <= ToleranceCm;
		return bAlongX || bAlongY;
	}

	uint64 MakeEdgeKey(const int32 A, const int32 B)
	{
		const uint64 Low = static_cast<uint64>(FMath::Min(A, B));
		const uint64 High = static_cast<uint64>(FMath::Max(A, B));
		return (High << 32) | Low;
	}

	// 직선 구간의 중간 정점을 버린다. 유지된 직전 정점과 다음 정점을 잇는 선에서
	// 벗어난 거리가 오차 안이면 중간 정점은 정보가 없다.
	void SimplifyPolyline(TArray<FVector>& Points, const float ToleranceCm)
	{
		if (Points.Num() < 3 || ToleranceCm <= 0.f)
		{
			return;
		}

		TArray<FVector> Kept;
		Kept.Reserve(Points.Num());
		Kept.Add(Points[0]);

		for (int32 Index = 1; Index + 1 < Points.Num(); ++Index)
		{
			const FVector& Previous = Kept.Last();
			const FVector& Current = Points[Index];
			const FVector& Next = Points[Index + 1];

			const FVector2D Segment(Next.X - Previous.X, Next.Y - Previous.Y);
			const FVector2D ToCurrent(Current.X - Previous.X, Current.Y - Previous.Y);
			const double LengthSq = Segment.SizeSquared();

			double OffsetCm = 0.0;
			if (LengthSq <= KINDA_SMALL_NUMBER)
			{
				OffsetCm = ToCurrent.Size();
			}
			else
			{
				const double Cross = Segment.X * ToCurrent.Y - Segment.Y * ToCurrent.X;
				OffsetCm = FMath::Abs(Cross) / FMath::Sqrt(LengthSq);
			}

			if (OffsetCm > ToleranceCm)
			{
				Kept.Add(Current);
			}
		}

		Kept.Add(Points.Last());
		Points = MoveTemp(Kept);
	}

	bool EmitLane(const TArray<FVector>& Points, const FFF_CrowdRoadLaneBuildParams& Params,
	              TArray<FFF_CrowdSplineLane>& OutLanes)
	{
		if (Points.Num() < 2)
		{
			return false;
		}

		TArray<FVector> Simplified = Points;
		SimplifyPolyline(Simplified, Params.SimplifyToleranceCm);

		FFF_CrowdSplineLane Lane;
		Lane.Points = MoveTemp(Simplified);
		Lane.HalfWidthCm = Params.HalfWidthCm;
		Lane.bOneWay = false;
		Lane.RebuildDerived();

		if (!Lane.IsUsable())
		{
			return false;
		}

		OutLanes.Add(MoveTemp(Lane));
		return true;
	}
}

namespace CrowdRoadLanes
{
	FFF_CrowdRoadLaneBuildStats BuildLanes(
		const TArray<FFF_CrowdRoadTile>& Tiles,
		const FFF_CrowdRoadLaneBuildParams& Params,
		TArray<FFF_CrowdSplineLane>& OutLanes)
	{
		OutLanes.Reset();

		FFF_CrowdRoadLaneBuildStats Stats;
		Stats.TileCount = Tiles.Num();

		if (Tiles.Num() < 2)
		{
			Stats.IsolatedTileCount = Tiles.Num();
			return Stats;
		}

		// 좌표로 정렬해 호출부의 시설 순회 순서를 결과에서 지운다.
		TArray<FFF_CrowdRoadTile> Sorted = Tiles;
		Sorted.Sort([](const FFF_CrowdRoadTile& A, const FFF_CrowdRoadTile& B)
		{
			if (A.CenterCm.X != B.CenterCm.X) return A.CenterCm.X < B.CenterCm.X;
			if (A.CenterCm.Y != B.CenterCm.Y) return A.CenterCm.Y < B.CenterCm.Y;
			return A.CenterCm.Z < B.CenterCm.Z;
		});

		float MaxHalfPitchCm = 0.f;
		for (const FFF_CrowdRoadTile& Tile : Sorted)
		{
			MaxHalfPitchCm = FMath::Max(MaxHalfPitchCm, GetTileHalfPitchCm(Tile));
		}

		const float BucketSizeCm = FMath::Max(MaxHalfPitchCm * 2.f, 1.f);

		TMap<FIntPoint, TArray<int32>> Buckets;
		for (int32 Index = 0; Index < Sorted.Num(); ++Index)
		{
			const FIntPoint Key(
				FMath::FloorToInt(Sorted[Index].CenterCm.X / BucketSizeCm),
				FMath::FloorToInt(Sorted[Index].CenterCm.Y / BucketSizeCm));
			Buckets.FindOrAdd(Key).Add(Index);
		}

		TArray<TArray<int32>> Adjacency;
		Adjacency.SetNum(Sorted.Num());

		for (int32 Index = 0; Index < Sorted.Num(); ++Index)
		{
			const FIntPoint Key(
				FMath::FloorToInt(Sorted[Index].CenterCm.X / BucketSizeCm),
				FMath::FloorToInt(Sorted[Index].CenterCm.Y / BucketSizeCm));

			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					const TArray<int32>* Bucket = Buckets.Find(FIntPoint(Key.X + OffsetX, Key.Y + OffsetY));
					if (!Bucket)
					{
						continue;
					}

					for (const int32 Other : *Bucket)
					{
						if (Other <= Index)
						{
							continue;
						}
						if (!AreTilesAdjacent(Sorted[Index], Sorted[Other], Params.MaxStepCm))
						{
							continue;
						}

						Adjacency[Index].Add(Other);
						Adjacency[Other].Add(Index);
						++Stats.EdgeCount;
					}
				}
			}
		}

		for (TArray<int32>& Neighbours : Adjacency)
		{
			Neighbours.Sort();
		}

		for (int32 Index = 0; Index < Sorted.Num(); ++Index)
		{
			if (Adjacency[Index].Num() == 0)
			{
				++Stats.IsolatedTileCount;
			}
			else if (Adjacency[Index].Num() != 2)
			{
				++Stats.JunctionCount;
			}
		}

		TSet<uint64> VisitedEdges;
		VisitedEdges.Reserve(Stats.EdgeCount);

		// 교차점·막다른길에서 출발해 degree 2 구간을 따라가다 다음 갈림에서 끊는다.
		auto TraceFrom = [&](const int32 StartNode, const int32 FirstNeighbour, TArray<FVector>& OutPoints)
		{
			OutPoints.Reset();
			OutPoints.Add(Sorted[StartNode].CenterCm);

			int32 Previous = StartNode;
			int32 Current = FirstNeighbour;

			VisitedEdges.Add(MakeEdgeKey(Previous, Current));
			OutPoints.Add(Sorted[Current].CenterCm);

			while (Adjacency[Current].Num() == 2)
			{
				const int32 Next = Adjacency[Current][0] == Previous
					? Adjacency[Current][1]
					: Adjacency[Current][0];

				const uint64 EdgeKey = MakeEdgeKey(Current, Next);
				if (VisitedEdges.Contains(EdgeKey))
				{
					break;
				}

				VisitedEdges.Add(EdgeKey);
				OutPoints.Add(Sorted[Next].CenterCm);

				Previous = Current;
				Current = Next;
			}
		};

		TArray<FVector> Points;

		for (int32 Index = 0; Index < Sorted.Num(); ++Index)
		{
			if (Adjacency[Index].Num() == 2 || Adjacency[Index].Num() == 0)
			{
				continue;
			}

			for (const int32 Neighbour : Adjacency[Index])
			{
				if (VisitedEdges.Contains(MakeEdgeKey(Index, Neighbour)))
				{
					continue;
				}

				TraceFrom(Index, Neighbour, Points);
				EmitLane(Points, Params, OutLanes);
			}
		}

		// 갈림이 하나도 없는 고리는 위 순회가 건드리지 않는다. 남은 간선에서 출발시키면
		// 추적이 제자리로 돌아와 첫 점이 끝에 한 번 더 들어가므로, 그대로 닫힌 레인이 된다
		// (RebuildDerived가 첫 점과 끝 점이 같은 것을 보고 bClosedLoop을 세운다).
		for (int32 Index = 0; Index < Sorted.Num(); ++Index)
		{
			if (Adjacency[Index].Num() != 2)
			{
				continue;
			}

			for (const int32 Neighbour : Adjacency[Index])
			{
				if (VisitedEdges.Contains(MakeEdgeKey(Index, Neighbour)))
				{
					continue;
				}

				TraceFrom(Index, Neighbour, Points);

				if (EmitLane(Points, Params, OutLanes) && OutLanes.Last().bClosedLoop)
				{
					++Stats.ClosedLoopCount;
				}
			}
		}

		if (OutLanes.Num() > Params.MaxLanes)
		{
			OutLanes.SetNum(Params.MaxLanes);
			Stats.bTruncated = true;
		}

		return Stats;
	}
}
