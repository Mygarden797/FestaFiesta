// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_NeedState.generated.h"

// FFF_NeedState의 성분을 런타임에 지목하기 위한 키. 
// 집계 지표 조회(UFF_NpcNeedMetricsSubsystem)와 UI 표시에 쓴다. 
// 구조체 필드 순서와 반드시 일치시킨다.
// Info·Safety가 없는 것은 의도적이다 — FF_NeedState 상단 주석 참고.
UENUM(BlueprintType)
enum class EFF_NeedType : uint8
{
	Hunger,
	Thirst,
	Restroom,
	Fatigue,
	Stimulation,

	// 배열 크기·순회용. 실제 Need가 아니다
	Count UMETA(Hidden)
};

struct FFF_NeedPriorityWeights;

/**
 * NPC 욕구 상태. 0.0(해소됨) ~ 1.0(한계)로 정규화한다.
 * Need는 "화장실 Need 미해소율이 34%였다"와 같이 트윈의 진단 근거가 된다.
 *
 * 각 Need가 EFFZSFacilityCategory(FF_ZoneSystem, 7종)에 의해 실제로 해소될 수 있다.
 * 
 * | Need			| 해소 카테고리											| 비고
 * |				|														|
 * | Hunger			| Booth(FoodStall 토큰)									| 다대포차·청춘포차의 개별 트럭
 * | Thirst			| Booth(FoodStall·Nightlife 토큰) + Rest(Cooling 토큰만) | "오아시스 존"이 그늘+음수대+무릎나눔 복합 시설
 * | Restroom		| Toilet												| 1:1 (소분류 4종 전부)
 * | Fatigue		| Rest													| 소분류 4종 전부
 * | Stimulation	| Performance, PhotoSpot, Booth(Nightlife·MarketGoods·CraftExperience·ActivityGame 토큰)	| Performance=다대포차 스테이지(콘서트), Booth(Nightlife)=선셋클럽(EDM 비치클럽)·선셋펍(하우스DJ 라운지)
 *
 * Info는 "몰라서 헤맨다"는 욕구를 뒷받침할 근거가 없어서 제외 - 필요해지면 그때 추가한다.
 * Safety는 이중 반영을 막기 위해 제외 - 안전 Need를 만들면 그 "위험"이 결국 밀집 위험이고,
 * 밀집은 이미 Utility의 density_cost 항이 맡고 있다.
 *
 * P0는 Hunger·Restroom만 갱신한다. 나머지는 필드만 두고 Phase 4에서 켠다.
 * Stimulation은 Live 시간이 지날수록 쌓여 메인 공연 시작 전 무대 앞으로 몰리는 유입을 만들어낸다. (Phase 2)
 *
 * 증가·해소 규칙은 FFF_NeedRules(FF_NeedRules.h)에 있다. 이 구조체는 값만 들고 있고,
 * 값을 움직이는 경로는 Advance(시간 경과)와 Relieve(시설 이용 완료) 둘뿐이다 —
 * 그래서 여기에 Setter를 두지 않았다.
 *
 * "P0는 Hunger·Restroom만"은 코드 분기가 아니라 데이터다. 나머지 3종은
 * need_*_minutes_to_limit 계수가 0이어서 안 쌓인다 — 켜는 것은 CSV 값 변경이면 된다.
 */
USTRUCT(BlueprintType)
struct FFF_NeedState
{
	GENERATED_BODY()

	// P0 대상. Booth(식음료 서브셋)로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Hunger = 0.f;

	// P0 대상. Toilet으로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Restroom = 0.f;

	// Phase 4(P0 확장 후보 아님). Booth(FoodStall·Nightlife) 또는 Rest_Cooling으로 해소. 공공데이터(기온) 주입 후보
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Thirst = 0.f;

	// Phase 4(P0 확장 후보 아님). Rest 소분류 4종 전부로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Fatigue = 0.f;

	// Phase 2 대상. Performance·PhotoSpot 또는 Booth(Nightlife·MarketGoods·CraftExperience·ActivityGame)로 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Stimulation = 0.f;

	// EFF_NeedType으로 성분 하나를 읽는다. 집계·UI가 5개 필드를 각각 분기하지 않게 하기 위한 것
	float GetComponent(EFF_NeedType NeedType) const
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      return Hunger;
		case EFF_NeedType::Thirst:      return Thirst;
		case EFF_NeedType::Restroom:    return Restroom;
		case EFF_NeedType::Fatigue:     return Fatigue;
		case EFF_NeedType::Stimulation: return Stimulation;
		default:                        return 0.f;
		}
	}

	/**
	 * 성분 구분 없이 "지금 이 개체가 얼마나 급한가" (0~1).
	 *
	 * FFF_NeedReliefFlags::GetMaxUrgency 는 '특정 시설이 해소할 수 있는 성분만' 보지만,
	 * GetMaxWeightedUrgency는 '전 성분(해소해 줄 시설이 있든 없든 쌓인 욕구 자체의 크기)'을 본다.
	 * 그래서 "이 시설이 얼마나 매력적인가"(선택)가 아니라 "이 개체가 얼마나 지쳤나"(이탈)에 쓴다.
	 */
	float GetMaxWeightedUrgency(const FFF_NeedPriorityWeights& Priority) const;

	// 그룹 Need 집계 — 멤버별 Need의 성분별 최대값. (평균으로 하면 한 멤버의 급한 욕구가 1/4로 희석된다)
	// 한 명이라도 화장실이 급하면 일행 전체가 움직임
	static FFF_NeedState MakeAggregated(const FFF_NeedState& A, const FFF_NeedState& B)
	{
		FFF_NeedState Result;
		Result.Hunger      = FMath::Max(A.Hunger,      B.Hunger);
		Result.Restroom    = FMath::Max(A.Restroom,    B.Restroom);
		Result.Thirst      = FMath::Max(A.Thirst,      B.Thirst);
		Result.Fatigue     = FMath::Max(A.Fatigue,     B.Fatigue);
		Result.Stimulation = FMath::Max(A.Stimulation, B.Stimulation);
		return Result;
	}
};

/**
 * Need 성분별 "얼마나 급한가" 가중치 (0~1). Utility 식의 need 항에서 원본 Need 값에 곱한다.
 *
 * [중요] Restroom을 1.0으로 고정!
 * `weight_need`(1.8)는 "한계에 가까운 Need가 weight_attraction(1.0)+lambda_affinity(0.6)=1.6을 이긴다"는 부등식으로 정한 값
 * 가장 급한 성분이 1.0이어야 그 계산이 그대로 성립한다 — Restroom을 낮추면 `weight_need`도 다시 잡아야 한다.
 *
 * 순서(Restroom > Hunger > Thirst > Fatigue > Stimulation)는 생리적 급함의 상식적 순서
 * 값은 `utility_coefficients_v0.csv`에서 읽는다 — 기본값은 그 CSV와 같게 유지한다.
 */
USTRUCT(BlueprintType)
struct FFF_NeedPriorityWeights
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Hunger = 0.85f;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Thirst = 0.75f;

	// 가장 급한 성분. 1.0 고정 — 위 주석의 weight_need 부등식이 이 값에 걸려 있다
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Restroom = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Fatigue = 0.60f;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Stimulation = 0.50f;

	float Get(EFF_NeedType NeedType) const
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      return Hunger;
		case EFF_NeedType::Thirst:      return Thirst;
		case EFF_NeedType::Restroom:    return Restroom;
		case EFF_NeedType::Fatigue:     return Fatigue;
		case EFF_NeedType::Stimulation: return Stimulation;
		default:                        return 0.f;
		}
	}
};

// FFF_NeedPriorityWeights 가 완성된 뒤라야 정의할 수 있다. 선언은 FFF_NeedState 안에 있다.
inline float FFF_NeedState::GetMaxWeightedUrgency(const FFF_NeedPriorityWeights& Priority) const
{
	float Urgency = 0.f;
	Urgency = FMath::Max(Urgency, Hunger      * Priority.Hunger);
	Urgency = FMath::Max(Urgency, Thirst      * Priority.Thirst);
	Urgency = FMath::Max(Urgency, Restroom    * Priority.Restroom);
	Urgency = FMath::Max(Urgency, Fatigue     * Priority.Fatigue);
	Urgency = FMath::Max(Urgency, Stimulation * Priority.Stimulation);
	return FMath::Clamp(Urgency, 0.f, 1.f);
}

/**
 * 시설 하나가 해소할 수 있는 Need 성분 플래그. 위 매핑 표를 그대로 옮긴 것이다.
 *
 * 판정 자체는 FFF_ZoneSystemBridge::GetNeedReliefFlags 에서 한다.
 * 
 * [참고] 구조체를 Integration 이 아니라 여기 두는 이유
 * FFF_DestinationCandidate(Destination 모듈)도 이 값을 실어야 하는데, Destination 이
 * Integration 을 include 하면 순환 의존이 되기 때문이다 — Need 개념의 소유자는 Party 모듈이다.
 */
USTRUCT(BlueprintType)
struct FFF_NeedReliefFlags
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	bool bRelievesHunger = false;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	bool bRelievesThirst = false;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	bool bRelievesRestroom = false;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	bool bRelievesFatigue = false;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	bool bRelievesStimulation = false;

	bool RelievesAnything() const
	{
		return bRelievesHunger || bRelievesThirst || bRelievesRestroom || bRelievesFatigue || bRelievesStimulation;
	}

	/**
	 * 이 시설이 해소할 수 있는 Need 성분들 중 "가장 급한 값 (0~1)". 이다.
	 * 각 성분은 FFF_NeedPriorityWeights 로 성분별 급함을 반영한 뒤 비교한다.
	 *
	 * 합이 아니라 최대값인 이유: 합을 쓰면 3가지를 해소하는 시설이 각 성분이 아무리 안 급해도
	 * 1가지만 해소하는 시설을 항상 이긴다.
	 */
	float GetMaxUrgency(const FFF_NeedState& Needs, const FFF_NeedPriorityWeights& Priority = FFF_NeedPriorityWeights()) const
	{
		float Urgency = 0.f;
		if (bRelievesHunger)      { Urgency = FMath::Max(Urgency, Needs.Hunger      * Priority.Hunger); }
		if (bRelievesThirst)      { Urgency = FMath::Max(Urgency, Needs.Thirst      * Priority.Thirst); }
		if (bRelievesRestroom)    { Urgency = FMath::Max(Urgency, Needs.Restroom    * Priority.Restroom); }
		if (bRelievesFatigue)     { Urgency = FMath::Max(Urgency, Needs.Fatigue     * Priority.Fatigue); }
		if (bRelievesStimulation) { Urgency = FMath::Max(Urgency, Needs.Stimulation * Priority.Stimulation); }
		return FMath::Clamp(Urgency, 0.f, 1.f);
	}
};
