// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_DeterministicRandom.generated.h"

/**
 * 고정 Seed 기반 개체 변주 — 순수 해시·분포 수학. DataTable 의존성 없음.
 * 설계는 data/metadata/individual_variation_spec.md 참조.
 *
 * 같은 (GlobalSimulationSeed, StableAgentId, TraitKey, SpawnGeneration, RandomPurpose) 조합은
 * 항상 같은 값을 낸다. 금지 입력: 현재 시각, 포인터 주소, Actor 스폰 순서, 프레임 번호.
 */
class FFF_DeterministicRandom
{
public:
	// TraitKey마다 독립된 난수 스트림을 만든다 — 하나의 스트림을 순서대로 소비하지 않으므로
	// 나중에 특성을 추가해도 기존 특성값이 바뀌지 않는다.
	static uint32 MakeTraitSeed(int32 GlobalSimulationSeed, int32 StableAgentId, FName TraitKey, int32 SpawnGeneration, FName RandomPurpose);

	// TraitSeed 하나로 [0,1) 균등난수 하나를 결정적으로 뽑는다.
	static float MakeUniformFraction(uint32 TraitSeed);

	// 균등난수 하나를 삼각분포(Min, Mode, Max)의 값으로 바꾼다 (역누적분포함수).
	static float SampleTriangularFromFraction(float Min, float Mode, float Max, float UniformFraction01);
};

/**
 * FFF_IndividualVariationResolver 가 만들어내는, 개체 하나에 대해 확정된 변주값 묶음.
 * 전부 곱셈용 multiplier이며 기본값 1.0(=변주 없음)에서 벗어난 정도가 개체차다.
 * (leisureliness_axis 만 예외 — 직접 곱하지 않는 공유 성격축)
 */
USTRUCT(BlueprintType)
struct FFF_ResolvedIndividualTraits
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float WalkSpeedMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float PatienceMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float CrowdToleranceMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float PreferenceStrengthMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float StayDurationMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float ReevaluateIntervalMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float LeisurelinessAxis = 0.5f;
};

/**
 * data/unreal/individual_variation_knobs_v0.csv 로 임포트한 FFF_IndividualVariationRow
 * DataTable을 읽어 NPC 한 명의 확정된 개체 변주값을 만든다.
 *
 * patience_multiplier·stay_duration_multiplier는 leisureliness_axis와 CorrelationStrength
 * 비율로 균등난수 단계에서 블렌딩한다 (individual_variation_spec.md 7절). 나머지는 완전 독립.
 */
class FFF_IndividualVariationResolver
{
public:
	static FFF_ResolvedIndividualTraits Resolve(
		const UDataTable* VariationTable,
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 SpawnGeneration,
		float CorrelationStrength);
};
