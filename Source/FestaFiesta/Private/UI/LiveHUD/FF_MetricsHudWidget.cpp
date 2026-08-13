// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_MetricsHudWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UI/Widgets/FF_MetricRowWidget.h"
#include "UI/FF_UILog.h"

// Texture2D·UIMetricBandSettings는 ApplyBand 구현이 UFF_MetricRowWidget으로 옮겨가면서 필요 없어졌다.

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_FestivalViewModelSubsystem* UFF_MetricsHudWidget::GetViewModel() const
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

void UFF_MetricsHudWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindViewModel();
}

void UFF_MetricsHudWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 화면에 다시 붙을 때 구독이 끊겨 있을 수 있다.
	BindViewModel();

	// Bind + Sync의 Sync.
	// 이벤트는 앞으로의 변화만 알려준다. 이미 지나간 현재값은 여기서 직접 읽어 반영한다.
	RefreshAll();
}

void UFF_MetricsHudWidget::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_MetricsHudWidget::BindViewModel()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		// 조용히 넘어가지 않는다. 이게 없으면 화면에 아무 숫자도 안 뜨는데 원인을 알 수 없다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] UFF_FestivalViewModelSubsystem을 찾지 못했다. 지표가 갱신되지 않는다."), *GetName());
		return;
	}

	CachedViewModel = ViewModel;

	// AddUniqueDynamic만 쓴다. 중복 구독을 API가 막아준다.
	ViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleSimSatisfactionChanged);
	ViewModel->OnScoreSatisfactionChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleScoreSatisfactionChanged);
	ViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleSafetyChanged);
	ViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleGoldChanged);
	ViewModel->OnTotalRevenueChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleTotalRevenueChanged);
	ViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleTodayRevenueChanged);
	ViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleStageViewChanged);
	ViewModel->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_MetricsHudWidget::HandleRemainingSecondsChanged);
}

void UFF_MetricsHudWidget::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleSimSatisfactionChanged);
		CachedViewModel->OnScoreSatisfactionChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleScoreSatisfactionChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleSafetyChanged);
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleGoldChanged);
		CachedViewModel->OnTotalRevenueChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleTotalRevenueChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleTodayRevenueChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleStageViewChanged);
		CachedViewModel->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_MetricsHudWidget::HandleRemainingSecondsChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_MetricsHudWidget::RefreshAll()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// 표시 항목이 적으므로 부분 갱신하지 않고 전체를 다시 선언한다.
	// 이벤트가 어떤 순서로 들어와도 결과가 같아진다.
	HandleSimSatisfactionChanged(ViewModel->GetSimSatisfaction());
	HandleScoreSatisfactionChanged(ViewModel->GetScoreSatisfaction());
	HandleSafetyChanged(ViewModel->GetSafety());
	HandleGoldChanged(ViewModel->GetGold());
	HandleTotalRevenueChanged(ViewModel->GetTotalRevenue());
	HandleTodayRevenueChanged(ViewModel->GetTodayRevenue());
	HandleStageViewChanged(ViewModel->GetCurrentStage(), ViewModel->GetCurrentDay());
	HandleRemainingSecondsChanged(FMath::CeilToInt(ViewModel->GetRemainingSeconds()));
}

void UFF_MetricsHudWidget::ApplyBand(int32 Value, UTextBlock* BandLabel, UImage* BandIcon)
{
	// 2026-08-11: 구현을 UFF_MetricRowWidget으로 옮겼다. 구간 규칙이 두 곳에 있으면 갈라진다.
	// 이 함수는 기존 WBP_MetricsHud 호환을 위해 남겨 둔 위임이다.
	// 이 HUD를 MetricRow 3개로 재구성하면 함께 사라진다.
	UFF_MetricRowWidget::ApplyBand(Value, BandLabel, BandIcon);
}

void UFF_MetricsHudWidget::HandleSimSatisfactionChanged(int32 NewValue)
{
	if (Text_SimSatisfaction)
	{
		Text_SimSatisfaction->SetText(FText::AsNumber(NewValue));
	}

	ApplyBand(NewValue, Text_SimSatisfactionBand, Image_SimSatisfactionBand);
}

void UFF_MetricsHudWidget::HandleScoreSatisfactionChanged(int32 NewValue)
{
	if (Text_ScoreSatisfaction)
	{
		Text_ScoreSatisfaction->SetText(FText::AsNumber(NewValue));
	}

	ApplyBand(NewValue, Text_ScoreSatisfactionBand, Image_ScoreSatisfactionBand);
}

void UFF_MetricsHudWidget::HandleSafetyChanged(int32 NewValue)
{
	if (Text_Safety)
	{
		Text_Safety->SetText(FText::AsNumber(NewValue));
	}

	ApplyBand(NewValue, Text_SafetyBand, Image_SafetyBand);
}

void UFF_MetricsHudWidget::HandleGoldChanged(int32 NewValue)
{
	if (Text_Gold)
	{
		Text_Gold->SetText(FText::AsNumber(NewValue));
	}
}

void UFF_MetricsHudWidget::HandleTotalRevenueChanged(int32 NewValue)
{
	// 위젯 이름은 Text_Revenue지만 담기는 값은 축제 누적 매출이다. 잔액(Gold)이 아니다.
	if (Text_Revenue)
	{
		Text_Revenue->SetText(FText::AsNumber(NewValue));
	}
}

void UFF_MetricsHudWidget::HandleTodayRevenueChanged(int32 NewValue)
{
	if (Text_TodayRevenue)
	{
		Text_TodayRevenue->SetText(FText::AsNumber(NewValue));
	}
}

void UFF_MetricsHudWidget::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	if (Text_Stage)
	{
		const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
		Text_Stage->SetText(ViewModel ? ViewModel->GetCurrentStageDisplayText() : FText::GetEmpty());
	}

	if (Text_Day)
	{
		Text_Day->SetText(FText::Format(LOCTEXT("DayFormat", "Day {0}"), FText::AsNumber(NewDay)));
	}
}

void UFF_MetricsHudWidget::HandleRemainingSecondsChanged(int32 NewValue)
{
	if (Text_RemainingTime)
	{
		Text_RemainingTime->SetText(FormatSeconds(NewValue));
	}
}

FText UFF_MetricsHudWidget::FormatSeconds(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 Minutes = Clamped / 60;
	const int32 Seconds = Clamped % 60;

	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Seconds));
}

#undef LOCTEXT_NAMESPACE
