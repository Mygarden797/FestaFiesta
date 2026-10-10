// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Staff/FF_StaffPromotionRowWidget.h"

#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

UFF_StaffPromotionRowWidget::UFF_StaffPromotionRowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 승급 버튼이 비활성화돼도 같은 직원 행에 키보드·게임패드 포커스를 남긴다.
	SetIsFocusable(true);
}

void UFF_StaffPromotionRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Promote)
	{
		Button_Promote->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePromoteClicked);
	}
	RefreshRow();
}

void UFF_StaffPromotionRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshRow();
}

void UFF_StaffPromotionRowWidget::NativeDestruct()
{
	if (Button_Promote)
	{
		Button_Promote->OnClicked.RemoveDynamic(this, &ThisClass::HandlePromoteClicked);
	}
	OnPromoteRequested.Unbind();
	bRestoreButtonFocus = false;
	Super::NativeDestruct();
}

void UFF_StaffPromotionRowWidget::SetStaffMember(
	const FFF_StaffMember& Member, EFF_StaffPromotionState State, bool bPending)
{
	CurrentMember = Member;
	CurrentState = State;
	bHasMember = true;
	bRequestPending = bPending;
	RefreshRow();
}

void UFF_StaffPromotionRowWidget::SetRequestPending(bool bPending)
{
	bRequestPending = bPending;
	RefreshRow();
}

void UFF_StaffPromotionRowWidget::SetAssignmentStatus(const FText& Facility, const FText& Duty)
{
	AssignmentFacility = Facility;
	AssignmentDuty = Duty;
	bHasAssignmentStatus = true;
	RefreshAssignmentStatus();
}

void UFF_StaffPromotionRowWidget::RefreshAssignmentStatus()
{
	if (!bHasAssignmentStatus) { return; }
	if (Text_AssignmentFacility)
	{
		Text_AssignmentFacility->SetText(AssignmentFacility);
		Text_AssignmentFacility->SetToolTipText(AssignmentFacility);
	}
	if (Text_AssignmentStatus)
	{
		Text_AssignmentStatus->SetText(AssignmentDuty);
		Text_AssignmentStatus->SetToolTipText(AssignmentDuty);
	}
}

FText UFF_StaffPromotionRowWidget::GetStaffDisplayName() const
{
	return FText::Format(StaffNameFormat,
		FFormatNamedArguments{{TEXT("Id"), FText::AsNumber(CurrentMember.StaffId)}});
}

FText UFF_StaffPromotionRowWidget::GetPromotionStatusText(EFF_StaffPromotionState State) const
{
	switch (State)
	{
	case EFF_StaffPromotionState::Ready: return ReadyText;
	case EFF_StaffPromotionState::NotStarted: return NotStartedText;
	case EFF_StaffPromotionState::NotFound: return NotFoundText;
	case EFF_StaffPromotionState::MaxLevel: return MaxLevelText;
	case EFF_StaffPromotionState::HiredToday: return HiredTodayText;
	case EFF_StaffPromotionState::AlreadyPromotedToday: return AlreadyPromotedTodayText;
	case EFF_StaffPromotionState::Unavailable:
	default: return UnavailableText;
	}
}

FText UFF_StaffPromotionRowWidget::GetCompactPromotionStatusText(EFF_StaffPromotionState State) const
{
	switch (State)
	{
	case EFF_StaffPromotionState::Ready: return PromoteLabelText;
	case EFF_StaffPromotionState::NotStarted:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactNotStarted", "축제 시작 전");
	case EFF_StaffPromotionState::NotFound:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactNotFound", "명단 확인 필요");
	case EFF_StaffPromotionState::MaxLevel:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactMaxLevel", "최대 레벨");
	case EFF_StaffPromotionState::HiredToday:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactHiredToday", "오늘 고용");
	case EFF_StaffPromotionState::AlreadyPromotedToday:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactAlreadyPromoted", "오늘 승급 완료");
	case EFF_StaffPromotionState::Unavailable:
	default:
		return NSLOCTEXT("FestaFiestaStaffTraining", "CompactUnavailable", "승급 불가");
	}
}

void UFF_StaffPromotionRowWidget::RefreshRow()
{
	// 데이터가 없을 때는 Designer에서 만든 미리보기를 보존한다.
	if (!bHasMember) { return; }
	RefreshAssignmentStatus();
	if (Text_StaffName) { Text_StaffName->SetText(GetStaffDisplayName()); }
	if (Text_StaffKind) { Text_StaffKind->SetText(CurrentMember.bIsBaseStaff ? BaseStaffText : HiredStaffText); }
	if (Text_StaffLevel)
	{
		Text_StaffLevel->SetText(FText::Format(StaffLevelFormat,
			FFormatNamedArguments{{TEXT("Level"), FText::AsNumber(CurrentMember.TrainingLevel)}}));
	}
	const FText FullStatus = bRequestPending ? PendingStatusText : GetPromotionStatusText(CurrentState);
	const bool bShowPromoteButton = !bCompactManagementRow || CurrentState == EFF_StaffPromotionState::Ready;
	if (bCompactManagementRow) { SetToolTipText(FullStatus); }
	if (Text_PromotionStatus)
	{
		const FText CompactStatus = bRequestPending ? PendingLabelText : GetCompactPromotionStatusText(CurrentState);
		Text_PromotionStatus->SetText(bCompactManagementRow ? CompactStatus : FullStatus);
		Text_PromotionStatus->SetToolTipText(bCompactManagementRow ? FullStatus : FText::GetEmpty());
		Text_PromotionStatus->SetVisibility(bCompactManagementRow && bShowPromoteButton
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (Text_Promote) { Text_Promote->SetText(bRequestPending ? PendingLabelText : PromoteLabelText); }
	if (Button_Promote)
	{
		const bool bCanPromote = CurrentState == EFF_StaffPromotionState::Ready && !bRequestPending;
		const bool bHadButtonFocus = Button_Promote->HasAnyUserFocus();
		Button_Promote->SetIsEnabled(bCanPromote);
		Button_Promote->SetVisibility(bShowPromoteButton ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Promote->SetToolTipText(bCompactManagementRow ? FullStatus : FText::GetEmpty());
		if (!bCanPromote && bHadButtonFocus)
		{
			bRestoreButtonFocus = true;
			SetFocus();
		}
		if (!bRequestPending && bRestoreButtonFocus)
		{
			// 갱신 중 사용자가 다른 곳으로 옮긴 포커스는 빼앗지 않는다.
			if (bCanPromote && HasAnyUserFocus()) { Button_Promote->SetFocus(); }
			bRestoreButtonFocus = false;
		}
	}
	if (bCompactManagementRow && SizeBox_Promote)
	{
		SizeBox_Promote->SetVisibility(bShowPromoteButton
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFF_StaffPromotionRowWidget::HandlePromoteClicked()
{
	if (!bHasMember || bRequestPending || CurrentState != EFF_StaffPromotionState::Ready
		|| !OnPromoteRequested.IsBound())
	{
		return;
	}
	// 소유 위젯이 명령을 보내기 전부터 같은 버튼의 중복 클릭을 막는다.
	SetRequestPending(true);
	OnPromoteRequested.Execute(CurrentMember.StaffId);
}
