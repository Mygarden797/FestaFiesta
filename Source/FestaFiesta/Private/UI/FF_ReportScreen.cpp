// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_ReportScreen.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/FF_CommonButtonBase.h"
#include "UI/FF_FestivalViewModelSubsystem.h"
#include "UI/FF_MetricRowWidget.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_FestivalViewModelSubsystem* UFF_ReportScreen::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
	}
	return nullptr;
}

void UFF_ReportScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindViewModel();

	if (Button_NextDay)
	{
		Button_NextDay->OnFFButtonClicked.AddUniqueDynamic(this, &UFF_ReportScreen::HandleNextDayClicked);

		// 라벨을 코드에서 정한다.
		// 이 버튼은 타이틀 버튼 WBP를 재사용하므로 WBP 기본값이 `플레이(미리보기용)`다.
		// 2026-08-12 검증에서 그 문구가 그대로 화면에 나왔다. WBP마다 손으로 고치는 대신
		// 화면이 자기 버튼의 문구를 소유한다 — 재사용 버튼을 다른 화면에 써도 같은 사고가 안 난다.
		Button_NextDay->SetButtonLabel(LOCTEXT("Report_NextDayButton", "다음 날"));
	}
	else
	{
		// 이 버튼이 없으면 Day 2로 넘어갈 방법이 없다. 조용히 넘기면 원인을 못 찾는다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] Button_NextDay가 없다. Report에서 다음 날로 진행할 수 없다."), *GetName());
	}
}

void UFF_ReportScreen::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면이 다시 붙을 때 구독이 끊겨 있을 수 있다.
	BindViewModel();

	// Bind + Sync의 Sync. 이벤트는 앞으로의 변화만 알려준다.
	RefreshAll();
}

void UFF_ReportScreen::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_ReportScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	// 스택에서 다시 활성화될 때는 NativeConstruct가 불리지 않는다.
	// Report는 Day마다 다시 뜨므로 여기서도 현재값을 맞춘다.
	RefreshAll();
}

void UFF_ReportScreen::BindViewModel()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] UFF_FestivalViewModelSubsystem을 찾지 못했다. Report 숫자가 갱신되지 않는다."), *GetName());
		return;
	}

	CachedViewModel = ViewModel;

	// AddUniqueDynamic만 쓴다. 중복 구독을 API가 막아준다.
	ViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_ReportScreen::HandleSimSatisfactionChanged);
	ViewModel->OnScoreSatisfactionChanged.AddUniqueDynamic(this, &UFF_ReportScreen::HandleScoreSatisfactionChanged);
	ViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_ReportScreen::HandleSafetyChanged);
	ViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_ReportScreen::HandleTodayRevenueChanged);

	// Stage·Day가 바뀌면 제목과 다음 날 버튼 상태가 함께 바뀐다.
	ViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_ReportScreen::HandleStageViewChanged);
}

void UFF_ReportScreen::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_ReportScreen::HandleSimSatisfactionChanged);
		CachedViewModel->OnScoreSatisfactionChanged.RemoveDynamic(this, &UFF_ReportScreen::HandleScoreSatisfactionChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_ReportScreen::HandleSafetyChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_ReportScreen::HandleTodayRevenueChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_ReportScreen::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_ReportScreen::SetRow(UFF_MetricRowWidget* Row, const FText& Caption, int32 Value, bool bShowBand)
{
	if (!Row)
	{
		return;
	}

	Row->SetBandVisible(bShowBand);
	Row->SetMetric(Caption, Value);
}

void UFF_ReportScreen::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// 화면 문구에 내부 이름을 쓰지 않는다. SimSatisfaction → 관객 만족도.
	SetRow(Row_SimSatisfaction, LOCTEXT("Report_SimSatisfaction", "관객 만족도"), ViewModel->GetSimSatisfaction(), true);
	SetRow(Row_ScoreSatisfaction, LOCTEXT("Report_ScoreSatisfaction", "평가 점수"), ViewModel->GetScoreSatisfaction(), true);
	SetRow(Row_Safety, LOCTEXT("Report_Safety", "안전"), ViewModel->GetSafety(), true);

	// 매출은 금액이라 구간 표시를 끈다. 0~100 기준을 금액에 적용하면 항상 "나쁨"이 된다.
	SetRow(Row_TodayRevenue, LOCTEXT("Report_TodayRevenue", "오늘 매출"), ViewModel->GetTodayRevenue(), false);

	RefreshTitle();
	RefreshBreakdown();
	RefreshNextDayButton();
}

void UFF_ReportScreen::RefreshTitle()
{
	if (!Text_Title)
	{
		return;
	}

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const int32 Day = ViewModel ? ViewModel->GetCurrentDay() : 0;

	Text_Title->SetText(FText::Format(LOCTEXT("Report_Title", "Day {0} 결과"), FText::AsNumber(Day)));
}

void UFF_ReportScreen::RefreshBreakdown()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	const int32 SimValue = ViewModel->GetSimSatisfaction();
	const int32 ScoreValue = ViewModel->GetScoreSatisfaction();

	// !! 임시 계산 !! FinaleBonus를 소유자가 별도 값으로 주면 이 뺄셈을 지운다.
	// 지금 두 지표가 같은 더미라 0으로 나온다. FF.SetMetrics 62 82 88로 검증한다.
	const int32 FinaleBonus = ScoreValue - SimValue;

	if (Text_FinaleBonus)
	{
		Text_FinaleBonus->SetText(FText::AsNumber(FinaleBonus));
	}

	if (Text_SatisfactionBreakdown)
	{
		// 지표를 왜 둘로 나눴는지가 이 한 줄로 설명된다. 그래서 툴팁을 두지 않는다.
		Text_SatisfactionBreakdown->SetText(FText::Format(
			LOCTEXT("Report_Breakdown", "관객 만족도 {0} + 피날레 {1} = 평가 점수 {2}"),
			FText::AsNumber(SimValue),
			FText::AsNumber(FinaleBonus),
			FText::AsNumber(ScoreValue)));
	}
}

void UFF_ReportScreen::RefreshNextDayButton()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	const bool bCanAdvance = ViewModel && ViewModel->CanAdvanceToNextDay();

	if (Button_NextDay)
	{
		Button_NextDay->SetIsEnabled(bCanAdvance);
	}

	if (!Text_NextDayHint)
	{
		return;
	}

	// 비활성 버튼만 두면 왜 못 누르는지 알 수 없다. 이유를 문구로 함께 준다 (AT-011).
	//
	// !! 이유를 뭉치지 않는다 !!
	// 2026-08-12 검증에서 Day 1 · Live 단계인데 "마지막 날입니다"가 떴다.
	// `CanAdvanceToNextDay()`가 false인 이유가 둘(마지막 날 / 아직 Report가 아님)인데
	// 한 문구로 처리했기 때문이다. 화면에 틀린 사실을 쓰는 것은 빈 칸보다 나쁘다.
	if (bCanAdvance)
	{
		Text_NextDayHint->SetText(FText::GetEmpty());
		Text_NextDayHint->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (ViewModel && ViewModel->IsReportStage() && ViewModel->IsFinalDay())
	{
		Text_NextDayHint->SetText(LOCTEXT("Report_LastDayHint", "마지막 날입니다. 축제가 곧 마무리됩니다."));
		Text_NextDayHint->SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}

	// Report 단계가 아닌데 이 화면이 떠 있는 상태다. 제품에서는 생기지 않고 검증 중에만 보인다.
	// 플레이어에게 설명할 사실이 없으므로 아무 문구도 쓰지 않는다.
	Text_NextDayHint->SetText(FText::GetEmpty());
	Text_NextDayHint->SetVisibility(ESlateVisibility::Collapsed);
}

void UFF_ReportScreen::HandleSimSatisfactionChanged(int32 NewValue)
{
	// 분해식이 두 지표를 함께 쓰므로 부분 갱신하지 않는다.
	RefreshAll();
}

void UFF_ReportScreen::HandleScoreSatisfactionChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_ReportScreen::HandleSafetyChanged(int32 NewValue)
{
	SetRow(Row_Safety, LOCTEXT("Report_Safety", "안전"), NewValue, true);
}

void UFF_ReportScreen::HandleTodayRevenueChanged(int32 NewValue)
{
	SetRow(Row_TodayRevenue, LOCTEXT("Report_TodayRevenue", "오늘 매출"), NewValue, false);
}

void UFF_ReportScreen::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	RefreshTitle();
	RefreshNextDayButton();
}

void UFF_ReportScreen::HandleNextDayClicked(UFF_CommonButtonBase* Button)
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] ViewModel이 없어 다음 날 요청을 보낼 수 없다."), *GetName());
		return;
	}

	// !! 낙관적 갱신 금지 !! 여기서 화면을 닫거나 Day를 올려 표시하지 않는다.
	// 요청만 보내고 결과는 OnStageViewChanged로 돌아온다 (UI 규칙 2).
	ViewModel->RequestAdvanceToNextDay();

	BP_OnNextDayRequested();
}

#undef LOCTEXT_NAMESPACE
