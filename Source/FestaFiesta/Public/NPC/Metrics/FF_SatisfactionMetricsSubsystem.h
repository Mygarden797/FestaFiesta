// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FF_SatisfactionMetricsSubsystem.generated.h"

class UDataTable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnSimSatisfactionChanged, int32, NewSimSatisfaction);

// 만족도 산출 계수. 
// UtilityCoefficient DataTable(`FFF_UtilityCoefficientRow`)에서 개별 행으로 로드하며, 
// 테이블이 없거나 행이 없으면 여기 적힌 기본값으로 대체한다
struct FFF_SatisfactionCoefficients
{
	// 세 입력의 혼합 가중치. 셋을 더해 1이 안 되면 Commit에서 정규화
	// 동일 가중치(1/3씩)로 시작한다(draft_assumption)
	float WeightWait = 1.f / 3.f;
	float WeightDensity = 1.f / 3.f;
	float WeightPreference = 1.f / 3.f;

	// 이 대기시간(초) 이상이면 대기 점수가 0으로 바닥난다
	float WaitRefSeconds = 300.f;

	// PreferenceTerm(Choose()가 이미 계산한 -lambda_affinity~+lambda_affinity 스케일 값)을
	// -1~1로 되돌리는 기준값. lambda_affinity 기본값(0.6)을 그대로 물려받았다 —
	// 그 계수가 바뀌면 이 값도 같이 봐야 한다(draft_assumption, 자동 동기화 아님)
	float PreferenceTermRef = 0.6f;

	// LOS 등급별 점수(0~100, 높을수록 쾌적). 
	// FCrowdDensityGrid::ECrowdLOS 순서(A~F)와 인덱스로 1:1 대응
	float LOSGradeScore[6] = { 100.f, 85.f, 65.f, 40.f, 15.f, 0.f };

	// GradeIndex가 범위를 벗어나면 가장 나쁜 등급(F)으로 클램프
	float ScoreForLOSGrade(uint8 GradeIndex) const
	{
		return LOSGradeScore[FMath::Clamp<int32>(GradeIndex, 0, 5)];
	}

	static FFF_SatisfactionCoefficients LoadFromTable(const UDataTable* UtilityCoefficientTable);
};

/**
 * 만족도(SimSatisfaction) 집계 계산기 — 대기·밀도·취향매칭 세 입력을 0~100으로 정규화해 가중합한다.
 *
 * === 입력 세 가지와 채우는 쪽 ===
 * - 대기: `UFF_CrowdSimulationSubsystem::AdvanceNeeds`가 Zone `BuildReadModel()`의
 *   시설별 `AverageCompletedWaitSeconds`를 시설당 한 번씩 넣는다. 완료자가 없는 시설(0.0)은 안 넣는다.
 *   
 * - 밀도: 같은 함수가 `CrowdReadModel.OccupiedCells`를 순회하며 셀마다 LOS 등급 점수를
 *   그 셀의 인원 수로 가중해서 넣는다. 다만 Fruin LOS는 원래 보행 흐름 기준이라 "공연 보려고 몰린 붐빔"에 그대로 맞는지는 미검증이다.
 *   
 * - 취향매칭: `UFF_CrowdDecisionSubsystem::HandlePartyNeedsDecision`이 `Choose()`가
 *   이미 계산한 `PreferenceTerm`을 결정 확정 시점에 샘플링한다.
 *
 * === 한계 ===
 * 대기 입력은 "완료된 방문"만 잡기 때문에 만석 거절(JoinRejected)이나 인내심 이탈로 못 끝낸 방문은 표본에서 빠진다.
 * 생존자 편향이 있다는 뜻이다. Need 미해소율도 이 산식에 안 들어간다. 둘 다
 * 알려진 gap이며 utility_ai_todo.md §5-D에 열린 결정으로 남겨뒀다(향후 확장 후보)
 *
 * === 표본 0일 때 ===
 * `FFF_NeedMetricsAggregator`와 다르게 0으로 리셋하지 않고 직전 확정값을 유지한다.
 * 이 지표는 Overview HUD에 상시 노출되는 단일 숫자이기 때문이다.
 * 세 입력 중 하나라도 비면 부분 데이터라는 뜻으로 가장 적게 들어온 입력의 표본 수를 돌려준다
 */
struct FFF_SatisfactionAggregator
{
	// 집계 주기 시작. 누적 버퍼를 비운다
	void Begin();

	// 시설 하나의 평균 완료 대기시간(초). Begin() 없이 부르면 false
	bool AddWaitSample(float CompletedWaitSeconds);

	// 점유 셀 하나의 쾌적도 점수(0~100, ScoreForLOSGrade로 미리 매긴 값)와 그 셀의 인원 수.
	// Weight가 0 이하면 무시한다(빈 셀을 넣을 이유가 없다)
	bool AddDensitySample(float Score0To100, int32 Weight);

	// 확정된 결정 하나의 취향매칭 정도. -1(비선호)~1(선호). Choose()가 no_choice를
	// 돌려준 회차는 애초에 호출하지 않는다
	bool AddPreferenceSample(float PreferenceNorm);

	// 누적을 확정한다. 값이 실제로 바뀌었으면 true
	bool Commit(const FFF_SatisfactionCoefficients& Coefficients);

	int32 GetSimSatisfaction() const { return CommittedSatisfaction; }
	int32 GetWaitScore() const { return CommittedWaitScore; }
	int32 GetDensityScore() const { return CommittedDensityScore; }
	int32 GetPreferenceScore() const { return CommittedPreferenceScore; }

	// 이번 확정 주기에 들어간 표본 수 중 최솟값 — 세 입력 중 하나라도 비면 그 사실이 드러난다
	int32 GetSampleCount() const { return CommittedSampleCount; }

	bool IsAggregating() const { return bAggregating; }

private:
	// 첫 Commit 전 중립값. 더미 72를 그대로 물려받지 않는다 — "데이터가 아직 없다"를
	// "이미 잘 되고 있다"로 보이게 하지 않기 위해서다
	static constexpr int32 NeutralScore = 50;

	int32 CommittedSatisfaction = NeutralScore;
	int32 CommittedWaitScore = NeutralScore;
	int32 CommittedDensityScore = NeutralScore;
	int32 CommittedPreferenceScore = NeutralScore;
	int32 CommittedSampleCount = 0;

	double PendingWaitSum = 0.0;
	int32 PendingWaitCount = 0;
	double PendingDensityWeightedScoreSum = 0.0;
	int64 PendingDensityWeight = 0;
	double PendingPreferenceSum = 0.0;
	int32 PendingPreferenceCount = 0;
	bool bAggregating = false;
};

/**
 * 만족도(SimSatisfaction) 집계 지표
 * UI(Overview HUD)용 읽기 창구. 위 계산기를 감싸 델리게이트만 붙인다.
 *
 * 채우는 쪽은 `UFF_CrowdSimulationSubsystem`(대기·밀도)과 `UFF_CrowdDecisionSubsystem` (취향매칭)이다. 
 * 확정 주기는 기본 30스텝 = 1초
 */
UCLASS()
class FESTAFIESTA_API UFF_SatisfactionMetricsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 읽기 (UI) ---

	// 0~100. 대기·밀도·취향 매칭에서 나오는 순수 시뮬레이션 값
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetSimSatisfaction() const { return Aggregator.GetSimSatisfaction(); }

	// 대기 입력만의 하위 점수. 0~100, 디버그·분해 표시용
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetWaitScore() const { return Aggregator.GetWaitScore(); }

	// 밀도 입력만의 하위 점수. 0~100, 디버그·분해 표시용
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetDensityScore() const { return Aggregator.GetDensityScore(); }

	// 취향매칭 입력만의 하위 점수. 0~100, 디버그·분해 표시용
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetPreferenceScore() const { return Aggregator.GetPreferenceScore(); }

	// 이번 확정 주기의 최소 표본 수. 0이면 세 입력 중 하나 이상이 이번 주기엔 안 들어왔다는 뜻
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Satisfaction")
	int32 GetSampleCount() const { return Aggregator.GetSampleCount(); }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|NPC|Satisfaction")
	FFF_OnSimSatisfactionChanged OnSimSatisfactionChanged;

	// --- 쓰기 (UFF_CrowdSimulationSubsystem / UFF_CrowdDecisionSubsystem 측에서 호출) ---

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC|Satisfaction")
	void BeginAggregation() { Aggregator.Begin(); }

	void AddWaitSample(float CompletedWaitSeconds) { Aggregator.AddWaitSample(CompletedWaitSeconds); }
	void AddDensitySample(float Score0To100, int32 Weight) { Aggregator.AddDensitySample(Score0To100, Weight); }
	void AddPreferenceSample(float PreferenceNorm) { Aggregator.AddPreferenceSample(PreferenceNorm); }

	// 누적을 확정하고, 값이 실제로 바뀌었을 때만 델리게이트를 쏜다. Coefficients는
	// 호출부가 이미 밀도 표본을 매기려고 로드해 둔 것을 그대로 넘긴다 — 여기서 또
	// DataTable을 읽지 않는다
	void CommitAggregation(const FFF_SatisfactionCoefficients& Coefficients);

private:
	FFF_SatisfactionAggregator Aggregator;
};
