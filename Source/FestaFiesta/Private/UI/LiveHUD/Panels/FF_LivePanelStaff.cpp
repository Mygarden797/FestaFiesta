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
	int32 TotalStaff, int32 BaseStaff, int32 TemporaryStaff, int32 AssignedStaff)
{
	FFF_LiveStaffSummary Result;

	// 내역은 알아도 전체를 모를 수 있고 그 반대도 된다. 각자 그대로 넘긴다 —
	// 하나가 없다고 나머지까지 숨기면 볼 수 있는 것까지 못 보게 된다.
	Result.Total = (TotalStaff >= 0) ? TotalStaff : INDEX_NONE;
	Result.Base = (BaseStaff >= 0) ? BaseStaff : INDEX_NONE;
	Result.Temporary = (TemporaryStaff >= 0) ? TemporaryStaff : INDEX_NONE;
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
		"알바는 하루짜리라 셋업과 개장 준비에서만 뽑습니다. 라이브 중에는 늘릴 수 없습니다."));

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

	// 🔴 셋 다 받는다. 알바를 뽑으면 전체가 바뀌고, 프로그램을 편성하면 배치가 바뀐다.
	// 하나만 받으면 남는 인원 계산의 한쪽만 갱신되어 두 숫자가 서로 안 맞는 순간이 생긴다.
	CachedViewModel->OnTotalStaffCountChanged.AddUniqueDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
	CachedViewModel->OnAvailableStaffCountChanged.AddUniqueDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
	CachedViewModel->OnScheduledProgramsChanged.AddUniqueDynamic(this, &UFF_LivePanelStaff::HandleScheduledProgramsChanged);
}

void UFF_LivePanelStaff::UnbindViewModel()
{
	// 해제는 조건부, 정리는 무조건.
	if (CachedViewModel)
	{
		CachedViewModel->OnTotalStaffCountChanged.RemoveDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
		CachedViewModel->OnAvailableStaffCountChanged.RemoveDynamic(this, &UFF_LivePanelStaff::HandleStaffCountChanged);
		CachedViewModel->OnScheduledProgramsChanged.RemoveDynamic(this, &UFF_LivePanelStaff::HandleScheduledProgramsChanged);
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
		ViewModel->GetTemporaryStaffCount(),
		ViewModel->GetStaffAssignedToPrograms());

	const FText Unknown = LOCTEXT("LiveStaff_Unknown", "—");

	// --- 요약 ---
	SetTextOrHide(Text_TotalStaff, (Summary.Total != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveStaff_TotalValue", "{0}명"), FText::AsNumber(Summary.Total))
		: Unknown);

	// 내역은 둘 다 알아야 말이 된다. `기본 6 · 알바 —` 은 읽는 사람을 헷갈리게 한다.
	SetTextOrHide(Text_StaffBreakdown,
		(Summary.Base != INDEX_NONE && Summary.Temporary != INDEX_NONE)
			? FText::Format(
				LOCTEXT("LiveStaff_Breakdown", "기본 {0} · 알바 {1}"),
				FText::AsNumber(Summary.Base), FText::AsNumber(Summary.Temporary))
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
			"임금은 그대로 나가는데 아무 프로그램에도 묶여 있지 않습니다.");
		break;

	case EFF_LiveStaffIdleState::AllBusy:
		IdleValue = LOCTEXT("LiveStaff_IdleNone", "없음");
		IdleNote = LOCTEXT("LiveStaff_AllBusyNote", "전원이 프로그램에 배치되어 있습니다.");
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

	// --- 오늘 인건비 ---
	const int32 Wage = ViewModel->GetDailyStaffWageGold();
	SetTextOrHide(Text_DailyWage, (Wage != INDEX_NONE)
		? FText::Format(LOCTEXT("LiveStaff_Wage", "오늘 알바 임금 {0} / 일"), FText::AsNumber(Wage))
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

	// 오늘 편성한 id 를 카탈로그와 맞춰 이름·필요 인원을 얻는다.
	// 편성 목록 자체에는 카드 정보가 없어서 이 조합이 필요하다.
	const TArray<FName> ScheduledIds = ViewModel->GetScheduledProgramIds();
	const TArray<FFF_ProgramCard> Catalog = ViewModel->GetProgramCatalog();

	int32 RowCount = 0;
	for (const FName& Id : ScheduledIds)
	{
		const FFF_ProgramCard* Card = Catalog.FindByPredicate(
			[&Id](const FFF_ProgramCard& Entry) { return Entry.ProgramId == Id; });

		if (!Card)
		{
			// 편성은 됐는데 카탈로그에 없다. 줄을 빼먹지 않고 id 로라도 남긴다 —
			// 조용히 지우면 인원 합이 안 맞는 이유를 찾을 수 없다.
			UE_LOG(LogFestaFiestaUI, Warning,
				TEXT("[%s] 편성된 `%s` 를 카탈로그에서 못 찾았다."), *GetName(), *Id.ToString());
		}

		UTextBlock* Row = NewObject<UTextBlock>(this);
		if (!Row)
		{
			continue;
		}

		Row->SetText(Card
			? FText::Format(
				LOCTEXT("LiveStaff_AssignmentRow", "{0} · {1}명"),
				Card->DisplayName, FText::AsNumber(Card->RequiredStaff))
			: FText::Format(LOCTEXT("LiveStaff_AssignmentUnknown", "{0} · 정보 없음"),
				FText::FromName(Id)));

		// 이름이 `Slot` 이면 `UWidget::Slot` 을 가려서 -WarningsAsErrors 에 걸린다.
		if (UVerticalBoxSlot* RowSlot = Box_Assignments->AddChildToVerticalBox(Row))
		{
			RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
		}
		++RowCount;
	}

	// 빈 목록에 아무것도 안 두면 고장난 것으로 보인다. 왜 비었는지 적는다.
	SetTextOrHide(Text_AssignmentsEmpty, (RowCount > 0)
		? FText::GetEmpty()
		: LOCTEXT("LiveStaff_NoAssignment",
			"오늘 편성한 유료 프로그램이 없어 인력이 묶여 있지 않습니다."));

	Box_Assignments->SetVisibility((RowCount > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void UFF_LivePanelStaff::HandleStaffCountChanged(int32 NewValue)
{
	// 인자를 쓰지 않는다. 델리게이트 둘이 이 함수를 부르고, 어느 쪽이 왔든
	// 전체를 다시 읽어야 남는 인원 계산의 두 값이 서로 맞는다.
	RefreshAll();
}

void UFF_LivePanelStaff::HandleScheduledProgramsChanged()
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
