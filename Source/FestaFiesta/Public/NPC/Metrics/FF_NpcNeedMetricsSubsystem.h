// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NPC/Party/FF_NeedState.h"
#include "FF_NpcNeedMetricsSubsystem.generated.h"

// (NeedType, 새 미해소율 0~100)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnNeedUnmetRatioChanged, EFF_NeedType, NeedType, float, NewRatioPercent);

/**
 * Need 집계 계산기
 *
 * === 단위·범위 ===
 * - 입력(`FFF_NeedState`의 각 성분): 0.0(해소됨) ~ 1.0(한계)
 * - `GetUnmetRatioPercent`: 0~100 (퍼센트)
 * - `GetAverage`: 0.0~1.0
 * - 미해소 판정: 성분 값 >= `UnmetThreshold` (기본 0.7)
 *
 * === 사용 흐름 ===
 * Begin();                                  // 집계 주기 시작
 * for (각 NPC) { AddSample(NPC.Needs); }    // 표본 누적 — 배열을 만들지 않는다
 * Commit(OutChangedNeeds);                  // 확정. 값이 바뀐 Need만 OutChangedNeeds로 알려준다
 * 
 * 집계 중에 값이 흔들려 보이지 않도록 Commit 전에는 이전 확정값이 그대로 보인다
 */
struct FFF_NeedMetricsAggregator
{
	static constexpr int32 NeedTypeCount = static_cast<int32>(EFF_NeedType::Count);

	// 집계 주기 시작. 누적 버퍼를 비운다
	void Begin();

	// 표본 하나 추가. Begin() 없이 부르면 false를 돌려주고 무시한다
	bool AddSample(const FFF_NeedState& Needs);

	// 누적을 확정한다. 값이 실제로 바뀐 Need만 OutChangedNeeds에 담아 돌려준다
	void Commit(TArray<EFF_NeedType>& OutChangedNeeds);

	// 미해소 비율. 0~100 (퍼센트). 표본이 없으면 0
	float GetUnmetRatioPercent(EFF_NeedType NeedType) const;

	// 평균값. 0.0~1.0. 미해소율과 달리 임계값에 의존하지 않는다
	float GetAverage(EFF_NeedType NeedType) const;

	// 직전 확정 집계에 들어간 표본 수. 0이면 "데이터 없음"이며 미해소율 0%와 다른 상태다
	int32 GetSampleCount() const { return CommittedSampleCount; }

	float GetUnmetThreshold() const { return UnmetThreshold; }
	void SetUnmetThreshold(float NewThreshold) { UnmetThreshold = FMath::Clamp(NewThreshold, 0.f, 1.f); }

	bool IsAggregating() const { return bAggregating; }

private:
	// 확정값 — Getter가 읽는 쪽
	float CommittedUnmetRatioPercent[NeedTypeCount] = {};
	float CommittedAverage[NeedTypeCount] = {};
	int32 CommittedSampleCount = 0;

	// 누적 중 — AddSample이 쓰는 쪽. Commit 전까지 Getter에 노출되지 않는다
	int32 PendingUnmetCount[NeedTypeCount] = {};
	double PendingSum[NeedTypeCount] = {};
	int32 PendingSampleCount = 0;
	bool bAggregating = false;

	// 미해소 판정 기준. draft_assumption — 실측 후 조정
	float UnmetThreshold = 0.7f;
};

/**
 * Need 집계 지표 — UI·Report(CauseId 롤업)용 읽기 창구. 위 계산기를 감싸 델리게이트만 붙인다.
 *
 * === 누가 채우는가 ===
 * `UFF_CrowdSimulationSubsystem::AdvanceNeeds()` 가 채운다. Need 를 올리는 유닛 순회에
 * 표본 수집을 얹은 것이라 집계 전용 순회가 따로 돌지 않는다.
 *
 * 확정 주기는 `NeedMetricsIntervalSteps`(기본 30스텝 = 1초)다. 매 프레임 확정하면
 * `OnNeedUnmetRatioChanged` 가 초당 수십 번 뜬다. 그 순회는 Live 단계에서만 도므로
 * Live 밖에서는 값이 마지막 확정값 그대로 멈춰 있다.
 *
 * 표본은 파티가 아니라 개인 단위다 — 지표의 정의가 "이 Need 가 미해소인 방문객 비율"이다.
 *
 * 표본이 0이면 모든 미해소율은 0을 돌려주고 `GetSampleCount()`가 0이다
 * UI는 `GetSampleCount() == 0`을 "데이터 없음"으로 구분해서 0%와 혼동하지 말아야 한다.
 *
 * 값마다 "현재값 Getter + 변경 델리게이트"를 쌍으로 제공한다 (UI_API.md 원칙 3·4).
 */
UCLASS()
class FESTAFIESTA_API UFF_NpcNeedMetricsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 읽기 (UI) ---

	/** 이 Need가 미해소 상태인 방문객 비율. **0~100 (퍼센트)**. 표본이 없으면 0 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Need")
	float GetNeedUnmetRatio(EFF_NeedType NeedType) const { return Aggregator.GetUnmetRatioPercent(NeedType); }

	// 이 Need의 평균값. 0.0~1.0
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Need")
	float GetNeedAverage(EFF_NeedType NeedType) const { return Aggregator.GetAverage(NeedType); }

	// 직전 집계에 들어간 방문객 수. 0이면 아직 아무도 표본을 넣지 않았다는 뜻(스텁 상태)
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Need")
	int32 GetSampleCount() const { return Aggregator.GetSampleCount(); }

	// 미해소로 판정하는 기준값. 0.0~1.0. 기본 0.7 — 실측 근거 없는 draft_assumption
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|NPC|Need")
	float GetUnmetThreshold() const { return Aggregator.GetUnmetThreshold(); }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC|Need")
	void SetUnmetThreshold(float NewThreshold) { Aggregator.SetUnmetThreshold(NewThreshold); }

	// 값이 바뀐 Need에 대해서만 발행된다. 구독 직후 GetNeedUnmetRatio로 현재값을 한 번 읽을 것
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|NPC|Need")
	FFF_OnNeedUnmetRatioChanged OnNeedUnmetRatioChanged;

	// --- 쓰기 (UFF_CrowdSimulationSubsystem::AdvanceNeeds 가 부른다) ---

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC|Need")
	void BeginAggregation() { Aggregator.Begin(); }

	// 집계 중인 버킷에 NPC 한 명의 Need를 넣는다. 배열을 만들지 않으려고 누적식으로 받는다
	void AddSample(const FFF_NeedState& Needs);

	// 누적을 확정하고, 값이 바뀐 Need에 대해서만 델리게이트를 발행한다
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|NPC|Need")
	void CommitAggregation();

private:
	FFF_NeedMetricsAggregator Aggregator;
};
