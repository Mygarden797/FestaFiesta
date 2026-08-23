// Fill out your copyright notice in the Description page of Project Settings.

#include "CrowdSystem/Types/FF_CrowdAvoidanceConfig.h"

bool FFF_CrowdAvoidanceConfig::Validate(FString& OutError) const
{
	if (NeighbourRadiusMeters <= 0.f)
	{
		OutError = TEXT("NeighbourRadiusMeters가 0 이하다");
		return false;
	}

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

	if (MaxExponentArg <= 0.f || MaxAccelerationMps2 <= 0.f || MaxSocialAccelMps2 <= 0.f)
	{
		OutError = TEXT("MaxExponentArg / MaxAccelerationMps2 / MaxSocialAccelMps2가 0 이하다 — 안정성 클램프를 끄면 안 된다");
		return false;
	}

	if (MaxSocialAccelMps2 > MaxAccelerationMps2)
	{
		OutError = FString::Printf(
			TEXT("MaxSocialAccelMps2(%.1f) > MaxAccelerationMps2(%.1f) — 비접촉 상한이 전체 상한보다 크면 분리 클램프가 무의미하다"),
			MaxSocialAccelMps2, MaxAccelerationMps2);
		return false;
	}

	if (MaxWallOpposingAccelMps2 >= 0.f && MaxOpposingAccelMps2 >= 0.f
		&& MaxWallOpposingAccelMps2 < MaxOpposingAccelMps2)
	{
		OutError = FString::Printf(
			TEXT("MaxWallOpposingAccelMps2(%.2f) < MaxOpposingAccelMps2(%.2f) — 벽이 개체보다 약하면 부스를 통과한다"),
			MaxWallOpposingAccelMps2, MaxOpposingAccelMps2);
		return false;
	}

	if (LaneBiasGain < 0.f)
	{
		OutError = TEXT("LaneBiasGain이 음수다 — 마주 오는 사람 쪽으로 붙는다(왼쪽 통행이 아니라 충돌 유도다)");
		return false;
	}

	if (RelaxationTau <= 0.f)
	{
		OutError = TEXT("RelaxationTau가 0 이하다 — 구동력이 발산한다");
		return false;
	}

	return true;
}
