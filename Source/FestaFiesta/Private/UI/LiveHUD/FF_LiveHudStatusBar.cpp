// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LiveHudStatusBar.h"

#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

UFF_FestivalViewModelSubsystem* UFF_LiveHudStatusBar::GetViewModel() const
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

void UFF_LiveHudStatusBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindViewModel();

	if (Button_Pause)
	{
		Button_Pause->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandlePauseClicked);
	}
	if (Button_Play)
	{
		Button_Play->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandlePlayClicked);
	}
	if (Button_FastForward)
	{
		Button_FastForward->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandleFastForwardClicked);
	}
	if (Button_Skip)
	{
		Button_Skip->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandleSkipClicked);
	}
}

void UFF_LiveHudStatusBar::NativeConstruct()
{
	Super::NativeConstruct();

	// 위젯이 화면에 다시 붙을 때 구독이 끊겨 있을 수 있다.
	BindViewModel();

	// Bind + Sync의 Sync. 이벤트는 앞으로의 변화만 알려준다.
	RefreshAll();
}

void UFF_LiveHudStatusBar::NativeDestruct()
{
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_LiveHudStatusBar::BindViewModel()
{
	UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] ViewModel을 찾지 못했다. 상태 바가 갱신되지 않는다."), *GetName());
		return;
	}

	CachedViewModel = ViewModel;

	ViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandleGoldChanged);
	ViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandleStageViewChanged);
	ViewModel->OnRemainingSecondsChanged.AddUniqueDynamic(this, &UFF_LiveHudStatusBar::HandleRemainingSecondsChanged);
}

void UFF_LiveHudStatusBar::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LiveHudStatusBar::HandleGoldChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_LiveHudStatusBar::HandleStageViewChanged);
		CachedViewModel->OnRemainingSecondsChanged.RemoveDynamic(this, &UFF_LiveHudStatusBar::HandleRemainingSecondsChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LiveHudStatusBar::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	// 값이 없으면 칸을 접는다. `0`이나 `-`를 넣으면 틀린 사실을 화면에 쓰는 것이 된다.
	if (Value.IsEmpty())
	{
		TextBlock->SetText(FText::GetEmpty());
		TextBlock->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFF_LiveHudStatusBar::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	HandleGoldChanged(ViewModel->GetGold());
	HandleStageViewChanged(ViewModel->GetCurrentStage(), ViewModel->GetCurrentDay());
	HandleRemainingSecondsChanged(FMath::CeilToInt(ViewModel->GetRemainingSeconds()));

	RefreshClock();
	RefreshPlaybackControls();
	RefreshPlaceholders();
}

void UFF_LiveHudStatusBar::HandleGoldChanged(int32 NewValue)
{
	if (Text_Gold)
	{
		// 천 단위 구분은 FText::AsNumber가 로케일에 맞게 처리한다. 직접 문자열을 만들지 않는다.
		Text_Gold->SetText(FText::AsNumber(NewValue));
	}
}

void UFF_LiveHudStatusBar::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();

	if (Text_DayStage && ViewModel)
	{
		Text_DayStage->SetText(FText::Format(
			LOCTEXT("LiveHud_DayStage", "{0}일차 · {1}"),
			FText::AsNumber(NewDay),
			ViewModel->GetCurrentStageDisplayText()));
	}

	// Stage가 바뀌면 오늘 길이도 바뀐다. 진행률 분모를 다시 관측한다.
	ObservedMaxRemainingSeconds = 0;

	RefreshClock();
	RefreshPlaybackControls();
}

void UFF_LiveHudStatusBar::HandleRemainingSecondsChanged(int32 NewValue)
{
	SetTextOrHide(Text_RemainingTime, FormatSeconds(NewValue));

	// 총 길이를 주는 API가 없어 관측한 최대값을 분모로 쓴다.
	// 단계 진입 직후 첫 값이 총 길이이므로 이 방식으로 대개 맞는다.
	// 총 길이 Getter가 생기면 이 근사를 지운다.
	ObservedMaxRemainingSeconds = FMath::Max(ObservedMaxRemainingSeconds, NewValue);

	if (Progress_Program)
	{
		const float Percent = (ObservedMaxRemainingSeconds > 0)
			? 1.f - (static_cast<float>(NewValue) / static_cast<float>(ObservedMaxRemainingSeconds))
			: 0.f;
		Progress_Program->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	}

	RefreshClock();
}

void UFF_LiveHudStatusBar::RefreshClock()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// Live가 아니면 빈 값이 온다. 그때는 시계를 숨긴다 — Setup 중에 `00:00`을 띄우면 안 된다.
	SetTextOrHide(Text_Clock, ViewModel->GetLiveClockText());
}

void UFF_LiveHudStatusBar::RefreshPlaybackControls()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	const bool bLocked = ViewModel->IsTimeScaleLocked();

	// 잠금 구간에서는 배속 버튼만 막는다. 스킵은 별개 판정이다.
	if (Button_Play)
	{
		Button_Play->SetIsEnabled(!bLocked);
	}
	if (Button_FastForward)
	{
		Button_FastForward->SetIsEnabled(!bLocked);
	}
	if (Button_Pause)
	{
		Button_Pause->SetIsEnabled(!bLocked);
	}

	if (Button_Skip)
	{
		Button_Skip->SetIsEnabled(ViewModel->CanSkipCurrentStage());
	}

	// 비활성 이유를 문구로 함께 준다. 흐려짐만으로는 왜 못 누르는지 알 수 없다 (AT-011).
	SetTextOrHide(Text_TimeScaleHint, bLocked
		? LOCTEXT("LiveHud_TimeScaleLocked", "메인 공연 중에는 배속을 바꿀 수 없습니다.")
		: FText::GetEmpty());
}

void UFF_LiveHudStatusBar::RefreshPlaceholders()
{
	// 소유자 API가 없는 값 3개. 모르면 숨긴다.
	SetTextOrHide(Text_VisitorCount,
		(VisitorCount >= 0) ? FText::AsNumber(VisitorCount) : FText::GetEmpty());

	SetTextOrHide(Text_Staff,
		(StaffAvailable >= 0 && StaffTotal > 0)
			? FText::Format(LOCTEXT("LiveHud_Staff", "{0} / {1}"), FText::AsNumber(StaffAvailable), FText::AsNumber(StaffTotal))
			: FText::GetEmpty());

	SetTextOrHide(Text_NextProgram, NextProgramText);
}

void UFF_LiveHudStatusBar::Debug_SetPlaceholders(int32 InVisitorCount, int32 InStaffAvailable, int32 InStaffTotal, const FText& InNextProgram)
{
	VisitorCount = InVisitorCount;
	StaffAvailable = InStaffAvailable;
	StaffTotal = InStaffTotal;
	NextProgramText = InNextProgram;

	RefreshPlaceholders();
}

void UFF_LiveHudStatusBar::HandlePauseClicked()
{
	// 일시정지도 배속 요청이다. 0배속을 Stage 시스템이 어떻게 다룰지는 저쪽 판단이다.
	// UI가 시간을 직접 멈추지 않는다 (UI 규칙 1).
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSetTimeScale(0.f);
	}
}

void UFF_LiveHudStatusBar::HandlePlayClicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSetTimeScale(NormalTimeScale);
	}
}

void UFF_LiveHudStatusBar::HandleFastForwardClicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSetTimeScale(FastTimeScale);
	}
}

void UFF_LiveHudStatusBar::HandleSkipClicked()
{
	if (UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel())
	{
		ViewModel->RequestSkipCurrentStage();
	}
}

FText UFF_LiveHudStatusBar::FormatSeconds(int32 TotalSeconds)
{
	const int32 Clamped = FMath::Max(0, TotalSeconds);
	const int32 Minutes = Clamped / 60;
	const int32 Seconds = Clamped % 60;

	return FText::FromString(FString::Printf(TEXT("%d:%02d"), Minutes, Seconds));
}

#undef LOCTEXT_NAMESPACE
