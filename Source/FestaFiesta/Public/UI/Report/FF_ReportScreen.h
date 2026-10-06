// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "UI/Widgets/FF_PanelScrollStyle.h"
#include "UI/Widgets/FF_SignedAmountStyle.h"
#include "FF_ReportScreen.generated.h"

class UButton;
class UTextBlock;
class UProgressBar;
class UVerticalBox;
class UFF_FestivalViewModelSubsystem;
class UFF_FinalReportDayRowWidget;
struct FFF_FinalReportView;

/**
 * 축제를 끝낸 뒤 확정 평가와 전체 기간의 결산을 보여주는 화면.
 * 값은 ViewModel에서 읽고 로비 복귀는 UIFlow에 요청한다. 평가·경제·시간은 변경하지 않는다.
 * 글꼴·위치·표의 열 폭은 파생 WBP가 소유하며 여기서는 문구와 값만 갱신한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Final Report Screen"))
class FESTAFIESTA_API UFF_ReportScreen : public UFF_ActivatableScreenBase
{

	GENERATED_BODY()

public:
	UFF_ReportScreen(const FObjectInitializer& ObjectInitializer);

	/** 종료 평가가 준비되면 한 번 표시한다. 아직 준비 중이면 다음 틱에 다시 시도한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|FinalReport")
	void RefreshAll();

	/** 최초 제작 도구용. 이미 지정한 행 클래스는 보존한다. */
	void ConfigureDayRowDefaults(TSubclassOf<UFF_FinalReportDayRowWidget> InRowClass);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	virtual bool NativeOnHandleBackAction() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UFUNCTION()
	void HandleLobbyClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Title;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_FestivalSummary;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Grade;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_TotalScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Headline;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_WeakestAxisComment;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_SatisfactionScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_VisitorScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_FinanceScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_BuzzScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UProgressBar> Progress_SatisfactionScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UProgressBar> Progress_VisitorScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UProgressBar> Progress_FinanceScore;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UProgressBar> Progress_BuzzScore;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_TotalAdmittedVisitors;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_TotalRevenue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_TotalExpense;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_NetProfit;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_DaysSummary;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UVerticalBox> Box_DayRows;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_VisitorHistoryNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Gold;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Refund;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Status;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UButton> Button_Lobby;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_LobbyLabel;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Rows")
	TSubclassOf<UFF_FinalReportDayRowWidget> DayRowClass;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Rows", meta = (ClampMin = "0"))
	float DayRowSpacing = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText TitleText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText FestivalSummaryFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText ScoreFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText VisitorCountFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText DaysSummaryFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText PositiveAmountFormat;
	/** 순이익 카드의 양수·음수·0 색. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Style")
	FFF_SignedAmountStyle NetProfitStyle;
	/** 본문·일별 표 스크롤바 모양. 셋업·라이브 패널과 같은 공용 막대다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Style")
	FFF_PanelScrollStyle ScrollStyle;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText VisitorHistoryNoteText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText UnknownValueText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText PendingText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText BackHintText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText LobbyUnavailableText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText DayRowsUnavailableText;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText LobbyLabelText;

private:
	void BindActions();
	UFF_FestivalViewModelSubsystem* GetViewModel();
	void ShowPending();
	void ApplyReport(const FFF_FinalReportView& Report);
	bool RebuildDayRows(const FFF_FinalReportView& Report);
	void SetStatus(const FText& Status);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
	bool bHasFinalReport = false;
};
