// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/Widgets/FF_SignedAmountStyle.h"
#include "FF_LivePanelReport.generated.h"

class UButton;
class UFF_FestivalViewModelSubsystem;
class UTextBlock;
class UProgressBar;
class UVerticalBox;
struct FFF_LedgerRow;

/** 원장 한 줄. 라벨·금액만 갱신하고 글꼴·폭·여백은 파생 WBP가 소유한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Daily Report Ledger Row"))
class FESTAFIESTA_API UFF_DailyReportLedgerRowWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void ConfigureRow(const FText& Label, const FText& Amount);
protected:
	virtual void NativePreConstruct() override;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Label;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Amount;
private:
	void RefreshRow();
	FText CurrentLabel;
	FText CurrentAmount;
	bool bHasRow = false;
};

/**
 * 날짜가 확정된 하루 결산을 표시한다. 원장·만족도는 ViewModel의 읽기 모델만 사용한다.
 * 관객 만족도와 피날레 보너스를 반영한 평가 만족도를 분리하고, 철거 환급도 매출과 분리한다.
 * 패널 자체는 시간·경제·단계를 변경하지 않으며 다음 진행만 소유 HUD에 요청한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Report"))
class FESTAFIESTA_API UFF_LivePanelReport : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	/** 최초 제작 도구용. 반복 생성 시 기존 행 클래스 편집을 덮어쓰지 않는다. */
	void ConfigureLedgerRowDefaults(TSubclassOf<UFF_DailyReportLedgerRowWidget> InRowClass);

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleAdvanceClicked();

	UFUNCTION()
	void HandleIntMetricChanged(int32 NewValue);

	UFUNCTION()
	void HandleReportChanged(int32 ReportDay);

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
	TObjectPtr<UTextBlock> Text_DayLabel;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_TodayExpense;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_TodayNetProfit;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Refund;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_LedgerNote;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UVerticalBox> Box_EarnRows;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UVerticalBox> Box_SpendRows;

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

	/** 관객 만족도와 평가 만족도의 차이를 설명한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_SatisfactionNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_WaitScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_DensityScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_PreferenceScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_NeedScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UProgressBar> Progress_WaitScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UProgressBar> Progress_DensityScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UProgressBar> Progress_PreferenceScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UProgressBar> Progress_NeedScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_DailyGoal;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_DailyGoalState;

	// --- 운영 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_Programs;

	/** 정산이 확정되기 전의 안내. 준비가 끝나면 접는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/**
	 * 비서 한마디(매출 1위 시설 · 불만 1위와 고칠 방법 · 방문객 목표). 여러 줄이라 자동 줄바꿈을 켠다.
	 * WBP 에 없으면 정산이 끝난 뒤 비는 Text_MissingNote 자리에 대신 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Report")
	TObjectPtr<UTextBlock> Text_AssistantComment;

	/** {Comment}: 비서 한마디 본문(줄바꿈 포함). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText AssistantCommentFormat = NSLOCTEXT("FestaFiestaLiveReport", "AssistantComment", "비서 한마디\n{Comment}");

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

	/** 숫자와 원본 항목명 이외의 문구는 WBP Class Defaults에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText DayTitleFormat = NSLOCTEXT("FestaFiestaLiveReport", "DayTitle", "{Day}일차 결산");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText DayLabelFormat = NSLOCTEXT("FestaFiestaLiveReport", "DayLabel", "{Day}일차");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText PendingReportText = NSLOCTEXT("FestaFiestaLiveReport", "Pending", "오늘의 정산을 마무리하고 있습니다.");	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText UnknownValueText = NSLOCTEXT("FestaFiestaLiveReport", "UnknownValue", "—");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText NotMeasuredText = NSLOCTEXT("FestaFiestaLiveReport", "NotMeasured", "아직 측정하지 않음");
	/** 수입·환급·순이익이 양수일 때. {Amount}: 금액. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText PositiveAmountFormat = NSLOCTEXT("FestaFiestaLiveReport", "PositiveAmount", "+{Amount}");
	/** 순이익의 양수·음수·0 색. Designer 글자색 대신 이 값이 적용된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Style")
	FFF_SignedAmountStyle NetProfitStyle;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText NoIncomeText = NSLOCTEXT("FestaFiestaLiveReport", "NoIncome", "기록된 수입이 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText NoExpenseText = NSLOCTEXT("FestaFiestaLiveReport", "NoExpense", "기록된 지출이 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText LedgerNoteText = NSLOCTEXT("FestaFiestaLiveReport", "LedgerNote", "순이익은 매출에서 지출을 뺀 금액입니다. 철거 환급은 별도로 표시합니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText SatisfactionNoteText = NSLOCTEXT("FestaFiestaLiveReport", "ScoreNote", "평가 만족도는 관객 만족도에 피날레 보너스를 반영한 값이며, 최대 100점입니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText GoalProgressFormat = NSLOCTEXT("FestaFiestaLiveReport", "GoalProgress", "{Admitted} / {Goal}명");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText GoalAchievedText = NSLOCTEXT("FestaFiestaLiveReport", "GoalAchieved", "✓ 목표 달성");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText GoalMissedText = NSLOCTEXT("FestaFiestaLiveReport", "GoalMissed", "목표 미달성");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText StaffFormat = NSLOCTEXT("FestaFiestaLiveReport", "Staff", "{Total}명 (고용 {Hired})");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	FText ProgramCountFormat = NSLOCTEXT("FestaFiestaLiveReport", "Programs", "{Count}개");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	TMap<FName, FText> EarnLabelOverrides;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Text")
	TMap<FName, FText> SpendLabelOverrides;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Rows", meta = (ClampMin = "0.0"))
	float RowSpacing = 8.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Rows", meta = (ClampMin = "0.0"))
	float AmountColumnMinWidth = 100.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Rows", meta = (ClampMin = "0.0"))
	float RowColumnGap = 16.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Report|Rows")
	TSubclassOf<UFF_DailyReportLedgerRowWidget> LedgerRowClass;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void RefreshAll();
	void RefreshAdvance();
	void RefreshLedgerRows(UVerticalBox* Box, const TArray<FFF_LedgerRow>& Rows,
		const TMap<FName, FText>& LabelOverrides, const FText& EmptyText, bool bExpense);
	void RefreshScore(UTextBlock* Label, UProgressBar* Bar, int32 Score, bool bAvailable);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
