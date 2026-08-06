// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Time/FF_FestivalStage.h"
#include "FF_FestivalViewModelSubsystem.generated.h"

class UFF_FestivalStageSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnIntMetricChanged, int32, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFF_OnStageViewChanged, EFF_FestivalStage, NewStage, int32, NewDay);

/**
 * UI용 Read Model.
 *
 * === 이 클래스의 존재 이유 ===
 * UI 위젯은 게임 시스템을 직접 참조하지 않고 **이것만** 본다.
 * 각 시스템이 바뀌어도 위젯을 고치지 않기 위해서다.
 *
 * === 현재 상태: 스텁 ===
 * 지표 4종은 하드코딩 더미다. 실제 소유자의 구현이 생기면 이 클래스 안쪽만 교체하고
 * 위젯은 그대로 둔다. 그게 이 계층을 두는 이유다.
 *
 * | 값                     | 실제 소유자      | 현재        |
 * |------------------------|------------------|-------------|
 * | Stage / Day            | 강민서           | **진짜 값** |
 * | 남은 시간              | 강민서           | **진짜 값** |
 * | SimSatisfaction        | 김시리 · 이정원  | 더미        |
 * | ScoreSatisfaction      | 김시리           | 더미        |
 * | Revenue                | 김시리           | 더미        |
 * | Safety                 | 이정원           | 더미        |
 *
 * === 값 규격 (2026-08-06) ===
 * - Satisfaction / Safety : int32 0~100, 높을수록 좋음.
 * - Revenue : int32 금액. 0~100 상한을 적용하지 않는다.
 *
 * Buzz는 여기 없다. 기획서에서 Utility AI의 점수 항이며 dev-schedule 기준 9월 투입 대상이다.
 * Overview 표시 항목은 Satisfaction·Revenue·Safety 셋이다.
 * 미확인 항목(갱신 주기, 좋고 나쁨 구간)은 리포 루트 UI_API.md 참고.
 *
 * === 시간을 자체 계산하지 않는다 ===
 * 시계 배율이 출처마다 달라 아직 미확정이다(30초 / 60초 / 코드 180초×4).
 * UI는 남은 시간을 스스로 계산하지 않고 Stage 시스템이 주는 값을 그대로 표시만 한다.
 * 근거: docs/memory/time-contract-draft-2026-08-06.md
 */
UCLASS()
class FESTAFIESTA_API UFF_FestivalViewModelSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- 지표 ---
	// 규칙: 값마다 "현재값 Getter + 변경 델리게이트"를 쌍으로 제공한다.
	// 델리게이트만 있으면 위젯이 늦게 생성될 때 초기값을 놓친다.

	/** 0~100. 대기·밀도·취향 매칭에서 나오는 순수 시뮬레이션 값. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSimSatisfaction() const { return SimSatisfaction; }

	/** 0~100. SimSatisfaction + FinaleBonus + 연출 보정. 등급 산출용 게임 점수. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetScoreSatisfaction() const { return ScoreSatisfaction; }

	/** 0~100. 높을수록 안전. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetSafety() const { return Safety; }

	/** 금액. 0~100 상한을 적용하지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Metrics")
	int32 GetRevenue() const { return Revenue; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnScoreSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSafetyChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnRevenueChanged;

	// --- Stage (강민서 시스템 중계) ---

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	EFF_FestivalStage GetCurrentStage() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	int32 GetCurrentDay() const;

	// !! Slot은 의도적으로 노출하지 않는다 !!
	//
	// 2026-08-06 12시 스크럼에서 하루 4 Slot 구조를 없애고 날짜마다 Live를 한 번만 돌리기로 했다.
	// 시간 담당의 제거 작업이 아직 원격에 안 올라와서 UFF_FestivalStageSubsystem::CurrentSlot은
	// 여전히 존재하지만, UI가 그걸 화면에 띄우면 곧 지워질 개념을 플레이어에게 노출하게 된다.
	// 제거가 실제로 들어와도 이 클래스는 안 깨진다.
	// 근거: docs/design/stage-and-time.md 5절

	/** 남은 시간(초). Stage 시스템이 주는 값을 그대로 돌려준다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	float GetRemainingSeconds() const;

	/**
	 * Stage 표시 문구.
	 * 모르는 Stage가 와도 빈 화면 대신 원본 이름을 돌려준다.
	 * enum 순서나 개수에 의존하지 않는다 — 기획 7단계와 코드 8단계가 아직 다르다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Stage")
	FText GetCurrentStageDisplayText() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnStageViewChanged OnStageViewChanged;

	/** 남은 시간이 1초 단위로 바뀔 때만 쏜다. 매 틱 쏘면 위젯이 초당 수십 번 갱신된다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Stage")
	FFF_OnIntMetricChanged OnRemainingSecondsChanged;

	// --- 스텁 전용. 실제 소유자 구현이 붙으면 삭제한다 ---

	/**
	 * 지표를 강제로 설정한다. 값이 실제로 바뀔 때만 델리게이트를 쏜다.
	 * 0~100 지표는 자동으로 클램프된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety, int32 InRevenue);

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return !IsTemplate(); }
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UFF_FestivalViewModelSubsystem, STATGROUP_Tickables);
	}

protected:
	UFUNCTION()
	void HandleStageChanged(EFF_FestivalStage PreviousStage, EFF_FestivalStage NewStage);

private:
	UFF_FestivalStageSubsystem* GetStageSubsystem() const;

	/** Stage 시스템 구독을 시도한다. 아직 없으면 재시도를 예약한다. */
	void TryBindStageSubsystem();

	// --- 지표 더미값 ---
	// !! 하드코딩 !! 실제 값이 생기면 이 초기화를 지우고 소유자 시스템 구독으로 바꾼다.

	UPROPERTY()
	int32 SimSatisfaction = 72;

	UPROPERTY()
	int32 ScoreSatisfaction = 72;

	UPROPERTY()
	int32 Safety = 88;

	UPROPERTY()
	int32 Revenue = 0;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> CachedStageSubsystem;

	/** 남은 시간 변화 감지용. 초 단위로 바뀔 때만 통지한다. */
	int32 LastBroadcastRemainingSeconds = INDEX_NONE;

	float PollAccumulator = 0.f;

	/** 폴링 주기(초). 0.1초면 초 단위 변화를 놓치지 않으면서 부하가 없다. */
	static constexpr float PollInterval = 0.1f;

	/** Stage 시스템을 못 찾았을 때 재시도한 횟수. 상한을 두고 로그를 남긴다. */
	int32 StageBindRetryCount = 0;
	static constexpr int32 MaxStageBindRetry = 100;

public:
	/** 0~100 지표의 상한. 위젯이 게이지 비율을 계산할 때 이 값을 쓴다. 하드코딩하지 않게 하기 위함. */
	static constexpr int32 MetricMax = 100;
};
