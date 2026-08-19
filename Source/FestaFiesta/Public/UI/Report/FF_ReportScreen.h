// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_ReportScreen.generated.h"

class UTextBlock;
class UWidget;
class UFF_CommonButtonBase;
class UFF_FestivalViewModelSubsystem;
class UFF_MetricRowWidget;

/**
 * Report 화면 — 하루가 끝나고 결과를 보여주고 다음 날로 넘긴다.
 *
 * === 이 화면이 게임 진행의 게이트다 ===
 * 2026-08-11 기준 `UFF_FestivalStageSubsystem`은 **Report만 자동 전환하지 않고** 플레이어의
 * `AdvanceToNextDay()` 호출을 기다린다. 즉 이 화면의 `다음 날` 버튼이 없으면 Day 2로 갈 수 없다.
 * 단순 결과 표시 화면이 아니다.
 *
 * === 레이아웃을 점수표로 만들지 않는다 ===
 * 결정된 방향은 `관측 사실 → 지표 영향 → 내일 바꿀 행동`의 **원인 연결**이다.
 * 8/14에는 1·2단만 채우고 3단은 자리만 비워 둔다. 점수표로 만들어 두면 드릴다운(AT-005)을
 * 붙일 때 처음부터 다시 짜야 한다.
 * 근거: docs/memory/ingame-operations-ui-flow-2026-08-07.md
 *
 * === Satisfaction을 분해식으로 보여주는 이유 ===
 * `관객 만족도 62 + 피날레 20 = 평가 점수 82`
 * 지표를 두 개로 나눈 이유가 화면 자체로 설명되므로 툴팁이 필요 없다.
 * 2026-08-06 결정 A안 — Overview는 SimSatisfaction만, Report는 둘 다 분해식으로.
 *
 * === 화면 라벨에 내부 이름을 쓰지 않는다 ===
 * `SimSatisfaction` → "관객 만족도", `ScoreSatisfaction` → "평가 점수".
 *
 * === CommonUI를 쓴다 ===
 * 전체 화면이고 입력을 독점하므로 `UFF_ActivatableScreenBase` 파생이다.
 * `UFF_UIRootLayout::PushScreen`으로 띄운다. 뷰포트에 직접 붙이지 않는다.
 *
 * !! 이 화면이 시간을 멈추지 않는다 !!
 * 정지 여부는 Stage 시스템이 결정한다. 화면은 요청만 한다 (UI 규칙 1).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Report Screen"))
class FESTAFIESTA_API UFF_ReportScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	/** 모든 표시를 현재값으로 다시 그린다. 부분 갱신하지 않아 이벤트 순서에 의존하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void RefreshAll();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;

	// --- 델리게이트 핸들러 ---

	UFUNCTION()
	void HandleSimSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleScoreSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleSafetyChanged(int32 NewValue);

	UFUNCTION()
	void HandleTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleNextDayClicked(UFF_CommonButtonBase* Button);

	/**
	 * 다음 날 요청을 실제로 보낸 뒤 BP에 알린다. 화면을 닫는 연출은 BP가 맡는다.
	 * C++이 화면 스택을 직접 pop하지 않는다 — 스택 소유는 RootLayout이다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI", meta = (DisplayName = "On Next Day Requested"))
	void BP_OnNextDayRequested();

protected:
	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	/** "Day 1 결과" 같은 제목. 문구는 C++에서 만든다(Day 번호가 들어가므로). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Title;

	// --- 1단: 관측 사실 (지표) ---
	// MetricRow를 쓴다. 개별 TextBlock을 지표마다 늘리지 않는다.

	/** 관객 만족도 = `SimSatisfaction`. 분해식의 왼쪽 항이며 드릴다운이 붙을 자리다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_MetricRowWidget> Row_SimSatisfaction;

	/** 평가 점수 = `ScoreSatisfaction`. 분해식의 결과 항. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_MetricRowWidget> Row_ScoreSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_MetricRowWidget> Row_Safety;

	/** 오늘 매출. 금액이라 구간 표시를 끈다 — 0~100 기준이 아니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_MetricRowWidget> Row_TodayRevenue;

	/**
	 * 분해식의 가운데 항 — 피날레 보너스.
	 *
	 * !! 아직 임시 계산이다 !! `ScoreSatisfaction - SimSatisfaction`으로 만든다.
	 * **소유자 값이 2026-08-18에 생겼다** — `UFF_FestivalProgressionSubsystem::GetFinaleSatisfactionBonus()`
	 * (`BlueprintPure`, 0~100 척도, 안 샀으면 0)와 `OnFinaleGradeChanged` 델리게이트다.
	 * ViewModel이 이 값을 중계하도록 붙인 뒤 위 임시 계산을 지운다.
	 * 그 전까지는 두 지표가 같은 더미라 `62 + 0 = 62`로 보인다. `FF.SetMetrics 62 82 88`로 검증한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_FinaleBonus;

	/** "관객 만족도 62 + 피날레 20 = 평가 점수 82" 한 줄. 위 Row들과 병행 표시 가능. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SatisfactionBreakdown;

	// --- 3단: 내일 바꿀 행동 — 8/14에는 비워 둔다 ---

	/**
	 * 원인·행동 제안이 들어갈 자리.
	 *
	 * `CauseId` 롤업이 아직 없어 채울 수 없다. **자리만 잡아 둔다.**
	 * 드릴다운은 왼쪽 항(관객 만족도)에 건다 — FinaleBonus는 지불로만 바뀌므로 원인 분석 대상이 아니다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> Panel_CauseDrilldown;

	// --- 조작 ---

	/**
	 * 다음 날로 넘어가는 버튼. **이 화면의 존재 이유다.**
	 * Report가 아니거나 마지막 날이면 ViewModel의 `CanAdvanceToNextDay()`가 false가 되어 비활성된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_NextDay;

	/** 마지막 날에 `다음 날` 대신 보여줄 문구. 비활성 버튼만 두면 왜 못 누르는지 알 수 없다(AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_NextDayHint;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	/** 지표 한 줄을 캡션과 함께 채운다. 위젯이 없으면 조용히 건너뛴다. */
	static void SetRow(UFF_MetricRowWidget* Row, const FText& Caption, int32 Value, bool bShowBand);

	void RefreshTitle();
	void RefreshBreakdown();
	void RefreshNextDayButton();

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
