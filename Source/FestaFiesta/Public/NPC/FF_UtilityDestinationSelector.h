// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_UtilityDestinationSelector.generated.h"

class UDataTable;

// 항별 점수·최종 선택 이유 디버그 로그. 기본은 출력 안 함 — 콘솔에서
// `Log LogFestaFiestaNpc Verbose` 로 켠다 (방문객 규모가 커서 기본 노출 시 로그가 넘칠 수 있음).
DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaNpc, Log, All);

/**
 * 목적지 후보 하나의 런타임 입력. DataTable Row가 아니다 — 매 평가마다 호출부가 채워서 넘긴다.
 * DistanceMeters는 v0에서 NPC-목적지 월드 위치의 직선거리 근사다. 축제장 레이아웃·내비메시
 * 확정 후 실제 보행거리로 교체한다 (utility_ai_spec.md 3절).
 */
USTRUCT(BlueprintType)
struct FFF_DestinationCandidate
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	FName DestinationId;

	// affinity_prior/festival_activity_category_mapping_v0.csv의 festival_category와 동일 키
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	FName Category;

	// 0~1로 이미 정규화된 값 (attraction_norm 입력). 정규화 방법은 부스 설정 쪽 설계 대기
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float BaseAttraction = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float DistanceMeters = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float EstimatedWaitMinutes = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float CurrentOccupancy = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float Capacity = 1.f;

	// 설치 완료 + 현재 운영시간 + 라이브 이벤트로 인한 임시 폐쇄 아님을 호출부가 미리 합쳐서 넘긴다.
	// selector는 왜 닫혔는지 모른다 — 후보에 넣을지 여부만 안다.
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	bool bIsOpen = true;
};

/** 디버그·감사용 항별 점수. no_choice도 하나의 "후보"로 포함된다. */
USTRUCT(BlueprintType)
struct FFF_DestinationUtilityBreakdown
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	FName DestinationId;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float AttractionTerm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float PreferenceTerm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float DistanceCostTerm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float WaitCostTerm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float DensityCostTerm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float TotalUtility = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float ChoiceProbability = 0.f;
};

USTRUCT(BlueprintType)
struct FFF_DestinationDecision
{
	GENERATED_BODY()

	// NAME_None이면 no_choice가 선택된 것
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	FName ChosenDestinationId;

	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	bool bIsNoChoice = false;

	// 항별 점수 + 최종 선택 이유 디버그 로그용. no_choice 항목 포함
	UPROPERTY(BlueprintReadOnly, Category = "Decision")
	TArray<FFF_DestinationUtilityBreakdown> AllCandidateScores;
};

/**
 * Utility AI 목적지 선택기 v0. 상태 없는 순수 함수만 제공한다 — NPC 상태(현재 목적지,
 * 마지막 평가 시각 등) 보관은 이 클래스의 책임이 아니다. utility_ai_spec.md 2·5절 참조.
 */
class FFF_UtilityDestinationSelector
{
public:
	/**
	 * PreferenceVectorByCategory는 아키타입+동반유형+개체변주(preference_strength_multiplier)까지
	 * 이미 합성된 원본 pp-delta 값이다 (정규화 전). Choose 내부에서 preference_vector_scale_ref로 정규화한다.
	 * ChoiceRoundIndex는 이 NPC가 몇 번째로 목적지를 재평가하는지 — Decision 시드에 반영되어
	 * 같은 회차·같은 상태면 항상 같은 선택이 나오게 한다.
	 */
	static FFF_DestinationDecision Choose(
		const TArray<FFF_DestinationCandidate>& Candidates,
		const TMap<FName, float>& PreferenceVectorByCategory,
		const UDataTable* UtilityCoefficientTable,
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 ChoiceRoundIndex);

	/**
	 * PersonalWaitThresholdMinutes/PersonalDensityThresholdRatio/PersonalReevaluateIntervalSeconds는
	 * 이미 FFF_IndividualVariationResolver의 개체 변주가 적용된 값을 호출부가 넘긴다 — 이 함수는
	 * 개체 변주를 다시 계산하지 않는다.
	 */
	static bool ShouldReevaluate(
		float CurrentWaitMinutes, float PersonalWaitThresholdMinutes,
		float CurrentDensityRatio, float PersonalDensityThresholdRatio,
		float SecondsSinceLastEvaluation, float PersonalReevaluateIntervalSeconds,
		bool bDestinationOpenStateChanged, bool bLiveEventStateChanged);
};
