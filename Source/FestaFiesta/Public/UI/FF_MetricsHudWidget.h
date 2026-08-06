// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "FF_MetricsHudWidget.generated.h"

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
 * - Heat Map: Fruin 구간(2/4/5 인/㎡)은 확정됐지만 원본 FFF_CrowdReadModel이 아직 스텁이다.
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
	void HandleRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

protected:
	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 한다 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_SimSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_ScoreSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Revenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Safety;

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

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
