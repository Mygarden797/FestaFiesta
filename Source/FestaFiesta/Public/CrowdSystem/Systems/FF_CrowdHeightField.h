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
		int32 BoundaryProbeCount = 0; // Additional sampler calls, including rejected attempts.
		int32 BoundaryCellCount = 0;
		int32 UnknownBoundaryCellCount = 0;

		double HeightSpanCm() const { return MaxHeightCm - MinHeightCm; }
	};

	bool Build(const FVector2D& CenterCm, double HalfSizeCm, double SpacingCm, FHeightSampler Sampler);

	// Opt-in for small edge grids only. A discontinuity stores two height classes
	// and a sampled boundary instead of interpolating a fictitious ramp between
	// them. Unsupported/missing geometry and exhausted budgets remain unknown.
	// The returned count includes every additional sampler attempt. A straight
	// local boundary is validated at independent interior points; features hidden
	// between all samples are outside this representation's spatial contract.
	int32 RefineDiscontinuities(double MinimumHeightGapCm, double SpatialToleranceCm,
		int32 MaximumAdditionalProbes, FHeightSampler Sampler);
	// Compact rejection marker when a caller cannot afford an edge subgrid.
	// Its bitset is bounded by the existing grid, with no additional sampling.
	void MarkCellUnknown(const FVector2D& WorldXYCm);

	void Reset();

	bool IsValid() const { return NumX >= 2 && NumY >= 2 && Heights.Num() == NumX * NumY && Stats.HitCount > 0; }

	// 통행 상한 위 표본은 높이 후보에서도 제외한다. 셀에 후보가 없으면 false다.
	bool TryGetHeightCm(const FVector2D& WorldXYCm, double& OutHeightCm) const;

	// 네 꼭짓점의 높이 차가 이 값을 넘으면 절벽으로 보고 보간 대신 가장 가까운 꼭짓점 값을 쓴다.
	// 0이면 항상 보간한다.
	//
	// 보간만 쓰면 개체가 절벽을 지날 때 격자 한 칸(SpacingCm)을 걷는 동안 비스듬히 떠오른다.
	// 간격이 넓을수록 그 경사가 길어져 공중을 걷는 것처럼 보인다. 계단은 계단으로 올라야 한다.
	void SetCliffStepCm(double InCliffStepCm) { CliffStepCm = FMath::Max(InCliffStepCm, 0.0); }

	// 지면 Z가 이 값을 넘는 XY를 통행 불가로 본다. 세우지 않으면 전부 통행 가능이다.
	//
	// 조회 시점에만 쓰이는 값이라 격자를 다시 굽지 않아도 바뀐다 — CliffStepCm과 같은 규약이다.
	void SetWalkableCeilingCm(double InCeilingCm) { WalkableCeilingCm = InCeilingCm; bHasWalkableCeiling = true; }

	void ClearWalkableCeiling() { bHasWalkableCeiling = false; }

	// 지면을 못 찾은 꼭짓점도 통행 불가로 볼지. 실제 구멍이면 막는 것이 맞지만 트레이스가
	// 놓친 지형이면 없던 벽이 생긴다. 그래서 기본은 통과다.
	void SetBlockUnknownGround(bool bInBlockUnknownGround) { bBlockUnknownGround = bInBlockUnknownGround; }

	// 이 XY를 밟을 수 있는가.
	//
	// 상한은 칸의 가장 낮은 꼭짓점으로 판정한다. 네 꼭짓점이 전부 상한을 넘어야 높은 지형이다.
	// 최댓값으로 보면 나무·화분처럼 꼭짓점 하나에만 얹힌 물체의 높이가 칸 전체(SpacingCm 제곱)를
	// 통행 불가로 만들어, 바닥이 상한보다 한참 낮은 길 위의 개체가 줄지어 멈춘다. 가장 가까운
	// 꼭짓점으로 보는 것도 안 된다 — 물체 곁 2.5m 안은 여전히 막힌다.
	// 대신 높은 지형의 통행 경계는 한 칸(SpacingCm) 늦게 선다. 경계를 조이려면 SpacingCm을 줄인다.
	//
	// 구멍 처리는 bBlockUnknownGround를 따른다. 켜면 칸의 네 꼭짓점 중 하나라도 지면이 없을 때 막는다.
	//
	// 상한이 없거나 격자가 없으면 true다 — 높이 격자를 끈 맵에서 전원이 못 움직이는 일을 막는다.
	bool IsWalkable(const FVector2D& WorldXYCm) const;

	struct FCellProbe
	{
		double CornerHeightCm[4] = { 0.0, 0.0, 0.0, 0.0 };	// 좌하, 우하, 좌상, 우상
		bool bCornerValid[4] = { false, false, false, false };
		FVector2D CornerXYCm[4] = { FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector };
		bool bHasDiscontinuity = false;
		bool bBoundaryResolved = false;
		bool bQueryOnHighSide = false;
		uint8 HighCornerMask = 0;
		FVector2D BoundaryStartCm = FVector2D::ZeroVector;
		FVector2D BoundaryEndCm = FVector2D::ZeroVector;
		double BoundaryToleranceCm = 0.0;
	};

	// 진단용. IsWalkable이 보는 네 꼭짓점의 좌표·높이·유효 여부를 그대로 낸다. 격자가 없으면 false.
	bool ProbeCell(const FVector2D& WorldXYCm, FCellProbe& OutProbe) const;

	const FBuildStats& GetStats() const { return Stats; }

	int32 GetNumX() const { return NumX; }
	int32 GetNumY() const { return NumY; }
	SIZE_T GetAllocatedSizeBytes() const { return Heights.GetAllocatedSize() + Valid.GetAllocatedSize() + Discontinuities.GetAllocatedSize() + UnknownCells.GetAllocatedSize(); }

private:
	// WorldXY가 속한 셀의 좌하단 꼭짓점 인덱스와 셀 안의 보간 계수를 낸다. 격자가 없으면 false.
	// 높이 조회와 통행 판정이 같은 셀을 보게 하려고 한 곳에 모았다 — 두 곳에서 따로 계산하면
	// 클램프 한 줄이 어긋나는 것만으로 '밟을 수 있다고 판정한 칸의 높이'가 달라진다.
	bool TryLocateCell(const FVector2D& WorldXYCm, int32& OutIx, int32& OutIy,
	                   double& OutTx, double& OutTy) const;

	// 행 우선(Y * NumX + X). 꼭짓점 값이며 셀 값이 아니다
	TArray<double> Heights;

	// 지면을 못 찾은 꼭짓점 표시. NaN을 센티넬로 쓰지 않는 이유는 NaN이 보간에 섞이면 결과가 조용히 NaN이 되어 개체가 사라지기 때문이다
	TArray<uint8> Valid;

	struct FDiscontinuityCell
	{
		FVector2D StartCm = FVector2D::ZeroVector;
		FVector2D EndCm = FVector2D::ZeroVector;
		FVector2D HighNormal = FVector2D::ZeroVector;
		double ToleranceCm = 0.0;
		uint8 HighCornerMask = 0;
		bool bResolved = false;
	};
	// Keys use the lower-left vertex index. Empty for ordinary height fields.
	TMap<int32, FDiscontinuityCell> Discontinuities;
	TBitArray<> UnknownCells;

	FVector2D MinCornerCm = FVector2D::ZeroVector;
	double SpacingCm = 0.0;
	double CliffStepCm = 0.0;
	double WalkableCeilingCm = 0.0;
	bool bHasWalkableCeiling = false;
	bool bBlockUnknownGround = false;
	int32 NumX = 0;
	int32 NumY = 0;

	FBuildStats Stats;
};
