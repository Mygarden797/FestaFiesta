// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_DeterministicRandom.generated.h"

class UDataTable;

/**
 * 고정 Seed 기반 개체 변주 — 순수 해시·분포 수학. DataTable 의존성 없음.
 * 설계는 data/metadata/spec/individual_variation_spec.md 참조.
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

	/**
	 * 새로운 곳을 얼마나 좋아하는가. Utility 식의 revisit_penalty 항에 곱한다(2026-08-10 추가).
	 *
	 * 1.0 보다 크면 이미 가본 곳을 더 피하고, 작으면 익숙한 곳을 다시 가는 데 거리낌이 없다.
	 * 0 이면 방문 이력을 아예 신경 쓰지 않는다.
	 *
	 * 다른 multiplier 와 달리 **페널티에 곱한다** — 값이 커질수록 그 후보의 효용이 내려간다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float NoveltySeekingMultiplier = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float LeisurelinessAxis = 0.5f;
};

/**
 * FFF_UtilityDestinationSelector::ShouldReevaluate가 받는 "이 개인의" 재평가 임계값.
 * 전역 계수(wait_threshold_minutes 등)에 개체 변주 multiplier를 곱해서 만든다.
 */
USTRUCT(BlueprintType)
struct FFF_PersonalReevaluationThresholds
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float WaitThresholdMinutes = 15.f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float DensityThresholdRatio = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float ReevaluateIntervalSeconds = 120.f;
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

	/**
	 * 그룹 멤버용 개체 변주. **NPC를 스폰할 때 이 함수를 부르고 결과를 캐싱하면 된다** —
	 * 개체 변주는 자동으로 걸리지 않는다. `StableAgentId`가 만들어져도 누군가 Resolve를
	 * 호출하지 않으면 아무 변주도 적용되지 않는다 (crowdai_handoff_note.md §1).
	 *
	 * Resolve()를 두 번 부른다:
	 *   - `StableAgentId` 시드 → 개인 특성 4종 + LeisurelinessAxis
	 *   - `GroupId` 시드       → WalkSpeedMultiplier / StayDurationMultiplier 만 가져다 덮음
	 *
	 * 그래서 같은 그룹은 같은 속도로 걷고 같은 시간 머문다(동행이 유지된다). 인내심·혼잡내성·
	 * 취향강도·재평가주기까지 공유하면 개체 변주의 원래 목적(복제처럼 안 보이게 하기)이
	 * 무의미해지므로 그 4종은 계속 개체별 독립이다 (individual_variation_spec.md 8-A절).
	 *
	 * Resolve()의 시그니처를 안 바꾸고 wrapper로 둔 이유: 그룹 공유는 호출부의 정책이고,
	 * Resolve() 자체는 "이 시드로 이 값"이라는 순수 대응 관계만 알아야 한다.
	 *
	 * @param GroupId 그룹 식별자. `INDEX_NONE`이면 solo로 취급해 Resolve() 한 번과 완전히 같은 결과를 낸다.
	 */
	static FFF_ResolvedIndividualTraits ResolveWithGroupSharing(
		const UDataTable* VariationTable,
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 GroupId,
		int32 SpawnGeneration,
		float CorrelationStrength);

	/**
	 * Resolve()가 만든 개체 변주값을 전역 재평가 계수(UtilityCoefficientTable의
	 * wait_threshold_minutes/density_threshold_ratio/reevaluate_interval_seconds)에
	 * 곱해서 "이 개인의" 임계값을 만든다. individual_variation_spec.md 5절 매핑 그대로.
	 */
	static FFF_PersonalReevaluationThresholds ResolvePersonalThresholds(
		const FFF_ResolvedIndividualTraits& Traits,
		const UDataTable* UtilityCoefficientTable);
};
