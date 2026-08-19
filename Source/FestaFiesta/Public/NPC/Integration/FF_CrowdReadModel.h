// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_CrowdReadModel.generated.h"

/**
 * Crowd(이정원) → Decision(김시리) 읽기 전용 스냅샷.
 *
 * ### 범위 주의 — 시설·존·큐는 여기 없다
 * (FF_ZoneSystem 플러그인(한광훈)이 이미 FFFZSReadModel 로 시설 상태·큐 길이·존 인구·밀도 비율을 제공한다.
 * 그것을 여기에 복사하지 않는다)
 * 
 * 이 구조체는 "ZoneSystem 이 갖고 있지 않은 것만" 담는다.
 *
 * | 데이터            | 소유            | 어디서 읽나                |
 * |-------------------|-----------------|----------------------------|
 * | 시설 상태·큐 길이 | ZoneSystem(한광훈) | FFFZSFacilityReadModel   |
 * | 존 인구·밀도 비율 | ZoneSystem(한광훈) | FFFZSZoneReadModel       |
 * | 셀 격자 인/㎡     | Crowd(이정원)   | **이 구조체**              |
 * | 에이전트 위치·속도| Crowd(이정원)   | FUnitData (CrowdAI 브랜치) |
 *
 *
 * ### 왜 셀 격자를 따로 두는가
 * 
 * FFFZSZoneReadModel::DensityRatio 는 ReportedPopulation / SoftCapacity 다. 
 * SoftCapacity 는 기획자가 정한 정원 숫자이므로 면적 기반이 아니다.
 * Fruin Level of Service(인/㎡)와 직접 비교할 수 없다.
 *
 * ### 폴링 금지
 *
 * Crowd 가 매 프레임 밀도를 계산해도 결정 측은 N틱(초안 30틱)마다 갱신되는 이 사본만 읽는다.
 * 결정 측이 Crowd 내부 상태를 매 틱 들여다보는 코드를 만들지 않는다 — 병렬화가 막히고 레이스 컨디션이 생긴다.
 *
 * ### 상태 (2026-08-18)
 *
 * **채우는 쪽이 붙었다** — `UFF_CrowdSimulationSubsystem::PublishCrowdReadModel`이
 * `SnapshotIntervalSteps`(기본 30 고정 스텝, 1초)마다 발행한다. 읽는 쪽은 아직 없다.
 * 셀 크기와 좌표 원점은 아래 `FFF_CrowdMetrics`로 확정됐다. `ZoneAreaSqm`만 여전히 홀드다.
 */

/**
 * ★ 지표 셀 크기의 단일 출처.
 *
 * 이 값이 두 곳에 따로 적혀 있으면 같은 밀도를 두 이름으로 다르게 재게 된다.
 * 실제로 그런 상태였다 — `FCrowdDensityGrid`는 200UU(2m), 이 구조체는 5m 기본값이었고,
 * 둘 다 "인/㎡"라는 같은 이름으로 리포트에 나갈 예정이었다. **면적이 6.25배 다르면
 * Fruin 등급이 통째로 달라진다.** 5m 셀에서 LOS C인 구역이 2m 셀에서는 F가 될 수 있다.
 *
 * ### 왜 5m가 아니라 2m인가
 *
 *   1. 8/15~8/18에 기록한 밀도·혼잡 셀 수치가 전부 200UU(2m) 격자에서 나왔다.
 *      5m로 바꾸면 그 수치를 다시 만들어야 하고, 회피 이식의 P0 기준선도 무효가 된다.
 *   2. 회피의 이웃 반경(`NeighbourRadiusMeters = 2`)과 같은 스케일이다.
 *      집계 창이 상호작용 거리보다 크면 "밀어내기가 일어나는 구간"이 셀 하나에 뭉개진다.
 *   3. Fruin LOS는 보행자 개인 점유 면적 기준이다. 4㎡ 창은 사람 대여섯 명 규모로,
 *      병목을 셀 경계가 아니라 셀 안쪽에서 잡을 수 있는 크기다.
 *
 * 바꿔야 한다면 여기 한 곳만 고치고 **그 순간 이전 밀도 수치는 전부 폐기**해야 한다.
 */
namespace FFF_CrowdMetrics
{
	// 분석 격자 한 변(m). Fruin LOS 산출의 기준 면적은 이 값의 제곱이다.
	inline constexpr float AnalysisCellSizeMeters = 2.f;

	// 같은 값의 UU 표현. FCrowdDensityGrid::Build가 UU를 받으므로 둘 다 둔다.
	// 곱셈이 아니라 상수로 적어 둔 이유: 여기서 100을 곱하는 코드가 여러 곳에 생기면
	// 그중 하나가 빠졌을 때 조용히 다른 격자가 만들어진다.
	inline constexpr float AnalysisCellSizeUU = 200.f;
}

// 2m × 2m 격자 한 칸의 집계값(§FFF_CrowdMetrics). Heat Map · 밀도 지표 · AT-016 산점도가 전부 이 하나에서 나온다.
USTRUCT(BlueprintType)
struct FFF_CrowdCellSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 CellX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 CellY = 0;

	// 이 칸에 현재 있는 인원. 스냅샷 시점의 순간값이다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 Occupancy = 0;

	/**
	 * 직전 집계 구간 동안 이 칸에 **새로 들어온** 인원. AT-016의 유량 축.
	 *
	 * '통과'를 셀 진입 횟수로 정의한다 — 개체가 셀 경계를 넘은 순간 목적 셀에서 1 올라간다.
	 * 그래서 **같은 칸에 계속 서 있는 사람은 0이다.** 그게 요점이다:
	 * Occupancy가 높은데 Throughput이 0에 가까우면 **막힌 것**이고,
	 * 둘이 같이 높으면 **흐르는 중**이다. 밀도만으로는 이 둘을 구분할 수 없다.
	 *
	 * 스냅샷을 발행할 때마다 0으로 되돌린다(구간 누적, 누적 총량이 아니다).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 Throughput = 0;

	// 인/㎡ — Fruin 판정과 직접 비교 가능한 실제 단위.
	// 2 주의 / 4 위험 / 5 심각. Heat Map 색상 스케일도 이 구간에 매핑한다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	float DensityPerSqm = 0.f;
};

USTRUCT(BlueprintType)
struct FFF_CrowdReadModel
{
	GENERATED_BODY()

	/**
	 * 셀 크기(m). 발행 시점에 **격자에서 읽어 채운다** — 기본값을 믿고 쓰지 말 것.
	 *
	 * 소비자가 이 값으로 면적을 되계산해야 한다. 상수로 적어 두면 격자 쪽이 바뀌는 날
	 * 리포트만 조용히 옛 면적으로 남는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	float CellSizeMeters = FFF_CrowdMetrics::AnalysisCellSizeMeters;

	// 셀 (0,0)의 좌하단 월드 좌표, UU. CellX/CellY를 월드 위치로 되돌리는 기준점이다.
	// 밀도 격자와 원점을 공유하지 않으면 Heat Map이 실제 인파와 한 칸씩 어긋난다.
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	FVector GridOriginUU = FVector::ZeroVector;

	// 인원이 0인 칸은 넣지 않는다 — 실측 레벨 전역이면 대부분의 칸이 비어 있다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	TArray<FFF_CrowdCellSample> OccupiedCells;

	// [홀드] ZoneSystem의 AFFZSGameplayZone::GetBoundaryAreaSquareMeters() 값이 FFFZSZoneReadModel(외부 공개 스냅샷)에 노출되도록 요청함
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	TMap<FName, float> ZoneAreaSqm;

	// 이 스냅샷이 만들어진 프레임. 결정 로그에 그대로 남겨 "무엇을 보고 결정했는지"를 재현한다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 SnapshotFrame = INDEX_NONE;

	// 스냅샷 시점의 시뮬레이션 시각(현실 분 기준)
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	float SimTimeMinutes = 0.f;
};
