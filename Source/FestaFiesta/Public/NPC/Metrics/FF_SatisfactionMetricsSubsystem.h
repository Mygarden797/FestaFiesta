// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_SatisfactionMetricsSubsystem.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSimSatisfactionChanged, int32, NewSimSatisfaction);

/**
 * 만족도 산출 계수.
 * UtilityCoefficient DataTable(`FFF_UtilityCoefficientRow`)에서 개별 행으로 로드하며,
 * 테이블이 없거나 행이 없으면 여기 적힌 기본값으로 대체한다.
 *
 * === 2단 계층 구조 ===
 * 대기·밀도·취향매칭·Need 충족도 네 입력을 평평하게 한 번에 섞지 않고, 두 축으로 먼저 묶는다. 
 * FESTPERF(축제 서비스 품질 척도)의 "Environment"(청결·혼잡·화장실)·"Core Service"(콘텐츠 매력) 구분과 
 * ACSI류 구조적 만족도 모델을 참고했다
 *
 *   EnvironmentScore = WeightEnvironmentWait * WaitScore + WeightEnvironmentDensity * DensityScore
 *   PersonalFitScore = WeightPersonalFitPreference * PreferenceScore + WeightPersonalFitNeed * NeedFulfillmentScore
 *   SimSatisfaction  = WeightEnvironment * EnvironmentScore + WeightPersonalFit * PersonalFitScore
 */
struct FFF_SatisfactionCoefficients
{
	// 1단계: 환경 축 vs 개인 부합도 축. 합이 1이 안 되면 Commit에서 정규화한다
	float WeightEnvironment = 0.5f;
	float WeightPersonalFit = 0.5f;

	// 환경 축 내부: 대기 vs 밀도
	// 균등가중(0.5/0.5)으로 시작하는 이유: 축제 만족도 연구에서 "중요도 가중을
	// 정교화해도 단순 평균 대비 예측 타당도가 딱히 개선되지 않는다"는 결과가 있다
	float WeightEnvironmentWait = 0.5f;
	float WeightEnvironmentDensity = 0.5f;

	// 개인 부합도 축 내부: 취향매칭 vs Need 충족도
	float WeightPersonalFitPreference = 0.5f;
	float WeightPersonalFitNeed = 0.5f;

	// 이 대기시간(초) 이상이면 대기 점수가 0으로 바닥난다
	float WaitRefSeconds = 300.f;

	// PreferenceTerm(Choose()가 이미 계산한 -lambda_affinity~+lambda_affinity 스케일 값)을
	// -1~1로 되돌리는 기준값. lambda_affinity 기본값(0.6)을 그대로 물려받았다 —
	// 그 계수가 바뀌면 이 값도 같이 봐야 한다(draft_assumption, 자동 동기화 아님)
	float PreferenceTermRef = 0.6f;

	// LOS 등급별 점수(0~100, 높을수록 쾌적). FCrowdDensityGrid::ECrowdLOS 순서(A~F)와
	// 인덱스로 1:1 대응
	float LOSGradeScore[6] = { 100.f, 85.f, 65.f, 40.f, 15.f, 0.f };

	// GradeIndex가 범위를 벗어나면 가장 나쁜 등급(F)으로 클램프
	float ScoreForLOSGrade(uint8 GradeIndex) const
	{
		return LOSGradeScore[FMath::Clamp<int32>(GradeIndex, 0, 5)];
	}

	// 시설 커버리지: 그 욕구를 해소하는 운영 중 시설 수가 목표에 못 미치면 Need 충족도에 비율을 곱한다.
	// 체류가 짧아 개인 욕구가 한계까지 거의 차지 않으므로, 이게 없으면 시설이 모자란 배치도 충족도가 100 근처로 나온다
	float NeedCoverageTargetHunger = 7.f;
	float NeedCoverageTargetRestroom = 2.f;
	float NeedCoverageTargetStimulation = 10.f;
	float NeedCoverageFloor = 0.3f;

	// 0이면 시설이 하나도 없어도 하한만큼은 남긴다. 목표가 0 이하이면 커버리지를 보지 않는다
	float NeedCoverageMultiplier(int32 OpenFacilityCount, float Target) const
	{
		if (Target <= 0.f)
		{
			return 1.f;
		}
		const float Floor = FMath::Clamp(NeedCoverageFloor, 0.f, 1.f);
		return FMath::Clamp(static_cast<float>(OpenFacilityCount) / Target, Floor, 1.f);
	}

	static FFF_SatisfactionCoefficients LoadFromTable(const UDataTable* UtilityCoefficientTable);
};

/**
 * 만족도(SimSatisfaction) 집계 계산기 — 대기·밀도·취향매칭·Need 충족도 네 입력을 0~100으로
 * 정규화해 2단 계층으로 가중합한다(위 struct 주석 참조).
 *
 * === 입력 네 가지와 채우는 쪽 ===
 * - 대기: `UFF_CrowdSimulationSubsystem::AdvanceNeeds`가 Zone `BuildReadModel()`의
 *   시설별 `AverageCompletedWaitSeconds`를 시설당 한 번씩 넣는다(`AddWaitSample`).
 *   같은 함수가 이번 창의 큐 가입 성공률(수락 / (수락+거절+인내이탈))도 `SetQueueSuccessRate`로
 *   같이 넣는다 — 완료된 방문만 재던 생존자 편향을 이 성공률로 보정한다. 완료자가 이번
 *   창에 하나도 없어도 성공률만 있으면 "안 기다렸다(100점)를 성공률만큼 깎는" 것으로
 *   대체한다 — 전원 거절당한 창이 통째로 안 잡히는 것을 막기 위해서다
 *   
 * - 밀도: 같은 함수가 `CrowdReadModel.OccupiedCells`를 순회하며 셀마다 LOS 등급 점수를
 *   그 셀의 인원 수로 가중해서 넣는다(`AddDensitySample`). Fruin LOS는 원래 보행 흐름
 *   기준이라 "공연 보려고 몰린 붐빔"에 그대로 맞는지는 미검증이다(draft_assumption)
 *   
 * - 취향매칭: `UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision`이 `Choose()`가
 *   이미 계산한 `PreferenceTerm`을 결정 확정 시점에 샘플링한다(`AddPreferenceSample`,
 *   파티 단위, no_choice는 애초에 표본이 아니다). 결정 처리는 집계 창이 닫힌 뒤에 도므로
 *   창 밖의 표본은 모아 두었다가 다음 창이 이어받는다
 *   
 * - Need 충족도: 같은 `AdvanceNeeds`가 `UFF_NpcNeedMetricsSubsystem`의 현재 켜져 있는
 *   Need 성분(Hunger·Restroom·Stimulation — Thirst·Fatigue는 Phase 게이트로 아직 0이라
 *   넣으면 항상 100%로 왜곡된다)별 미해소율을 `100 - 미해소율`로 뒤집어 넣는다
 *
 * === 알려진 한계 ===
 * 대기 성공률 계산이 "이번 1초 창"의 수락/거절/이탈 델타 기준이라 표본이 작을 때(초당 시도 1~2건) 노이즈가 크다 — 
 * 창을 늘리는 것은 별도 결정. LOS가 보행 흐름 기준이라는 점은 여전히 미검증이다
 *
 * === 표본 0일 때 ===
 * `FFF_NeedMetricsAggregator`와 다르게 0으로 리셋하지 않고 직전 확정값을 유지한다.
 * `GetSampleCount()`로 "이번 주기엔 갱신 안 됨"을 구분할 수 있다
 * 네 입력 중 하나라도 비면 부분 데이터라는 뜻으로 가장 적게 들어온 입력의 표본 수를 돌려준다
 */
struct FFF_SatisfactionAggregator
{
	// 집계 주기 시작. 누적 버퍼를 비운다
	void Begin();

	// 시설 하나의 평균 완료 대기시간(초). Begin() 없이 부르면 false
	bool AddWaitSample(float CompletedWaitSeconds);

	// 이번 창의 큐 가입 성공률(0~1) = 수락 / (수락+거절+인내이탈). 이번 창에 시도가
	// 하나도 없었으면 호출하지 않는다(호출 안 하면 곱하지 않고 1.0으로 취급)
	bool SetQueueSuccessRate(float SuccessRate01);

	// 점유 셀 하나의 쾌적도 점수(0~100, ScoreForLOSGrade로 미리 매긴 값)와 그 셀의 인원 수.
	// Weight가 0 이하면 무시한다(빈 셀을 넣을 이유가 없다). bCrowded 는 그 칸이 붐비는 등급(D 이하)인지 —
	// 결산이 "붐비는 곳에 있던 손님 비율"을 근거로 보여 준다
	bool AddDensitySample(float Score0To100, int32 Weight, bool bCrowded = false);

	// 확정된 결정 하나의 취향매칭 정도. -1(비선호)~1(선호). Choose()가 no_choice를
	// 돌려준 회차는 애초에 호출하지 않는다.
	// 다른 입력과 달리 Begin() 없이 불러도 받는다 — 결정은 집계 창 밖에서 확정되므로,
	// 창 밖의 표본은 모아 두었다가 다음 Begin() 이 이어받는다
	bool AddPreferenceSample(float PreferenceNorm);

	// Need 성분 하나의 충족도(0~100 = 100 - 미해소율%). 켜져 있는 성분마다 한 번씩 부른다
	bool AddNeedFulfillmentSample(float FulfillmentScore0To100);

	// 방문객 한 명에게 붙은 감점(0~100점). 활성 방문객마다 한 번씩 부른다
	// 감점이 없는 사람도 0으로 넣어야 평균이 "전체 방문객 기준"이 된다. 
	// 사건 처리 실패 같은 외부 요인이라, 블렌드가 끝난 최종 점수에서 평균 감점을 뺀다.
	bool AddPenaltySample(float PenaltyPoints);

	// 누적을 확정한다. 값이 실제로 바뀌었으면 true
	bool Commit(const FFF_SatisfactionCoefficients& Coefficients);

	int32 GetSimSatisfaction() const { return CommittedSatisfaction; }
	int32 GetWaitScore() const { return CommittedWaitScore; }
	int32 GetDensityScore() const { return CommittedDensityScore; }
	int32 GetPreferenceScore() const { return CommittedPreferenceScore; }
	int32 GetNeedFulfillmentScore() const { return CommittedNeedFulfillmentScore; }
	int32 GetEnvironmentScore() const { return CommittedEnvironmentScore; }
	int32 GetPersonalFitScore() const { return CommittedPersonalFitScore; }

	// 최종 점수에서 빠진 평균 감점(점). 0이면 감점이 없다. 디버그·분해 표시용
	int32 GetPenaltyPoints() const { return CommittedPenaltyPoints; }

	// 감점을 빼기 전 점수. 결산 만족도 분해가 "100 에서 항목별로 깎인 점수"를 이 값에 맞춘다
	int32 GetBaseSatisfaction() const { return CommittedBaseSatisfaction; }

	// 하위 점수의 근거(마지막 확정 창 기준). 모르면 음수.
	// 평균 완료 대기(초) · 줄 가입 성공률(0~1) · 붐비는 칸에 있던 인원 비율(0~1) · 취향에 맞는 곳을 고른 결정 비율(0~1)
	float GetAverageWaitSeconds() const { return CommittedAverageWaitSeconds; }
	float GetQueueSuccessRate() const { return CommittedQueueSuccessRate; }
	float GetCrowdedShare() const { return CommittedCrowdedShare; }
	float GetPreferenceMatchShare() const { return CommittedPreferenceMatchShare; }
	int32 GetPreferenceSampleCount() const { return CommittedPreferenceSampleCount; }

	// 이번 확정 주기에 들어간 표본 수 중 최솟값 — 넷 중 하나라도 비면 그 사실이 드러난다
	int32 GetSampleCount() const { return CommittedSampleCount; }

	bool IsAggregating() const { return bAggregating; }

private:
	// 첫 Commit 전 중립값. 더미 72를 그대로 물려받지 않는다 — "데이터가 아직 없다"를
	// "이미 잘 되고 있다"로 보이게 하지 않기 위해서다
	static constexpr int32 NeutralScore = 50;

	// 감점을 빼기 전 점수. 이번 주기에 블렌드할 재료가 없으면 이 값에서 다시 감점을 뺀다 —
	// 이미 감점된 CommittedSatisfaction 에서 또 빼면 감점이 창마다 쌓인다
	int32 CommittedBaseSatisfaction = NeutralScore;
	int32 CommittedPenaltyPoints = 0;

	int32 CommittedSatisfaction = NeutralScore;
	int32 CommittedWaitScore = NeutralScore;
	int32 CommittedDensityScore = NeutralScore;
	int32 CommittedPreferenceScore = NeutralScore;
	int32 CommittedNeedFulfillmentScore = NeutralScore;
	int32 CommittedEnvironmentScore = NeutralScore;
	int32 CommittedPersonalFitScore = NeutralScore;
	int32 CommittedSampleCount = 0;
	float CommittedAverageWaitSeconds = -1.f;
	float CommittedQueueSuccessRate = -1.f;
	float CommittedCrowdedShare = -1.f;
	float CommittedPreferenceMatchShare = -1.f;
	int32 CommittedPreferenceSampleCount = 0;

	double PendingWaitSum = 0.0;
	int32 PendingWaitCount = 0;
	float PendingQueueSuccessRate = 1.f;
	bool bHasQueueSuccessRateThisWindow = false;
	double PendingDensityWeightedScoreSum = 0.0;
	int64 PendingDensityWeight = 0;
	int64 PendingCrowdedWeight = 0;
	double PendingPreferenceSum = 0.0;
	int32 PendingPreferenceCount = 0;
	int32 PendingPreferenceMatchedCount = 0;
	// 집계 창 밖에서 들어온 취향 표본. 다음 Begin 이 Pending 으로 넘겨받고 비운다
	double CarriedPreferenceSum = 0.0;
	int32 CarriedPreferenceCount = 0;
	int32 CarriedPreferenceMatchedCount = 0;
	double PendingNeedFulfillmentSum = 0.0;
	int32 PendingNeedFulfillmentCount = 0;
	double PendingPenaltySum = 0.0;
	int32 PendingPenaltyCount = 0;
	bool bAggregating = false;
};

/**
 * 만족도(SimSatisfaction) 집계 지표
 * UI(Overview HUD)용 읽기 창구. 위 계산기를 감싸 델리게이트만 붙인다.
 *
 * 채우는 쪽은 `UFF_CrowdSimulationSubsystem`(대기·큐 성공률·밀도·Need 충족도)과
 * `UFF_CrowdDecisionSubsystem`(취향매칭)이다. 확정 주기는 `NeedMetricsIntervalSteps`와
 * 같은 스텝 게이트를 재사용한다(기본 30스텝 = 1초) — 별도 순회를 새로 만들지 않는다.
 */
UCLASS()
class FESTAFIESTA_API UFF_SatisfactionMetricsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 읽기 (UI) ---

	/** 0~100. 대기·밀도·취향 매칭·Need 충족도에서 나오는 순수 시뮬레이션 값 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetSimSatisfaction() const { return Aggregator.GetSimSatisfaction(); }

	/** 대기 입력만의 하위 점수(성공률 보정 포함). 0~100, 디버그·분해 표시용 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetWaitScore() const { return Aggregator.GetWaitScore(); }

	/** 밀도 입력만의 하위 점수. 0~100, 디버그·분해 표시용 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetDensityScore() const { return Aggregator.GetDensityScore(); }

	/** 취향매칭 입력만의 하위 점수. 0~100, 디버그·분해 표시용 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetPreferenceScore() const { return Aggregator.GetPreferenceScore(); }

	/** Need 충족도 입력만의 하위 점수. 0~100, 디버그·분해 표시용 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetNeedFulfillmentScore() const { return Aggregator.GetNeedFulfillmentScore(); }

	/** 환경 축(대기+밀도) 점수. 0~100 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetEnvironmentScore() const { return Aggregator.GetEnvironmentScore(); }

	/** 개인 부합도 축(취향+Need) 점수. 0~100 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetPersonalFitScore() const { return Aggregator.GetPersonalFitScore(); }

	/** 이번 확정 주기의 최소 표본 수. 0이면 네 입력 중 하나 이상이 이번 주기엔 안 들어왔다는 뜻 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetSampleCount() const { return Aggregator.GetSampleCount(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|NPC|Satisfaction")
	FFF_OnSimSatisfactionChanged OnSimSatisfactionChanged;

	// --- 쓰기 (UFF_CrowdSimulationSubsystem / UFF_CrowdDecisionSubsystem이 부른다) ---

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC|Satisfaction")
	void BeginAggregation() { Aggregator.Begin(); }

	void AddWaitSample(float CompletedWaitSeconds) { Aggregator.AddWaitSample(CompletedWaitSeconds); }
	void SetQueueSuccessRate(float SuccessRate01) { Aggregator.SetQueueSuccessRate(SuccessRate01); }
	void AddDensitySample(float Score0To100, int32 Weight, bool bCrowded = false) { Aggregator.AddDensitySample(Score0To100, Weight, bCrowded); }
	void AddPreferenceSample(float PreferenceNorm) { Aggregator.AddPreferenceSample(PreferenceNorm); }
	void AddNeedFulfillmentSample(float FulfillmentScore0To100) { Aggregator.AddNeedFulfillmentSample(FulfillmentScore0To100); }
	void AddPenaltySample(float PenaltyPoints) { Aggregator.AddPenaltySample(PenaltyPoints); }

	/** 방문객 전체 평균으로 최종 점수에서 빠진 감점(점). 0이면 감점이 없다 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetPenaltyPoints() const { return Aggregator.GetPenaltyPoints(); }

	/** 결산 만족도 분해용 읽기. 의미는 FFF_SatisfactionAggregator 의 같은 이름 함수 */
	int32 GetBaseSatisfaction() const { return Aggregator.GetBaseSatisfaction(); }
	float GetAverageWaitSeconds() const { return Aggregator.GetAverageWaitSeconds(); }
	float GetQueueSuccessRate() const { return Aggregator.GetQueueSuccessRate(); }
	float GetCrowdedShare() const { return Aggregator.GetCrowdedShare(); }
	float GetPreferenceMatchShare() const { return Aggregator.GetPreferenceMatchShare(); }
	int32 GetPreferenceSampleCount() const { return Aggregator.GetPreferenceSampleCount(); }

	/** 마지막 확정에 쓴 계수(가중치 · 대기 기준 · 커버리지 목표). 확정 전이면 기본값 */
	const FFF_SatisfactionCoefficients& GetLastCoefficients() const { return LastCoefficients; }

	// 누적을 확정하고, 값이 실제로 바뀌었을 때만 델리게이트를 쏜다. Coefficients는
	// 호출부가 이미 밀도 표본을 매기려고 로드해 둔 것을 그대로 넘긴다 — 여기서 또
	// DataTable을 읽지 않는다
	void CommitAggregation(const FFF_SatisfactionCoefficients& Coefficients);

	/** 새 축제를 시작할 때 이전 판의 집계를 중립값으로 되돌린다. 이 서브시스템은 레벨을 건너 살아남는다. */
	void ResetForNewFestival();

private:
	UFUNCTION()
	void HandleFestivalStarted() { ResetForNewFestival(); }

	FFF_SatisfactionAggregator Aggregator;
	FFF_SatisfactionCoefficients LastCoefficients;
};
