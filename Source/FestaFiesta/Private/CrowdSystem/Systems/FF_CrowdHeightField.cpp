// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Systems/FF_CrowdHeightField.h"

void FFF_CrowdHeightField::Reset()
{
	Heights.Reset();
	Valid.Reset();
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
