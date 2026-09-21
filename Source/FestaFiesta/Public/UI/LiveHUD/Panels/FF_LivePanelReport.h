// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelReport.generated.h"

class UButton;
class UFF_FestivalViewModelSubsystem;
class UTextBlock;

/**
 * 마감·리포트의 `리포트` 패널 — 하루 성적표.
 *
 * === 무엇을 보여주나 ===
 *   돈    오늘 매출 · 전체 매출 · 보유 금화
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
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Report"))
class FESTAFIESTA_API UFF_LivePanelReport : public UFF_LivePanelBase
{
	GENERATED_BODY()

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION()
	void HandleAdvanceClicked();

	UFUNCTION()
	void HandleIntMetricChanged(int32 NewValue);

	/**
	 * 단계가 바뀌면 버튼의 뜻이 바뀐다 — 마감에서 리포트로, 3일차면 `축제 마치기` 로.
	 * 이걸 안 받으면 패널을 열어 둔 채 단계가 넘어갔을 때 **한 단계 전의 버튼**이 남는다.
	 */
	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

protected:
	// --- 돈 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_TotalRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Gold;

	// --- 평가 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_SimSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_ScoreSatisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Safety;

	/** 두 만족도가 왜 다른지. 값이 같으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_SatisfactionNote;

	// --- 운영 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Programs;

	/** 아직 못 보여주는 것. 안 적으면 빠진 줄 모른다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_MissingNote;

	// === 다음 날로 (2026-09-02 이동) ===
	//
	// 🔴 하단 바가 아니라 **패널 안**에 둔다. 결과를 읽는 자리와 넘기는 자리를 붙여야
	// "읽고 나서 넘긴다" 는 순서가 손에 잡힌다. 바에도 두면 버튼이 둘이 된다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UButton> Button_Advance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_AdvanceLabel;

	/** 막힌 이유 또는 남은 대기 초. 버튼만 흐려 두면 왜 못 누르는지 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_AdvanceReason;

	/**
	 * 못 누르는 상태일 때의 버튼 투명도.
	 *
	 * 🔴 2026-09-03 에 **5초 대기를 걷어냈다.** 결과를 다 읽었는지는 플레이어가 안다 —
	 * 억지로 세어 봐야 그 5초 동안 아무것도 안 하고 버튼만 보게 된다 (백종태 결정).
	 * 이제 흐려지는 경우는 마감을 건너뛸 수 없을 때뿐이고, 그때는 이유가 함께 뜬다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DimmedOpacity = 0.35f;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();
	void RefreshAdvance();

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
