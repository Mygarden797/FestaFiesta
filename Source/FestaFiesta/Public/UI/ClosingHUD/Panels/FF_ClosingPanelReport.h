// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/ClosingHUD/FF_ClosingPanelBase.h"
#include "FF_ClosingPanelReport.generated.h"

class UButton;
class UFF_FestivalViewModelSubsystem;
class UTextBlock;

/**
 * 마감·리포트의 `리포트` 패널 — 하루 성적표.
 *
 * === 무엇을 보여주나 ===
 *   돈    오늘 매출 · 전체 매출 · 보유 금화 · 미납
 *   평가  관객 만족도 · 평가 점수 · 안전
 *   운영  인력 · 오늘 편성한 프로그램 수
 *
 * === 🔴 아직 "왜" 를 말하지 못한다 ===
 * 기획이 요구하는 것은 "만족도 62점 → 클릭 2번 → 슬롯 3의 화장실 부족" 까지 내려가는
 * 드릴다운인데, 그 원본인 `CauseId` 롤업이 아직 없다. 지금은 **숫자만** 있고 그 사실을
 * 화면에 적는다 — 없는 것을 없다고 말해야 다음에 무엇을 붙일지 안 잊는다.
 *
 * === 만족도를 둘 다 띄우는 이유 ===
 * `SimSatisfaction`(대기·밀도·취향)과 `ScoreSatisfaction`(+ 피날레 보너스·연출 보정)은
 * **다른 지표**다. 라이브 중에는 값이 같아서 라이브 HUD 에서는 하나만 띄웠지만,
 * 결산에서는 보너스가 붙어 갈라지므로 둘 다 보여야 차이가 보인다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Closing Panel - Report"))
class FESTAFIESTA_API UFF_ClosingPanelReport : public UFF_ClosingPanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	/** 읽는 시간을 세는 데 쓴다. 끌어 옮기기는 부모가 같은 곳에서 처리한다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleAdvanceClicked();

	UFUNCTION()
	void HandleIntMetricChanged(int32 NewValue);

protected:
	// --- 돈 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_TotalRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_Gold;

	/** 미납은 0 이면 숨긴다. `미납 0` 은 없는 문제를 있는 것처럼 만든다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_Arrears;

	// --- 평가 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_SimSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_ScoreSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_Safety;

	/** 두 만족도가 왜 다른지. 값이 같으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_SatisfactionNote;

	// --- 운영 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_Programs;

	/** 아직 못 보여주는 것. 안 적으면 빠진 줄 모른다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_MissingNote;

	// === 다음 날로 (2026-09-02 이동) ===
	//
	// 🔴 하단 바가 아니라 **패널 안**에 둔다. 결과를 읽는 자리와 넘기는 자리를 붙여야
	// "읽고 나서 넘긴다" 는 순서가 손에 잡힌다. 바에도 두면 버튼이 둘이 된다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UButton> Button_Advance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_AdvanceLabel;

	/** 막힌 이유 또는 남은 대기 초. 버튼만 흐려 두면 왜 못 누르는지 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Report")
	TObjectPtr<UTextBlock> Text_AdvanceReason;

	/**
	 * 패널을 연 뒤 이만큼 지나야 버튼이 열린다(초).
	 *
	 * === 왜 기다리게 하나 ===
	 * 결과를 안 읽고 바로 넘기는 것을 막는다. **다만 못 누르게만 하면 고장으로 보이므로**
	 * 남은 초를 세어 보여 주고 버튼도 흐리게 두었다가 또렷해진다 — 기다리는 중이라는 것이
	 * 눈에 보여야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Report", meta = (ClampMin = "0.0"))
	float ReadDelaySeconds = 5.f;

	/** 기다리는 동안의 버튼 투명도. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Report", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.35f;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();
	void RefreshAdvance();

	/** 패널을 연 뒤 흐른 시간. 열 때마다 0 으로 돌아간다. */
	float TimeSinceOpened = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
