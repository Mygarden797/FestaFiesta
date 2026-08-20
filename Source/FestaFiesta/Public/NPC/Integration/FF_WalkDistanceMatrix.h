// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 존 앵커 사이의 **보행거리**(cm) 캐시. 결정 경로는 이 행렬만 읽는다.
 *
 * ── 왜 행렬인가 (재평가마다 경로탐색을 하지 않는 이유) ──
 *
 * 목적지 재평가는 파티 수 × 후보 수만큼 돈다(500명·후보 12개면 초당 수천 회다).
 * 거기서 NavMesh 질의를 하면 프레임 예산이 통째로 날아가고, 무엇보다
 * **결정론이 깨진다** — `FindPathAsync`의 완료 순서는 프레임률에 의존하고,
 * 경로가 달라지면 위치가 달라지고, 위치가 달라지면 다음 재평가의 거리가 달라져
 * 결정 자체가 갈린다. 같은 시드로 두 번 돌려도 다른 결과가 나온다.
 *
 * 그래서 존 앵커 사이만 미리 한 번 재서 캐시한다. 존 수 N은 한 자리 수라
 * N^2 질의는 배치가 바뀔 때 한 번이면 충분하다.
 *
 * ── 거리 근사 ──
 *
 *   d(파티, 시설) ≈ |파티 → 출발존 앵커| + M[출발존][도착존] + |도착존 앵커 → 시설|
 *                     (직선)              (보행거리)            (직선)
 *
 * 같은 존 안이면 앵커를 경유하지 않고 순수 직선거리를 쓴다 — 존 내부는 대개 열린 공간이고,
 * 앵커를 경유시키면 옆 부스가 앵커까지 돌아가는 거리로 잡혀 오히려 틀린다.
 *
 * 이 근사가 직선거리보다 나은 점은 **강·무대·펜스 건너편이 멀게 나온다**는 것뿐이다.
 * 존 내부 정확도는 직선거리와 같다. 그 이상이 필요하면 시설별 행렬로 올려야 하고,
 * 그때는 N이 시설 수가 되므로 비용 구조가 달라진다.
 *
 * ── 경로 길이 제공자를 주입받는 이유 ──
 *
 * NavMesh를 직접 부르지 않는다. 그러면 이 클래스가 UWorld와 NavigationSystem에 묶여
 * 자동화 테스트에서 월드를 띄워야 하고, "강 건너편이 멀게 나오는가"를 검증하려면
 * 테스트용 NavMesh를 구워야 한다. 제공자를 함수로 받으면 테스트가 합성 지형을 줄 수 있다.
 */
struct FESTAFIESTA_API FFF_WalkDistanceMatrix
{
	/**
	 * 두 월드 지점 사이의 보행거리(cm)를 재는 함수.
	 *
	 * @return false = 경로가 없다(연결이 끊겼거나 NavMesh 밖). 그 쌍은 행렬에 기록되지 않는다
	 */
	using FPathLengthProvider = TFunctionRef<bool(const FVector& FromCm, const FVector& ToCm, float& OutLengthCm)>;

	/** 존 하나의 대표 지점과 인접 정보. 앵커는 **월드** cm다(읽기 모델의 로컬 앵커를 변환해 넣는다). */
	struct FZoneAnchor
	{
		FName ZoneId = NAME_None;
		FVector WorldLocationCm = FVector::ZeroVector;

		// 존 인접 그래프. 여기 있는데 경로가 없으면 불일치로 보고한다(§Mismatches).
		TArray<FName> NeighborZoneIds;
	};

	/** 빌드 결과 보고. 조용히 실패하면 "거리가 좀 이상한" 상태로 끝나므로 반드시 로그로 낼 것. */
	struct FBuildStats
	{
		int32 ZoneCount = 0;
		int32 QueryCount = 0;
		int32 FailedPairs = 0;

		/**
		 * 존 인접 그래프에는 이웃인데 NavMesh로는 경로가 없는 쌍.
		 *
		 * 이게 비어 있지 않다는 것은 **배치와 지형이 어긋났다**는 뜻이다 —
		 * 존 그래프는 후보 필터링에 쓰이므로(인접 존 밖은 후보가 못 된다),
		 * 여기 걸린 쌍이 있으면 "후보에는 들어오는데 걸어갈 수 없는 시설"이 생긴다.
		 */
		TArray<FString> Mismatches;
	};

	/**
	 * 앵커 목록으로 행렬을 짓는다. 기존 내용은 버린다.
	 *
	 * ★ 존 순서는 ZoneId 사전순으로 정렬해 인덱스를 부여한다 — 등록 순서가 결과에 새어 들어가지 않게.
	 *   같은 배치에서 두 번 지으면 비트 단위로 같은 행렬이 나와야 한다.
	 *
	 * ★ 비대칭을 유지한다(A→B와 B→A를 따로 잰다). 대칭화하면 계단·일방 통행이 생기는 날
	 *   조용히 거짓이 된다. 질의 수는 N(N-1)이고 N이 작아 문제되지 않는다.
	 */
	void Build(const TArray<FZoneAnchor>& Anchors, FPathLengthProvider PathLength, FBuildStats* OutStats = nullptr);

	void Reset();

	// 존이 2개 이상 등록됐는가. 1개 이하면 행렬이 할 일이 없다(전부 같은 존 = 직선거리)
	bool IsValid() const { return ZoneIds.Num() >= 2; }

	int32 GetZoneCount() const { return ZoneIds.Num(); }

	/**
	 * 출발존 → 도착존 보행거리(cm).
	 *
	 * @return false = 등록되지 않은 존이거나 경로가 없다. **호출자는 직선거리로 폴백해야 한다** —
	 *         여기서 큰 값을 돌려주면 "아주 먼 시설"로 취급되어 후보에서 조용히 사라진다.
	 *         폴백은 틀리지만 눈에 보이고, 조용한 배제는 틀린데 안 보인다.
	 */
	bool TryGetWalkDistanceCm(FName FromZoneId, FName ToZoneId, float& OutLengthCm) const;

	// 존 앵커의 월드 위치. 거리 합성의 마지막 구간(앵커 → 시설)에 쓴다
	bool TryGetZoneAnchorCm(FName ZoneId, FVector& OutWorldLocationCm) const;

private:
	// ZoneId 사전순. 인덱스가 행렬의 행/열이다
	TArray<FName> ZoneIds;
	TArray<FVector> AnchorsCm;

	// 크기 N*N. 인덱스 [From * N + To]. 경로가 없는 쌍은 음수로 남긴다
	TArray<float> DistancesCm;

	static constexpr float NoPath = -1.f;

	int32 IndexOfZone(FName ZoneId) const;
};
