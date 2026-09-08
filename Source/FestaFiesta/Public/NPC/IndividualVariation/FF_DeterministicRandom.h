// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_DeterministicRandom.generated.h"

class UDataTable;

// 고정 Seed 기반 개체 변주 — 순수 해시·분포 수학. DataTable 의존성 없음.
// 같은 (GlobalSimulationSeed, StableAgentId, TraitKey, SpawnGeneration, RandomPurpose) 조합은 항상 같은 값을 낸다.

/*
 * - GlobalSimulationSeed: 시뮬레이션 전체의 기준 시드
 * - StableAgentId: 에이전트의 고정 식별자
 * - TraitKey: 계산할 특성의 이름
 * - SpawnGeneration: 같은 ID가 재사용될 때 생성 세대를 구분하는 값
 * - RandomPurpose: 외형 생성, 의사결정 등 난수의 사용 목적
 */

class FFF_DeterministicRandom
{
public:
	// FName의 문자열 내용 자체를 FNV-1a 방식으로 해시하여 uint32 값으로 바꾼다.
	// FName의 내부 인덱스는 실행 환경이나 이름 등록 순서에 따라 달라질 수 있으므로
	// TraitKey 는 ASCII snake_case 으로 작성한다.
	static uint32 MakeStableNameHash(FName Name);

	// 다섯 가지 식별값을 섞어 특정 에이전트의 특정 특성 전용 시드를 만든다.
	//
	// 도메인을 모르는 순수 함수다. 개체 변주 밖에서도 재사용하며, 2026-09-08 기준 실사용
	// RandomPurpose 는 4종이다 — Generation(변주·방문객 분포) / Decision(목적지·배회) /
	// NeedInitial(초기 Need) / Gate(입장 게이트, 이정원 분할 스폰).
	// 새 purpose 를 쓰면 individual_variation_spec.md 4절에 적을 것. 안 적으면 어떤 난수가
	// 어디서 갈라지는지 추적할 방법이 없다.
	static uint32 MakeTraitSeed(int32 GlobalSimulationSeed, int32 StableAgentId, FName TraitKey, int32 SpawnGeneration, FName RandomPurpose);

	// 에이전트의 목적지 선택(softmax 샘플링)에 사용할 결정론적 시드를 만든다.
	static uint32 MakeDecisionSeed(int32 GlobalSimulationSeed, int32 StableAgentId, int32 ChoiceRoundIndex);

	// TraitSeed 하나로 [0,1) 균등난수 하나를 결정적으로 뽑는다.
	static float MakeUniformFraction(uint32 TraitSeed);

	// 균등난수 하나를 삼각분포(Min, Mode, Max)의 값으로 바꾼다.
	// 역누적분포함수를 사용하므로 결과는 Min과 Max 사이에 위치하며, Mode에 가까운 값이 상대적으로 자주 나온다.
	static float SampleTriangularFromFraction(float Min, float Mode, float Max, float UniformFraction01);
};

// FFF_IndividualVariationResolver 가 만들어내는, 개체 하나에 대해 확정된 변주값 묶음.
// 전부 곱셈용 multiplier이며 기본값 1.0(=변주 없음)에서 벗어난 정도가 개체차다.

USTRUCT(BlueprintType)
struct FFF_ResolvedIndividualTraits
{
	GENERATED_BODY()

	// NPC의 이동속도
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float WalkSpeedMultiplier = 1.f;

	// 대기열에서 얼마나 오래 참을지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float PatienceMultiplier = 1.f;

	// 혼잡한 장소를 얼마나 견디는지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float CrowdToleranceMultiplier = 1.f;

	// NPC 아키타입의 장소 취향이 얼마나 강하게 적용될지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float PreferenceStrengthMultiplier = 1.f;

	// 축제에 얼마나 오래 머무를지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float StayDurationMultiplier = 1.f;

	// 목적지를 얼마나 자주 다시 판단할지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float ReevaluateIntervalMultiplier = 1.f;

	// 새로운 곳을 얼마나 좋아하는가.
	// 이미 방문했던 장소를 또 방문하는 것에 대한 페널티
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float NoveltySeekingMultiplier = 1.f;

	// NPC가 얼마나 느긋한지 - 직접 소비하지 않음
	// PatienceMultiplier, StayDurationMultiplier 계산 과정에 이미 반영됨
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float LeisurelinessAxis = 0.5f;
};

// NPC마다 목적지를 다시 판단하는 기준을 저장
USTRUCT(BlueprintType)
struct FFF_PersonalReevaluationThresholds
{
	GENERATED_BODY()

	// 현재 목적지의 예상 대기시간이 얼마나 길어지면 다시 판단할지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float WaitThresholdMinutes = 15.f;

	// 목적지가 얼마나 붐비면 다시 판단할지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float DensityThresholdRatio = 0.85f;

	// 특별한 문제가 없어도 몇 초마다 목적지를 다시 검토할지
	UPROPERTY(BlueprintReadOnly, Category = "IndividualVariation")
	float ReevaluateIntervalSeconds = 120.f;
};

// CSV에서 가져온 특성 범위를 읽고, NPC별로 확정된 개체 특성을 계산한다.
// 특성 이름                  Min   Mode  Max
// walk_speed_multiplier     0.8   1.0   1.2

// NPC의 결정론적 난수를 이용해 이 범위 안에서 실제 값을 뽑는다.
// NPC 42
// walk_speed_multiplier     = 1.08
class FFF_IndividualVariationResolver
{
public:
	
	// 한 개의 ID를 기준으로 개체 특성 전체를 계산하는 기본 함수
	static FFF_ResolvedIndividualTraits Resolve(
		const UDataTable* VariationTable,
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 SpawnGeneration,
		float CorrelationStrength);
	
	/*
	 * Resolve()에 동행 그룹 정책을 추가한 함수
	 *
	 * <내부 처리>
	 * 1. StableAgentId로 Resolve() → 개인별 특성 전체 계산
	 * 2. GroupId로 Resolve() → 그룹 공통 특성 전체 계산
	 * 3. 그룹 결과 중 아래 두 개만 개인 결과에 덮어쓰기 → WalkSpeedMultiplier, StayDurationMultiplier
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

	// 앞에서 계산한 NPC 특성을 실제 재평가 기준으로 변환
	static FFF_PersonalReevaluationThresholds ResolvePersonalThresholds(
		const FFF_ResolvedIndividualTraits& Traits,
		const UDataTable* UtilityCoefficientTable);
};
