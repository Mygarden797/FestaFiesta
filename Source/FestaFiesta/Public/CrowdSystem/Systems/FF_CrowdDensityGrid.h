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


	void Build(const FVector& InOriginUU, const FVector2D& InSizeUU, float InCellSizeUU);


	void Update(const TArray<FUnitData>& Units);

	void Reset();

	bool IsBuilt() const { return NumX > 0 && NumY > 0; }

	float GetDensityAtLocation(const FVector& WorldLocation) const;

	ECrowdLOS GetLOSAtLocation(const FVector& WorldLocation) const;

	float GetMaxDensity() const { return MaxDensity; }

	int32 GetCongestedCellCount() const { return CongestedCellCount; }

	int32 GetSampledUnitCount() const { return SampledUnitCount; }

	int32 GetNumX() const { return NumX; }
	int32 GetNumY() const { return NumY; }
	float GetCellSizeUU() const { return CellSizeUU; }
	const TArray<float>& GetDensityField() const { return Density; }

	FVector GetCellCenterWorld(int32 X, int32 Y) const;

	static ECrowdLOS DensityToLOS(float DensityPerSqM);

	static FColor LOSToColor(ECrowdLOS LOS);


	void DrawDebug(const UWorld* World, float ZOffsetUU = 20.f, float MinDensityToDraw = 0.05f) const;

private:
	int32 WorldToCellIndex(const FVector& WorldLocation) const;

	FVector OriginUU = FVector::ZeroVector;

	float CellSizeUU = 200.f;


	float CellAreaSqM = 4.f;

	int32 NumX = 0;
	int32 NumY = 0;

	TArray<int32> CellCounts;

	TArray<float> Density;

	float MaxDensity = 0.f;
	int32 CongestedCellCount = 0;
	int32 SampledUnitCount = 0;
};
