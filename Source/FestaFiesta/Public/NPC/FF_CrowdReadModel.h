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
 * !!! 스텁 !!! 채우는 쪽도 읽는 쪽도 아직 없다. 셀 크기·기록 주기·좌표 원점은 미합의.
 */

// 5m × 5m 격자 한 칸의 집계값. Heat Map · 밀도 지표 · AT-016 산점도가 전부 이 하나에서 나온다.
USTRUCT(BlueprintType)
struct FFF_CrowdCellSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 CellX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 CellY = 0;

	// 이 칸에 현재 있는 인원
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	int32 Occupancy = 0;

	// 직전 집계 구간 동안 이 칸을 통과한 인원. AT-016 의 유량 축
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

	// 셀 크기(m). 5.0 으로 시작하되 합의 전까지 하드코딩하지 않는다
	UPROPERTY(BlueprintReadOnly, Category = "Crowd")
	float CellSizeMeters = 5.f;

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
