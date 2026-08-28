// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelOperations.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveOperations"

// =============================================================================
// FFF_LiveOperationsRules — 순수 함수
// =============================================================================

FFF_LiveGoalProgress FFF_LiveOperationsRules::BuildGoalProgress(int32 VisitorCount, int32 DailyGoal)
{
	FFF_LiveGoalProgress Result;

	// 🔴 못 읽은 값을 0 으로 바꾸지 않는다. `0%` 는 "아무도 안 왔다"는 확정 사실이 되는데,
	// 관객 수 소유자 API 가 아직 없어서 지금은 그 오해가 기본 상태가 된다.
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

	// ★ 어떤 값이 아직 진짜가 아닌지 화면에 적는다. 안 적으면 더미 숫자를 실제로 믿게 된다.
	SetTextOrHide(Text_DummyNotice, LOCTEXT("LiveOps_DummyNotice",
		"관객 만족도·안전은 아직 임시 값이고, 관객 수는 소유자 API 가 없어 비어 있습니다."));

	BindViewModel();
	RefreshAll();
}

void UFF_LivePanelOperations::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
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
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 지표가 전부 비어 보인다."), *GetName());
		return;
	}

	CachedViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleSimSatisfactionChanged);
	CachedViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleSafetyChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleTodayRevenueChanged);
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LivePanelOperations::HandleGoldChanged);
}

void UFF_LivePanelOperations::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleSimSatisfactionChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleSafetyChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleTodayRevenueChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LivePanelOperations::HandleGoldChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LivePanelOperations::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// --- 지표 4종 ---
	// ★ 만족도는 `SimSatisfaction` 하나만 쓴다. Live 중에는 `ScoreSatisfaction` 과 값이 같아서
	//   둘 다 띄우면 다른 지표로 읽힌다 (U6 계획).
	SetTextOrHide(Text_Satisfaction, FText::AsNumber(ViewModel->GetSimSatisfaction()));
	SetTextOrHide(Text_Safety, FText::AsNumber(ViewModel->GetSafety()));
	SetTextOrHide(Text_TodayRevenue, FText::AsNumber(ViewModel->GetTodayRevenue()));
	SetTextOrHide(Text_Gold, FText::AsNumber(ViewModel->GetGold()));

	// --- 일일 목표 ---
	const int32 DailyGoal = ViewModel->GetDailyVisitorGoal();
	const int32 BonusGold = ViewModel->GetDailyGoalBonusGold();
	GoalProgress = FFF_LiveOperationsRules::BuildGoalProgress(VisitorCount, DailyGoal);

	SetTextOrHide(Text_GoalProgress, GoalProgress.IsKnown()
		? FText::Format(
			LOCTEXT("LiveOps_GoalProgress", "{0} / {1}명"),
			FText::AsNumber(VisitorCount), FText::AsNumber(DailyGoal))
		: LOCTEXT("LiveOps_GoalUnknown", "아직 알 수 없습니다"));

	// !! 막대만으로는 얼마나 남았는지 알 수 없다 !! 숫자와 문구를 함께 둔다 (AT-011).
	FText GoalDetail;
	switch (GoalProgress.State)
	{
	case EFF_LiveGoalState::Reached:
		GoalDetail = (BonusGold != INDEX_NONE)
			? FText::Format(LOCTEXT("LiveOps_GoalReached", "달성 · 보너스 +{0}"), FText::AsNumber(BonusGold))
			: LOCTEXT("LiveOps_GoalReachedNoBonus", "달성");
		break;

	case EFF_LiveGoalState::InProgress:
		GoalDetail = (BonusGold != INDEX_NONE)
			? FText::Format(
				LOCTEXT("LiveOps_GoalRemaining", "{0}명 더 오면 +{1}"),
				FText::AsNumber(GoalProgress.Remaining), FText::AsNumber(BonusGold))
			: FText::Format(
				LOCTEXT("LiveOps_GoalRemainingNoBonus", "{0}명 남음"),
				FText::AsNumber(GoalProgress.Remaining));
		break;

	default:
		// 모를 때는 이유를 적는다. 빈 줄로 두면 고장난 것으로 보인다.
		GoalDetail = LOCTEXT("LiveOps_GoalNoData", "관객 수를 아직 읽지 못했습니다.");
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
	// 지금 말할 수 있는 것은 미납뿐이다. 사건·대기열 원인은 소유자 API 가 오면 붙인다.
	const int32 Arrears = ViewModel->GetArrears();
	SetGroupTextOrHide(Box_Warning, Text_Warning, (Arrears > 0)
		? FText::Format(
			LOCTEXT("LiveOps_WarnArrears", "임대료 {0} 이 밀려 있습니다. 매출이 들어와도 잔액이 늘지 않습니다."),
			FText::AsNumber(Arrears))
		: FText::GetEmpty());
}

void UFF_LivePanelOperations::Debug_SetVisitorCount(int32 InVisitorCount)
{
	VisitorCount = InVisitorCount;
	RefreshAll();
}

void UFF_LivePanelOperations::HandleSimSatisfactionChanged(int32 NewValue)
{
	RefreshAll();
}

void UFF_LivePanelOperations::HandleSafetyChanged(int32 NewValue)
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

#undef LOCTEXT_NAMESPACE
