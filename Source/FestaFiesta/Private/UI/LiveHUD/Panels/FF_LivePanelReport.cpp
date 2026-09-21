// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelReport.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveReport"

void UFF_LivePanelReport::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 아직 못 하는 것을 적는다. 숫자만 두면 "왜 그런지" 를 물을 곳이 없다는 사실이
	// 화면에서 사라진다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveReport_MissingNote",
		"어느 시설이 만족도를 깎았는지는 아직 볼 수 없습니다. 원인 집계가 붙으면 여기에 들어갑니다."));

	if (Button_Advance)
	{
		Button_Advance->OnClicked.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleAdvanceClicked);
	}

	// 🔴 열 때마다 0 으로 되돌린다. 닫았다 다시 열면 **다시 읽는 시간**이다 —
	// 한 번 열어 두고 닫아 둔 것을 "읽었다" 로 치면 대기의 뜻이 사라진다.

	BindViewModel();
	RefreshAll();
}

void UFF_LivePanelReport::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

UFF_FestivalViewModelSubsystem* UFF_LivePanelReport::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LivePanelReport::BindViewModel()
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
	CachedViewModel->OnGoldChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnTodayRevenueChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnTotalRevenueChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnSimSatisfactionChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnScoreSatisfactionChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnSafetyChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
	CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &UFF_LivePanelReport::HandleStageViewChanged);
}

void UFF_LivePanelReport::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnGoldChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnTodayRevenueChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnTotalRevenueChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnSimSatisfactionChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnScoreSatisfactionChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnSafetyChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleIntMetricChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &UFF_LivePanelReport::HandleStageViewChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LivePanelReport::HandleAdvanceClicked()
{
	UFF_LiveHudScreen* Screen = GetOwningScreen();
	if (!Screen)
	{
		return;
	}

	// 여기서 미리 막지 않는다. 판정의 소유자는 화면과 Stage 시스템이다 (UI 규칙 2).
	Screen->RequestAdvance();
}

void UFF_LivePanelReport::RefreshAdvance()
{
	const UFF_LiveHudScreen* Screen = GetOwningScreen();
	if (!Screen)
	{
		return;
	}

	const EFF_LiveCtaState State = Screen->GetCtaState();

	// 넘길 수 있는 상태인지만 본다. 기다리게 하지 않는다.
	const bool bCanAdvance =
		(State != EFF_LiveCtaState::Hidden && State != EFF_LiveCtaState::BlockedNotReport);

	SetTextOrHide(Text_AdvanceLabel, Screen->GetCtaLabelText());

	if (Button_Advance)
	{
		Button_Advance->SetIsEnabled(bCanAdvance);

		// ★ 못 누르는 상태가 **눈에 보여야** 한다. 흐려짐 + 아래 이유 문구 둘 다 쓴다.
		Button_Advance->SetRenderOpacity(bCanAdvance ? 1.f : DimmedOpacity);

		Button_Advance->SetVisibility(State == EFF_LiveCtaState::Hidden
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}

	// !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011)
	SetTextOrHide(Text_AdvanceReason, Screen->GetCtaReasonText());
}

void UFF_LivePanelReport::HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	// 값도 함께 바뀐다 (마감 정산이 들어온다). 전부 다시 읽는다.
	RefreshAll();
}

void UFF_LivePanelReport::HandleIntMetricChanged(int32 NewValue)
{
	// 인자를 쓰지 않는다. 어느 값이 왔든 전부 다시 읽어야 서로 어긋나지 않는다.
	RefreshAll();
}

void UFF_LivePanelReport::RefreshAll()
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
			LOCTEXT("LiveReport_SatisfactionNote",
				"평가 점수는 관객 만족도에 피날레·연출 보정을 더한 값입니다 ({0} 차이)."),
			FText::AsNumber(Score - Sim))
		: FText::GetEmpty());

	// --- 운영 ---
	const int32 TotalStaff = ViewModel->GetTotalStaffCount();
	const int32 HiredStaff = ViewModel->GetHiredStaffCount();
	SetTextOrHide(Text_Staff, (TotalStaff >= 0 && HiredStaff >= 0)
		? FText::Format(
			LOCTEXT("LiveReport_Staff", "{0}명 (고용 {1})"),
			FText::AsNumber(TotalStaff), FText::AsNumber(HiredStaff))
		: FText::GetEmpty());

	SetTextOrHide(Text_Programs, FText::Format(
		LOCTEXT("LiveReport_Programs", "{0}개"),
		FText::AsNumber(ViewModel->GetScheduledProgramIds().Num())));

	RefreshAdvance();
}

#undef LOCTEXT_NAMESPACE
