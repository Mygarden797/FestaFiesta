// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
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

	/**
	 * 그리드를 만든다. 이미 만들어져 있으면 다시 만들고 기존 값은 버린다.
	 *
	 * @param InOriginUU   그리드 좌하단 월드 좌표(XY만 사용), UU
	 * @param InSizeUU     그리드가 덮는 가로/세로 크기, UU
	 * @param InCellSizeUU 셀 한 변의 길이, UU. 아래 CellAreaSqM 주석 참고
	 */
	void Build(const FVector& InOriginUU, const FVector2D& InSizeUU, float InCellSizeUU);

	/**
	 * 이번 스텝의 Agent 위치로 밀도를 다시 계산한다.
	 * Build를 부르지 않았으면 아무것도 하지 않는다.
	 */
	void Update(const TArray<FUnitData>& Units);

	/** 모든 셀 카운트/밀도를 0으로. 그리드 크기는 유지한다. */
	void Reset();

	bool IsBuilt() const { return NumX > 0 && NumY > 0; }

	/** 해당 월드 위치가 속한 셀의 밀도, 명/m^2. 그리드 밖이면 0. */
	float GetDensityAtLocation(const FVector& WorldLocation) const;

	/** 해당 월드 위치의 LOS 등급. 그리드 밖이면 A. */
	ECrowdLOS GetLOSAtLocation(const FVector& WorldLocation) const;

	/** 이번 스텝 전체 셀 중 최대 밀도, 명/m^2. 안전 판정에서 제일 먼저 보는 숫자다. */
	float GetMaxDensity() const { return MaxDensity; }

	/** LOS D 이상(1.08명/m^2 초과)인 셀의 개수. 혼잡 구역이 얼마나 넓은지를 나타낸다. */
	int32 GetCongestedCellCount() const { return CongestedCellCount; }

	/** 집계된 전체 인원 수. 활성 Agent 중 그리드 안에 있는 수와 같아야 한다. */
	int32 GetSampledUnitCount() const { return SampledUnitCount; }

	int32 GetNumX() const { return NumX; }
	int32 GetNumY() const { return NumY; }
	float GetCellSizeUU() const { return CellSizeUU; }
	const TArray<float>& GetDensityField() const { return Density; }

	/** (X, Y) 셀의 중심 월드 좌표. 디버그 드로우와 히트맵에서 쓴다. */
	FVector GetCellCenterWorld(int32 X, int32 Y) const;

	/** 밀도 값을 LOS 등급으로 환산. 그리드 없이도 쓸 수 있게 static. */
	static ECrowdLOS DensityToLOS(float DensityPerSqM);

	/** LOS 등급에 대응하는 디버그/히트맵 색상. A=녹색 ... F=적색. */
	static FColor LOSToColor(ECrowdLOS LOS);

	/**
	 * 셀 밀도를 색 박스로 그린다. 에디터에서 눈으로 확인할 때만 쓴다.
	 * 셀 수만큼 DrawDebugSolidBox를 부르므로 시연/디버깅 외에는 켜면 안 된다.
	 *
	 * @param MinDensityToDraw 이 값 이하인 셀은 그리지 않는다. 빈 셀까지 그리면 화면이 안 보인다.
	 */
	void DrawDebug(const UWorld* World, float ZOffsetUU = 20.f, float MinDensityToDraw = 0.05f) const;

private:
	/** 월드 XY -> 셀 인덱스. 그리드 밖이면 INDEX_NONE. */
	int32 WorldToCellIndex(const FVector& WorldLocation) const;

	/** 그리드 좌하단 월드 좌표. Z는 디버그 드로우 높이로만 쓴다. */
	FVector OriginUU = FVector::ZeroVector;

	float CellSizeUU = 200.f;

	/**
	 * 셀 하나의 면적, m^2. 밀도 = 셀 인원 수 / CellAreaSqM 이므로 이 값이 곧 "측정 창"이다.
	 *
	 * 이 크기를 잘못 잡으면 숫자가 의미를 잃는다. 1m 셀이면 혼자 서 있는 사람도 1.0명/m^2 —
	 * 이미 LOS E다. Fruin의 구간은 몇 m^2를 평균한 밀도를 전제로 하므로 측정 창도 그래야 한다.
	 * 기본값 200UU(2m) 셀은 4m^2를 평균한다.
	 */
	float CellAreaSqM = 4.f;

	int32 NumX = 0;
	int32 NumY = 0;

	/** 이번 스텝 셀별 인원 수. Update 시작에 0으로 밀고 다시 센다. */
	TArray<int32> CellCounts;

	/** 셀별 밀도, 명/m^2. 이게 외부에 공개되는 값이다. */
	TArray<float> Density;

	float MaxDensity = 0.f;
	int32 CongestedCellCount = 0;
	int32 SampledUnitCount = 0;
};
