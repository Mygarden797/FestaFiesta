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

	// 벽 컷오프는 "반발이 무시할 만해지는 거리"여야 한다. WallRepulsionB를 올려 놓고
	// 컷오프를 그대로 두면 그 경계에서 반발이 뚝 끊겨 사람이 벽 쪽으로 튄다 —
	// 계수를 아무리 봐도 원인이 안 보이는 종류의 버그라 여기서 잡는다.
	// (여기 오는 시점에 WallRepulsionB > 0, MaxExponentArg > 0 은 위에서 보장된다)
	if (WallCullRadiusMeters > 0.f)
	{
		// 70kg 기준 0.07 m/s^2. 구동력(3.0)의 2%면 경계에서 눈에 안 띈다.
		constexpr float MaxDiscardedForceN = 5.f;
		constexpr float ReferenceMassKg = 70.f;

		const float ArgAtCutoff = FMath::Min(WallCullRadiusMeters / WallRepulsionB, MaxExponentArg);
		const float DiscardedForceN = WallRepulsionA * FMath::Exp(-ArgAtCutoff);

		if (DiscardedForceN > MaxDiscardedForceN)
		{
			OutError = FString::Printf(
				TEXT("WallCullRadiusMeters(%.2fm)가 WallRepulsionB(%.2fm)에 비해 짧다 — ")
				TEXT("컷오프에서 %.1fN(약 %.2f m/s^2)을 버린다. 컷오프를 늘리거나 B를 줄일 것"),
				WallCullRadiusMeters, WallRepulsionB,
				DiscardedForceN, DiscardedForceN / ReferenceMassKg);
			return false;
		}
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
