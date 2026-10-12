// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Report/FF_FinalReportDayRowWidget.h"

#include "Components/TextBlock.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFinalReportDayRow"

UFF_FinalReportDayRowWidget::UFF_FinalReportDayRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DayFormat = LOCTEXT("Day", "{Day}일차");
	VisitorCountFormat = LOCTEXT("Visitors", "{Count}명");
	SatisfactionFormat = LOCTEXT("Satisfaction", "{Score}점");
	NoSatisfactionRecordText = LOCTEXT("NoSatisfactionRecord", "기록 없음");
	PositiveAmountFormat = LOCTEXT("PositiveAmount", "+{Amount}");
	UnknownValueText = LOCTEXT("UnknownValue", "—");
}

void UFF_FinalReportDayRowWidget::ConfigureDay(const FFF_FinalReportDayView& DayView)
{
	FFormatNamedArguments DayArgs;
	DayArgs.Add(TEXT("Day"), FText::AsNumber(DayView.Day));
	CurrentDay = DayView.Day > 0 ? FText::Format(DayFormat, DayArgs) : UnknownValueText;
	CurrentRevenue = DayView.bHasLedger
		? FFF_SignedAmountStyle::Format(DayView.Revenue, PositiveAmountFormat) : UnknownValueText;
	CurrentExpense = DayView.bHasLedger ? FFF_SignedAmountStyle::FormatExpense(DayView.Expense) : UnknownValueText;
	CurrentNetProfit = DayView.bHasLedger
		? FFF_SignedAmountStyle::Format(DayView.NetProfit, PositiveAmountFormat) : UnknownValueText;
	CurrentNetProfitAmount = DayView.bHasLedger ? TOptional<int64>(DayView.NetProfit) : TOptional<int64>();

	// 원장 매출이나 입장료로 과거 입장객을 역산하지 않는다. 원본 기록이 없으면 미기록이다.
	FFormatNamedArguments VisitorArgs;
	VisitorArgs.Add(TEXT("Count"), FText::AsNumber(DayView.AdmittedVisitors));
	CurrentAdmittedVisitors = DayView.AdmittedVisitors >= 0
		? FText::Format(VisitorCountFormat, VisitorArgs) : UnknownValueText;
	// 0점도 유효한 기록이다. 원본에 날짜를 대응시킬 수 없는 경우만 미기록으로 표시한다.
	FFormatNamedArguments SatisfactionArgs;
	SatisfactionArgs.Add(TEXT("Score"), FText::AsNumber(DayView.DailySatisfaction));
	CurrentSatisfaction = DayView.bHasDailySatisfaction && DayView.DailySatisfaction >= 0
		&& DayView.DailySatisfaction <= 100
		? FText::Format(SatisfactionFormat, SatisfactionArgs) : NoSatisfactionRecordText;
	bHasDay = true;
	RefreshRow();
}

void UFF_FinalReportDayRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshRow();
}

void UFF_FinalReportDayRowWidget::RefreshRow()
{
	// Designer 미리보기의 샘플 문구를 덮어쓰지 않는다.
	if (!bHasDay) { return; }
	if (Text_Day) { Text_Day->SetText(CurrentDay); }
	if (Text_Revenue) { Text_Revenue->SetText(CurrentRevenue); }
	if (Text_Expense) { Text_Expense->SetText(CurrentExpense); }
	if (Text_NetProfit) { Text_NetProfit->SetText(CurrentNetProfit); }
	NetProfitStyle.ApplyColor(Text_NetProfit, CurrentNetProfitAmount);
	if (Text_AdmittedVisitors) { Text_AdmittedVisitors->SetText(CurrentAdmittedVisitors); }
	if (Text_Satisfaction) { Text_Satisfaction->SetText(CurrentSatisfaction); }
}

#undef LOCTEXT_NAMESPACE
