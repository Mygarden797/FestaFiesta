// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelStaff.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Engine/GameInstance.h"
#include "UI/FF_UILog.h"
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

void UFF_LivePanelStaff::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 못 하는 것과 없는 것을 화면에 적는다. 안 적으면 고장난 것으로 읽힌다.
	SetTextOrHide(Text_HireNote, LOCTEXT("LiveStaff_HireNote",
		"고용은 셋업과 개장 준비에서만 합니다. 라이브 중에는 늘릴 수 없습니다."));

	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveStaff_MissingNote",
		"시설별 배치와 스태프 개인 상태는 아직 읽을 수 없습니다."));

	BindViewModel();
	RefreshAll();
}

void UFF_LivePanelStaff::NativeOnPanelClosed()
{
	UnbindViewModel();
	Super::NativeOnPanelClosed();
}

UFF_FestivalViewModelSubsystem* UFF_LivePanelStaff::GetViewModel() const
{
	if (CachedViewModel)
	{
		return CachedViewModel;
	}

	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
}

void UFF_LivePanelStaff::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	CachedViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;

	if (!CachedViewModel)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("[%s] ViewModel 을 찾지 못했다. 인력 숫자가 전부 비어 보인다."), *GetName());
		return;
	}
	
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
}

void UFF_LivePanelStaff::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
	}
	CachedViewModel = nullptr;
}

void UFF_LivePanelStaff::RefreshAll()
{
	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	Summary = FFF_LiveStaffRules::BuildSummary(
		ViewModel->GetTotalStaffCount(),
		ViewModel->GetBaseStaffCount(),
		ViewModel->GetHiredStaffCount(),
		// 사건 시스템이 오기 전까지 배치는 늘 0 이다. 읽을 창구가 생기면 여기를 바꾼다.
		/* AssignedToEvents */ 0);

	const FText Unknown = LOCTEXT("LiveStaff_Unknown", "—");

	// --- 요약 ---
	SetTextOrHide(Text_TotalStaff, (Summary.Total != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveStaff_TotalValue", "{0}명"), FText::AsNumber(Summary.Total))
		: Unknown);

	// 내역은 둘 다 알아야 말이 된다. `기본 6 · 고용 —` 은 읽는 사람을 헷갈리게 한다.
	SetTextOrHide(Text_StaffBreakdown,
		(Summary.Base != INDEX_NONE && Summary.Hired != INDEX_NONE)
			? FText::Format(
				LOCTEXT("LiveStaff_Breakdown", "기본 {0} · 고용 {1}"),
				FText::AsNumber(Summary.Base), FText::AsNumber(Summary.Hired))
			: FText::GetEmpty());

	SetTextOrHide(Text_AssignedStaff, (Summary.Assigned != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveStaff_AssignedValue", "{0}명"), FText::AsNumber(Summary.Assigned))
		: Unknown);

	// --- 남는 인원 ---
	// !! 숫자만 두지 않는다 !! 남는 것이 좋은 일인지 나쁜 일인지 숫자로는 알 수 없다 (AT-011).
	FText IdleValue = Unknown;
	FText IdleNote;
	switch (Summary.IdleState)
	{
	case EFF_LiveStaffIdleState::HasIdle:
		IdleValue = FText::Format(LOCTEXT("LiveStaff_IdleValue", "{0}명"), FText::AsNumber(Summary.Idle));
		IdleNote = LOCTEXT("LiveStaff_IdleNote",
			"사건이 나면 이 인원이 대응에 들어갑니다.");
		break;

	case EFF_LiveStaffIdleState::AllBusy:
		IdleValue = LOCTEXT("LiveStaff_IdleNone", "없음");
		IdleNote = LOCTEXT("LiveStaff_AllBusyNote", "전원이 사건 대응 중입니다.");
		break;

	case EFF_LiveStaffIdleState::Inconsistent:
		// 조용히 넘기지 않는다. 화면에 적어야 누가 알아챈다.
		IdleValue = LOCTEXT("LiveStaff_IdleBroken", "?");
		IdleNote = FText::Format(
			LOCTEXT("LiveStaff_InconsistentNote",
				"배치 인원({0})이 전체 인원({1})보다 많습니다. 값이 어긋났습니다."),
			FText::AsNumber(Summary.Assigned), FText::AsNumber(Summary.Total));

		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 배치 인원(%d)이 전체 인원(%d)보다 많다. 인력 소유자 쪽을 확인한다."),
			*GetName(), Summary.Assigned, Summary.Total);
		break;

	default:
		IdleNote = LOCTEXT("LiveStaff_IdleUnknownNote", "인원을 아직 읽지 못했습니다.");
		break;
	}
	SetTextOrHide(Text_IdleStaff, IdleValue);
	SetTextOrHide(Text_IdleNote, IdleNote);

	// --- 고용 단가 ---
	// 1회 구매라 "/ 일" 이 아니다. Live 중에는 못 뽑으므로 참고용 표시다.
	const int32 HireCost = ViewModel->GetStaffHireCostGold();
	SetTextOrHide(Text_DailyWage, (HireCost != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveStaff_HireCost", "1명 고용비 {0}"), FText::AsNumber(HireCost))
		: FText::GetEmpty());

	RefreshAssignments();
}

void UFF_LivePanelStaff::RefreshAssignments()
{
	if (!Box_Assignments)
	{
		return;
	}

	Box_Assignments->ClearChildren();

	const UFF_FestivalViewModelSubsystem* ViewModel = GetViewModel();
	if (!ViewModel)
	{
		return;
	}

	// 여기는 **사건 대응 중인 인원** 이 들어갈 자리다. 유료 프로그램은 인원을 쓰지 않으므로
	// 편성 목록을 여기에 그리지 않는다 — 그리면 "이 사람들이 묶여 있다" 는 거짓말이 된다.
	// 사건 시스템(UFF_FestivalEventSubsystem)이 오면 그 목록을 여기에 붙인다.
	SetTextOrHide(Text_AssignmentsEmpty,
		LOCTEXT("LiveStaff_NoAssignment", "지금 대응 중인 사건이 없습니다."));

	Box_Assignments->SetVisibility(ESlateVisibility::Collapsed);
}

void UFF_LivePanelStaff::HandleStaffCountChanged(int32 NewValue)
{
	// 인자를 쓰지 않는다. 총원 하나만 받아서는 남는 인원 계산의 다른 한쪽(배치)이
	// 안 맞을 수 있으니, 부를 때마다 전체를 다시 읽는다.
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
