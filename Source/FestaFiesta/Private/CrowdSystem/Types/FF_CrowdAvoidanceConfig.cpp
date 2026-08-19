// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"

bool FFF_CrowdAvoidanceConfig::Validate(FString& OutError) const
{
	if (NeighbourRadiusMeters <= 0.f)
	{
		OutError = TEXT("NeighbourRadiusMeters가 0 이하다");
		return false;
	}

	// 셀이 이웃 반경보다 작으면 3x3 셀 밖의 이웃을 놓친다. 크래시가 아니라
	// "회피가 좀 약한" 상태로 조용히 끝나서 나중에 원인을 찾기 어렵다.
	if (BroadphaseCellSizeMeters < NeighbourRadiusMeters)
	{
		OutError = FString::Printf(
			TEXT("BroadphaseCellSizeMeters(%.2fm) < NeighbourRadiusMeters(%.2fm) — 3x3 셀 밖의 이웃을 놓친다"),
			BroadphaseCellSizeMeters, NeighbourRadiusMeters);
		return false;
	}

	if (MaxNeighbours <= 0)
	{
		OutError = TEXT("MaxNeighbours가 0 이하다 — 이웃을 하나도 보지 않으므로 회피가 없는 것과 같다");
		return false;
	}

	if (RepulsionB <= 0.f || WallRepulsionB <= 0.f)
	{
		OutError = TEXT("RepulsionB / WallRepulsionB가 0 이하다 — exp 인자가 발산한다");
		return false;
	}

	if (MaxExponentArg <= 0.f || MaxAccelerationMps2 <= 0.f)
	{
		OutError = TEXT("MaxExponentArg / MaxAccelerationMps2가 0 이하다 — 안정성 클램프를 끄면 안 된다");
		return false;
	}

	return true;
}
