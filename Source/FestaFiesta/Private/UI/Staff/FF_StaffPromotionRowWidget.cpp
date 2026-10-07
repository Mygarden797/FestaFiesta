// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Staff/FF_StaffPromotionRowWidget.h"

#include "Components/Button.h"
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

void UFF_StaffPromotionRowWidget::RefreshRow()
{
	// 데이터가 없을 때는 Designer에서 만든 미리보기를 보존한다.
	if (!bHasMember) { return; }
	if (Text_StaffName) { Text_StaffName->SetText(GetStaffDisplayName()); }
	if (Text_StaffKind) { Text_StaffKind->SetText(CurrentMember.bIsBaseStaff ? BaseStaffText : HiredStaffText); }
	if (Text_StaffLevel)
	{
		Text_StaffLevel->SetText(FText::Format(StaffLevelFormat,
			FFormatNamedArguments{{TEXT("Level"), FText::AsNumber(CurrentMember.TrainingLevel)}}));
	}
	if (Text_PromotionStatus)
	{
		Text_PromotionStatus->SetText(bRequestPending ? PendingStatusText : GetPromotionStatusText(CurrentState));
	}
	if (Text_Promote) { Text_Promote->SetText(bRequestPending ? PendingLabelText : PromoteLabelText); }
	if (Button_Promote)
	{
		const bool bCanPromote = CurrentState == EFF_StaffPromotionState::Ready && !bRequestPending;
		const bool bHadButtonFocus = Button_Promote->HasAnyUserFocus();
		Button_Promote->SetIsEnabled(bCanPromote);
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
