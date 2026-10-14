#include "UI/Staff/FF_StaffFacilityRowWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FestaFiestaStaffManagement"

void UFF_StaffFacilityRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_Assign) Button_Assign->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAssign);
	if (Button_Unassign) Button_Unassign->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnassign);
	if (Button_Locate) Button_Locate->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLocate);
}

void UFF_StaffFacilityRowWidget::NativeDestruct()
{
	if (Button_Assign) Button_Assign->OnClicked.RemoveDynamic(this, &ThisClass::HandleAssign);
	if (Button_Unassign) Button_Unassign->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnassign);
	if (Button_Locate) Button_Locate->OnClicked.RemoveDynamic(this, &ThisClass::HandleLocate);
	OnAssignRequested.Unbind(); OnUnassignRequested.Unbind(); OnLocateRequested.Unbind();
	Super::NativeDestruct();
}

void UFF_StaffFacilityRowWidget::SetFacility(const FFF_StaffFacilityManagementView& View, bool bLive, bool bCompact)
{
	Current = View;
	if (Text_FacilityName) Text_FacilityName->SetText(View.Label);
	if (Text_AssignedCapacity) Text_AssignedCapacity->SetText(FText::Format(
		LOCTEXT("AssignedCapacity", "{0} / {1}명"), View.Assigned, View.Capacity));
	FText Status;
	if (bLive)
	{
		Status = FText::Format(LOCTEXT("FacilityDuty", "대기 {0} · 출동 {1} · 복귀 {2}"),
			View.Ready, View.Busy, View.Returning);
		if (View.Unavailable > 0) Status = FText::Format(LOCTEXT("UnavailableDuty", "{0} · 출동 불가 {1}"), Status, View.Unavailable);
		if (bCompact)
		{
			// 0인 상태는 생략하되 출동·복귀·불가 인원은 모두 남긴다.
			FText CompactStatus;
			const auto AppendCount = [&CompactStatus](const FText& Format, int32 Count)
			{
				if (Count <= 0) return;
				const FText Part = FText::Format(Format, Count);
				CompactStatus = CompactStatus.IsEmpty() ? Part
					: FText::Format(LOCTEXT("CompactDutySeparator", "{0} · {1}"), CompactStatus, Part);
			};
			AppendCount(LOCTEXT("CompactReady", "대기 {0}명"), View.Ready);
			AppendCount(LOCTEXT("CompactBusy", "출동 {0}명"), View.Busy);
			AppendCount(LOCTEXT("CompactReturning", "복귀 {0}명"), View.Returning);
			AppendCount(LOCTEXT("CompactUnavailable", "출동 불가 {0}명"), View.Unavailable);
			Status = !CompactStatus.IsEmpty() ? CompactStatus : View.Assigned == 0
				? LOCTEXT("CompactUnassignedFacility", "배정 없음") : LOCTEXT("CompactDutyUnknown", "상태 확인 중");
		}
	}
	else
	{
		// 필수 조건은 시설 종류 전체를 합산하므로 개별 0명 행을 필수 미달로 단정하지 않는다.
		Status = FText::Format(LOCTEXT("FacilityAssigned", "배정 {0}명 · 빈자리 {1}명"), View.Assigned, FMath::Max(0, View.Capacity - View.Assigned));
	}
	if (Text_FacilityStatus) Text_FacilityStatus->SetText(Status);
	if (Button_Assign)
	{
		Button_Assign->SetIsEnabled(View.bCanAssign);
		Button_Assign->SetToolTipText(View.bCanAssign ? LOCTEXT("AssignTip", "미배정 스태프 1명을 배정합니다.")
			: View.Assigned >= View.Capacity ? LOCTEXT("FullTip", "시설 배정 정원이 가득 찼습니다.")
			: LOCTEXT("AssignUnavailableTip", "지금 배정할 수 있는 미배정 인원이 없습니다."));
	}
	if (Button_Unassign)
	{
		Button_Unassign->SetIsEnabled(View.bCanUnassign);
		Button_Unassign->SetToolTipText(View.bCanUnassign ? LOCTEXT("UnassignTip", "출동하지 않은 스태프 1명의 배정을 해제합니다.")
			: LOCTEXT("UnassignUnavailableTip", "해제할 대기 인원이 없습니다. 출동·복귀 중에는 해제할 수 없습니다."));
	}
	if (Button_Locate) Button_Locate->SetIsEnabled(View.FacilityId.IsValid());
}

void UFF_StaffFacilityRowWidget::HandleAssign() { if (Current.bCanAssign) OnAssignRequested.ExecuteIfBound(Current.FacilityId); }
void UFF_StaffFacilityRowWidget::HandleUnassign() { if (Current.bCanUnassign) OnUnassignRequested.ExecuteIfBound(Current.FacilityId); }
void UFF_StaffFacilityRowWidget::HandleLocate() { if (Current.FacilityId.IsValid()) OnLocateRequested.ExecuteIfBound(Current.FacilityId); }

#undef LOCTEXT_NAMESPACE
