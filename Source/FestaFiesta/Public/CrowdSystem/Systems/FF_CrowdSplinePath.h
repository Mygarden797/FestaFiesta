// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// 베이크된 레인 하나. USplineComponent를 런타임에 쿼리하지 않고 폴리라인으로 들고 있는다.
// 닫힌 레인은 첫 점을 끝에 한 번 더 넣어 저작한다 — bClosedLoop이 그걸 보고 선다.
struct FESTAFIESTA_API FFF_CrowdSplineLane
{
	TArray<FVector> Points;
	TArray<float>   CumulativeDistCm;
	FBox            Bounds = FBox(ForceInit);
	float           HalfWidthCm = 150.f;
	bool            bOneWay = false;
	bool            bClosedLoop = false;
	bool            bBlocked = false;

	void RebuildDerived();

	bool IsUsable() const
	{
		return !bBlocked && Points.Num() >= 2 && CumulativeDistCm.Num() == Points.Num();
	}

	float TotalLengthCm() const
	{
		return CumulativeDistCm.Num() > 0 ? CumulativeDistCm.Last() : 0.f;
	}
};

namespace CrowdSplinePath
{
	struct FQueryParams
	{
		float EntrySnapRadiusCm = 2000.f;
		float ExitSnapRadiusCm  = 1500.f;
		float MaxDetourRatio    = 1.8f;
	};

	FESTAFIESTA_API bool FindClosestOnLane(const FFF_CrowdSplineLane& Lane, const FVector& QueryCm,
	                                       FVector& OutPointCm, float& OutArcCm, float& OutDist2DCm);

	// 단일 레인만 쓴다. 레인 여러 개를 잇는 것은 GetNavFallbackCount가 높게 나오면 그때 얹는다.
	FESTAFIESTA_API bool TryBuildPath(const TArray<FFF_CrowdSplineLane>& Lanes,
	                                  const FVector& StartCm, const FVector& TargetCm,
	                                  const FQueryParams& Params,
	                                  TArray<FVector>& OutPoints, int32& OutLaneIndex);
}
