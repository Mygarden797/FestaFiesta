// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdSplinePath.h"

void FFF_CrowdSplineLane::RebuildDerived()
{
	CumulativeDistCm.Reset();
	Bounds = FBox(ForceInit);
	bClosedLoop = false;

	if (Points.Num() == 0)
	{
		return;
	}

	CumulativeDistCm.Reserve(Points.Num());
	CumulativeDistCm.Add(0.f);
	Bounds += Points[0];

	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		const float StepCm = static_cast<float>(FVector::Dist2D(Points[Index - 1], Points[Index]));
		CumulativeDistCm.Add(CumulativeDistCm[Index - 1] + StepCm);
		Bounds += Points[Index];
	}

	bClosedLoop = Points.Num() >= 4 && Points[0].Equals(Points.Last(), 1.f);

	Bounds = Bounds.ExpandBy(FMath::Max(HalfWidthCm, 0.f));
}

namespace CrowdSplinePath
{
	namespace
	{
		constexpr float PointMergeEpsilonCm = 1.f;

		void AppendUnique(TArray<FVector>& Points, const FVector& Candidate)
		{
			if (Points.Num() > 0
				&& FVector::DistSquared2D(Points.Last(), Candidate) <= FMath::Square(PointMergeEpsilonCm))
			{
				return;
			}

			Points.Add(Candidate);
		}
	}

	bool FindClosestOnLane(const FFF_CrowdSplineLane& Lane, const FVector& QueryCm,
	                       FVector& OutPointCm, float& OutArcCm, float& OutDist2DCm)
	{
		if (Lane.Points.Num() < 2 || Lane.CumulativeDistCm.Num() != Lane.Points.Num())
		{
			return false;
		}

		float BestDistSq = TNumericLimits<float>::Max();

		for (int32 Index = 0; Index + 1 < Lane.Points.Num(); ++Index)
		{
			const FVector& A = Lane.Points[Index];
			const FVector& B = Lane.Points[Index + 1];

			const FVector2D AB(B.X - A.X, B.Y - A.Y);
			const FVector2D AQ(QueryCm.X - A.X, QueryCm.Y - A.Y);

			const float LenSq = static_cast<float>(AB.SizeSquared());
			const float T = LenSq > KINDA_SMALL_NUMBER
				? FMath::Clamp(static_cast<float>(FVector2D::DotProduct(AQ, AB)) / LenSq, 0.f, 1.f)
				: 0.f;

			const FVector Projected = FMath::Lerp(A, B, T);
			const float DistSq = static_cast<float>(FVector::DistSquared2D(Projected, QueryCm));

			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutPointCm = Projected;
				OutArcCm = FMath::Lerp(Lane.CumulativeDistCm[Index], Lane.CumulativeDistCm[Index + 1], T);
			}
		}

		OutDist2DCm = FMath::Sqrt(BestDistSq);
		return true;
	}

	bool TryBuildPath(const TArray<FFF_CrowdSplineLane>& Lanes,
	                  const FVector& StartCm, const FVector& TargetCm,
	                  const FQueryParams& Params,
	                  TArray<FVector>& OutPoints, int32& OutLaneIndex)
	{
		OutPoints.Reset();
		OutLaneIndex = INDEX_NONE;

		const float DirectCm = static_cast<float>(FVector::Dist2D(StartCm, TargetCm));
		if (DirectCm <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		const float MaxTotalCm = DirectCm * FMath::Max(Params.MaxDetourRatio, 1.f);

		float BestTotalCm = TNumericLimits<float>::Max();
		TArray<FVector> BestPoints;
		TArray<TPair<float, int32>> Between;

		for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
		{
			const FFF_CrowdSplineLane& Lane = Lanes[LaneIndex];
			if (!Lane.IsUsable())
			{
				continue;
			}

			FVector ExitPoint = FVector::ZeroVector;
			float ExitArcCm = 0.f;
			float ExitDistCm = 0.f;
			if (!FindClosestOnLane(Lane, TargetCm, ExitPoint, ExitArcCm, ExitDistCm)
				|| ExitDistCm > Params.ExitSnapRadiusCm)
			{
				continue;
			}

			FVector EntryPoint = FVector::ZeroVector;
			float EntryArcCm = 0.f;
			float EntryDistCm = 0.f;
			if (!FindClosestOnLane(Lane, StartCm, EntryPoint, EntryArcCm, EntryDistCm)
				|| EntryDistCm > Params.EntrySnapRadiusCm)
			{
				continue;
			}

			const float TotalLengthCm = Lane.TotalLengthCm();

			bool bReverse = false;
			float ArcSpanCm = 0.f;

			if (Lane.bClosedLoop && TotalLengthCm > KINDA_SMALL_NUMBER)
			{
				const float ForwardCm = FMath::Fmod(ExitArcCm - EntryArcCm + TotalLengthCm, TotalLengthCm);
				const float BackwardCm = TotalLengthCm - ForwardCm;

				if (!Lane.bOneWay && BackwardCm < ForwardCm)
				{
					bReverse = true;
					ArcSpanCm = BackwardCm;
				}
				else
				{
					ArcSpanCm = ForwardCm;
				}
			}
			else if (ExitArcCm >= EntryArcCm)
			{
				ArcSpanCm = ExitArcCm - EntryArcCm;
			}
			else if (Lane.bOneWay)
			{
				continue;
			}
			else
			{
				bReverse = true;
				ArcSpanCm = EntryArcCm - ExitArcCm;
			}

			const float CandidateTotalCm = EntryDistCm + ArcSpanCm + ExitDistCm;
			if (CandidateTotalCm > MaxTotalCm || CandidateTotalCm >= BestTotalCm)
			{
				continue;
			}

			Between.Reset();

			const int32 VertexCount = Lane.bClosedLoop ? Lane.Points.Num() - 1 : Lane.Points.Num();
			for (int32 Vertex = 0; Vertex < VertexCount; ++Vertex)
			{
				float RelativeCm = bReverse
					? EntryArcCm - Lane.CumulativeDistCm[Vertex]
					: Lane.CumulativeDistCm[Vertex] - EntryArcCm;

				if (Lane.bClosedLoop && TotalLengthCm > KINDA_SMALL_NUMBER)
				{
					RelativeCm = FMath::Fmod(RelativeCm + TotalLengthCm, TotalLengthCm);
				}

				if (RelativeCm > PointMergeEpsilonCm && RelativeCm < ArcSpanCm - PointMergeEpsilonCm)
				{
					Between.Emplace(RelativeCm, Vertex);
				}
			}

			Between.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
			{
				return A.Key < B.Key;
			});

			TArray<FVector> Candidate;
			Candidate.Reserve(Between.Num() + 3);

			AppendUnique(Candidate, EntryPoint);
			for (const TPair<float, int32>& Entry : Between)
			{
				AppendUnique(Candidate, Lane.Points[Entry.Value]);
			}
			AppendUnique(Candidate, ExitPoint);
			AppendUnique(Candidate, TargetCm);

			BestTotalCm = CandidateTotalCm;
			BestPoints = MoveTemp(Candidate);
			OutLaneIndex = LaneIndex;
		}

		if (OutLaneIndex == INDEX_NONE)
		{
			return false;
		}

		OutPoints = MoveTemp(BestPoints);
		return true;
	}
}
