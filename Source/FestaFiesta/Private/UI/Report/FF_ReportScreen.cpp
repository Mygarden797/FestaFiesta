// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Report/FF_ReportScreen.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/FF_EconomyDisplayTextLibrary.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "UI/Core/FF_UIFlowSubsystem.h"
#include "UI/Report/FF_FinalReportDayRowWidget.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaFinalReport"

namespace
{
	void SetReportText(UTextBlock* Text, const FText& Value)
	{
		if (Text) { Text->SetText(Value); }
	}

	void SetOptionalReportText(UTextBlock* Text, const FText& Value)
	{
		if (Text)
		{
			Text->SetText(Value);
			Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}

	void SetEvaluationAxis(UTextBlock* Text, UProgressBar* Bar, const int32 Score)
	{
		SetReportText(Text, FText::AsNumber(Score));
		// 원본 평가 점수의 0~100 범위를 진행 바의 0~1 표현으로만 변환한다.
		if (Bar) { Bar->SetPercent(FMath::Clamp(static_cast<float>(Score) / 100.f, 0.f, 1.f)); }
	}
}

UFF_ReportScreen::UFF_ReportScreen(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ScreenInputMode = EFF_ScreenInputMode::Menu;
	bIsBackHandler = true;
	TitleText = LOCTEXT("Title", "축제 최종 결산");
	FestivalSummaryFormat = LOCTEXT("FestivalSummary", "{Festival} · {Days}일간의 기록");
	ScoreFormat = LOCTEXT("Score", "{Score}점");
	VisitorCountFormat = LOCTEXT("VisitorCount", "{Count}명");
	DaysSummaryFormat = LOCTEXT("DaysSummary", "{Days}일 합계 결산");
	PositiveAmountFormat = LOCTEXT("PositiveAmount", "+{Amount}");
	VisitorHistoryNoteText = LOCTEXT("VisitorHistoryNote", "이전 날짜의 입장객 기록은 제공되지 않습니다.");
	UnknownValueText = LOCTEXT("UnknownValue", "—");
	PendingText = LOCTEXT("Pending", "축제의 마지막 기록을 정리하고 있습니다.");
	BackHintText = LOCTEXT("BackHint", "축제를 마치려면 ‘로비로’ 버튼을 눌러 주세요.");
	LobbyUnavailableText = LOCTEXT("LobbyUnavailable", "로비로 돌아갈 수 없습니다. 잠시 후 다시 눌러 주세요.");
	DayRowsUnavailableText = LOCTEXT("DayRowsUnavailable", "날짜별 내역을 표시할 수 없습니다.");
	LobbyLabelText = LOCTEXT("LobbyLabel", "로비로");
}

void UFF_ReportScreen::ConfigureDayRowDefaults(TSubclassOf<UFF_FinalReportDayRowWidget> InRowClass)
{
	if (!DayRowClass) { DayRowClass = InRowClass; }
}

void UFF_ReportScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ScrollStyle.ApplyTo(WidgetTree);
}

void UFF_ReportScreen::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (IsDesignTime()) { ScrollStyle.ApplyTo(WidgetTree); }
}

void UFF_ReportScreen::NativeConstruct()
{
	// 옛 Report WBP에 저장된 기본값으로 CommonUI의 자동 닫기가 다시 켜지지 않게 한다.
	bIsBackHandler = true;
	Super::NativeConstruct();
	BindActions();
	RefreshAll();
}

void UFF_ReportScreen::NativeDestruct()
{
	if (Button_Lobby)
	{
		Button_Lobby->OnClicked.RemoveDynamic(this, &UFF_ReportScreen::HandleLobbyClicked);
	}
	CachedViewModel = nullptr;
	bHasFinalReport = false;
	Super::NativeDestruct();
}

void UFF_ReportScreen::NativeOnActivated()
{
	BindActions();
	RefreshAll();
	Super::NativeOnActivated();
}

void UFF_ReportScreen::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// 종료 이벤트와 평가 동결 순서가 달라도 화면에 임시 평가가 나오지 않는다.
	// 한 번 받은 최종 값은 유지하고 이후에는 매 프레임 원본을 읽지 않는다.
	if (!bHasFinalReport) { RefreshAll(); }
}

void UFF_ReportScreen::BindActions()
{
	if (Button_Lobby)
	{
		Button_Lobby->OnClicked.AddUniqueDynamic(this, &UFF_ReportScreen::HandleLobbyClicked);
	}
}

UFF_FestivalViewModelSubsystem* UFF_ReportScreen::GetViewModel()
{
	if (!CachedViewModel)
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			CachedViewModel = GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
		}
	}
	return CachedViewModel;
}

void UFF_ReportScreen::RefreshAll()
{
	if (bHasFinalReport) { return; }

	SetReportText(Text_Title, TitleText);
	SetReportText(Text_LobbyLabel, LobbyLabelText);
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	FFF_FinalReportView Report;
	if (!ViewModel || !ViewModel->GetFinalReport(Report))
	{
		ShowPending();
		return;
	}

	bHasFinalReport = true;
	ApplyReport(Report);
	if (Button_Lobby)
	{
		Button_Lobby->SetIsEnabled(true);
		if (IsActivated()) { Button_Lobby->SetFocus(); }
	}
}

void UFF_ReportScreen::ShowPending()
{
	SetStatus(PendingText);
	if (Button_Lobby) { Button_Lobby->SetIsEnabled(false); }
	for (UTextBlock* Text : {Text_FestivalSummary.Get(), Text_Grade.Get(), Text_TotalScore.Get(),
		Text_SatisfactionScore.Get(), Text_VisitorScore.Get(), Text_FinanceScore.Get(), Text_BuzzScore.Get(),
		Text_TotalAdmittedVisitors.Get(), Text_TotalRevenue.Get(), Text_TotalExpense.Get(), Text_NetProfit.Get(),
		Text_DaysSummary.Get(), Text_Gold.Get(), Text_Refund.Get()})
	{
		SetReportText(Text, UnknownValueText);
	}
	NetProfitStyle.ApplyColor(Text_NetProfit, {});
	for (UProgressBar* Bar : {Progress_SatisfactionScore.Get(), Progress_VisitorScore.Get(),
		Progress_FinanceScore.Get(), Progress_BuzzScore.Get()})
	{
		if (Bar) { Bar->SetPercent(0.f); }
	}
	SetOptionalReportText(Text_Headline, FText::GetEmpty());
	SetOptionalReportText(Text_WeakestAxisComment, FText::GetEmpty());
	SetOptionalReportText(Text_VisitorHistoryNote, FText::GetEmpty());
	if (Box_DayRows) { Box_DayRows->ClearChildren(); }
}

void UFF_ReportScreen::ApplyReport(const FFF_FinalReportView& Report)
{
	FFormatNamedArguments SummaryArgs;
	SummaryArgs.Add(TEXT("Festival"), Report.FestivalName);
	SummaryArgs.Add(TEXT("Days"), FText::AsNumber(Report.DaysPlayed));
	SetReportText(Text_FestivalSummary, FText::Format(FestivalSummaryFormat, SummaryArgs));
	SetReportText(Text_DaysSummary, FText::Format(DaysSummaryFormat, SummaryArgs));
	SetReportText(Text_Grade, UFF_EconomyDisplayTextLibrary::GetFestivalEvaluationGradeText(Report.Evaluation.Grade));
	FFormatNamedArguments ScoreArgs;
	ScoreArgs.Add(TEXT("Score"), FText::AsNumber(Report.Evaluation.TotalScore));
	SetReportText(Text_TotalScore, FText::Format(ScoreFormat, ScoreArgs));
	SetOptionalReportText(Text_Headline, Report.Evaluation.Headline);
	SetOptionalReportText(Text_WeakestAxisComment, Report.Evaluation.WeakestAxisComment);
	SetEvaluationAxis(Text_SatisfactionScore, Progress_SatisfactionScore, Report.Evaluation.SatisfactionScore);
	SetEvaluationAxis(Text_VisitorScore, Progress_VisitorScore, Report.Evaluation.VisitorScore);
	SetEvaluationAxis(Text_FinanceScore, Progress_FinanceScore, Report.Evaluation.FinanceScore);
	SetEvaluationAxis(Text_BuzzScore, Progress_BuzzScore, Report.Evaluation.BuzzScore);

	FFormatNamedArguments VisitorArgs;
	VisitorArgs.Add(TEXT("Count"), FText::AsNumber(Report.TotalAdmittedVisitors));
	SetReportText(Text_TotalAdmittedVisitors, Report.TotalAdmittedVisitors >= 0
		? FText::Format(VisitorCountFormat, VisitorArgs) : UnknownValueText);
	SetReportText(Text_TotalRevenue, FText::AsNumber(Report.Revenue));
	SetReportText(Text_TotalExpense, FText::AsNumber(Report.Expense));
	SetReportText(Text_NetProfit, FFF_SignedAmountStyle::Format(Report.NetProfit, PositiveAmountFormat));
	NetProfitStyle.ApplyColor(Text_NetProfit, Report.NetProfit);
	// 보유액은 날짜별 잔액을 더하지 않는다. 환급도 매출·순이익에 다시 더하지 않는다.
	SetReportText(Text_Gold, FText::AsNumber(Report.Gold));
	SetReportText(Text_Refund, FText::AsNumber(Report.Refund));

	const bool bMissingVisitorHistory = Report.Days.ContainsByPredicate([](const FFF_FinalReportDayView& Day)
	{
		return Day.AdmittedVisitors == INDEX_NONE;
	});
	SetOptionalReportText(Text_VisitorHistoryNote, bMissingVisitorHistory
		? VisitorHistoryNoteText : FText::GetEmpty());
	SetStatus(RebuildDayRows(Report) ? FText::GetEmpty() : DayRowsUnavailableText);
}

bool UFF_ReportScreen::RebuildDayRows(const FFF_FinalReportView& Report)
{
	if (!Box_DayRows || !DayRowClass) { return Report.Days.IsEmpty(); }
	Box_DayRows->ClearChildren();
	for (const FFF_FinalReportDayView& Day : Report.Days)
	{
		UFF_FinalReportDayRowWidget* Row = CreateWidget<UFF_FinalReportDayRowWidget>(this, DayRowClass);
		if (!Row) { return false; }
		Row->ConfigureDay(Day);
		if (UVerticalBoxSlot* RowSlot = Box_DayRows->AddChildToVerticalBox(Row))
		{
			RowSlot->SetHorizontalAlignment(HAlign_Fill);
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, DayRowSpacing));
		}
	}
	return true;
}

void UFF_ReportScreen::SetStatus(const FText& Status)
{
	SetOptionalReportText(Text_Status, Status);
}

UWidget* UFF_ReportScreen::NativeGetDesiredFocusTarget() const
{
	if (Button_Lobby && Button_Lobby->GetIsEnabled()) { return Button_Lobby; }
	return Super::NativeGetDesiredFocusTarget();
}

bool UFF_ReportScreen::NativeOnHandleBackAction()
{
	// 자기 화면만 닫으면 종료된 축제 HUD로 돌아간다. 복귀는 명시적인 로비 버튼으로만 요청한다.
	SetStatus(bHasFinalReport ? BackHintText : PendingText);
	if (Button_Lobby && Button_Lobby->GetIsEnabled()) { Button_Lobby->SetFocus(); }
	return true;
}

FReply UFF_ReportScreen::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// CommonUI Back 액션 데이터가 비어 있어도 ESC/패드 뒤로가 화면만 지우지 못하게 한다.
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		if (!InKeyEvent.IsRepeat()) { NativeOnHandleBackAction(); }
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UFF_ReportScreen::HandleLobbyClicked()
{
	if (!bHasFinalReport) { return; }
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UFF_UIFlowSubsystem* Flow = GI->GetSubsystem<UFF_UIFlowSubsystem>())
		{
			// 종료·저장·레벨 이동은 기존 Flow가 책임진다. 여기서 화면을 먼저 없애지 않는다.
			Flow->RequestFinishFestival();
			return;
		}
	}
	SetStatus(LobbyUnavailableText);
}

#undef LOCTEXT_NAMESPACE
