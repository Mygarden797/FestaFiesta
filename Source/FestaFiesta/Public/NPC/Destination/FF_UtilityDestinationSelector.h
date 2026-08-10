// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"
#include "FF_UtilityDestinationSelector.generated.h"

class UDataTable;

// 항별 점수·최종 선택 이유 디버그 로그. 기본은 출력 안 함 — 콘솔에서
// `Log LogFestaFiestaNpc Verbose` 로 켠다 (방문객 규모가 커서 기본 노출 시 로그가 넘칠 수 있음).
DECLARE_LOG_CATEGORY_EXTERN(LogFestaFiestaNpc, Log, All);

// 목적지 후보 하나의 런타임 입력. DataTable Row가 아니다 — 매 평가마다 호출부가 채워서 넘긴다.
// DistanceMeters는 v0에서 NPC-목적지 월드 위치의 직선거리 근사다. 
// 축제장 레이아웃·내비메시 확정 후 실제 보행거리로 교체한다 (utility_ai_spec.md 3절).
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
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	bool bIsOpen = true;

	// 이 시설이 해소할 수 있는 Need 성분. FFF_ZoneSystemBridge::GetNeedReliefFlags 결과를 그대로 넣는다.
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	FFF_NeedReliefFlags NeedRelief;

	/**
	 * 이 개체가 이곳을 마지막으로 방문한 뒤 지난 분(Live 경과분 기준).
	 * **음수 = 안 가본 곳**이고 그게 기본값이다 — 0 은 "방금 다녀왔다"라서 뜻이 완전히 다르다.
	 *
	 * FFF_VisitHistory::ApplyToCandidates 가 채운다. 안 채우면 전부 "안 가본 곳"이 되어
	 * revisit 항이 0 이 된다(= 신규성 선호가 꺼진 상태로 동작).
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Destination")
	float MinutesSinceLastVisit = -1.f;
};

/**
 * no_choice(=balking, 긴 줄을 보고 줄에 들어가지 않음)가 뽑혔을 때 NPC가 실제로 하는 행동.
 *
 * 여기서는 대기행렬 이론의 세 이탈 현상 중 balking만 다룬다
 * reneging(줄에 서 있다가 이탈)은 ShouldReevaluate가 맡는다.
 * jockeying(다른 줄로 옮김)은 재평가 후 재선택이 맡는다.
 *
 * Rest(휴식)를 의도적으로 넣지 않았다: Rest 카테고리 시설(오아시스 존)이 이미 후보 집합에
 * 들어 있으므로 여기에 또 두면 같은 행동이 두 경로로 발생하고 "Rest 시설이 부족해서 못 쉬었다"는
 * 진단이 불가능해진다 (utility_ai_spec.md 2절 no-choice 대안).
 */
UENUM(BlueprintType)
enum class EFF_NoChoiceAction : uint8
{
	// 기본값. 통로를 배회한다. 밀집도에 기여하고 ConsecutiveNoChoiceCount를 올린다
	Wander,

	// 현재 Zone 전체가 혼잡할 때. 인접 Zone으로 이동한다
	Relocate,

	// 유일하게 영구적인 행동. 체류시간 소진 또는 연속 no_choice 누적이 인내 한계를 넘었을 때
	Leave
};

// 디버그·감사용 항별 점수. no_choice도 하나의 "후보"로 포함된다.
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

	// weight_need * (이 시설이 해소하는 Need 중 가장 급한 값). no_choice 항목에서는 항상 0
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float NeedTerm = 0.f;

	// -weight_revisit * 방문 잔상 * novelty_seeking_multiplier. 안 가본 곳은 0
	UPROPERTY(BlueprintReadOnly, Category = "Debug")
	float RevisitPenaltyTerm = 0.f;

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

// Utility AI 목적지 선택기 v0. 상태 없는 순수 함수만 제공한다
// NPC 상태(현재 목적지, 마지막 평가 시각 등) 보관은 이 클래스의 책임이 아니다. utility_ai_spec.md 2·5절 참조.
class FFF_UtilityDestinationSelector
{
public:
	/**
	 * PreferenceVectorByCategory는 아키타입+동반유형+개체변주(preference_strength_multiplier)까지
	 * 이미 합성된 원본 pp-delta 값이다 (정규화 전). Choose 내부에서 preference_vector_scale_ref로 정규화한다.
	 * ChoiceRoundIndex는 이 NPC가 몇 번째로 목적지를 재평가하는지 — Decision 시드에 반영되어
	 * 같은 회차·같은 상태면 항상 같은 선택이 나오게 한다.
	 *
	 * @param CurrentNeeds             이 NPC(그룹이면 FFF_NeedState::MakeAggregated 결과)의 Need 상태.
	 *                                 기본값(전부 0)이면 need 항이 0이 되어 예전과 동일하게 동작한다.
	 * @param ConsecutiveNoChoiceCount 직전까지 연속으로 no_choice가 나온 횟수. 이 값이 올라가면
	 *                                 no_choice_utility가 no_choice_utility_decay_per_round만큼 깎인다.
	 *                                 이게 없으면 "혼잡해서 배회 → 같은 후보 → 또 배회"가 영원히 반복된다.
	 * @param NoveltySeekingMultiplier 신규성 선호 개체 변주(`FFF_ResolvedIndividualTraits`). revisit
	 *                                 페널티에 곱한다. 1.0이 기본, 0이면 방문 이력을 신경 쓰지 않는다.
	 *                                 후보의 `MinutesSinceLastVisit`이 안 채워져 있으면 이 값과 무관하게
	 *                                 revisit 항이 0이다 — 둘 다 있어야 동작한다.
	 */
	static FFF_DestinationDecision Choose(
		const TArray<FFF_DestinationCandidate>& Candidates,
		const TMap<FName, float>& PreferenceVectorByCategory,
		const UDataTable* UtilityCoefficientTable,
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 ChoiceRoundIndex,
		const FFF_NeedState& CurrentNeeds = FFF_NeedState(),
		int32 ConsecutiveNoChoiceCount = 0,
		float NoveltySeekingMultiplier = 1.f);

	/**
	 * no_choice가 뽑힌 뒤 실제로 어떤 행동을 할지 정한다. Choose()와 분리한 이유는 이 판정에
	 * 필요한 상태(잔여 체류시간, 현재 Zone 밀집도)가 목적지 후보와 무관해서다 — 여기에까지
	 * 후보 정보를 끌고 들어가면 Choose()의 순수성이 깨진다.
	 *
	 * softmax가 아니라 "우선순위 캐스케이드"다. 근거 없는 효용값 3개를 새로 만들지 않기 위한
	 * 선택이며, 확률화하면 "왜 퇴장했는지"를 설명할 수 없게 된다.
	 *
	 * 판정 순서:
	 *   1. 체류시간 소진        → Leave
	 *   2. 연속 no_choice 한계  → Leave
	 *   3. 현재 Zone 혼잡       → Relocate
	 *   4. 그 외                → Wander
	 *
	 * Leave가 체류시간 게이트를 먼저 보는 것이 중요하다. 혼잡만으로 퇴장하게 두면
	 * "혼잡 → 대량 퇴장 → 혼잡 해소"가 반복되어 총 방문객 수(인원 보존)가 지켜지지 않는다
	 * (utility_ai_spec.md 7절 구조 검증).
	 *
	 * @param NoChoiceLeavePatienceRounds no_choice_leave_patience_rounds 계수. 0 이하면 이 조건을 끈다.
	 */
	static EFF_NoChoiceAction ResolveNoChoiceAction(
		float RemainingStayMinutes,
		int32 ConsecutiveNoChoiceCount,
		int32 NoChoiceLeavePatienceRounds,
		float CurrentZoneDensityRatio,
		float PersonalDensityThresholdRatio);

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
