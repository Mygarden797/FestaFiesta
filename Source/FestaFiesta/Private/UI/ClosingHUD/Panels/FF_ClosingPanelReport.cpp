// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ClosingHUD/Panels/FF_ClosingPanelReport.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/ClosingHUD/FF_ClosingHudScreen.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaClosingReport"

void UFF_ClosingPanelReport::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 아직 못 하는 것을 적는다. 숫자만 두면 "왜 그런지" 를 물을 곳이 없다는 사실이
	// 화면에서 사라진다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("ClosingReport_MissingNote",
		"어느 시설이 만족도를 깎았는지는 아직 볼 수 없습니다. 원인 집계가 붙으면 여기에 들어갑니다."));

	if (Button_Advance)
	{
		Button_Advance->OnClicked.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleAdvanceClicked);
	}

	// 🔴 열 때마다 0 으로 되돌린다. 닫았다 다시 열면 **다시 읽는 시간**이다 —
	// 한 번 열어 두고 닫아 둔 것을 "읽었다" 로 치면 대기의 뜻이 사라진다.
	TimeSinceOpened = 0.f;

	BindViewModel();
	RefreshAll();
}

void UFF_ClosingPanelReport::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

UFF_FestivalViewModelSubsystem* UFF_ClosingPanelReport::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_ClosingPanelReport::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 성적표가 비어 보인다."), *GetName());
		return;
	}

	// 결산 중에도 값이 움직인다 (마감 정산이 들어온다). 하나만 받고 나머지를 그때 읽으면
	// 갱신 시점이 어긋나므로, 바뀌는 것들을 다 받고 **매번 전부 다시 읽는다.**
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnTotalRevenueChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnArrearsChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnScoreSatisfactionChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	CachedViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
}

void UFF_ClosingPanelReport::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnTotalRevenueChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnArrearsChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnScoreSatisfactionChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_ClosingPanelReport::HandleIntMetricChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_ClosingPanelReport::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 접혀 있으면 세지 않는다. 패널은 캐시되어 살아 있다.
	if (GetVisibility() == ESlateVisibility::Collapsed)
	{
		return;
	}

	// 이미 다 셌으면 매 틱 다시 그리지 않는다.
	if (TimeSinceOpened >= ReadDelaySeconds)
	{
		return;
	}

	TimeSinceOpened += InDeltaTime;
	RefreshAdvance();
}

void UFF_ClosingPanelReport::HandleAdvanceClicked()
{
	UFF_ClosingHudScreen* Screen = OwningScreen.Get();
	if (!Screen)
	{
		return;
	}

	// 여기서 미리 막지 않는다. 판정의 소유자는 화면과 Stage 시스템이다 (UI 규칙 2).
	Screen->RequestAdvance();
}

void UFF_ClosingPanelReport::RefreshAdvance()
{
	const UFF_ClosingHudScreen* Screen = OwningScreen.Get();
	if (!Screen)
	{
		return;
	}

	const EFF_ClosingCtaState State = Screen->GetCtaState();
	const float Remaining = FMath::Max(0.f, ReadDelaySeconds - TimeSinceOpened);
	const bool bStillReading = Remaining > 0.f;

	// 단계가 아직 아니거나 마지막 날이면 대기와 무관하게 못 누른다.
	const bool bCanAdvance = (State == EFF_ClosingCtaState::Ready) && !bStillReading;

	SetTextOrHide(Text_AdvanceLabel, Screen->GetCtaLabelText());

	if (Button_Advance)
	{
		Button_Advance->SetIsEnabled(bCanAdvance);

		// ★ 흐렸다가 또렷해진다. 못 누르는 상태가 **눈에 보여야** 기다리는 중인 줄 안다.
		Button_Advance->SetRenderOpacity(bCanAdvance ? 1.f : DimmedOpacity);

		Button_Advance->SetVisibility(State == EFF_ClosingCtaState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}

	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	//
	// 대기 중에는 **남은 초를 센다.** `5 · 4 · 3 …` 이 줄어드는 것이 보여야
	// 고장이 아니라 기다리는 중이라는 것을 안다.
	const FText Reason = bStillReading
		? FText::Format(
			LOCTEXT("ClosingReport_ReadDelay", "{0}초 뒤에 넘어갈 수 있습니다"),
			FText::AsNumber(FMath::CeilToInt(Remaining)))
		: Screen->GetCtaReasonText();

	SetTextOrHide(Text_AdvanceReason, Reason);
}

void UFF_ClosingPanelReport::HandleIntMetricChanged(int32 NewValue)
{
	// 인자를 쓰지 않는다. 어느 값이 왔든 전부 다시 읽어야 서로 어긋나지 않는다.
	RefreshAll();
}

void UFF_ClosingPanelReport::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// --- 돈 ---
	SetTextOrHide(Text_TodayRevenue, FText::AsNumber(ViewModel->GetTodayRevenue()));
	SetTextOrHide(Text_TotalRevenue, FText::AsNumber(ViewModel->GetTotalRevenue()));
	SetTextOrHide(Text_Gold, FText::AsNumber(ViewModel->GetGold()));

	// 🔴 미납은 0 이면 숨긴다. `미납 0` 은 없는 문제를 있는 것처럼 만든다.
	const int32 Arrears = ViewModel->GetArrears();
	SetTextOrHide(Text_Arrears, (Arrears > 0)
		? FText::Format(LOCTEXT("ClosingReport_Arrears", "미납 {0}"), FText::AsNumber(Arrears))
		: FText::GetEmpty());

	// --- 평가 ---
	const int32 Sim = ViewModel->GetSimSatisfaction();
	const int32 Score = ViewModel->GetScoreSatisfaction();

	SetTextOrHide(Text_SimSatisfaction, FText::AsNumber(Sim));
	SetTextOrHide(Text_ScoreSatisfaction, FText::AsNumber(Score));
	SetTextOrHide(Text_Safety, FText::AsNumber(ViewModel->GetSafety()));

	// ★ 두 만족도가 같으면 설명이 필요 없다. 다를 때만 왜 다른지 적는다 —
	//   항상 띄우면 사람이 안 읽고, 다를 때 놓친다.
	SetTextOrHide(Text_SatisfactionNote, (Sim != Score)
		? FText::Format(
			LOCTEXT("ClosingReport_SatisfactionNote",
				"평가 점수는 관객 만족도에 피날레·연출 보정을 더한 값입니다 ({0} 차이)."),
			FText::AsNumber(Score - Sim))
		: FText::GetEmpty());

	// --- 운영 ---
	const int32 TotalStaff = ViewModel->GetTotalStaffCount();
	const int32 TempStaff = ViewModel->GetTemporaryStaffCount();
	SetTextOrHide(Text_Staff, (TotalStaff >= 0 && TempStaff >= 0)
		? FText::Format(
			LOCTEXT("ClosingReport_Staff", "{0}명 (알바 {1})"),
			FText::AsNumber(TotalStaff), FText::AsNumber(TempStaff))
		: FText::GetEmpty());

	SetTextOrHide(Text_Programs, FText::Format(
		LOCTEXT("ClosingReport_Programs", "{0}개"),
		FText::AsNumber(ViewModel->GetScheduledProgramIds().Num())));

	RefreshAdvance();
}

#undef LOCTEXT_NAMESPACE
