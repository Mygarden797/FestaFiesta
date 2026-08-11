// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Time/FF_FestivalStage.h"
#include "FF_FestivalViewModelSubsystem.generated.h"

class UFF_FestivalStageSubsystem;
// !! class가 아니라 struct다 !! (Core/Public/HAL/IConsoleManager.h)
// class로 쓰면 C4099가 나고, 이 프로젝트는 경고를 에러로 취급한다.
struct IConsoleCommand;

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
 * 지표 6종은 하드코딩 더미다. 실제 소유자의 구현이 생기면 이 클래스 안쪽만 교체하고
 * 위젯은 그대로 둔다. 그게 이 계층을 두는 이유다.
 *
 * | 값                     | 실제 소유자      | 현재        |
 * |------------------------|------------------|-------------|
 * | Stage / Day            | 강민서           | **진짜 값** |
 * | 남은 시간              | 강민서           | **진짜 값** |
 * | SimSatisfaction        | 김시리 · 이정원  | 더미        |
 * | ScoreSatisfaction      | 김시리           | 더미        |
 * | Safety                 | 이정원           | 더미        |
 * | Gold                   | 김시리           | 더미        |
 * | TotalRevenue           | 김시리           | 더미        |
 * | TodayRevenue           | 김시리           | 더미        |
 *
 * === 값 규격 (2026-08-06 결정) ===
 * - Satisfaction / Safety : int32 0~100, 높을수록 좋음.
 * - Gold / Revenue : int32 금액. 0~100 상한을 적용하지 않는다.
 *
 * **Gold와 Revenue는 다른 값이다.** 헷갈리면 화면에 틀린 숫자가 뜬다.
 * - `Gold`         : 지금 쓸 수 있는 **잔액**. 지출로 줄어든다. 초기 10,000, 축제 종료 시 초기화.
 * - `TotalRevenue` : 축제 전체 **누적 매출**. 줄어들지 않는다. 상단 바 표시용.
 * - `TodayRevenue` : 오늘 하루 매출. Report에서 "오늘 얼마 벌었나"에 쓴다.
 *
 * 대략 `Gold ≒ 초기자금 + TotalRevenue - 누적지출`이지만, **UI가 이 식을 계산하지 않는다.**
 * 셋 다 소유자에게서 그대로 받는다. 근거: docs/design/game-overview.md 5절
 *
 * Buzz는 여기 없다. 기획서에서 Utility AI의 점수 항이며 dev-schedule 기준 9월 투입 대상이다.
 * 좋고 나쁨 구간은 아직 미정이라 UI에서 색을 칠하지 않는다. 상세는 리포 루트 UI_API.md 참고.
 *
 * === 갱신 주기 (2026-08-06 결정) ===
 * 지표는 **일정 틱마다** 바뀐다. 소스가 매 틱 흔들려도 위젯을 매 틱 갱신하면 안 되므로
 * 여기서 `PollInterval`마다 샘플링하고 **정수값이 실제로 바뀔 때만** 델리게이트를 쏜다.
 * 지표를 int32로 둔 것이 여기서 이득이 된다. 위젯은 이 처리를 몰라도 된다.
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

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSimSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnScoreSatisfactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Metrics")
	FFF_OnIntMetricChanged OnSafetyChanged;

	// --- 경제 ---
	// Gold(잔액)와 Revenue(매출)는 다른 값이다. 클래스 주석의 구분표를 먼저 읽는다.

	/** 지금 쓸 수 있는 잔액. 지출로 줄어든다. 상단 바·Finale 구매 가능 판정에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetGold() const { return Gold; }

	/** 축제 전체 누적 매출. 줄어들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTotalRevenue() const { return TotalRevenue; }

	/** 오늘 하루 매출. Report의 "오늘 얼마 벌었나"에 쓴다. UI가 누적값에서 빼서 만들지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|ViewModel|Economy")
	int32 GetTodayRevenue() const { return TodayRevenue; }

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnGoldChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTotalRevenueChanged;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|ViewModel|Economy")
	FFF_OnIntMetricChanged OnTodayRevenueChanged;

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
	 * 0~100 지표를 강제로 설정한다. 값이 실제로 바뀔 때만 델리게이트를 쏘고 범위를 벗어나면 클램프한다.
	 * 경제 값은 단위가 달라 섞지 않는다. Debug_SetEconomy를 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetMetrics(int32 InSimSatisfaction, int32 InScoreSatisfaction, int32 InSafety);

	/** 경제 값을 강제로 설정한다. 금액이라 클램프하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_SetEconomy(int32 InGold, int32 InTotalRevenue, int32 InTodayRevenue);

	/** 현재 값 전부를 로그에 찍는다. 위젯이 없거나 화면을 못 볼 때 확인용. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|ViewModel|Debug")
	void Debug_DumpToLog() const;

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

	/**
	 * 소유자 시스템에서 지표를 읽어 온다. PollInterval마다 Tick에서 호출된다.
	 * 지금은 소유자가 없어 아무것도 하지 않는다 — 구현이 붙으면 **여기 한 곳만** 채운다.
	 * 변화 감지와 통지는 ApplyIntMetric이 이미 처리하므로 위젯은 손대지 않는다.
	 */
	void PollMetricsFromSources();

	/** 값이 실제로 바뀔 때만 통지한다. 같은 값을 계속 쏘면 위젯이 헛돈다. */
	static void ApplyIntMetric(int32& Current, int32 NewValue, FFF_OnIntMetricChanged& ChangedDelegate);

	// --- 지표 더미값 ---
	// !! 하드코딩 !! 실제 값이 생기면 이 초기화를 지우고 소유자 시스템 구독으로 바꾼다.

	UPROPERTY()
	int32 SimSatisfaction = 72;

	UPROPERTY()
	int32 ScoreSatisfaction = 72;

	UPROPERTY()
	int32 Safety = 88;

	/** 초기 Gold 10,000은 game-overview.md 밸런스 시드 v0의 `assumption`이다. 확정값이 아니다. */
	UPROPERTY()
	int32 Gold = 10000;

	UPROPERTY()
	int32 TotalRevenue = 0;

	UPROPERTY()
	int32 TodayRevenue = 0;

	UPROPERTY()
	TObjectPtr<UFF_FestivalStageSubsystem> CachedStageSubsystem;

	/** 남은 시간 변화 감지용. 초 단위로 바뀔 때만 통지한다. */
	int32 LastBroadcastRemainingSeconds = INDEX_NONE;

	float PollAccumulator = 0.f;

	/**
	 * 폴링 주기(초). 0.1초면 남은 시간의 초 단위 변화를 놓치지 않으면서 부하가 없다.
	 * 지표도 같은 주기로 샘플링하므로 위젯 갱신은 지표당 최대 초당 10회로 묶인다.
	 * 금액처럼 자주 흔들리는 값도 여기서 걸린다.
	 */
	static constexpr float PollInterval = 0.1f;

	/** Stage 시스템을 못 찾았을 때 재시도한 횟수. 상한을 두고 로그를 남긴다. */
	int32 StageBindRetryCount = 0;
	static constexpr int32 MaxStageBindRetry = 100;

#if !UE_BUILD_SHIPPING
	/**
	 * 디버그 콘솔 명령을 등록한다.
	 *
	 * === 왜 콘솔인가 ===
	 * 화면이 `ECommonInputMode::Menu`면 **게임 입력이 차단**되어, 레벨 블루프린트의 키 이벤트로는
	 * `Debug_Set*`을 부를 수 없다 (2026-08-07에 실제로 막혔다). 콘솔은 입력 모드와 무관하게 동작한다.
	 *
	 * === UFUNCTION(Exec)을 쓰지 않은 이유 ===
	 * 엔진의 `UGameInstance`에 `ProcessConsoleExec`이 없어서 **콘솔이 서브시스템의 Exec 함수를
	 * 찾아가지 않는다.** 그래서 `IConsoleManager`에 직접 등록한다.
	 *
	 * === 해제를 반드시 하는 이유 ===
	 * PIE는 한 에디터 세션에서 여러 번 돌아간다. 해제하지 않으면 두 번째 PIE에서 같은 이름 등록이
	 * 실패하고, 파괴된 this를 가리키는 델리게이트가 콘솔에 남는다.
	 * 람다는 `CreateWeakLambda`로 묶어 두 번째 방어선을 만든다.
	 */
	void RegisterDebugConsoleCommands();
	void UnregisterDebugConsoleCommands();

	IConsoleCommand* SetMetricsConsoleCommand = nullptr;
	IConsoleCommand* SetEconomyConsoleCommand = nullptr;
	IConsoleCommand* DumpMetricsConsoleCommand = nullptr;
#endif

public:
	/** 0~100 지표의 상한. 위젯이 게이지 비율을 계산할 때 이 값을 쓴다. 하드코딩하지 않게 하기 위함. */
	static constexpr int32 MetricMax = 100;
};
