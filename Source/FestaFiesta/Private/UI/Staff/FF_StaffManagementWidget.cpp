#include "UI/Staff/FF_StaffManagementWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/Staff/FF_StaffFacilityRowWidget.h"
#include "UI/Staff/FF_StaffTrainingWidget.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaStaffManagement"

namespace
{
	void SetManagementText(UTextBlock* Text, const FText& Value)
	{
		if (Text) Text->SetText(Value);
	}
	void SetManagementOptionalText(UTextBlock* Text, const FText& Value)
	{
		if (!Text) return;
		Text->SetText(Value);
		Text->SetVisibility(Value.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UFF_StaffManagementWidget::ConfigureFacilityRowDefaults(TSubclassOf<UFF_StaffFacilityRowWidget> InRowClass)
{
	if (!FacilityRowClass) FacilityRowClass = InRowClass;
}

void UFF_StaffManagementWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (Button_FillVacancies) Button_FillVacancies->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFillVacancies);
	if (Button_ViewIncident) Button_ViewIncident->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleViewIncident);
}

void UFF_StaffManagementWidget::NativeDestruct()
{
	DeactivateManagement();
	if (Button_FillVacancies) Button_FillVacancies->OnClicked.RemoveDynamic(this, &ThisClass::HandleFillVacancies);
	if (Button_ViewIncident) Button_ViewIncident->OnClicked.RemoveDynamic(this, &ThisClass::HandleViewIncident);
	OnRevealWorldRequested.Unbind(); OnIncidentRequested.Unbind();
	Super::NativeDestruct();
}

void UFF_StaffManagementWidget::ActivateManagement(bool bLive)
{
	DeactivateManagement();
	bActive = true;
	bLiveMode = bLive;
	RefreshElapsed = 0.f;
	if (const UGameInstance* GI = GetGameInstance()) ViewModel = GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
	if (ViewModel)
	{
		ViewModel->OnStaffAssignmentChanged.AddUniqueDynamic(this, &ThisClass::HandleChanged);
		ViewModel->OnStaffRosterChanged.AddUniqueDynamic(this, &ThisClass::HandleChanged);
		ViewModel->OnStaffCapacityChanged.AddUniqueDynamic(this, &ThisClass::HandleCountChanged);
		ViewModel->OnStageViewChanged.AddUniqueDynamic(this, &ThisClass::HandleStageChanged);
	}
	if (Widget_StaffTraining) Widget_StaffTraining->ActivateRoster();
	SetResult(FText::GetEmpty());
	Sync();
}

void UFF_StaffManagementWidget::DeactivateManagement()
{
	bActive = false;
	if (IsValid(ViewModel))
	{
		ViewModel->OnStaffAssignmentChanged.RemoveDynamic(this, &ThisClass::HandleChanged);
		ViewModel->OnStaffRosterChanged.RemoveDynamic(this, &ThisClass::HandleChanged);
		ViewModel->OnStaffCapacityChanged.RemoveDynamic(this, &ThisClass::HandleCountChanged);
		ViewModel->OnStageViewChanged.RemoveDynamic(this, &ThisClass::HandleStageChanged);
	}
	if (Widget_StaffTraining) Widget_StaffTraining->DeactivateRoster();
	ViewModel = nullptr;
	// 행과 스크롤 위치는 유지한다. 닫힌 화면의 버튼은 명령을 보낼 수 없다.
}

void UFF_StaffManagementWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bActive) return;
	RefreshElapsed += InDeltaTime;
	// 배정 이벤트 외 이동→작업→복귀 전환도 표시한다. 화면이 열린 동안만 읽는다.
	if (RefreshElapsed >= .5f) { RefreshElapsed = 0.f; Sync(); }
}

void UFF_StaffManagementWidget::HandleChanged() { Sync(); }
void UFF_StaffManagementWidget::HandleCountChanged(int32 Value) { Sync(); }
void UFF_StaffManagementWidget::HandleStageChanged(EFF_FestivalStage Stage, int32 Day) { Sync(); }

void UFF_StaffManagementWidget::Sync()
{
	if (!bActive || bChanging || bSyncing) return;
	const TGuardValue<bool> SyncGuard(bSyncing, true);
	Snapshot = ViewModel ? ViewModel->GetStaffManagementSnapshot() : FFF_StaffManagementSnapshot();
	const FText Unknown = LOCTEXT("Unknown", "—");
	const auto Count = [&](int32 Value) { return Snapshot.bAvailable ? FText::Format(LOCTEXT("Count", "{0}명"), Value) : Unknown; };
	SetManagementText(Text_Phase, ViewModel ? ViewModel->GetCurrentStageDisplayText() : Unknown);
	SetManagementText(Text_Subtitle, bLiveMode
		? LOCTEXT("LiveSubtitle", "출동 현황을 확인하고 대기 인원을 배정하세요.")
		: LOCTEXT("PrepSubtitle", "운영 전에 인원을 고용하고 안내시설에 배정하세요."));
	SetManagementText(Text_TotalStaff, Count(Snapshot.Total));
	SetManagementText(Text_StaffBreakdown, Snapshot.bAvailable && ViewModel
		? FText::Format(LOCTEXT("Breakdown", "기본 {0} · 고용 {1}"), ViewModel->GetBaseStaffCount(), ViewModel->GetHiredStaffCount()) : Unknown);
	SetManagementText(Text_SecondLabel, bLiveMode ? LOCTEXT("ReadyLabel", "출동 대기") : LOCTEXT("AssignedLabel", "배정"));
	SetManagementText(Text_SecondValue, Count(bLiveMode ? Snapshot.Dispatchable : Snapshot.Assigned));
	SetManagementText(Text_SecondNote, bLiveMode ? LOCTEXT("ReadyNote", "지금 출동할 수 있는 인원") : LOCTEXT("AssignedNote", "안내시설에 소속된 인원"));
	SetManagementText(Text_ThirdLabel, bLiveMode ? LOCTEXT("BusyLabel", "출동 · 복귀") : LOCTEXT("UnassignedLabel", "미배정"));
	SetManagementText(Text_ThirdValue, Count(bLiveMode ? Snapshot.Busy + Snapshot.Returning : Snapshot.Unassigned));
	SetManagementText(Text_ThirdNote, bLiveMode && Snapshot.bAvailable
		? FText::Format(LOCTEXT("BusyNote", "출동 {0} · 복귀 {1}"), Snapshot.Busy, Snapshot.Returning)
		: LOCTEXT("UnassignedNote", "안내시설에 배정해 주세요"));
	SetManagementText(Text_FourthLabel, bLiveMode ? LOCTEXT("UnassignedLabel", "미배정") : LOCTEXT("VacancyLabel", "빈자리"));
	SetManagementText(Text_FourthValue, Count(bLiveMode ? Snapshot.Unassigned : Snapshot.Vacancies));
	SetManagementText(Text_FourthNote, bLiveMode ? LOCTEXT("UnassignedNote", "안내시설에 배정해 주세요") : LOCTEXT("VacancyNote", "안내시설의 남은 배정 자리"));
	if (Button_FillVacancies)
	{
		const bool bHasTarget = Snapshot.Facilities.ContainsByPredicate([](const auto& F) { return F.bCanAssign; });
		Button_FillVacancies->SetIsEnabled(Snapshot.bCanManage && Snapshot.Unassigned > 0 && bHasTarget);
		Button_FillVacancies->SetToolTipText(LOCTEXT("FillTip", "필수 인원이 부족한 안내시설부터 미배정 스태프를 배정합니다."));
	}
	TSet<FGuid> CurrentIds;
	for (const auto& Facility : Snapshot.Facilities) CurrentIds.Add(Facility.FacilityId);
	for (auto It = Rows.CreateIterator(); It; ++It)
	{
		if (!CurrentIds.Contains(It.Key()))
		{
			if (It.Value()) It.Value()->RemoveFromParent();
			It.RemoveCurrent();
		}
	}
	bool bMissingRows = false;
	for (const auto& Facility : Snapshot.Facilities)
	{
		UFF_StaffFacilityRowWidget* Row = Rows.FindRef(Facility.FacilityId);
		if (!Row && Box_Facilities && FacilityRowClass)
		{
			Row = CreateWidget<UFF_StaffFacilityRowWidget>(this, FacilityRowClass);
			if (Row)
			{
				Rows.Add(Facility.FacilityId, Row);
				if (UVerticalBoxSlot* RowSlot = Box_Facilities->AddChildToVerticalBox(Row))
				{
					RowSlot->SetHorizontalAlignment(HAlign_Fill);
					RowSlot->SetPadding(FMargin(0, 0, 0, FacilityRowSpacing));
				}
			}
		}
		if (!Row) { bMissingRows = true; continue; }
		Row->OnAssignRequested.BindUObject(this, &ThisClass::HandleAssign);
		Row->OnUnassignRequested.BindUObject(this, &ThisClass::HandleUnassign);
		Row->OnLocateRequested.BindUObject(this, &ThisClass::HandleLocate);
		Row->SetFacility(Facility, bLiveMode);
	}
	SetManagementOptionalText(Text_FacilitiesEmpty, !Snapshot.bAvailable || bMissingRows
		? LOCTEXT("FacilitiesUnavailable", "안내시설 정보를 확인할 수 없습니다.")
		: Snapshot.Facilities.IsEmpty() ? LOCTEXT("NoFacilities", "배정할 안내시설이 없습니다. 배치 탭에서 안내시설을 설치하세요.") : FText::GetEmpty());
	FText Guide = Unknown;
	if (Snapshot.bAvailable)
	{
		if (Snapshot.Unassigned > 0 && Snapshot.Vacancies > 0)
			Guide = FText::Format(LOCTEXT("AssignGuide", "미배정 {0}명 · 빈자리 {1}명\n미배정 인원을 먼저 안내시설에 배정하세요."), Snapshot.Unassigned, Snapshot.Vacancies);
		else if (Snapshot.Unassigned > 0)
			Guide = FText::Format(LOCTEXT("CapacityGuide", "미배정 {0}명 · 빈자리 없음\n추가 배정하려면 안내시설이 더 필요합니다."), Snapshot.Unassigned);
		else if (Snapshot.Vacancies > 0)
			Guide = FText::Format(LOCTEXT("HireGuide", "미배정 인원 없음 · 빈자리 {0}명"), Snapshot.Vacancies);
		else Guide = LOCTEXT("AssignmentComplete", "✓ 모든 스태프가 안내시설에 배정되어 있습니다.");
		if (Snapshot.Unavailable > 0)
			Guide = FText::Format(LOCTEXT("CheckDeparture", "{0}\n! 출동 불가 {1}명 · 시설의 이동 상태와 출입구를 확인하세요."), Guide, Snapshot.Unavailable);
	}
	SetManagementText(Text_AssignmentGuide, Guide);
	if (Widget_StaffTraining) Widget_StaffTraining->ApplyManagementSnapshot(Snapshot);
	if (Group_Incident) Group_Incident->SetVisibility(bLiveMode ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	FText IncidentSummary = Snapshot.bIncidentSourceAvailable
		? LOCTEXT("NoIncident", "현재 대응할 사건이 없습니다.") : LOCTEXT("IncidentUnavailable", "사건 정보를 확인하고 있습니다.");
	if (!Snapshot.Incidents.IsEmpty())
	{
		const auto& Incident = Snapshot.Incidents[0];
		IncidentSummary = FText::Format(LOCTEXT("IncidentSummary", "{0} · {1}"), Incident.Title, Incident.Status);
		if (Snapshot.Incidents.Num() > 1) IncidentSummary = FText::Format(LOCTEXT("MoreIncidents", "{0} · 외 {1}건"), IncidentSummary, Snapshot.Incidents.Num() - 1);
	}
	SetManagementText(Text_IncidentSummary, IncidentSummary);
	if (Button_ViewIncident) Button_ViewIncident->SetIsEnabled(bLiveMode && !Snapshot.Incidents.IsEmpty());
	SetManagementText(Text_ManagementNote, bLiveMode
		? LOCTEXT("LiveNote", "고용은 셋업·아침 점검에서 가능합니다. 출동·복귀 중인 인원은 돌아온 뒤 배정을 바꿀 수 있습니다.")
		: LOCTEXT("PrepNote", "고용한 스태프는 축제 종료까지 유지됩니다. 고용 후 빈자리에 배정하세요."));
}

void UFF_StaffManagementWidget::HandleAssign(FGuid FacilityId)
{
	if (!bActive || bChanging || !ViewModel) return;
	bChanging = true;
	const bool bSuccess = ViewModel->RequestAssignAvailableStaff(FacilityId);
	bChanging = false;
	Sync();
	SetResult(bSuccess ? LOCTEXT("AssignSuccess", "✓ 스태프 1명을 배정했습니다.") : LOCTEXT("AssignFailure", "배정하지 못했습니다. 미배정 인원과 빈자리를 확인하세요."));
}

void UFF_StaffManagementWidget::HandleUnassign(FGuid FacilityId)
{
	if (!bActive || bChanging || !ViewModel) return;
	bChanging = true;
	const bool bSuccess = ViewModel->RequestUnassignAvailableStaff(FacilityId);
	bChanging = false;
	Sync();
	SetResult(bSuccess ? LOCTEXT("UnassignSuccess", "✓ 스태프 1명의 배정을 해제했습니다.") : LOCTEXT("UnassignFailure", "배정을 해제하지 못했습니다. 출동·복귀 중인 인원은 돌아온 뒤 해제할 수 있습니다."));
}

void UFF_StaffManagementWidget::HandleFillVacancies()
{
	if (!bActive || bChanging || !ViewModel) return;
	bChanging = true;
	const FFF_StaffAssignmentResult Result = ViewModel->RequestFillStaffVacancies();
	bChanging = false;
	Sync();
	SetResult(Result.Message);
}

void UFF_StaffManagementWidget::HandleLocate(FGuid FacilityId)
{
	if (!bActive || !ViewModel) return;
	if (ViewModel->RequestLocateProgramFacility(FacilityId)) OnRevealWorldRequested.ExecuteIfBound();
	else { Sync(); SetResult(LOCTEXT("LocateFailed", "시설 위치를 확인할 수 없습니다. 목록을 다시 확인하세요.")); }
}

void UFF_StaffManagementWidget::HandleViewIncident()
{
	if (!bActive || !bLiveMode) return;
	Sync();
	if (!Snapshot.Incidents.IsEmpty()) OnIncidentRequested.ExecuteIfBound(Snapshot.Incidents[0].Handle);
}

void UFF_StaffManagementWidget::SetResult(const FText& Result)
{
	SetManagementOptionalText(Text_AssignmentResult, Result);
}

#undef LOCTEXT_NAMESPACE
