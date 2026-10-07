// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/FF_SignedAmountStyle.h"
#include "FF_FinalReportDayRowWidget.generated.h"

class UTextBlock;
struct FFF_FinalReportDayView;

/** 최종 결산의 날짜별 한 줄. 데이터만 설정하고 글꼴·열 폭·여백은 파생 WBP가 소유한다. */
UCLASS(Abstract, meta = (DisplayName = "FF Final Report Day Row"))
class FESTAFIESTA_API UFF_FinalReportDayRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_FinalReportDayRowWidget(const FObjectInitializer& ObjectInitializer);
	void ConfigureDay(const FFF_FinalReportDayView& DayView);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Day;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Revenue;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_Expense;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_NetProfit;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI|FinalReport")
	TObjectPtr<UTextBlock> Text_AdmittedVisitors;

	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText DayFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText VisitorCountFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText PositiveAmountFormat;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Text")
	FText UnknownValueText;
	/** 순이익 칸의 양수·음수·0 색. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|FinalReport|Style")
	FFF_SignedAmountStyle NetProfitStyle;

private:
	void RefreshRow();
	FText CurrentDay;
	FText CurrentRevenue;
	FText CurrentExpense;
	FText CurrentNetProfit;
	TOptional<int64> CurrentNetProfitAmount;
	FText CurrentAdmittedVisitors;
	bool bHasDay = false;
};
