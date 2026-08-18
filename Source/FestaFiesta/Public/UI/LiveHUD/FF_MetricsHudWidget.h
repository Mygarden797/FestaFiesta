// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "FF_MetricsHudWidget.generated.h"

class UImage;
class UTextBlock;
class UFF_FestivalViewModelSubsystem;

/**
 * 지표를 숫자로만 표시하는 HUD.
 *
 * Phase 0(8/6~8/8)의 산출물이다. **예쁘게 만들지 않는다.**
 * 목적은 UI가 ViewModel에서 값을 읽어 화면에 띄우는 배선을 먼저 통과시키는 것이다.
 *
 * CommonUI를 쓰지 않는다. 실시간 HUD는 입력을 가져가면 안 되므로 일반 UUserWidget이다.
 * 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 아직 안 하는 것 ===
 * - 색상 구분: Satisfaction·Revenue·Safety의 좋고 나쁨 구간이 아직 정해지지 않았다.
 *   임의로 정해서 칠하면 나중에 전부 바꿔야 하고, 색만으로 구분하면 AT-011 위반이다.
 * - Heat Map: **밀도 구간이 확정되지 않았다 (2026-08-14 확인).** UI 문서는 3구간(초록 <2 / 노랑 2~4 /
 *   빨강 >=4 명/㎡)인데, Crowd가 구현한 `ECrowdLOS`는 Fruin 보행로 LOS 6등급
 *   (0.31 / 0.43 / 0.72 / 1.08 / 2.17 명/㎡)이라 자릿수가 다르다. 보행로 기준으로는 2.17부터 이미 최악 등급이라
 *   "빨강 4 이상"에 도달하지 않는다. 어느 표준을 쓸지 합의 전까지 색·구간을 코드로 굳히지 않는다.
 *   또한 원본 `FFF_CrowdReadModel::DensityPerSqm`은 아직 아무도 채우지 않는다 — 실값은 Crowd 내부
 *   `FCrowdDensityGrid`에만 있고 UI가 읽을 경로가 없다. 상세는 open-conflicts.md의 C-028·C-029.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Metrics HUD Widget"))
class FESTAFIESTA_API UFF_MetricsHudWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 모든 표시를 현재값으로 다시 그린다. 부분 갱신하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void RefreshAll();

	// --- 델리게이트 핸들러 ---

	UFUNCTION()
	void HandleSimSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleScoreSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleSafetyChanged(int32 NewValue);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleTotalRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

protected:
	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SimSatisfaction;

	/**
	 * 2026-08-06 결정(A안): **Overview HUD는 ScoreSatisfaction을 표시하지 않는다.**
	 * Live 중에는 FinaleBonus가 아직 없어 SimSatisfaction과 같은 숫자가 두 번 뜨고,
	 * 플레이어가 지금 개선할 수 있는 값도 SimSatisfaction뿐이다.
	 * ScoreSatisfaction은 Report(분해식)와 Finale 구매 화면(예상치)에서 쓴다.
	 *
	 * 이 스텁 HUD는 배선 확인용이라 있으면 표시하고 없어도 통과시킨다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_ScoreSatisfaction;

	/** 축제 전체 누적 매출. 위젯 이름은 기존 WBP 호환을 위해 Text_Revenue로 둔다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Revenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Safety;

	// Gold와 오늘 매출은 WBP에 아직 없다. 추가하면 자동으로 붙도록 Optional로 둔다.

	/** 잔액. 매출과 다른 값이다. 상단 바로 옮길 예정. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Gold;

	/** 오늘 매출. 주 사용처는 Report다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	// --- 구간 표시 (좋음 / 주의 / 나쁨) ---
	//
	// AT-011: 색만으로 구분하지 않는다. 라벨과 아이콘을 함께 놓는다.
	// 셋 다 없어도 숫자는 뜨므로 Optional이다. WBP에 넣으면 자동으로 붙는다.
	// 구간 값은 UFF_UIMetricBandSettings에서 읽는다. 여기 하드코딩하지 않는다.
	//
	// 2026-08-11: 재사용 위젯 `UFF_MetricRowWidget`을 뽑았다 (캡션 + 숫자 + 라벨 + 아이콘 한 벌).
	// 이 스텁 HUD는 기존 WBP 호환 때문에 아래 개별 바인딩을 유지한다.
	// **새 화면(Report·Overview)은 개별 바인딩을 늘리지 말고 MetricRow를 쓴다.**
	// 이 HUD를 MetricRow 3개로 재구성하는 것은 8/14 이후 정리 항목이다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SimSatisfactionBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_SimSatisfactionBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_ScoreSatisfactionBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_ScoreSatisfactionBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SafetyBand;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_SafetyBand;

	// Stage 표시는 나중에 상단 바로 옮길 예정이라 없어도 통과시킨다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Stage;

	/** Slot은 표시하지 않는다. 하루 4 Slot 구조가 제거될 예정이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Day;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	/** 초를 `m:ss`로 만든다. 계산은 C++에서 끝내고 위젯은 표시만 한다. */
	static FText FormatSeconds(int32 TotalSeconds);

	/**
	 * 0~100 값에 맞는 구간을 라벨·아이콘에 반영한다. 둘 중 없는 것은 건너뛴다.
	 * 구간을 못 찾으면 임의의 색으로 칠하지 않고 비운다 — 틀린 색이 빈 칸보다 나쁘다.
	 */
	static void ApplyBand(int32 Value, UTextBlock* BandLabel, UImage* BandIcon);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
