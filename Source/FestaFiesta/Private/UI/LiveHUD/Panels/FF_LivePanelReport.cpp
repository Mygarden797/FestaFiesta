// 하루 결산은 ViewModel이 확정한 날짜별 읽기 모델만 표시한다.
#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/LiveHUD/Panels/FF_SatisfactionBreakdownWidget.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

void UFF_DailyReportLedgerRowWidget::ConfigureRow(const FText& Label, const FText& Amount)
{
	CurrentLabel = Label;
	CurrentAmount = Amount;
	bHasRow = true;
	RefreshRow();
}

void UFF_DailyReportLedgerRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (bHasRow) { RefreshRow(); }
}

void UFF_DailyReportLedgerRowWidget::RefreshRow()
{
	if (Text_Label) { Text_Label->SetText(CurrentLabel); }
	if (Text_Amount)
	{
		Text_Amount->SetText(CurrentAmount);
		Text_Amount->SetVisibility(CurrentAmount.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UFF_LivePanelReport::ConfigureLedgerRowDefaults(TSubclassOf<UFF_DailyReportLedgerRowWidget> InRowClass)
{
	LedgerRowClass = InRowClass;
}

void UFF_LivePanelReport::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	if (Button_Advance) { Button_Advance->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAdvanceClicked); }
	BindViewModel();
	RefreshAll();
}

void UFF_LivePanelReport::NativeOnPanelClosed()
{
	if (Button_Advance) { Button_Advance->OnClicked.RemoveDynamic(this, &ThisClass::HandleAdvanceClicked); }
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelReport::NativeDestruct()
{
	if (Button_Advance) { Button_Advance->OnClicked.RemoveDynamic(this, &ThisClass::HandleAdvanceClicked); }
	UnbindViewModel();
	Super::NativeDestruct();
}

UFF_FestivalViewModelSubsystem* UFF_LivePanelReport::GetViewModel() const
{
	if (CachedViewModel) { return CachedViewModel; }
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LivePanelReport::BindViewModel()
{
	UnbindViewModel();
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	if (!CachedViewModel) { return; }
	// 실시간 매출/만족도를 섞지 않는다. 정산 확정과 날짜 변경 때만 같은 읽기 모델을 다시 읽는다.
	CachedViewModel->OnDailyReportReady.AddUniqueDynamic(this, &ThisClass::HandleReportChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &ThisClass::HandleStageViewChanged);
}

void UFF_LivePanelReport::UnbindViewModel()
{
	if (CachedViewModel)
	{
		CachedViewModel->OnDailyReportReady.RemoveDynamic(this, &ThisClass::HandleReportChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &ThisClass::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LivePanelReport::HandleAdvanceClicked()
{
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	if (!VM || !VM->GetDailyReport()) { RefreshAdvance(); return; }
	// 마감에서 눌렀으면 HUD 가 마감 건너뛰기와 다음 날을 차례로 요청한다.
	if (UFF_LiveHudScreen* Screen = GetOwningScreen()) { Screen->RequestAdvance(); }
}

void UFF_LivePanelReport::RefreshAdvance()
{
	const UFF_LiveHudScreen* Screen = GetOwningScreen();
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	// GetDailyReport 가 결산을 띄우는 단계(마감·결산)와 날짜를 이미 확인한다.
	const FFF_DailyReportView* Report = VM ? VM->GetDailyReport() : nullptr;
	const bool bReady = Report && Report->Day == VM->GetCurrentDay();
	const EFF_LiveCtaState State = Screen ? Screen->GetCtaState() : EFF_LiveCtaState::Hidden;
	const bool bStageReady = State != EFF_LiveCtaState::Hidden && State != EFF_LiveCtaState::BlockedNotReport;
	const bool bCanAdvance = bStageReady && bReady;
	SetTextOrHide(Text_AdvanceLabel, Screen ? Screen->GetCtaLabelText() : FText::GetEmpty());
	if (Button_Advance)
	{
		Button_Advance->SetIsEnabled(bCanAdvance);
		Button_Advance->SetRenderOpacity(bCanAdvance ? 1.f : DimmedOpacity);
		Button_Advance->SetVisibility(State == EFF_LiveCtaState::Hidden ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	const FText CtaReason = Screen ? Screen->GetCtaReasonText() : FText::GetEmpty();
	SetTextOrHide(Text_AdvanceReason, bStageReady && !bReady ? PendingReportText : CtaReason);
}

void UFF_LivePanelReport::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay) { RefreshAll(); }
void UFF_LivePanelReport::HandleIntMetricChanged(int32 NewValue) { RefreshAll(); }
void UFF_LivePanelReport::HandleReportChanged(int32 ReportDay) { RefreshAll(); }

void UFF_LivePanelReport::RefreshScore(UTextBlock* Label, UProgressBar* Bar, int32 Score, bool bAvailable)
{
	const bool bMeasured = bAvailable && Score >= 0;
	SetTextOrHide(Label, bMeasured ? FText::AsNumber(Score) : NotMeasuredText);
	if (Bar)
	{
		// 막대만 숨겨 자리를 유지한다. 미측정을 0점 막대로 보이지 않는다.
		Bar->SetVisibility(bMeasured ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
		if (bMeasured) { Bar->SetPercent(FMath::Clamp(static_cast<float>(Score) / 100.f, 0.f, 1.f)); }
	}
}

void UFF_LivePanelReport::RefreshLedgerRows(UVerticalBox* Box, const TArray<FFF_LedgerRow>& Rows,
	const TMap<FName, FText>& LabelOverrides, const FText& EmptyText, const bool bExpense)
{
	if (!Box) { return; }
	Box->ClearChildren();
	const auto AddRow = [this, Box](const FText& Label, const FText& Amount)
	{
		UWidget* Row = nullptr;
		if (LedgerRowClass)
		{
			if (UFF_DailyReportLedgerRowWidget* Entry = CreateWidget<UFF_DailyReportLedgerRowWidget>(this, LedgerRowClass))
			{
				Entry->ConfigureRow(Label, Amount);
				Row = Entry;
			}
		}
		if (!Row)
		{
			// 행 클래스가 없는 이전 에셋도 부모의 편집 가능한 글꼴/색으로 읽을 수 있다.
			UHorizontalBox* Columns = NewObject<UHorizontalBox>(this);
			UTextBlock* Name = NewObject<UTextBlock>(this); Name->SetText(Label); ApplyDynamicRowStyle(*Name);
			UHorizontalBoxSlot* NameSlot = Columns->AddChildToHorizontalBox(Name);
			NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); NameSlot->SetPadding(FMargin(0.f, 0.f, RowColumnGap, 0.f));
			NameSlot->SetVerticalAlignment(VAlign_Center);
			if (!Amount.IsEmpty())
			{
				USizeBox* AmountSize = NewObject<USizeBox>(this); AmountSize->SetMinDesiredWidth(AmountColumnMinWidth);
				UTextBlock* Value = NewObject<UTextBlock>(this); Value->SetText(Amount); ApplyDynamicRowStyle(*Value);
				Value->SetJustification(ETextJustify::Right); AmountSize->AddChild(Value);
				UHorizontalBoxSlot* ValueSlot = Columns->AddChildToHorizontalBox(AmountSize); ValueSlot->SetVerticalAlignment(VAlign_Center);
			}
			Row = Columns;
		}
		UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row);
		RowSlot->SetHorizontalAlignment(HAlign_Fill); RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, FMath::Max(0.f, RowSpacing)));
	};
	for (const FFF_LedgerRow& Row : Rows)
	{
		const FText* Override = LabelOverrides.Find(Row.ReasonId);
		AddRow(Override ? *Override : Row.DisplayName, bExpense
			? FFF_SignedAmountStyle::FormatExpense(Row.Amount)
			: FFF_SignedAmountStyle::Format(Row.Amount, PositiveAmountFormat));
	}
	if (Rows.IsEmpty() && !EmptyText.IsEmpty()) { AddRow(EmptyText, FText::GetEmpty()); }
}

TSharedRef<SWidget> UFF_LivePanelReport::RebuildWidget()
{
	// Slate 트리가 만들어지기 전에 끼워야 순서가 맞는다. 만든 뒤에는 InsertChildAt 이 UMG 슬롯 순서만 바꾸고 화면은 맨 끝에 붙는다
	EnsureSatisfactionBreakdown();
	return Super::RebuildWidget();
}

void UFF_LivePanelReport::EnsureSatisfactionBreakdown()
{
	if (Widget_SatisfactionBreakdown || !WidgetTree)
	{
		return;
	}
	// 결산 WBP 는 디자인 도구가 만든 에셋이라 건드리지 않는다. 만족도 카드 본문을 이름으로 찾아 평가 만족도 줄 위에 넣는다
	UVerticalBox* Body = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Report_SatisfactionBody")));
	if (!Body)
	{
		return;
	}
	Widget_SatisfactionBreakdown = CreateWidget<UFF_SatisfactionBreakdownWidget>(this, UFF_SatisfactionBreakdownWidget::StaticClass());
	if (!Widget_SatisfactionBreakdown)
	{
		return;
	}
	const UWidget* ScoreRow = WidgetTree->FindWidget(TEXT("Report_ScoreRow"));
	const int32 ScoreRowIndex = ScoreRow ? Body->GetChildIndex(ScoreRow) : INDEX_NONE;
	UPanelSlot* BreakdownSlot = ScoreRowIndex != INDEX_NONE
		? Body->InsertChildAt(ScoreRowIndex, Widget_SatisfactionBreakdown)
		: Body->AddChild(Widget_SatisfactionBreakdown);
	if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(BreakdownSlot))
	{
		BoxSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
}

void UFF_LivePanelReport::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* VM = GetViewModel();
	const FFF_DailyReportView* Report = VM ? VM->GetDailyReport() : nullptr;
	// 단계는 GetDailyReport 가 본다(마감도 결산을 띄운다). 여기서 결산 단계로 다시 좁히면
	// 마감의 결산이 버려져 버튼이 잠긴다. 날짜만 다시 확인한다.
	if (Report && Report->Day != VM->GetCurrentDay()) { Report = nullptr; }
	const int32 Day = Report ? Report->Day : VM ? VM->GetCurrentDay() : INDEX_NONE;
	const FFormatNamedArguments DayArgs{{TEXT("Day"), Day > 0 ? FText::AsNumber(Day) : UnknownValueText}};
	SetTextOrHide(Text_PanelTitle, FText::Format(DayTitleFormat, DayArgs));
	SetTextOrHide(Text_DayLabel, FText::Format(DayLabelFormat, DayArgs));
	const FText Comment = Report && !Report->AssistantComment.IsEmpty()
		? FText::FormatNamed(AssistantCommentFormat, TEXT("Comment"), Report->AssistantComment) : FText::GetEmpty();
	if (Text_AssistantComment)
	{
		Text_AssistantComment->SetAutoWrapText(true);
		SetTextOrHide(Text_AssistantComment, Comment);
		SetTextOrHide(Text_MissingNote, Report ? FText::GetEmpty() : PendingReportText);
	}
	else
	{
		if (Text_MissingNote && Report)
		{
			Text_MissingNote->SetAutoWrapText(true);
		}
		SetTextOrHide(Text_MissingNote, Report ? Comment : PendingReportText);
	}

	SetTextOrHide(Text_TodayRevenue, Report ? FFF_SignedAmountStyle::Format(Report->Revenue, PositiveAmountFormat) : UnknownValueText);
	SetTextOrHide(Text_TodayExpense, Report ? FFF_SignedAmountStyle::FormatExpense(Report->Expense) : UnknownValueText);
	// 순이익의 음수는 결측값이 아니다. ReadModel의 부호를 그대로 표시한다.
	SetTextOrHide(Text_TodayNetProfit, Report ? FFF_SignedAmountStyle::Format(Report->NetProfit, PositiveAmountFormat) : UnknownValueText);
	NetProfitStyle.ApplyColor(Text_TodayNetProfit, Report ? TOptional<int64>(Report->NetProfit) : TOptional<int64>());
	SetTextOrHide(Text_Refund, Report ? FFF_SignedAmountStyle::Format(Report->Refund, PositiveAmountFormat) : UnknownValueText);
	SetTextOrHide(Text_Gold, Report ? FText::AsNumber(Report->Gold) : UnknownValueText);
	SetTextOrHide(Text_TotalRevenue, Report ? FFF_SignedAmountStyle::Format(Report->TotalRevenue, PositiveAmountFormat) : UnknownValueText);
	SetTextOrHide(Text_LedgerNote, LedgerNoteText);
	const TArray<FFF_LedgerRow> NoRows;
	RefreshLedgerRows(Box_EarnRows, Report ? Report->EarnRows : NoRows, EarnLabelOverrides, Report ? NoIncomeText : FText::GetEmpty(), false);
	RefreshLedgerRows(Box_SpendRows, Report ? Report->SpendRows : NoRows, SpendLabelOverrides, Report ? NoExpenseText : FText::GetEmpty(), true);

	const bool bHasSatisfaction = Report && Report->bHasSatisfaction;
	RefreshScore(Text_SimSatisfaction, nullptr, Report ? Report->SimSatisfaction : INDEX_NONE, bHasSatisfaction);
	RefreshScore(Text_ScoreSatisfaction, nullptr, Report ? Report->ScoreSatisfaction : INDEX_NONE, bHasSatisfaction);
	RefreshScore(Text_WaitScore, Progress_WaitScore, Report ? Report->WaitScore : INDEX_NONE, bHasSatisfaction);
	RefreshScore(Text_DensityScore, Progress_DensityScore, Report ? Report->DensityScore : INDEX_NONE, bHasSatisfaction);
	RefreshScore(Text_PreferenceScore, Progress_PreferenceScore, Report ? Report->PreferenceScore : INDEX_NONE, bHasSatisfaction);
	RefreshScore(Text_NeedScore, Progress_NeedScore, Report ? Report->NeedScore : INDEX_NONE, bHasSatisfaction);
	if (Widget_SatisfactionBreakdown)
	{
		Widget_SatisfactionBreakdown->SetBreakdown(bHasSatisfaction ? Report->SatisfactionBreakdown : FFF_SatisfactionBreakdown());
	}
	SetTextOrHide(Text_SatisfactionNote, SatisfactionNoteText);
	// 안전도에는 확정된 측정 계약이 없다. 이전 임시값 88을 결과로 표시하지 않는다.
	SetTextOrHide(Text_Safety, NotMeasuredText);

	const bool bHasGoal = Report && Report->AdmittedVisitors >= 0 && Report->DailyVisitorGoal >= 0;
	SetTextOrHide(Text_DailyGoal, bHasGoal ? FText::Format(GoalProgressFormat, FFormatNamedArguments{
		{TEXT("Admitted"), FText::AsNumber(Report->AdmittedVisitors)},
		{TEXT("Goal"), FText::AsNumber(Report->DailyVisitorGoal)}}) : NotMeasuredText);
	SetTextOrHide(Text_DailyGoalState, bHasGoal ? (Report->bDailyGoalAchieved ? GoalAchievedText : GoalMissedText) : NotMeasuredText);
	SetTextOrHide(Text_Staff, Report && Report->TotalStaff >= 0 && Report->HiredStaff >= 0
		? FText::Format(StaffFormat, FFormatNamedArguments{
			{TEXT("Total"), FText::AsNumber(Report->TotalStaff)}, {TEXT("Hired"), FText::AsNumber(Report->HiredStaff)}}) : UnknownValueText);
	SetTextOrHide(Text_Programs, Report && Report->Programs >= 0
		? FText::Format(ProgramCountFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Report->Programs)}}) : UnknownValueText);
	RefreshAdvance();
}
