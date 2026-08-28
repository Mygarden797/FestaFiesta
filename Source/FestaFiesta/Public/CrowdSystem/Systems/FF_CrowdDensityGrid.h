// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Integration/FF_CrowdReadModel.h"
#include "FF_CrowdDensityGrid.generated.h"

struct FUnitData;

/**
 * Fruin 보행 서비스 수준(Level of Service).
 * 보행로 기준 밀도 구간이며, 축제 안전계획에서 "이 구역이 위험한가"를 판정하는 표준 지표다.
 * A가 가장 여유롭고 F가 군중 사고 위험 구간.
 */
UENUM(BlueprintType)
enum class ECrowdLOS : uint8
{
	A	UMETA(DisplayName = "A (여유)"),
	B	UMETA(DisplayName = "B (원활)"),
	C	UMETA(DisplayName = "C (보통)"),
	D	UMETA(DisplayName = "D (혼잡)"),
	E	UMETA(DisplayName = "E (매우 혼잡)"),
	F	UMETA(DisplayName = "F (위험)")
};

/**
 * 균일 격자 기반 군중 밀도 그리드.
 *
 * 매 스텝 Agent 위치를 셀에 집계해서 셀당 밀도(명/m^2)를 구하고, 그 값을 Fruin LOS 등급으로 환산한다.
 * 목적은 두 가지다.
 *   1. 혼잡도 계측 — 어느 구역이 몇 명/m^2 인지, 최댓값은 얼마인지 (분석 결과물)
 *   2. 나중에 붙일 회피/우회 로직이 읽을 밀도장 (동작 입력)
 * 둘이 같은 배열을 보게 해야 "리포트와 실제 동작이 다른" 상황이 생기지 않는다.
 *
 * UObject가 아니라 순수 struct다. Subsystem이 값으로 하나 들고 있으면 되고,
 * 매 프레임 도는 코드라 UObject 오버헤드를 붙일 이유가 없다.
 */
USTRUCT()
struct FESTAFIESTA_API FCrowdDensityGrid
{
	GENERATED_BODY()


	void Build(const FVector& InOriginUU, const FVector2D& InSizeUU, float InCellSizeUU);


	/**
	 * 셀 점유를 다시 세고, 셀 진입(Throughput)을 누적한다.
	 *
	 * 점유는 매번 새로 세고 진입은 **누적한다.** 밀도는 "지금 몇 명인가"이고
	 * 유량은 "구간 동안 몇 명이 지나갔나"라서 초기화 주기가 다르다 —
	 * 누적을 끊는 것은 ResetThroughputWindow(스냅샷 발행 시점)의 몫이다.
	 *
	 * @param Units 풀 전체. **인덱스가 AgentIndex여야 한다** — 진입 판정이
	 *              개체별 '직전 셀'을 이 인덱스로 기억한다
	 */
	void Update(const TArray<FUnitData>& Units);

	void Reset();

	/**
	 * 유량 집계 구간을 끊는다. 스냅샷을 발행한 직후에 부른다.
	 *
	 * 이걸 안 부르면 Throughput이 세션 시작부터의 누적 총량이 되어
	 * "지금 막혀 있는가"를 판정할 수 없다 — 이미 흘려보낸 인원에 묻힌다.
	 * 개체별 '직전 셀' 기억은 지우지 않는다. 지우면 구간이 바뀌는 프레임마다
	 * 전원이 제자리에서 한 번씩 진입한 것으로 세어져 유량이 부풀려진다.
	 */
	void ResetThroughputWindow();

	bool IsBuilt() const { return NumX > 0 && NumY > 0; }

	float GetDensityAtLocation(const FVector& WorldLocation) const;

	ECrowdLOS GetLOSAtLocation(const FVector& WorldLocation) const;

	float GetMaxDensity() const { return MaxDensity; }

	int32 GetCongestedCellCount() const { return CongestedCellCount; }

	int32 GetSampledUnitCount() const { return SampledUnitCount; }

	int32 GetNumX() const { return NumX; }
	int32 GetNumY() const { return NumY; }
	float GetCellSizeUU() const { return CellSizeUU; }

	// 셀 한 변(m). ReadModel의 CellSizeMeters는 이 값으로 채운다 — 상수를 다시 적지 말 것
	float GetCellSizeMeters() const { return CellSizeUU * 0.01f; }

	// 셀 하나의 면적(㎡). 밀도 = 인원 / 이 값
	float GetCellAreaSqM() const { return CellAreaSqM; }

	// 셀 (0,0)의 좌하단 월드 좌표. Heat Map이 셀 좌표를 월드로 되돌릴 때의 기준점
	const FVector& GetOriginUU() const { return OriginUU; }

	const TArray<float>& GetDensityField() const { return Density; }

	// 셀별 현재 인원. Density와 같은 인덱스 공간(Y * NumX + X)
	const TArray<int32>& GetCellCounts() const { return CellCounts; }

	// 셀별 구간 진입 인원. ResetThroughputWindow 이후 누적분이다
	const TArray<int32>& GetCellThroughput() const { return CellThroughput; }

	FVector GetCellCenterWorld(int32 X, int32 Y) const;

	static ECrowdLOS DensityToLOS(float DensityPerSqM);

	static FColor LOSToColor(ECrowdLOS LOS);


	void DrawDebug(const UWorld* World, float ZOffsetUU = 20.f, float MinDensityToDraw = 0.05f) const;

private:
	int32 WorldToCellIndex(const FVector& WorldLocation) const;

	FVector OriginUU = FVector::ZeroVector;

	// ★ 상수를 여기 다시 적지 않는다. 지표 셀 크기의 단일 출처는 FFF_CrowdMetrics다
	float CellSizeUU = FFF_CrowdMetrics::AnalysisCellSizeUU;

	float CellAreaSqM = FFF_CrowdMetrics::AnalysisCellSizeMeters
		* FFF_CrowdMetrics::AnalysisCellSizeMeters;

	int32 NumX = 0;
	int32 NumY = 0;

	TArray<int32> CellCounts;

	// 구간 진입 인원. Update가 누적하고 ResetThroughputWindow가 끊는다
	TArray<int32> CellThroughput;

	/**
	 * 개체별 직전 셀 인덱스. 크기 = 풀 크기, 인덱스 = AgentIndex.
	 *
	 * 진입 판정에 필요하다 — 셀이 바뀌었는지는 이전 값을 알아야 판단할 수 있다.
	 * 비활성 슬롯은 INDEX_NONE으로 되돌린다. 안 그러면 슬롯이 재사용될 때
	 * 퇴장자의 마지막 셀이 새 방문객의 '직전 셀'이 되어, 맵 반대편에서
	 * 순간이동한 것처럼 엉뚱한 셀의 유량이 오른다.
	 */
	TArray<int32> LastCellIndexByAgent;

	TArray<float> Density;

	float MaxDensity = 0.f;
	int32 CongestedCellCount = 0;
	int32 SampledUnitCount = 0;
};
