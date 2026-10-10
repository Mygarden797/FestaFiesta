// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdHeightField.h"

void FFF_CrowdHeightField::Reset()
{
	Heights.Reset();
	Valid.Reset();
	Discontinuities.Reset();
	UnknownCells.Reset();
	MinCornerCm = FVector2D::ZeroVector;
	SpacingCm = 0.0;
	NumX = 0;
	NumY = 0;
	Stats = FBuildStats();
}

bool FFF_CrowdHeightField::Build(const FVector2D& CenterCm, double HalfSizeCm, double InSpacingCm,
                                 FHeightSampler Sampler)
{
	Reset();

	if (HalfSizeCm <= 0.0 || InSpacingCm <= 0.0)
	{
		return false;
	}

	const int32 Divisions = FMath::Max(1, FMath::CeilToInt(2.0 * HalfSizeCm / InSpacingCm));

	NumX = Divisions + 1;
	NumY = Divisions + 1;
	SpacingCm = (2.0 * HalfSizeCm) / static_cast<double>(Divisions);
	MinCornerCm = CenterCm - FVector2D(HalfSizeCm, HalfSizeCm);

	const int32 Total = NumX * NumY;
	Heights.SetNumZeroed(Total);
	Valid.SetNumZeroed(Total);

	Stats.SampleCount = Total;

	bool bAnyHit = false;

	for (int32 Iy = 0; Iy < NumY; ++Iy)
	{
		for (int32 Ix = 0; Ix < NumX; ++Ix)
		{
			const FVector2D QueryCm(
				MinCornerCm.X + static_cast<double>(Ix) * SpacingCm,
				MinCornerCm.Y + static_cast<double>(Iy) * SpacingCm);

			double HeightCm = 0.0;
			if (!Sampler(QueryCm, HeightCm))
			{
				continue;
			}

			const int32 Index = Iy * NumX + Ix;
			Heights[Index] = HeightCm;
			Valid[Index] = 1;

			if (!bAnyHit)
			{
				Stats.MinHeightCm = HeightCm;
				Stats.MaxHeightCm = HeightCm;
				bAnyHit = true;
			}
			else
			{
				Stats.MinHeightCm = FMath::Min(Stats.MinHeightCm, HeightCm);
				Stats.MaxHeightCm = FMath::Max(Stats.MaxHeightCm, HeightCm);
			}

			++Stats.HitCount;
		}
	}

	return bAnyHit;
}

int32 FFF_CrowdHeightField::RefineDiscontinuities(const double MinimumHeightGapCm,
	const double SpatialToleranceCm, const int32 MaximumAdditionalProbes, FHeightSampler Sampler)
{
	Discontinuities.Reset();
	UnknownCells.Reset();
	Stats.BoundaryProbeCount = Stats.BoundaryCellCount = Stats.UnknownBoundaryCellCount = 0;
	if (!IsValid() || MinimumHeightGapCm <= 0.0 || SpatialToleranceCm <= 0.0) return 0;
	const auto Sample = [&](const FVector2D& XY, double& Z)
	{
		if (Stats.BoundaryProbeCount >= MaximumAdditionalProbes) return false;
		++Stats.BoundaryProbeCount;
		return Sampler(XY, Z) && FMath::IsFinite(Z);
	};
	constexpr int32 Perimeter[4] = {0, 1, 3, 2};
	constexpr int32 MaxBoundaryIterations = 12;
	for (int32 Y = 0; Y < NumY - 1; ++Y)
	{
		for (int32 X = 0; X < NumX - 1; ++X)
		{
			const int32 Key = Y * NumX + X;
			FCellProbe Probe;
			ProbeCell(MinCornerCm + FVector2D((X + .5) * SpacingCm, (Y + .5) * SpacingCm), Probe);
			TArray<double, TInlineAllocator<4>> Sorted;
			for (int32 C = 0; C < 4; ++C)
				if (Probe.bCornerValid[C]) Sorted.Add(Probe.CornerHeightCm[C]);
			if (Sorted.Num() == 0) continue;
			Sorted.Sort();
			if (Sorted.Num() == 4 && Sorted.Last() - Sorted[0] <= MinimumHeightGapCm) continue;
			FDiscontinuityCell& Cell = Discontinuities.Add(Key);
			Cell.ToleranceCm = SpatialToleranceCm;
			++Stats.BoundaryCellCount;
			// Invalid is the default until both classes and their boundary pass.
			++Stats.UnknownBoundaryCellCount;
			if (Sorted.Num() != 4) continue;
			int32 Split = 0;
			for (int32 C = 1; C < 3; ++C)
				if (Sorted[C + 1] - Sorted[C] > Sorted[Split + 1] - Sorted[Split]) Split = C;
			const double LowMin = Sorted[0], LowMax = Sorted[Split];
			const double HighMin = Sorted[Split + 1], HighMax = Sorted[3];
			// Continuous steep slopes are still ordinary scalar fields. Three
			// separated terraces cannot be represented by this two-class cell.
			if (HighMin - LowMax <= MinimumHeightGapCm)
			{
				Discontinuities.Remove(Key);
				--Stats.BoundaryCellCount;
				--Stats.UnknownBoundaryCellCount;
				continue;
			}
			if (LowMax - LowMin > MinimumHeightGapCm * .5 || HighMax - HighMin > MinimumHeightGapCm * .5) continue;
			const double Threshold = (LowMax + HighMin) * .5;
			const auto Classify = [&](const double Z, bool& bHigh)
			{
				bHigh = Z > Threshold;
				const double Min = bHigh ? HighMin : LowMin, Max = bHigh ? HighMax : LowMax;
				return Z >= Min - MinimumHeightGapCm * .25 && Z <= Max + MinimumHeightGapCm * .25;
			};
			for (int32 C = 0; C < 4; ++C)
				if (Probe.CornerHeightCm[C] > Threshold) Cell.HighCornerMask |= 1 << C;
			int32 Crossings = 0;
			for (int32 E = 0; E < 4; ++E)
				Crossings += ((Cell.HighCornerMask >> Perimeter[E]) & 1) != ((Cell.HighCornerMask >> Perimeter[(E + 1) % 4]) & 1);
			if (Crossings != 2) continue; // Checkerboards require more than one boundary.
			FVector2D Crossing[2];
			int32 CrossingCount = 0;
			bool bValidBoundary = true;
			for (int32 E = 0; E < 4 && bValidBoundary; ++E)
			{
				const int32 A = Perimeter[E], B = Perimeter[(E + 1) % 4];
				const bool bAHigh = (Cell.HighCornerMask & (1 << A)) != 0;
				if (bAHigh == ((Cell.HighCornerMask & (1 << B)) != 0)) continue;
				FVector2D NearA = Probe.CornerXYCm[A], NearB = Probe.CornerXYCm[B];
				for (int32 Iteration = 0; Iteration < MaxBoundaryIterations
					&& FVector2D::Distance(NearA, NearB) > SpatialToleranceCm; ++Iteration)
				{
					const FVector2D Mid = (NearA + NearB) * .5;
					double Z = 0.0;
					bool bHigh = false;
					if (!Sample(Mid, Z) || !Classify(Z, bHigh)) { bValidBoundary = false; break; }
					if (bHigh == bAHigh) NearA = Mid; else NearB = Mid;
				}
				if (FVector2D::Distance(NearA, NearB) > SpatialToleranceCm) bValidBoundary = false;
				Crossing[CrossingCount++] = (NearA + NearB) * .5;
			}
			if (!bValidBoundary || CrossingCount != 2) continue;
			Cell.StartCm = Crossing[0];
			Cell.EndCm = Crossing[1];
			const FVector2D Direction = Cell.EndCm - Cell.StartCm;
			Cell.HighNormal = FVector2D(-Direction.Y, Direction.X).GetSafeNormal();
			if (Cell.HighNormal.IsNearlyZero()) continue;
			for (int32 C = 0; C < 4; ++C)
				if (Cell.HighCornerMask & (1 << C))
				{
					if (FVector2D::DotProduct(Probe.CornerXYCm[C] - Cell.StartCm, Cell.HighNormal) < 0.0) Cell.HighNormal *= -1.0;
					break;
				}
			// Hold-outs reject observed curved/multiple boundaries or a third
			// floor hidden from the four corners. They are not physics queries at
			// runtime, nor a guarantee about arbitrary unsampled subcell islands.
			const FVector2D Holdouts[] = {{.5, .5}, {.25, .25}, {.75, .25}, {.25, .75}, {.75, .75}};
			for (const FVector2D& UV : Holdouts)
			{
				const FVector2D XY = Probe.CornerXYCm[0] + UV * SpacingCm;
				double Z = 0.0;
				bool bHigh = false;
				if (!Sample(XY, Z) || !Classify(Z, bHigh)) { bValidBoundary = false; break; }
				const double Side = FVector2D::DotProduct(XY - Cell.StartCm, Cell.HighNormal);
				if (FMath::Abs(Side) > SpatialToleranceCm && bHigh != (Side >= 0.0)) { bValidBoundary = false; break; }
			}
			if (bValidBoundary)
			{
				Cell.bResolved = true;
				--Stats.UnknownBoundaryCellCount;
			}
		}
	}
	return Stats.BoundaryProbeCount;
}

void FFF_CrowdHeightField::MarkCellUnknown(const FVector2D& WorldXYCm)
{
	int32 X = 0, Y = 0;
	double Tx = 0.0, Ty = 0.0;
	if (!TryLocateCell(WorldXYCm, X, Y, Tx, Ty)) return;
	const int32 Key = Y * NumX + X;
	if (FDiscontinuityCell* Cell = Discontinuities.Find(Key))
	{
		if (Cell->bResolved) ++Stats.UnknownBoundaryCellCount;
		Cell->bResolved = false;
		return;
	}
	if (UnknownCells.Num() == 0) UnknownCells.Init(false, NumX * NumY);
	if (UnknownCells[Key]) return;
	UnknownCells[Key] = true;
	++Stats.BoundaryCellCount;
	++Stats.UnknownBoundaryCellCount;
}

bool FFF_CrowdHeightField::TryLocateCell(const FVector2D& WorldXYCm, int32& OutIx, int32& OutIy,
                                        double& OutTx, double& OutTy) const
{
	if (!IsValid())
	{
		return false;
	}

	const double Fx = FMath::Clamp((WorldXYCm.X - MinCornerCm.X) / SpacingCm, 0.0, static_cast<double>(NumX - 1));
	const double Fy = FMath::Clamp((WorldXYCm.Y - MinCornerCm.Y) / SpacingCm, 0.0, static_cast<double>(NumY - 1));

	OutIx = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Fx)), 0, NumX - 2);
	OutIy = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Fy)), 0, NumY - 2);

	OutTx = FMath::Clamp(Fx - static_cast<double>(OutIx), 0.0, 1.0);
	OutTy = FMath::Clamp(Fy - static_cast<double>(OutIy), 0.0, 1.0);

	return true;
}

bool FFF_CrowdHeightField::IsWalkable(const FVector2D& WorldXYCm) const
{
	if (!Discontinuities.IsEmpty() || UnknownCells.Num() > 0)
	{
		int32 X = 0, Y = 0;
		double Tx = 0.0, Ty = 0.0;
		if (TryLocateCell(WorldXYCm, X, Y, Tx, Ty))
		{
			if (UnknownCells.IsValidIndex(Y * NumX + X) && UnknownCells[Y * NumX + X]) return !bBlockUnknownGround;
			if (const FDiscontinuityCell* Cell = Discontinuities.Find(Y * NumX + X))
			{
				if (!Cell->bResolved) return !bBlockUnknownGround;
				double Z = 0.0;
				return TryGetHeightCm(WorldXYCm, Z);
			}
		}
	}
	if (!bHasWalkableCeiling)
	{
		return true;
	}

	int32 Ix = 0;
	int32 Iy = 0;
	double Tx = 0.0;
	double Ty = 0.0;
	if (!TryLocateCell(WorldXYCm, Ix, Iy, Tx, Ty))
	{
		return true;
	}

	const int32 I00 = Iy * NumX + Ix;

	if (bBlockUnknownGround)
	{
		for (const int32 Index : { I00, I00 + 1, I00 + NumX, I00 + NumX + 1 })
		{
			if (!Valid.IsValidIndex(Index) || Valid[Index] == 0)
			{
				return false;
			}
		}
	}

	bool bAnyValid = false;
	double LowestCm = 0.0;
	for (const int32 Index : { I00, I00 + 1, I00 + NumX, I00 + NumX + 1 })
	{
		if (!Valid.IsValidIndex(Index) || Valid[Index] == 0)
		{
			continue;
		}
		LowestCm = bAnyValid ? FMath::Min(LowestCm, Heights[Index]) : Heights[Index];
		bAnyValid = true;
	}

	return !(bAnyValid && LowestCm > WalkableCeilingCm);
}

bool FFF_CrowdHeightField::ProbeCell(const FVector2D& WorldXYCm, FCellProbe& OutProbe) const
{
	OutProbe = FCellProbe();
	int32 Ix = 0;
	int32 Iy = 0;
	double Tx = 0.0;
	double Ty = 0.0;
	if (!TryLocateCell(WorldXYCm, Ix, Iy, Tx, Ty))
	{
		return false;
	}

	const int32 Offsets[4][2] = { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const int32 Cx = Ix + Offsets[Corner][0];
		const int32 Cy = Iy + Offsets[Corner][1];
		const int32 Index = Cy * NumX + Cx;

		OutProbe.CornerXYCm[Corner] = MinCornerCm + FVector2D(Cx * SpacingCm, Cy * SpacingCm);
		OutProbe.bCornerValid[Corner] = Valid.IsValidIndex(Index) && Valid[Index] != 0;
		OutProbe.CornerHeightCm[Corner] = OutProbe.bCornerValid[Corner] ? Heights[Index] : 0.0;
	}
	if (UnknownCells.IsValidIndex(Iy * NumX + Ix) && UnknownCells[Iy * NumX + Ix]) OutProbe.bHasDiscontinuity = true;
	if (const FDiscontinuityCell* Cell = Discontinuities.Find(Iy * NumX + Ix))
	{
		OutProbe.bHasDiscontinuity = true;
		OutProbe.bBoundaryResolved = Cell->bResolved;
		OutProbe.HighCornerMask = Cell->HighCornerMask;
		OutProbe.BoundaryStartCm = Cell->StartCm;
		OutProbe.BoundaryEndCm = Cell->EndCm;
		OutProbe.BoundaryToleranceCm = Cell->ToleranceCm;
		const FVector2D ClampedXY = MinCornerCm + FVector2D(Ix + Tx, Iy + Ty) * SpacingCm;
		OutProbe.bQueryOnHighSide = Cell->bResolved && FVector2D::DotProduct(ClampedXY - Cell->StartCm, Cell->HighNormal) >= 0.0;
	}
	return true;
}

bool FFF_CrowdHeightField::TryGetHeightCm(const FVector2D& WorldXYCm, double& OutHeightCm) const
{
	int32 Ix = 0;
	int32 Iy = 0;
	double Tx = 0.0;
	double Ty = 0.0;
	if (!TryLocateCell(WorldXYCm, Ix, Iy, Tx, Ty))
	{
		return false;
	}

	const int32 I00 = Iy * NumX + Ix;
	const int32 I10 = I00 + 1;
	const int32 I01 = I00 + NumX;
	const int32 I11 = I01 + 1;

	const double W00 = (1.0 - Tx) * (1.0 - Ty);
	const double W10 = Tx * (1.0 - Ty);
	const double W01 = (1.0 - Tx) * Ty;
	const double W11 = Tx * Ty;
	if (UnknownCells.IsValidIndex(I00) && UnknownCells[I00]) return false;

	if (const FDiscontinuityCell* Cell = Discontinuities.Find(I00))
	{
		if (!Cell->bResolved) return false;
		const FVector2D ClampedXY = MinCornerCm + FVector2D(Ix + Tx, Iy + Ty) * SpacingCm;
		const bool bHigh = FVector2D::DotProduct(ClampedXY - Cell->StartCm, Cell->HighNormal) >= 0.0;
		const int32 Indices[] = {I00, I10, I01, I11};
		const double Weights[] = {W00, W10, W01, W11};
		double Sum = 0.0, WeightSum = 0.0, Fallback = 0.0;
		bool bHasCandidate = false;
		for (int32 C = 0; C < 4; ++C)
		{
			const int32 Index = Indices[C];
			if (bHigh != ((Cell->HighCornerMask & (1 << C)) != 0)
				|| Valid[Index] == 0 || (bHasWalkableCeiling && Heights[Index] > WalkableCeilingCm)) continue;
			Sum += Heights[Index] * Weights[C];
			WeightSum += Weights[C];
			Fallback = Heights[Index];
			bHasCandidate = true;
		}
		if (!bHasCandidate) return false;
		OutHeightCm = WeightSum > 0.0 ? Sum / WeightSum : Fallback;
		return true;
	}

	// IsWalkable deliberately admits a cell with any corner below the ceiling:
	// a tree or roof at one sample must not block the surrounding ground. Height
	// selection must use that same admitted ground, including the cliff and
	// zero-weight fallbacks, rather than snapping feet onto the excluded roof.
	const auto IsGroundCandidate = [this](int32 Index)
	{
		return Valid.IsValidIndex(Index) && Valid[Index] != 0
			&& (!bHasWalkableCeiling || Heights[Index] <= WalkableCeilingCm);
	};

	// 절벽 판정을 먼저 한다. 네 꼭짓점 높이 차가 임계를 넘으면 보간이 아니라 계단으로 다룬다 —
	// 보간하면 개체가 한 칸을 걷는 동안 비스듬히 떠올라 공중을 걷는 것처럼 보인다.
	if (CliffStepCm > 0.0)
	{
		double MinCornerZ = 0.0;
		double MaxCornerZ = 0.0;
		int32 CornerCount = 0;

		for (const int32 Index : { I00, I10, I01, I11 })
		{
			if (!IsGroundCandidate(Index))
			{
				continue;
			}

			const double H = Heights[Index];
			if (CornerCount == 0)
			{
				MinCornerZ = H;
				MaxCornerZ = H;
			}
			else
			{
				MinCornerZ = FMath::Min(MinCornerZ, H);
				MaxCornerZ = FMath::Max(MaxCornerZ, H);
			}
			++CornerCount;
		}

		if (CornerCount > 0 && (MaxCornerZ - MinCornerZ) > CliffStepCm)
		{
			const int32 NearIx = (Tx < 0.5) ? Ix : Ix + 1;
			const int32 NearIy = (Ty < 0.5) ? Iy : Iy + 1;
			const int32 NearIndex = NearIy * NumX + NearIx;

			if (IsGroundCandidate(NearIndex))
			{
				OutHeightCm = Heights[NearIndex];
				return true;
			}

			OutHeightCm = MaxCornerZ;
			return true;
		}
	}

	double WeightSum = 0.0;
	double HeightSum = 0.0;

	auto Accumulate = [&](int32 Index, double Weight)
	{
		if (Weight > 0.0 && IsGroundCandidate(Index))
		{
			HeightSum += Heights[Index] * Weight;
			WeightSum += Weight;
		}
	};

	Accumulate(I00, W00);
	Accumulate(I10, W10);
	Accumulate(I01, W01);
	Accumulate(I11, W11);

	if (WeightSum <= 0.0)
	{
		int32 ValidCount = 0;
		double Sum = 0.0;
		for (const int32 Index : { I00, I10, I01, I11 })
		{
			if (IsGroundCandidate(Index))
			{
				Sum += Heights[Index];
				++ValidCount;
			}
		}

		if (ValidCount == 0)
		{
			return false;
		}

		OutHeightCm = Sum / static_cast<double>(ValidCount);
		return true;
	}

	OutHeightCm = HeightSum / WeightSum;
	return true;
}
