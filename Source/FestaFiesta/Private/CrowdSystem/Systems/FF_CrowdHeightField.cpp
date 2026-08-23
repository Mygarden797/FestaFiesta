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

bool FFF_CrowdHeightField::TryGetHeightCm(const FVector2D& WorldXYCm, double& OutHeightCm) const
{
	if (!IsValid())
	{
		return false;
	}

	const double Fx = FMath::Clamp((WorldXYCm.X - MinCornerCm.X) / SpacingCm, 0.0, static_cast<double>(NumX - 1));
	const double Fy = FMath::Clamp((WorldXYCm.Y - MinCornerCm.Y) / SpacingCm, 0.0, static_cast<double>(NumY - 1));

	const int32 Ix = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Fx)), 0, NumX - 2);
	const int32 Iy = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(Fy)), 0, NumY - 2);

	const double Tx = FMath::Clamp(Fx - static_cast<double>(Ix), 0.0, 1.0);
	const double Ty = FMath::Clamp(Fy - static_cast<double>(Iy), 0.0, 1.0);

	const int32 I00 = Iy * NumX + Ix;
	const int32 I10 = I00 + 1;
	const int32 I01 = I00 + NumX;
	const int32 I11 = I01 + 1;

	const double W00 = (1.0 - Tx) * (1.0 - Ty);
	const double W10 = Tx * (1.0 - Ty);
	const double W01 = (1.0 - Tx) * Ty;
	const double W11 = Tx * Ty;

	double WeightSum = 0.0;
	double HeightSum = 0.0;

	auto Accumulate = [&](int32 Index, double Weight)
	{
		if (Weight > 0.0 && Valid.IsValidIndex(Index) && Valid[Index] != 0)
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
			if (Valid.IsValidIndex(Index) && Valid[Index] != 0)
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
