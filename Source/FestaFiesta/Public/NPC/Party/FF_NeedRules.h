// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"
#include "FF_NeedRules.generated.h"

class UDataTable;

/**
 * Need 증가 속도. 분당 증가량이 아니라 "한계(1.0)에 닿는 데 걸리는 분"으로 표현한다.
 *
 * 분당 0.00417 vs "4시간이면 배고픔이 한계에 닿는다"는 근거를 붙일 수도 있고 반박할 수도 있는 문장이 된다.
 *
 * 0 이면 그 Need 는 쌓이지 않는다.
 */
USTRUCT(BlueprintType)
struct FFF_NeedRiseRates
{
	GENERATED_BODY()

	// P0. 성인 식사 간격 4시간 가정 — draft_assumption, 실측 아님
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float HungerMinutesToLimit = 240.f;

	// P0. 주간 배뇨 간격 2.5시간 가정 — draft_assumption. 축제라 음료 섭취가 많아 식사보다 짧게 잡았다
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float RestroomMinutesToLimit = 150.f;

	// Phase 4. 기온 공공데이터를 주입하면 동적으로 올릴 후보
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float ThirstMinutesToLimit = 0.f;

	// Phase 4
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float FatigueMinutesToLimit = 0.f;

	/**
	 * Phase 2 — 켜져 있다. Live 후반으로 갈수록 Performance·PhotoSpot·Booth(Nightlife) 쪽 끌림이
	 * 장내 전체에서 같이 커진다 — 무대는 수가 적고 크니 그 끌림이 한 곳에 모인다(AT-016).
	 *
	 * 240 은 실측이 아니라 앵커다. live_stage_duration_minutes 와 같은 값이라
	 * "Live 를 통째로 머문 사람이 딱 한계에 닿는다"는 문장이 근거로 선다.
	 *
	 * 한계에서의 세기는 weight_need(1.8) × need_priority_stimulation(0.5) = 0.9 다.
	 * 설계 의도는 attraction(1.0) + preference(0.6) = 1.6 보다 작아 취향을 뒤집지 못하는 것이었지만,
	 * BaseAttraction 이 아직 0 이라 실제 경쟁자는 preference(0.6) 하나뿐이다 — 지금은 뒤집는다.
	 * attraction_norm 이 들어오면 경쟁자가 1.6 으로 돌아오므로 이 값은 재조정 대상이다.
	 *
	 * [특징 1] 특정 시각의 급증은 이 값이 만들지 않는다. 완만히 오르는 추세뿐이다 —
	 * 매 틱 (경과분 / 240) 을 더하는 선형 누적기라 곡선이 꺾이는 지점이 원리적으로 없고,
	 * 초기값과 입장 시각이 개체마다 흩어져 있어 합쳐도 부드럽다.
	 * 이 값이 만드는 것은 "무대를 고를 이유"까지이고, 공연 시각의 계단은
	 * 무대 bIsOpen 게이팅(FFF_ZoneSystemBridge::BuildCandidates)이,
	 * 그 계단을 같은 순간에 보이게 하는 것은
	 * UFF_CrowdSimulationSubsystem::RequestRedecisionForIdleParties 가 만든다.
	 * [특징 2] 무대 전용이 아니다. 포토존·나이트라이프 부스도 같이 해소하므로 끌림이 셋으로 나뉜다
	 *
	 * [짝] need_stimulation_relief_amount 와 함께 움직인다. 해소량이 0 이면 Relieve 가 그 성분을
	 * 통째로 건너뛰어 모두의 Stimulation 이 1.0 에 붙고, 동적 끌림이 아니라 전원에게 똑같이 걸린
	 * 상수가 되어 아무것도 구분하지 못한다. 한쪽만 0 으로 되돌리지 말 것.
	 */
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float StimulationMinutesToLimit = 240.f;

	float GetMinutesToLimit(EFF_NeedType NeedType) const
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      return HungerMinutesToLimit;
		case EFF_NeedType::Thirst:      return ThirstMinutesToLimit;
		case EFF_NeedType::Restroom:    return RestroomMinutesToLimit;
		case EFF_NeedType::Fatigue:     return FatigueMinutesToLimit;
		case EFF_NeedType::Stimulation: return StimulationMinutesToLimit;
		default:                        return 0.f;
		}
	}

	// 분당 증가량. MinutesToLimit 이 0 이하면 0(안 쌓임)이다 — 0 으로 나누지 않는다.
	float GetRisePerMinute(EFF_NeedType NeedType) const
	{
		const float MinutesToLimit = GetMinutesToLimit(NeedType);
		return MinutesToLimit > KINDA_SMALL_NUMBER ? (1.f / MinutesToLimit) : 0.f;
	}
};

/**
 * 시설을 한 번 이용했을 때 깎이는 Need 량.
 *
 * 재방문을 만드는 것은 (1) `MinutesToLimit` 이 다시 쌓아주는 것과 (2) 입장 시점 초기값이다.
 * 
 * [참고] 해소량은 재방문을 만들지 않는다.
 * `Relieve` 는 `max(0, Need - ReliefAmount)` 로 clamp 한다.
 * 그래서 Need 가 해소량보다 작으면 0.8 과 1.0 의 결과가 완전히 같다
 */
USTRUCT(BlueprintType)
struct FFF_NeedReliefAmounts
{
	GENERATED_BODY()

	// 부분 해소 — 위 주석 참조
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Hunger = 0.8f;

	// 완전 해소
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Restroom = 1.0f;

	// 아래 2종은 대응하는 MinutesToLimit 이 0 이라 지금은 쓰이지 않는다.
	// 값을 미리 넣어두면 "근거 있는 값"처럼 보이므로 Phase 를 켤 때 같이 정한다.
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Thirst = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Fatigue = 0.f;

	// 완전 해소. 공연 관람은 배고픔과 달리 한 번에 끝나는 경험이다.
	// 재방문은 StimulationMinutesToLimit 이 다시 쌓아주는 쪽에서 나온다.
	// 0 으로 되돌리면 Relieve 가 이 성분을 통째로 건너뛴다 — 상승 계수와 짝이다.
	UPROPERTY(BlueprintReadWrite, Category = "Need")
	float Stimulation = 1.0f;

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

/**
 * 체류시간을 Need 로 깎은 결과. 빠진 예산과 그중 Need 몫을 나눠서 돌려준다.
 *
 * ### 먼저 알아야 하는 것
 * `RemainingStayMinutes` 는 시계가 아니라 "예산"이다
 * 
 *     배수 2.0, Need 한계 → 실제 10분이 지날 때마다 예산이 20 씩 빠진다
 *     체류 예산 240 으로 입장한 개체는 실제로는 120분만 머물고 나간다.
 *
 * NPC 가 앞으로 더 머물 의향이 있는 분"이고, 드레인은 이 잔량을 시계보다 빨리 깎는다.
 * 그래서 아래 값이 `ElapsedMinutes` 보다 클 수 있다.
 *
 * "체류시간 분포가 왜 짧아졌나" 진단이 용이하도록 Need 몫을 따로 준다.
 * 필드로 저장하지 않고 매번 돌려주는 값이다(누적은 호출부의 몫).
 */
USTRUCT(BlueprintType)
struct FFF_StayConsumption
{
	GENERATED_BODY()

	// `RemainingStayMinutes` 에서 뺄 값. 단위는 "체류 예산 분"이다.
	UPROPERTY(BlueprintReadOnly, Category = "Need")
	float ConsumedStayBudgetMinutes = 0.f;

	// 위 값 중 Need 때문에 추가로 빠진 예산 분. 배수가 1.0(기능 꺼짐)이면 0 이다.
	UPROPERTY(BlueprintReadOnly, Category = "Need")
	float ExtraStayBudgetFromNeed = 0.f;
};

/**
 * FFF_NeedState 를 실제로 움직이는 규칙. 상태를 갖지 않는 순수 함수만 제공한다 —
 * (FFF_UtilityDestinationSelector 와 같은 형태)
 * 
 * ### 시간 기준
 * `ElapsedMinutes` 는 'Live 경과분'이다(`utility_ai_spec.md` 의 모든 시간 필드와 같은 기준).
 * 실시간 초가 아니다 — 게임 시간 배속이 바뀌어도 Need 는 축제 시각을 따라 쌓여야 한다.
 *
 * ### 확정된 결정 2건 (사용자, 2026-08-10)
 *
 * **1. 줄에서 기다리는 동안에도 Need 는 계속 쌓인다.** `Advance` 를 부르는 쪽이 대기 중 여부로
 * 분기하지 않는다. 줄이 길면 배고픔이 더 쌓여 다음 재평가에서 다른 선택이 나오고, 그게
 * reneging(줄 이탈)의 동기가 된다 — 급한 욕구가 다른 시설을 이기게 되니까.
 * **L3 가 "대기 중에는 Advance 를 건너뛰는" 최적화를 넣지 말 것.** 동작이 바뀐다.
 *
 * **2. 입장 시점 Need 는 0 이 아니다** — `MakeInitialNeeds` 가 개체별로 흩뿌린다. 현실에서는
 * 마지막 식사·화장실로부터 시간이 지난 채 입장하기 때문이다. 이게 없으면 전원이 같은 Need 로
 * 출발해서 배고픔 곡선이 장내에서 동기화되고, 식음료 쏠림이 한 번에 몰린다.
 */
class FFF_NeedRules
{
public:
	/**
	 * 시간이 지난 만큼 Need 를 올린다. 0~1 로 clamp 한다.
	 *
	 * clamp 가 중요한 이유: Utility 식이 `weight_need`(1.8) × Need 를 곱하는데, 그 1.8 은
	 * "Need 최대치가 1.0"을 전제로 weight_attraction + lambda_affinity(1.6)보다 크게 잡은 값이다
	 * (FF_NeedState.h 의 FFF_NeedPriorityWeights 주석). 1.0 을 넘기면 그 부등식이 깨진다.
	 *
	 * ElapsedMinutes 가 음수면 아무것도 하지 않는다 — 시간이 거꾸로 가는 것은 호출부 버그이고,
	 * 그대로 계산하면 Need 가 조용히 줄어들어 "해소된 것"처럼 보인다.
	 */
	static FFF_NeedState Advance(
		const FFF_NeedState& Current,
		float ElapsedMinutes,
		const FFF_NeedRiseRates& RiseRates);

	/**
	 * 시설 이용을 마쳤을 때 해당 성분을 깎는다. 0 아래로 내려가지 않는다.
	 *
	 * 어떤 성분을 깎는지는 FFF_ZoneSystemBridge::GetNeedReliefFlags 가 판정한 결과를 그대로 쓴다 —
	 * 여기서 카테고리를 다시 해석하지 않는다(같은 판단이 두 곳에 있으면 갈라진다).
	 *
	 * 호출 시점은 "이용 완료"다. 목적지를 고른 순간이나 도착한 순간이 아니다 — 줄만 서고
	 * 포기한 NPC 의 Need 가 풀려버리면 대기열 이탈이 이득이 된다.
	 */
	static FFF_NeedState Relieve(
		const FFF_NeedState& Current,
		const FFF_NeedReliefFlags& ReliefFlags,
		const FFF_NeedReliefAmounts& ReliefAmounts);

	/**
	 * 입장 시점 Need. 개체마다 다르게, 같은 개체는 항상 같게.
	 *
	 * ### 왜 0 에서 시작하지 않는가
	 *
	 * 전원이 0 에서 출발하면 배고픔 곡선이 장내에서 동기화된다 — 입장 시각만 다를 뿐 "몇 분 뒤에
	 * 배고파지는가"가 모두 같아서, 식음료 쏠림이 계단처럼 한 번에 몰린다. 현실에서는 마지막
	 * 식사·화장실로부터 지난 시간이 사람마다 다르다.
	 *
	 * ### 어떻게 흩뿌리는가 — "이미 N분이 지난 상태로 입장한다"
	 *
	 * "한계까지 몇 %나 와 있는 상태로 입장하는가"를 뽑는다. Need 는 한계까지 0→1 로 선형이므로
	 * **그 비율이 곧 Need 값이다** — 성분마다 따로 계수를 둘 필요가 없다(한계 시간이 다른 것은
	 * 이후 `Advance` 가 알아서 반영한다. 화장실은 150분, 배고픔은 240분 기준으로 각자 오른다).
	 *
	 *     초기값 = Triangular(0, ModeFraction, MaxFraction)   ← 그 자체가 Need 값(0~1)
	 *
	 * 기본값 Mode 0.15 / Max 0.50 은 **근거 없음(draft_assumption)**이다. 다만 모양에는 이유가
	 * 있다 — 사람들은 보통 식사·화장실을 해결하고 나오므로 낮은 쪽에 몰리게 했고, Max 를 0.5 로
	 * 끊은 것은 입장 직후 곧바로 화장실로 직행하는 개체를 v0 에서 만들지 않기 위한 상한이다.
	 *
	 * 성분별로 **독립된 난수 스트림**을 쓴다 — 배고픈 사람이 화장실도 급하다는 상관을 넣을 근거가
	 * 없다. 넣으려면 `FFF_IndividualVariationResolver` 의 leisureliness 상관처럼 명시적으로 한다.
	 *
	 * 쌓이지 않는 Need(`MinutesToLimit == 0`)는 0 에서 시작한다 — 안 쌓이는데 초기값이 있으면
	 * 영원히 그 값에 머문다.
	 *
	 * @param StableAgentId  crowdAI 가 Hash(GroupID, 그룹 내 순번)으로 계산해 주는 값
	 * @param RiseRates      **켜진 Need 를 가려내는 데만 쓴다** — 안 쌓이는 성분은 0 에서 시작한다
	 */
	static FFF_NeedState MakeInitialNeeds(
		int32 GlobalSimulationSeed,
		int32 StableAgentId,
		int32 SpawnGeneration,
		const FFF_NeedRiseRates& RiseRates,
		float ModeFractionOfLimit = 0.15f,
		float MaxFractionOfLimit = 0.50f);

	/**
	 * 시간이 지난 만큼 체류시간을 소모한다.
	 * 해소하지 못한 Need 가 쌓여 있으면 더 빨리 소모된다.
	 *
	 * ### 왜 퇴장 조건을 늘리지 않고 체류시간을 깎는가
	 * "Need 가 한계면 퇴장"이라는 게이트를 따로 만들면 판정 지점이 둘이 되고,
	 * 붐빔 → 다수가 동시에 한계 → 대량 퇴장 → 붐빔 해소 → 남은 사람은 만족. 
	 * 실제 축제에서 안 보이는 톱니 모양이 만들어진다.
	 * 체류시간을 깎으면 퇴장 판정은 여전히 "체류시간 소진" 한 곳이고, 이탈은 개체마다
	 * 다른 시각에 흩어져 일어난다(초기 Need 가 흩뿌려져 있으므로).
	 *
	 * ### 기본값 1.0 = 기능 꺼짐
	 * `DrainMultiplierAtMaxNeed` 의 근거 있는 값이 없다. 그럴듯한 상수(1.5 등)를 기본값으로
	 * 넣으면 "근거 있는 값"처럼 보이므로 **중립값 1.0(= 안 깎임)** 을 기본으로 둔다.
	 * 켜는 것은 코드 수정이 아니라 CSV 값 변경이다 — P0 Need 게이트와 같은 방식이다.
	 *
	 * ### 선형 계산
	 *     배수    = 1 + (DrainMultiplierAtMaxNeed - 1) × 전 성분 최대 가중 급함(0~1)
	 *     소모 분 = ElapsedMinutes × 배수
	 *
	 * ### 부르지 말아야 하는 때
	 * 예약 프로그램 잠금 중에는 부르지 않는다.
	 * 예약은 체류시간을 프로그램 종료까지 늘려놓는데(`reservation_program_spec.md` §5 ②), 
	 * 그 늘린 시간을 Need 가 다시 깎으면 예약자가 프로그램 도중에 나간다. 
	 * 
	 * Need 증가(`Advance`)는 잠금 중에도 계속 부른다.
	 *
	 * @param Needs                     현재 Need. Advance 를 부른 '뒤'의 값을 넣는다
	 * @param ElapsedMinutes            Live 경과분. 음수·0 이면 아무것도 소모하지 않는다
	 * @param DrainMultiplierAtMaxNeed  한계 Need 에서의 소모 배수. 1.0 이면 기능 꺼짐
	 */
	static FFF_StayConsumption ConsumeStay(
		const FFF_NeedState& Needs,
		float ElapsedMinutes,
		float DrainMultiplierAtMaxNeed,
		const FFF_NeedPriorityWeights& Priority = FFF_NeedPriorityWeights());

	// utility_coefficients DataTable 에서 읽는다. 테이블이 없거나 행이 없으면 위 구조체 기본값을 쓴다.
	static FFF_NeedRiseRates LoadRiseRates(const UDataTable* UtilityCoefficientTable);

	// 기본값 1.0 = 기능 꺼짐. ConsumeStay 주석 참조.
	static float LoadStayDrainMultiplierAtMaxNeed(const UDataTable* UtilityCoefficientTable);
	static FFF_NeedReliefAmounts LoadReliefAmounts(const UDataTable* UtilityCoefficientTable);

	// 초기값 흩뿌리기 계수 2개를 DataTable 에서 읽어 MakeInitialNeeds 에 넘길 값으로 돌려준다.
	static void LoadInitialNeedFractions(
		const UDataTable* UtilityCoefficientTable,
		float& OutModeFractionOfLimit,
		float& OutMaxFractionOfLimit);
};
