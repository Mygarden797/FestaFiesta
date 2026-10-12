// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

// =============================================================================
// FFF_LiveOperationsRules — 순수 함수
// =============================================================================

FFF_LiveGoalProgress FFF_LiveOperationsRules::BuildGoalProgress(int32 VisitorCount, int32 DailyGoal)
{
	FFF_LiveGoalProgress Result;

	// 미집계는 실제 입장객 0명과 구분한다.
	if (VisitorCount < 0 || DailyGoal <= 0)
	{
		return Result;
	}

	Result.Ratio = FMath::Clamp(static_cast<float>(VisitorCount) / static_cast<float>(DailyGoal), 0.f, 1.f);

	if (VisitorCount >= DailyGoal)
	{
		Result.State = EFF_LiveGoalState::Reached;
		// 달성한 뒤 `0명 남음` 을 띄우면 아직 뭔가 남은 것처럼 읽힌다. 값을 두지 않는다.
		Result.Remaining = INDEX_NONE;
		return Result;
	}

	Result.State = EFF_LiveGoalState::InProgress;
	Result.Remaining = DailyGoal - VisitorCount;
	return Result;
}

// =============================================================================
// UFF_LivePanelOperations
// =============================================================================

void UFF_LivePanelOperations::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	BindViewModel();
	RefreshAll();
}

void UFF_LivePanelOperations::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelOperations::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_LivePanelOperations::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!CachedViewModel || !bHasBuzzValue || !Bar_Buzz || DisplayedBuzzRatio == TargetBuzzRatio) return;

	DisplayedBuzzRatio = FMath::FInterpTo(DisplayedBuzzRatio, TargetBuzzRatio, InDeltaTime, BuzzInterpSpeed);
	if (FMath::IsNearlyEqual(DisplayedBuzzRatio, TargetBuzzRatio, 0.0001f)) DisplayedBuzzRatio = TargetBuzzRatio;
	Bar_Buzz->SetPercent(DisplayedBuzzRatio);
}

UFF_FestivalViewModelSubsystem* UFF_LivePanelOperations::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LivePanelOperations::BindViewModel()
{
	UnbindViewModel();
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 지표가 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleSimSatisfactionChanged);
	CachedViewModel->OnAdmittedVisitorsTodayChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleAdmittedVisitorsChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleStageViewChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleTodayRevenueChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleGoldChanged);
	CachedViewModel->OnBuzzChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleBuzzChanged);
}

void UFF_LivePanelOperations::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleSimSatisfactionChanged);
		CachedViewModel->OnAdmittedVisitorsTodayChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleAdmittedVisitorsChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleStageViewChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleTodayRevenueChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleGoldChanged);
		CachedViewModel->OnBuzzChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleBuzzChanged);
	}
	CachedViewModel = nullptr;
	bHasBuzzValue = false;
}

void UFF_LivePanelOperations::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	// --- 지표 4종 ---
	// ★ 만족도는 `SimSatisfaction` 하나만 쓴다. Live 중에는 `ScoreSatisfaction` 과 값이 같아서
	//   둘 다 띄우면 다른 지표로 읽힌다 (U6 계획).
	SetTextOrHide(Text_Satisfaction, ViewModel ? FText::AsNumber(ViewModel->GetSimSatisfaction()) : MissingValueText);
	SetTextOrHide(Text_SatisfactionNote, ViewModel ? SatisfactionNoteText : GoalUnknownText);
	// 안전의 VM 기본값 88은 아직 실제 집계가 아니다. 점수처럼 표시하지 않는다.
	SetTextOrHide(Text_Safety, MissingValueText);
	SetTextOrHide(Text_TodayRevenue, ViewModel ? FText::AsNumber(ViewModel->GetTodayRevenue()) : MissingValueText);
	SetTextOrHide(Text_Gold, ViewModel ? FText::AsNumber(ViewModel->GetGold()) : MissingValueText);
	SetTextOrHide(Text_DummyNotice, FooterNoteText);

	const int32 Day = ViewModel ? ViewModel->GetCurrentDay() : INDEX_NONE;
	SetTextOrHide(Text_Day, Day > 0
		? FText::Format(DayTextFormat, FFormatNamedArguments{{TEXT("Day"), FText::AsNumber(Day)}}) : MissingValueText);

	// --- 일일 목표 ---
	// 현재 체류 인원이 아니라 오늘 실제 입장한 누적 인원이다.
	const int32 VisitorCount = ViewModel ? ViewModel->GetAdmittedVisitorsToday() : INDEX_NONE;
	const int32 DailyGoal = ViewModel ? ViewModel->GetDailyVisitorGoal() : INDEX_NONE;
	const int32 BonusGold = ViewModel ? ViewModel->GetDailyGoalBonusGold() : INDEX_NONE;
	GoalProgress = FFF_LiveOperationsRules::BuildGoalProgress(VisitorCount, DailyGoal);

	SetTextOrHide(Text_GoalProgress, GoalProgress.IsKnown()
		? FText::Format(
			GoalProgressTextFormat,
			FFormatNamedArguments{{TEXT("Current"), FText::AsNumber(VisitorCount)}, {TEXT("Goal"), FText::AsNumber(DailyGoal)}})
		: MissingValueText);
	SetTextOrHide(Text_GoalPercent, GoalProgress.IsKnown()
		? FText::Format(GoalPercentTextFormat, FFormatNamedArguments{{TEXT("Percent"), FText::AsNumber(FMath::FloorToInt(GoalProgress.Ratio * 100.f))}})
		: MissingValueText);

	// !! 막대만으로는 얼마나 남았는지 알 수 없다 !! 숫자와 문구를 함께 둔다 (AT-011).
	FText GoalDetail;
	switch (GoalProgress.State)
	{
	case EFF_LiveGoalState::Reached:
		GoalDetail = BonusGold >= 0
			? FText::Format(ViewModel && ViewModel->IsDailyGoalBonusPaid() ? GoalBonusPaidTextFormat : GoalReachedTextFormat,
				FFormatNamedArguments{{TEXT("Bonus"), FText::AsNumber(BonusGold)}})
			: GoalReachedNoBonusText;
		break;

	case EFF_LiveGoalState::InProgress:
		GoalDetail = BonusGold >= 0
			? FText::Format(
				GoalRemainingTextFormat,
				FFormatNamedArguments{{TEXT("Remaining"), FText::AsNumber(GoalProgress.Remaining)}, {TEXT("Bonus"), FText::AsNumber(BonusGold)}})
			: FText::Format(
				GoalRemainingNoBonusTextFormat,
				FFormatNamedArguments{{TEXT("Remaining"), FText::AsNumber(GoalProgress.Remaining)}});
		break;

	default:
		GoalDetail = GoalUnknownText;
		break;
	}
	SetTextOrHide(Text_GoalDetail, GoalDetail);

	// 🔴 모르면 막대째 숨긴다. `0%` 로 그리면 "아무도 안 왔다"는 틀린 사실이 된다.
	if (Bar_GoalProgress)
	{
		Bar_GoalProgress->SetPercent(GoalProgress.Ratio);
		Bar_GoalProgress->SetVisibility(GoalProgress.IsKnown()
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// --- 주의 항목 ---
	// 지금은 띄울 것이 없어 칸째 숨긴다.
	// 사건·대기열 원인은 소유자 API 가 오면 여기에 붙인다.
	SetGroupTextOrHide(Box_Warning, Text_Warning, FText::GetEmpty());
	RefreshBuzz();
}

void UFF_LivePanelOperations::RefreshBuzz()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	float Value = 0.f;
	float ChangeToday = 0.f;
	const bool bWasKnown = bHasBuzzValue;
	bHasBuzzValue = ViewModel && ViewModel->TryGetBuzzValues(Value, ChangeToday);
	if (!bHasBuzzValue)
	{
		SetTextOrHide(Text_BuzzValue, MissingValueText);
		SetTextOrHide(Text_BuzzChange, FText::GetEmpty());
		SetTextOrHide(Text_BuzzNote, BuzzUnavailableText);
		if (Bar_Buzz) Bar_Buzz->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	FNumberFormattingOptions NumberOptions;
	NumberOptions.SetMaximumFractionalDigits(1);
	SetTextOrHide(Text_BuzzValue, FText::Format(BuzzValueTextFormat,
		FFormatNamedArguments{{TEXT("Value"), FText::AsNumber(Value, &NumberOptions)}}));
	// 직전 이벤트와의 차이가 아니라 원본이 저장한 오늘 아침 기준을 사용한다.
	// 소수 첫째 자리로 보이는 변화만 부호를 붙여 +0 / −0 표시를 피한다.
	const float VisibleChange = static_cast<float>(FMath::RoundToInt(ChangeToday * 10.f)) / 10.f;
	SetTextOrHide(Text_BuzzChange, VisibleChange == 0.f ? BuzzUnchangedText
		: FText::Format(VisibleChange > 0.f ? BuzzIncreaseTextFormat : BuzzDecreaseTextFormat,
			FFormatNamedArguments{{TEXT("Change"), FText::AsNumber(FMath::Abs(VisibleChange), &NumberOptions)}}));
	SetTextOrHide(Text_BuzzNote, BuzzNoteText);

	// 원본 계약 0~100을 막대의 0~1로만 변환한다. 화제성이나 방문객을 새로 계산하지 않는다.
	TargetBuzzRatio = FMath::Clamp(Value / 100.f, 0.f, 1.f);
	if (!bWasKnown || BuzzInterpSpeed <= 0.f) DisplayedBuzzRatio = TargetBuzzRatio;
	if (Bar_Buzz)
	{
		Bar_Buzz->SetPercent(DisplayedBuzzRatio);
		Bar_Buzz->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UFF_LivePanelOperations::HandleBuzzChanged(float NewValue, float PreviousValue)
{
	RefreshBuzz();
}

void UFF_LivePanelOperations::HandleSimSatisfactionChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_LivePanelOperations::HandleAdmittedVisitorsChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_LivePanelOperations::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	RefreshAll();
}

void UFF_LivePanelOperations::HandleTodayRevenueChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_LivePanelOperations::HandleGoldChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_LivePanelOperations::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

void UFF_LivePanelOperations::SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value)
{
	SetTextOrHide(TextBlock, Value);

	if (!Group)
	{
		return;
	}

	Group->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}
