// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * 균일 격자 브로드페이즈. 좌표는 미터(FVector2f), 2D다.
 *
 * ── 무엇을 푸는가 ──
 *
 * "반경 2m 안의 이웃"을 5,000명 전원에 대해 구해야 한다. 전수 비교는 5,000^2 = 2,500만이다.
 * 격자로 나누면 각자 자기 셀 주변 3x3만 보면 되므로 개체당 상수 시간에 가까워진다.
 *
 * ── 왜 카운팅 정렬인가 ──
 *
 * TMap<셀좌표, TArray<인덱스>>는 매 스텝 해시 + 동적 할당이 5,000번 일어난다.
 * 카운팅 정렬은 두 번의 선형 순회로 끝나고, 정상 상태에서 추가 할당이 0이다.
 *
 *   1) 각 셀의 개체 수를 센다               -> CellCounts
 *   2) 누적합으로 각 셀의 시작 오프셋을 만든다 -> CellStart
 *   3) 다시 순회하며 CellItems에 인덱스를 흘려 넣는다
 *
 * ── 왜 UnitDatas를 직접 받지 않는가 ──
 *
 * 인자가 TArray<FVector2f>인 덕분에 이 구조체가 FUnitData·Hot/Cold 레이아웃·UU/m 규약을
 * 전혀 모른다. 호출자(UFF_CrowdSimulationSubsystem)가 미러 배열을 만들어 넘긴다.
 * 그래서 나중에 Queue·Throughput 집계가 같은 격자를 재사용할 수 있다.
 *
 * ── 이 클래스가 UObject가 아닌 이유 ──
 *
 * 매 스텝 재구축되는 순수 계산 구조이고 GC 추적 대상이 없다.
 */
struct FESTAFIESTA_API FFF_CrowdSpatialGrid
{
	/**
	 * 격자를 재구축한다.
	 *
	 * [중요] Active 필터링은 여기서 한다. GatherNearest 시점에 거르면
	 * 죽은 에이전트의 낡은 위치가 "가장 가까운 N개" 자리를 차지해
	 * 살아있는 개체의 반발력이 조용히 약해진다.
	 *
	 * @param Pos        위치 미러 배열(m). 인덱스는 AgentIndex와 같아야 한다
	 * @param Active     활성 플래그 미러 배열. Pos와 같은 인덱스 공간
	 * @param ActiveIds  활성 인덱스 조밀 배열. 풀 전수 순회를 피하기 위한 것
	 * @param CellSize   셀 한 변(m). 반드시 이웃 반경 이상
	 */
	void Build(const TArray<FVector2f>& Pos, const TArray<uint8>& Active,
	           const TArray<int32>& ActiveIds, float CellSize);

	/**
	 * P 주변 Radius 안의 이웃을 최대 MaxOut개 모은다. 자기 자신도 포함되므로 호출자가 건너뛴다.
	 *
	 * [중요] 결과는 반드시 '인덱스 오름차순으로 정렬해' 반환한다.
	 * 같은 이웃 집합이라도 힘을 더하는 순서가 다르면 부동소수 합산 결과의
	 * 마지막 비트가 달라지고, 그 차이가 60초 뒤엔 눈에 보이는 위치 차이가 된다.
	 *
	 * MaxOut을 넘으면 가까운 것부터 자른다 — 먼 이웃의 기여는 exp 감쇠로
	 * 이미 무시할 만하므로 정확도 손실이 거의 없다.
	 *
	 * @param OutIndices 호출자가 재사용하는 버퍼. 내부에서 Reset만 하고 할당하지 않는다
	 */
	void GatherNearest(const TArray<FVector2f>& Pos, const FVector2f& P,
	                   float Radius, int32 MaxOut, TArray<int32>& OutIndices) const;

	/**
	 * 막힌 개체의 탈출용 — 인접 셀 중 가장 덜 붐비는 곳을 고른다.
	 * 처음부터 넣는다. 나중에 붙이면 "가끔 안 움직이는 NPC"를 재현하느라 하루를 쓴다.
	 */
	bool FindLeastCrowdedNeighbourCell(const FVector2f& P, FVector2f& OutCellCenter, int32* OutTieCount = nullptr) const;

	// 격자가 유효한지. 무효 상태로 진행하면 음수 인덱스 접근이 난다 — 진입점에서 차단한다.
	bool IsValid() const { return CellSizeMeters > 0.f && GridDimX > 0 && GridDimY > 0; }

	// 셀 좌표 <-> 월드(m).
	void WorldToCell(const FVector2f& P, int32& OutX, int32& OutY) const;
	int32 CellIndex(int32 X, int32 Y) const;

	// --- 진단용 조회 ---------------------------------------------------------------------
	// P1(격자만 이식, 힘은 0) 단계에서 "격자가 실제로 이웃을 찾는가"를 확인하는 데 쓴다.

	int32 GetNumCells() const { return GridDimX * GridDimY; }
	int32 GetDimX() const { return GridDimX; }
	int32 GetDimY() const { return GridDimY; }
	float GetCellSizeMeters() const { return CellSizeMeters; }

	// 격자에 실제로 들어간 개체 수. Build에 넘긴 ActiveIds 중 유효했던 것들이다.
	int32 GetNumItems() const { return CellItems.Num(); }

private:
	// 각 셀의 CellItems 내 시작 오프셋. 길이는 셀 수 + 1(마지막은 총 개수).
	TArray<int32> CellStart;

	// 셀 순으로 정렬된 에이전트 인덱스.
	TArray<int32> CellItems;

	// 셀 카운팅 임시 버퍼. 매 스텝 재사용하여 할당을 피한다.
	TArray<int32> CellCounts;

	// 격자 원점(m). 음수 좌표를 다루기 위해 최소 경계를 원점으로 잡는다.
	FVector2f GridOrigin = FVector2f::ZeroVector;

	float CellSizeMeters = 0.f;
	int32 GridDimX = 0;
	int32 GridDimY = 0;
};
