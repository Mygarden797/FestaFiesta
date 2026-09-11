// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CrowdSystem/Systems/FF_CrowdSplinePath.h"

// 인게임에 깔린 도로 타일에서 순회 레인을 굽는다.
//
// 저작 레인(AFF_CrowdLane)은 스플라인 하나가 곧 레인 하나지만, 건설로 놓인 도로는 4m 타일이
// 격자로 흩어져 있을 뿐 "길"이라는 단위가 없다. 여기서 타일을 인접 그래프로 묶고 교차점에서
// 잘라 세그먼트 하나당 레인 하나를 만든다.
//
// ★ 교차점에서 자르는 이유는 TryBuildPath가 레인을 하나만 쓰기 때문이다. 세그먼트 끝에 도착한
//   개체가 그 지점에서 레인을 다시 고르면 교차로에서 갈림길을 한 번 뽑는 것과 같아진다.
//   종단점 쌍마다 관통 경로를 미리 이어 구우면 레인 수가 종단점 수의 제곱으로 늘어난다.
//
// ★ 월드를 모르는 순수 함수다. 타일 목록만 주면 같은 입력에 항상 같은 레인 배열이 나온다.
//   레인 인덱스가 배회 추첨에 들어가므로 순서가 흔들리면 결정론이 그대로 깨진다 —
//   그래서 입력을 좌표로 정렬한 뒤 시작한다. 호출부의 시설 순회 순서에 기대지 않는다.

struct FFF_CrowdRoadTile
{
	FVector CenterCm = FVector::ZeroVector;
	FVector2D HalfExtentCm = FVector2D::ZeroVector;
	float YawDegrees = 0.f;
};

struct FFF_CrowdRoadLaneBuildParams
{
	// 레인 반폭. 타일 반치수와 같게 두면 도로 폭이 곧 레인 폭이 된다.
	float HalfWidthCm = 200.f;

	// 직선 구간의 중간 정점을 이 오차 안에서 버린다. 격자 경로는 90도로만 꺾이므로
	// 실제로는 완전 직선 구간만 접힌다.
	float SimplifyToleranceCm = 50.f;

	// 두 타일의 높이차가 이보다 크면 잇지 않는다. 단차를 사이에 둔 도로가 한 레인으로
	// 묶이면 개체가 절벽을 가로지르는 목적지를 받는다.
	float MaxStepCm = 60.f;

	// 레인 수 상한. 넘으면 잘라내고 bTruncated를 세운다.
	int32 MaxLanes = 256;
};

struct FFF_CrowdRoadLaneBuildStats
{
	int32 TileCount = 0;
	int32 EdgeCount = 0;
	int32 JunctionCount = 0;
	int32 IsolatedTileCount = 0;
	int32 ClosedLoopCount = 0;
	bool bTruncated = false;
};

namespace CrowdRoadLanes
{
	// 타일 → 레인. OutLanes는 항상 먼저 비운다.
	FESTAFIESTA_API FFF_CrowdRoadLaneBuildStats BuildLanes(
		const TArray<FFF_CrowdRoadTile>& Tiles,
		const FFF_CrowdRoadLaneBuildParams& Params,
		TArray<FFF_CrowdSplineLane>& OutLanes);
}
