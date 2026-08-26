// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 개체 하나가 따라가는 코스 경로.
 *
 * ── 왜 필요한가 ────────────────────────────────────────────────────────────────
 * 사회력의 벽 반발은 **국소적인 힘**이다. 목적지가 부스 반대편이면 구동력과 벽 반발이
 * 평형을 이루는 지점에서 사람이 벽에 붙어 접선 방향으로 미끄러질 뿐, 건물 뒤로 돌아가지
 * 않는다. 오목한 형상이나 부스 두 개 사이 모퉁이에서는 아예 갇힌다.
 * 계수를 아무리 만져도 이건 안 없어진다 — 우회는 힘이 아니라 경로의 문제다.
 *
 * 그래서 표준 구조인 2계층으로 간다.
 *   전역 경로 (여기)          : NavMesh가 뽑은 코너 목록. 건물을 돌아가는 큰 그림
 *   국소 회피 (FF_CrowdForces): 사람-사람 간격과 벽 여유. 웨이포인트 사이의 미세 조정
 *
 * ── 왜 FUnitData에 안 넣었는가 ──────────────────────────────────────────────────
 * TArray는 24바이트 + 힙이다. 5천 개체 전원에게 달면 FUnitData가 커져 회피 미러 순회의
 * 캐시 지역성이 나빠진다. 경로를 드는 것은 파티 리더뿐(전체의 1/N)이므로 병렬 배열로 뺀다.
 * 미러 배열(ScratchPos2DMeters 등)과 같은 규약이다 — 인덱스는 AgentIndex와 같다.
 *
 * Points는 시작점(현재 위치)을 뺀 남은 코스 지점이고, 마지막 원소는 최종 목적지다.
 * NextIndex가 Points.Num()이면 전부 소비했다는 뜻이다.
 * PlannedForTarget은 이 경로를 뽑을 때의 목적지이고, 지금 목적지와 어긋나면 경로가 낡았다.
 * bQueryFailed는 쿼리 실패(또는 목적지가 코앞이라 뽑을 필요가 없음)를 기억해 매 스텝
 * 되묻는 것을 막는다 — 목적지가 바뀌면 Reset되므로 영구 포기가 아니다.
 */
struct FFF_CrowdPathFollower
{
	TArray<FVector> Points;

	int32 NextIndex = 0;

	FVector PlannedForTarget = FVector::ZeroVector;

	bool bHasPath = false;

	bool bQueryFailed = false;

	void Reset()
	{
		Points.Reset();
		NextIndex = 0;
		PlannedForTarget = FVector::ZeroVector;
		bHasPath = false;
		bQueryFailed = false;
	}

	bool HasRemainingWaypoint() const { return bHasPath && Points.IsValidIndex(NextIndex); }
};

namespace CrowdPathFollow
{
	FESTAFIESTA_API int32 AdvanceToReachableWaypoint(const TArray<FVector>& Points, int32 NextIndex,
	                                                 const FVector& Position, float AcceptanceRadiusUU);
}
