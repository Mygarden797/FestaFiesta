// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// 지형 높이 격자. XY로 지면 Z(cm)를 조회한다.
//
// 시뮬이 2D라(Velocity.Z = 0) 개체 Z는 스폰 값에서 변하지 않는다. Landscape가 조각된
// 맵에서는 전원이 한 높이의 평면 위를 걷게 되므로, 격자를 한 번 굽고 조회해 지면에 올린다.
//
// 매 스텝 트레이스를 하지 않는 이유는 비용(500명 x 30Hz = 초당 15,000회)과 결정론이다 —
// 트레이스가 액터 로드 상태에 의존하면 같은 시드로 두 번 돌려도 결과가 갈린다.
// 지형은 배치와 무관하게 정적이라 무효화 신호도 필요 없다.
//
// 샘플러를 주입받으므로 이 구조체는 UWorld도 NavigationSystem도 모른다 —
// 테스트가 합성 지형을 수식으로 줄 수 있다.
//
// 전제: XY 하나에 걸을 수 있는 Z가 하나뿐이다(위아래가 겹치는 구조가 없는 맵).
// 그 전제에서는 도착 판정·밀도 격자를 2D로 두는 것이 옳다. 다층이 생기면 전제부터 다시 본다.
struct FESTAFIESTA_API FFF_CrowdHeightField
{
	// XY(cm)에서 지면 Z(cm)를 얻는다. 지면이 없으면 false.
	using FHeightSampler = TFunctionRef<bool(const FVector2D& WorldXYCm, double& OutHeightCm)>;

	struct FBuildStats
	{
		int32 SampleCount = 0;		// 격자 꼭짓점 수
		int32 HitCount = 0;			// 지면을 찾은 수
		double MinHeightCm = 0.0;
		double MaxHeightCm = 0.0;

		double HeightSpanCm() const { return MaxHeightCm - MinHeightCm; }
	};

	bool Build(const FVector2D& CenterCm, double HalfSizeCm, double SpacingCm, FHeightSampler Sampler);

	void Reset();

	bool IsValid() const { return NumX >= 2 && NumY >= 2 && Heights.Num() == NumX * NumY && Stats.HitCount > 0; }

	bool TryGetHeightCm(const FVector2D& WorldXYCm, double& OutHeightCm) const;

	// 네 꼭짓점의 높이 차가 이 값을 넘으면 절벽으로 보고 보간 대신 가장 가까운 꼭짓점 값을 쓴다.
	// 0이면 항상 보간한다.
	//
	// 보간만 쓰면 개체가 절벽을 지날 때 격자 한 칸(SpacingCm)을 걷는 동안 비스듬히 떠오른다.
	// 간격이 넓을수록 그 경사가 길어져 공중을 걷는 것처럼 보인다. 계단은 계단으로 올라야 한다.
	void SetCliffStepCm(double InCliffStepCm) { CliffStepCm = FMath::Max(InCliffStepCm, 0.0); }

	const FBuildStats& GetStats() const { return Stats; }

	int32 GetNumX() const { return NumX; }
	int32 GetNumY() const { return NumY; }

private:
	// 행 우선(Y * NumX + X). 꼭짓점 값이며 셀 값이 아니다
	TArray<double> Heights;

	// 지면을 못 찾은 꼭짓점 표시. NaN을 센티넬로 쓰지 않는 이유는 NaN이 보간에 섞이면 결과가 조용히 NaN이 되어 개체가 사라지기 때문이다
	TArray<uint8> Valid;

	FVector2D MinCornerCm = FVector2D::ZeroVector;
	double SpacingCm = 0.0;
	double CliffStepCm = 0.0;
	int32 NumX = 0;
	int32 NumY = 0;

	FBuildStats Stats;
};
