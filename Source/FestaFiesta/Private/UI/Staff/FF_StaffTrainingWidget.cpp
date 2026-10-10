// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Staff/FF_StaffTrainingWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/Staff/FF_StaffPromotionRowWidget.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

namespace
{
	void SetStaffTrainingText(UTextBlock* Text, const FText& Value)
	{
		if (Text) { Text->SetText(Value); }
	}

	void SetOptionalStaffTrainingText(UTextBlock* Text, const FText& Value)
	{
		if (Text)
		{
			Text->SetText(Value);
			Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		}
	}
}

void UFF_StaffTrainingWidget::ConfigureStaffRowDefaults(TSubclassOf<UFF_StaffPromotionRowWidget> InRowClass)
{
	if (!StaffRowClass) { StaffRowClass = InRowClass; }
}

void UFF_StaffTrainingWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_AllStaff)
	{
		Button_AllStaff->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAllStaffClicked);
	}
	if (Button_UnassignedStaff)
	{
		Button_UnassignedStaff->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnassignedStaffClicked);
	}
	RefreshFilterLabels();
	// 생성만으로는 구독하지 않는다. 소유 패널이 먼저 활성화한 경우에만 현재 표시를 맞춘다.
	if (bRosterActive) { SyncRoster(); }
}

void UFF_StaffTrainingWidget::NativeDestruct()
{
	DeactivateRoster();
	if (Button_AllStaff)
	{
		Button_AllStaff->OnClicked.RemoveDynamic(this, &ThisClass::HandleAllStaffClicked);
	}
	if (Button_UnassignedStaff)
	{
		Button_UnassignedStaff->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnassignedStaffClicked);
	}
	Super::NativeDestruct();
}

void UFF_StaffTrainingWidget::ApplyManagementSnapshot(const FFF_StaffManagementSnapshot& Snapshot)
{
	ManagementSnapshot = Snapshot;
	bHasManagementSnapshot = true;
	SyncRoster();
	RefreshFilterLabels();
}

void UFF_StaffTrainingWidget::ActivateRoster()
{
	bRosterActive = true;
	bSyncAfterStageEvent = true;
	BindViewModel();
	SetResult(FText::GetEmpty());
	SyncRoster();
}

void UFF_StaffTrainingWidget::DeactivateRoster()
{
	bRosterActive = false;
	UnbindViewModel();
	bRequestInFlight = false;
	bRosterDirty = false;
	bSyncAfterStageEvent = false;
	for (const auto& Pair : RowsByStaffId)
	{
		if (Pair.Value) { Pair.Value->OnPromoteRequested.Unbind(); }
	}
	RefreshFilterLabels();
	// 목록·ScrollBox는 남겨 두어 패널을 다시 열어도 같은 위치를 유지한다.
}

void UFF_StaffTrainingWidget::BindViewModel()
{
	UnbindViewModel();
	if (const UGameInstance* GI = GetGameInstance())
	{
		CachedViewModel = GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
	}
	if (CachedViewModel)
	{
		CachedViewModel->OnStaffRosterChanged.AddUniqueDynamic(this, &ThisClass::HandleRosterChanged);
		CachedViewModel->OnStageViewChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
	}
}

void UFF_StaffTrainingWidget::UnbindViewModel()
{
	if (IsValid(CachedViewModel))
	{
		CachedViewModel->OnStaffRosterChanged.RemoveDynamic(this, &ThisClass::HandleRosterChanged);
		CachedViewModel->OnStageViewChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
	}
	CachedViewModel = nullptr;
}

FText UFF_StaffTrainingWidget::FormatCount(int32 Count) const
{
	return Count >= 0 ? FText::Format(CountFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Count)}}) : UnknownValueText;
}

void UFF_StaffTrainingWidget::SyncRoster()
{
	if (!bRosterActive) { return; }
	if (bRequestInFlight)
	{
		bRosterDirty = true;
		return;
	}

	const UFF_FestivalViewModelSubsystem* ViewModel = CachedViewModel;
	SetStaffTrainingText(Text_TotalStaff, FormatCount(ViewModel ? ViewModel->GetTotalStaffCount() : INDEX_NONE));
	SetStaffTrainingText(Text_BaseStaff, FormatCount(ViewModel ? ViewModel->GetBaseStaffCount() : INDEX_NONE));
	SetStaffTrainingText(Text_HiredStaff, FormatCount(ViewModel ? ViewModel->GetHiredStaffCount() : INDEX_NONE));
	SetOptionalStaffTrainingText(Text_RosterNote, RosterNoteText);
	RefreshFilterLabels();

	TArray<FFF_StaffMember> Roster = ViewModel ? ViewModel->GetStaffRoster() : TArray<FFF_StaffMember>();
	Roster.Sort([](const FFF_StaffMember& Left, const FFF_StaffMember& Right) { return Left.StaffId < Right.StaffId; });
	TSet<int32> CurrentIds;
	for (const FFF_StaffMember& Member : Roster) { CurrentIds.Add(Member.StaffId); }
	for (auto It = RowsByStaffId.CreateIterator(); It; ++It)
	{
		if (!CurrentIds.Contains(It.Key()))
		{
			if (UFF_StaffPromotionRowWidget* Row = It.Value())
			{
				Row->OnPromoteRequested.Unbind();
				Row->RemoveFromParent();
			}
			It.RemoveCurrent();
		}
	}

	bool bMissingRows = false;
	int32 VisibleRowCount = 0;
	for (const FFF_StaffMember& Member : Roster)
	{
		UFF_StaffPromotionRowWidget* Row = RowsByStaffId.FindRef(Member.StaffId);
		if (!Row && Box_StaffRoster && StaffRowClass)
		{
			Row = CreateWidget<UFF_StaffPromotionRowWidget>(this, StaffRowClass);
			if (Row)
			{
				RowsByStaffId.Add(Member.StaffId, Row);
				if (UVerticalBoxSlot* RowSlot = Box_StaffRoster->AddChildToVerticalBox(Row))
				{
					RowSlot->SetHorizontalAlignment(HAlign_Fill);
					RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, StaffRowSpacing));
				}
			}
		}
		if (Row)
		{
			Row->OnPromoteRequested.BindUObject(this, &ThisClass::HandlePromoteRequested);
			Row->SetStaffMember(Member, ViewModel->GetStaffPromotionState(Member.StaffId));
			const FFF_StaffMemberManagementView* Management = bHasManagementSnapshot && ManagementSnapshot.bAvailable
				? ManagementSnapshot.Members.FindByPredicate([&Member](const FFF_StaffMemberManagementView& Candidate)
				{
					return Candidate.StaffId == Member.StaffId;
				}) : nullptr;
			if (Management)
			{
				Row->SetAssignmentStatus(Management->FacilityLabel, Management->DutyLabel);
			}
			else if (bHasManagementSnapshot)
			{
				Row->SetAssignmentStatus(UnknownValueText,
					NSLOCTEXT("FestaFiestaStaffTraining", "AssignmentUnknown", "확인 중"));
			}
			const bool bShowRow = !bOnlyUnassigned || (Management && Management->bUnassigned);
			Row->SetVisibility(bShowRow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			VisibleRowCount += bShowRow ? 1 : 0;
		}
		else
		{
			bMissingRows = true;
		}
	}

	const bool bRosterKnown = ViewModel && ViewModel->GetTotalStaffCount() >= 0;
	const bool bFilterKnown = !bOnlyUnassigned || (bHasManagementSnapshot && ManagementSnapshot.bAvailable);
	SetOptionalStaffTrainingText(Text_RosterEmpty, bMissingRows || !bRosterKnown || !bFilterKnown
		? RosterUnavailableText : Roster.IsEmpty() ? RosterEmptyText
		: VisibleRowCount == 0 && bOnlyUnassigned ? UnassignedEmptyText : FText::GetEmpty());
}

void UFF_StaffTrainingWidget::RefreshFilterLabels()
{
	const bool bKnown = bHasManagementSnapshot && ManagementSnapshot.bAvailable;
	const FText Total = bKnown ? FText::AsNumber(ManagementSnapshot.Total) : UnknownValueText;
	const FText Unassigned = bKnown ? FText::AsNumber(ManagementSnapshot.Unassigned) : UnknownValueText;
	SetStaffTrainingText(Text_AllStaffFilter, FText::Format(bOnlyUnassigned
		? NSLOCTEXT("FestaFiestaStaffTraining", "AllFilter", "전체 {0}")
		: NSLOCTEXT("FestaFiestaStaffTraining", "AllFilterSelected", "✓ 전체 {0}"), Total));
	SetStaffTrainingText(Text_UnassignedStaffFilter, FText::Format(bOnlyUnassigned
		? NSLOCTEXT("FestaFiestaStaffTraining", "UnassignedFilterSelected", "✓ 미배정 {0}")
		: NSLOCTEXT("FestaFiestaStaffTraining", "UnassignedFilter", "미배정 {0}"), Unassigned));
	if (Button_AllStaff) { Button_AllStaff->SetIsEnabled(bRosterActive && !bRequestInFlight); }
	if (Button_UnassignedStaff) { Button_UnassignedStaff->SetIsEnabled(bRosterActive && bKnown && !bRequestInFlight); }
}

void UFF_StaffTrainingWidget::HandleAllStaffClicked()
{
	if (!bRosterActive || bRequestInFlight || !bOnlyUnassigned) { return; }
	bOnlyUnassigned = false;
	SyncRoster();
}

void UFF_StaffTrainingWidget::HandleUnassignedStaffClicked()
{
	if (!bRosterActive || bRequestInFlight || bOnlyUnassigned
		|| !bHasManagementSnapshot || !ManagementSnapshot.bAvailable) { return; }
	bOnlyUnassigned = true;
	SyncRoster();
}

void UFF_StaffTrainingWidget::HandleRosterChanged()
{
	if (!bRosterActive) { return; }
	if (bRequestInFlight)
	{
		// 원본 명령 안에서 동기 방송된다. 호출 중인 직원 행을 제거하거나 새로 만들지 않는다.
		bRosterDirty = true;
		return;
	}
	SyncRoster();
}

void UFF_StaffTrainingWidget::HandleStageChanged(EFF_FestivalStage NewStage, int32 NewDay)
{
	if (!bRosterActive) { return; }
	// Stage와 경제 Bridge의 구독 순서에 관계없이 새 날짜 정산이 끝난 뒤 한 번 맞춘다.
	bSyncAfterStageEvent = true;
	if (bRequestInFlight)
	{
		bRosterDirty = true;
		return;
	}
	SetResult(FText::GetEmpty());
	// 날짜·단계는 직접 판정하지 않는다. 원본의 처리 날짜를 쓰는 VM 판정을 다시 읽는다.
	SyncRoster();
}

void UFF_StaffTrainingWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bRosterActive && bSyncAfterStageEvent && !bRequestInFlight)
	{
		bSyncAfterStageEvent = false;
		SyncRoster();
	}
}

void UFF_StaffTrainingWidget::SetRowsPending(bool bPending)
{
	for (const auto& Pair : RowsByStaffId)
	{
		if (Pair.Value) { Pair.Value->SetRequestPending(bPending); }
	}
	RefreshFilterLabels();
}

FText UFF_StaffTrainingWidget::GetFailureReason(int32 StaffId) const
{
	const EFF_StaffPromotionState State = CachedViewModel
		? CachedViewModel->GetStaffPromotionState(StaffId) : EFF_StaffPromotionState::Unavailable;
	if (State == EFF_StaffPromotionState::Ready) { return PromotionRetryText; }
	const UFF_StaffPromotionRowWidget* Row = RowsByStaffId.FindRef(StaffId);
	if (!Row && StaffRowClass) { Row = StaffRowClass->GetDefaultObject<UFF_StaffPromotionRowWidget>(); }
	return Row ? Row->GetPromotionStatusText(State) : PromotionRetryText;
}

void UFF_StaffTrainingWidget::HandlePromoteRequested(int32 StaffId)
{
	if (!bRosterActive || bRequestInFlight) { return; }
	UFF_FestivalViewModelSubsystem* ViewModel = CachedViewModel;
	if (!ViewModel || !ViewModel->CanPromoteStaff(StaffId))
	{
		SyncRoster();
		SetResult(FText::Format(PromotionFailureFormat,
			FFormatNamedArguments{{TEXT("Reason"), GetFailureReason(StaffId)}}));
		return;
	}

	const UFF_StaffPromotionRowWidget* BeforeRow = RowsByStaffId.FindRef(StaffId);
	const int32 BeforeLevel = BeforeRow ? BeforeRow->GetTrainingLevel() : INDEX_NONE;
	bRequestInFlight = true;
	bRosterDirty = false;
	SetRowsPending(true);
	SetResult(PendingResultText);
	const bool bAccepted = ViewModel->RequestPromoteStaff(StaffId);
	const bool bReceivedRosterUpdate = bRosterDirty;
	bRequestInFlight = false;
	bRosterDirty = false;
	if (!bRosterActive) { return; }

	// 레벨은 먼저 올리지 않는다. 명령이 끝난 뒤 원본 명단을 한 번 읽어 같은 행을 갱신한다.
	SyncRoster();
	const UFF_StaffPromotionRowWidget* UpdatedRow = RowsByStaffId.FindRef(StaffId);
	if (bAccepted && bReceivedRosterUpdate && UpdatedRow && UpdatedRow->GetTrainingLevel() > BeforeLevel)
	{
		SetResult(FText::Format(PromotionSuccessFormat, FFormatNamedArguments{
			{TEXT("Name"), UpdatedRow->GetStaffDisplayName()},
			{TEXT("Level"), FText::AsNumber(UpdatedRow->GetTrainingLevel())}}));
	}
	else if (bAccepted)
	{
		SetResult(PromotionUnconfirmedText);
	}
	else
	{
		SetResult(FText::Format(PromotionFailureFormat,
			FFormatNamedArguments{{TEXT("Reason"), GetFailureReason(StaffId)}}));
	}
}

void UFF_StaffTrainingWidget::SetResult(const FText& Result)
{
	SetOptionalStaffTrainingText(Text_PromotionResult, Result);
}
