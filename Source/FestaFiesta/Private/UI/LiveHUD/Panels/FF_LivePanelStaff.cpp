// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"

#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/WidgetSwitcher.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
#include "UI/Staff/FF_StaffTrainingWidget.h"
#include "UI/Staff/FF_StaffManagementWidget.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveStaff"

// =============================================================================
// FFF_LiveStaffRules — 순수 함수
// =============================================================================

FFF_LiveStaffSummary FFF_LiveStaffRules::BuildSummary(
	int32 TotalStaff, int32 BaseStaff, int32 HiredStaff, int32 AssignedStaff)
{
	FFF_LiveStaffSummary Result;

	// 내역은 알아도 전체를 모를 수 있고 그 반대도 된다. 각자 그대로 넘긴다 —
	// 하나가 없다고 나머지까지 숨기면 볼 수 있는 것까지 못 보게 된다.
	Result.Total = (TotalStaff >= 0) ? TotalStaff : INDEX_NONE;
	Result.Base = (BaseStaff >= 0) ? BaseStaff : INDEX_NONE;
	Result.Hired = (HiredStaff >= 0) ? HiredStaff : INDEX_NONE;
	Result.Assigned = (AssignedStaff >= 0) ? AssignedStaff : INDEX_NONE;

	// 🔴 남는 인원은 **둘 다 알아야** 나온다. 하나라도 모르면 `0명` 이 아니라 `모름` 이다.
	if (Result.Total == INDEX_NONE || Result.Assigned == INDEX_NONE)
	{
		Result.IdleState = EFF_LiveStaffIdleState::Unknown;
		return Result;
	}

	// 배치가 전체보다 많은 것은 데이터가 어긋난 것이다. 0 으로 깎으면 `전원 배치` 처럼
	// 보여서 버그가 화면에서 사라진다. 상태를 따로 두고 그대로 알린다.
	if (Result.Assigned > Result.Total)
	{
		Result.IdleState = EFF_LiveStaffIdleState::Inconsistent;
		return Result;
	}

	Result.Idle = Result.Total - Result.Assigned;
	Result.IdleState = (Result.Idle > 0)
		? EFF_LiveStaffIdleState::HasIdle
		: EFF_LiveStaffIdleState::AllBusy;

	return Result;
}

// =============================================================================
// UFF_LivePanelStaff
// =============================================================================

void UFF_LivePanelStaff::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_StaffOverview) Button_StaffOverview->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStaffOverviewClicked);
	if (Button_StaffTraining) Button_StaffTraining->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStaffTrainingClicked);
	SelectStaffTab(false);
}

void UFF_LivePanelStaff::SelectStaffTab(bool bTraining)
{
	if (Widget_StaffManagement)
	{
		if (Widget_StaffTraining) Widget_StaffTraining->DeactivateRoster();
		if (bStaffPanelOpen)
		{
			Widget_StaffManagement->OnRevealWorldRequested.BindUObject(this, &ThisClass::RequestClose);
			Widget_StaffManagement->OnIncidentRequested.BindUObject(this, &ThisClass::HandleManagementIncident);
			Widget_StaffManagement->ActivateManagement(true);
		}
		return;
	}
	bTrainingSelected = bTraining && Widget_StaffTraining && Switcher_StaffContent;
	if (Switcher_StaffContent) Switcher_StaffContent->SetActiveWidgetIndex(bTrainingSelected ? 1 : 0);
	if (Indicator_StaffOverview) Indicator_StaffOverview->SetVisibility(bTrainingSelected ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	if (Indicator_StaffTraining) Indicator_StaffTraining->SetVisibility(bTrainingSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	if (Widget_StaffTraining)
	{
		if (bStaffPanelOpen && bTrainingSelected) Widget_StaffTraining->ActivateRoster();
		else Widget_StaffTraining->DeactivateRoster();
	}
}

void UFF_LivePanelStaff::HandleStaffOverviewClicked() { SelectStaffTab(false); }
void UFF_LivePanelStaff::HandleStaffTrainingClicked() { SelectStaffTab(true); }

void UFF_LivePanelStaff::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	bStaffPanelOpen = true;

	BindViewModel();
	RefreshAll();
	SelectStaffTab(bTrainingSelected);
}

void UFF_LivePanelStaff::NativeOnPanelClosed()
{
	bStaffPanelOpen = false;
	if (Widget_StaffManagement) Widget_StaffManagement->DeactivateManagement();
	if (Widget_StaffTraining) Widget_StaffTraining->DeactivateRoster();
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelStaff::NativeDestruct()
{
	bStaffPanelOpen = false;
	if (Widget_StaffManagement) Widget_StaffManagement->DeactivateManagement();
	if (Widget_StaffTraining) Widget_StaffTraining->DeactivateRoster();
	UnbindViewModel();
	Super::NativeDestruct();
}

void UFF_LivePanelStaff::HandleManagementIncident(FFF_IncidentHandle Handle)
{
	UFF_LiveHudScreen* Screen = GetOwningScreen();
	if (!Screen) return;
	RequestClose();
	Screen->RequestOpenIncident(Handle);
}

UFF_FestivalViewModelSubsystem* UFF_LivePanelStaff::GetViewModel() const
{
	if (IsValid(CachedViewModel))
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LivePanelStaff::BindViewModel()
{
	UnbindViewModel();
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 인력 숫자가 전부 비어 보인다."), *GetName());
		return;
	}
	
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
	CachedViewModel->OnStaffCapacityChanged.AddUniqueDynamic(this, &ThisClass::HandleStaffCountChanged);
	CachedViewModel->OnStaffAssignmentChanged.AddUniqueDynamic(this, &ThisClass::HandleStaffAssignmentChanged);
}

void UFF_LivePanelStaff::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (IsValid(CachedViewModel))
	{
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
		CachedViewModel->OnStaffCapacityChanged.RemoveDynamic(this, &ThisClass::HandleStaffCountChanged);
		CachedViewModel->OnStaffAssignmentChanged.RemoveDynamic(this, &ThisClass::HandleStaffAssignmentChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LivePanelStaff::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	// 이전 축제의 값이 남지 않도록 ViewModel이 없을 때도 모든 표시를 갱신한다.
	Summary = FFF_LiveStaffRules::BuildSummary(
		ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE,
		ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE,
		ViewModel ? ViewModel->GetHiredStaffCount() : INDEX_NONE,
		ViewModel ? ViewModel->GetBusyStaffCount() : INDEX_NONE);

	const int32 MaxTotal = ViewModel ? ViewModel->GetMaxTotalStaffCount() : INDEX_NONE;
	const int32 Unassigned = ViewModel ? ViewModel->GetUnassignedStaffCount() : INDEX_NONE;
	SetTextOrHide(Text_HireNote, HireNoteText);
	SetTextOrHide(Text_MissingNote, Unassigned >= 0
		? FText::Format(UnassignedGuideTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Unassigned)}})
		: AssignmentGuideText);

	// --- 요약 ---
	const FText CapacityValue = FText::Format(CapacityTextFormat, FFormatNamedArguments{
		{TEXT("Total"), Summary.Total >= 0 ? FText::AsNumber(Summary.Total) : MissingValueText},
		{TEXT("Capacity"), MaxTotal >= 0 ? FText::AsNumber(MaxTotal) : MissingValueText}});
	SetTextOrHide(Text_TotalStaff, CapacityValue);
	SetTextOrHide(Text_Capacity, CapacityValue);

	// 구성 칸은 각각 알려진 원본 값을 표시한다. 막대 합계의 일관성은 별도로 확인한다.
	SetTextOrHide(Text_BaseStaff, FormatCount(Summary.Base));
	SetTextOrHide(Text_HiredStaff, FormatCount(Summary.Hired));
	SetTextOrHide(Text_UnassignedStaff, FormatCount(Unassigned));
	SetTextOrHide(Text_StaffBreakdown,
		(Summary.Base >= 0 && Summary.Hired >= 0)
			? FText::Format(BreakdownTextFormat, FFormatNamedArguments{
				{TEXT("Base"), FText::AsNumber(Summary.Base)}, {TEXT("Hired"), FText::AsNumber(Summary.Hired)}})
			: FText::GetEmpty());

	SetTextOrHide(Text_AssignedStaff, FormatCount(Summary.Assigned));
	RefreshCapacity(MaxTotal);

	// --- 남는 인원 ---
	// !! 숫자만 두지 않는다 !! 남는 것이 좋은 일인지 나쁜 일인지 숫자로는 알 수 없다 (AT-011).
	FText IdleValue = FormatCount(Summary.Idle);
	FText IdleNote;
	switch (Summary.IdleState)
	{
	case EFF_LiveStaffIdleState::HasIdle:
		IdleNote = IdleNoteText;
		break;

	case EFF_LiveStaffIdleState::AllBusy:
		IdleNote = Summary.Total == 0 ? NoStaffNoteText : AllBusyNoteText;
		break;

	case EFF_LiveStaffIdleState::Inconsistent:
		IdleNote = FText::Format(InconsistentTextFormat, FFormatNamedArguments{
			{TEXT("Busy"), FText::AsNumber(Summary.Assigned)}, {TEXT("Total"), FText::AsNumber(Summary.Total)}});
		break;

	default:
		IdleNote = CheckingText;
		break;
	}
	SetTextOrHide(Text_IdleStaff, IdleValue);
	SetTextOrHide(Text_IdleNote, IdleNote);

	// --- 고용 단가 ---
	// 1회 구매라 "/ 일" 이 아니다. Live 중에는 못 뽑으므로 참고용 표시다.
	const int32 HireCost = ViewModel ? ViewModel->GetStaffHireCostGold() : INDEX_NONE;
	SetTextOrHide(Text_DailyWage, FText::Format(HireCostTextFormat, FFormatNamedArguments{
		{TEXT("Cost"), HireCost >= 0 ? FText::AsNumber(HireCost) : MissingValueText}}));

	RefreshAssignments();
}

void UFF_LivePanelStaff::RefreshAssignments()
{
	// 개별 사건 Read Model이 없으므로 인원수로 사건 건수나 해결 상태를 추정하지 않는다.
	if (Box_Assignments)
	{
		if (Box_Assignments->GetChildrenCount() > 0)
		{
			Box_Assignments->ClearChildren();
		}
		Box_Assignments->SetVisibility(ESlateVisibility::Collapsed);
	}

	const int32 BusyCount = Summary.Assigned;
	SetTextOrHide(Text_AssignmentsEmpty, BusyCount > 0
		? FText::Format(ResponseActiveTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(BusyCount)}})
		: BusyCount == 0 ? ResponseIdleText : ResponseUnknownText);

	if (Image_ResponseStatus)
	{
		UTexture2D* Icon = BusyCount > 0 ? ResponseActiveIcon.Get()
			: BusyCount == 0 ? ResponseIdleIcon.Get() : ResponseUnknownIcon.Get();
		Image_ResponseStatus->SetBrushFromTexture(Icon, false);
		Image_ResponseStatus->SetVisibility(Icon ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_LivePanelStaff::RefreshCapacity(const int32 Capacity)
{
	const bool bTotalKnown = Summary.Total >= 0 && Capacity >= 0;
	const bool bValidTotal = bTotalKnown && Summary.Total <= Capacity;
	const bool bBreakdownKnown = Summary.Base >= 0 && Summary.Hired >= 0;
	const bool bValidBreakdown = bValidTotal && bBreakdownKnown
		&& static_cast<int64>(Summary.Base) + Summary.Hired == Summary.Total;

	if (Progress_StaffCapacity)
	{
		Progress_StaffCapacity->SetPercent(bValidTotal && Capacity > 0 ? static_cast<float>(Summary.Total) / Capacity : 0.f);
		Progress_StaffCapacity->SetVisibility(bValidTotal ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Progress_BaseCapacity)
	{
		Progress_BaseCapacity->SetPercent(bValidBreakdown && Capacity > 0 ? static_cast<float>(Summary.Base) / Capacity : 0.f);
		Progress_BaseCapacity->SetVisibility(bValidBreakdown ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	SetTextOrHide(Text_CapacityNote, !bTotalKnown ? CapacityUnknownText
		: !bValidTotal ? CapacityInconsistentText
		: !bBreakdownKnown ? BreakdownUnknownText
		: !bValidBreakdown ? BreakdownInconsistentText : CapacityNoteText);
}

FText UFF_LivePanelStaff::FormatCount(const int32 Count) const
{
	return Count >= 0 ? FText::Format(CountTextFormat, FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Count)}})
		: MissingValueText;
}

void UFF_LivePanelStaff::HandleStaffCountChanged(int32 NewValue)
{
	// 인자를 쓰지 않는다. 총원 하나만 받아서는 남는 인원 계산의 다른 한쪽(배치)이
	// 안 맞을 수 있으니, 부를 때마다 전체를 다시 읽는다.
	RefreshAll();
}

void UFF_LivePanelStaff::HandleStaffAssignmentChanged()
{
	RefreshAll();
}

void UFF_LivePanelStaff::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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

#undef LOCTEXT_NAMESPACE
