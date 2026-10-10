// 스태프 관리 화면은 원본 명단/시설/출동을 읽고 배정 경계에 명령만 전달한다.
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#include "Building/FF_BuildCatalogSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_RequiredFacilitiesSubsystem.h"
#include "Incident/FF_IncidentSubsystem.h"
#include "Save/FF_SaveSubsystem.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Staff/FF_StaffSubsystem.h"
#include "Time/FF_FestivalStageSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaStaffManagement"
namespace FFStaffManagement
{
	bool CanManage(const UFF_FestivalViewModelSubsystem& ViewModel)
	{
		const UGameInstance* GI = ViewModel.GetGameInstance();
		const UFF_SaveSubsystem* Save = GI ? GI->GetSubsystem<UFF_SaveSubsystem>() : nullptr;
		const UFF_FestivalProgressionSubsystem* Progression = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
		const UFF_FestivalStageSubsystem* Stages = GI ? GI->GetSubsystem<UFF_FestivalStageSubsystem>() : nullptr;
		if (!GI || !GI->GetWorld() || !Progression || !Progression->IsFestivalStarted() || !Stages) { return false; }
		const EFF_FestivalStage Stage = Stages->CurrentStage;
		return (!Save || !Save->IsRestoring()) &&
			(Stage == EFF_FestivalStage::Setup || Stage == EFF_FestivalStage::MorningCheck || Stage == EFF_FestivalStage::Live);
	}

	FText DutyText(EFF_StaffDuty Duty)
	{
		switch (Duty)
		{
		case EFF_StaffDuty::Unassigned: return LOCTEXT("Unassigned", "미배정");
		case EFF_StaffDuty::Ready: return LOCTEXT("Ready", "출동 대기");
		case EFF_StaffDuty::Moving: return LOCTEXT("Moving", "현장 이동 중");
		case EFF_StaffDuty::Working: return LOCTEXT("Working", "현장 대응 중");
		case EFF_StaffDuty::Returning: return LOCTEXT("Returning", "복귀 중");
		case EFF_StaffDuty::Reserved: return LOCTEXT("Reserved", "출동 준비 · 상태 확인");
		default: return LOCTEXT("Unavailable", "출발 위치 확인 필요");
		}
	}
}

FFF_StaffManagementSnapshot UFF_FestivalViewModelSubsystem::GetStaffManagementSnapshot() const
{
	FFF_StaffManagementSnapshot Result;
	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	const UFF_FestivalProgressionSubsystem* Progression = GetProgressionSubsystem();
	const UFF_StaffFacilitySubsystem* Staffing = World ? World->GetSubsystem<UFF_StaffFacilitySubsystem>() : nullptr;
	const UFFZSWorldSubsystem* Zone = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
	const UFF_SaveSubsystem* Save = GI ? GI->GetSubsystem<UFF_SaveSubsystem>() : nullptr;
	if (!Progression || !Progression->IsFestivalStarted() || !Staffing || !Zone || (Save && Save->IsRestoring())) { return Result; }
	// 시설 경계가 레지스트리 변경을 반영한 뒤 명단을 읽는다. UI가 소속을 직접 보정하지 않는다.
	Result.Capacity = Staffing->GetTotalFacilityCapacity();
	// 위 조회가 정원 정리 알림을 발행할 수 있으므로 복원/축제 전환 여부를 다시 확인한다.
	if (GI->GetWorld() != World || !Progression->IsFestivalStarted() || (Save && Save->IsRestoring())) { return FFF_StaffManagementSnapshot(); }
	Result.bAvailable = true;
	Result.bCanManage = FFStaffManagement::CanManage(*this);
	Result.Dispatchable = Staffing->GetDispatchableStaffCount();
	const TArray<FFF_StaffMember> Roster = Progression->GetStaffRoster();
	Result.Total = Roster.Num();
	const FFFZSReadModel ReadModel = Zone->BuildReadModel();
	const UFF_BuildCatalogSubsystem* Catalog = GI->GetSubsystem<UFF_BuildCatalogSubsystem>();
	const UFF_RequiredFacilitiesSubsystem* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
	const FFF_RequiredFacilitiesStatus Requirements = Required ? Required->GetRequiredFacilitiesStatus() : FFF_RequiredFacilitiesStatus();
	const UFF_StaffSubsystem* Bodies = World->GetSubsystem<UFF_StaffSubsystem>();

	// 프로그램 목록과 같은 GUID별 표시 번호를 사용한다. 시설 이동/철거로 남은 시설 번호가 바뀌지 않는다.
	if (ProgramFacilityIdentityWorld.Get() != World)
	{
		ProgramFacilityIdentityWorld = World;
		ProgramFacilityInstanceNumbers.Reset();
		ProgramFacilityNextNumbers.Reset();
	}
	for (const FFFZSFacilityReadModel& Facility : ReadModel.Facilities)
	{
		if (Facility.FacilityInstanceId.IsValid() && !ProgramFacilityInstanceNumbers.Contains(Facility.FacilityInstanceId))
		{
			int32& Last = ProgramFacilityNextNumbers.FindOrAdd(Facility.FacilityDefinitionId);
			ProgramFacilityInstanceNumbers.Add(Facility.FacilityInstanceId, ++Last);
		}
		const int32 Capacity = Staffing->GetFacilityCapacity(Facility.FacilityInstanceId);
		if (Capacity <= 0) { continue; }
		FFF_StaffFacilityManagementView& Row = Result.Facilities.AddDefaulted_GetRef();
		Row.FacilityId = Facility.FacilityInstanceId;
		Row.DefinitionId = Facility.FacilityDefinitionId;
		Row.Capacity = Capacity;
		Row.Assigned = Staffing->GetAssignedStaffCount(Row.FacilityId);
		Row.bCanAssign = Result.bCanManage && Staffing->CanAssignAvailableStaff(Row.FacilityId);
		Row.bCanUnassign = Result.bCanManage && Staffing->CanUnassignAvailableStaff(Row.FacilityId);
		Row.bRequiredStaff = Requirements.Facilities.ContainsByPredicate([&Row](const FFF_RequiredFacilityRow& Requirement)
		{
			return Requirement.FacilityDefinitionId == Row.DefinitionId && Requirement.RequiresStaffAssignment();
		});
		FText Name = Catalog ? Catalog->ResolveFacilityDisplayName(Row.DefinitionId) : Facility.DisplayName;
		if (Name.IsEmpty()) { Name = LOCTEXT("Facility", "안내 시설"); }
		FNumberFormattingOptions NumberFormat;
		NumberFormat.SetMinimumIntegralDigits(2).SetUseGrouping(false);
		Row.Label = FText::Format(LOCTEXT("FacilityNumber", "{0} #{1}"), Name,
			FText::AsNumber(ProgramFacilityInstanceNumbers.FindRef(Row.FacilityId), &NumberFormat));
		if (const AFFZSGameplayZone* GameplayZone = Zone->FindZone(Facility.ZoneId))
		{
			Row.LocationLabel = !GameplayZone->DisplayName.IsEmpty() ? GameplayZone->DisplayName : FText::FromName(Facility.ZoneId);
		}
		Result.Vacancies += FMath::Max(0, Capacity - Row.Assigned);
	}
	Result.Facilities.Sort([](const FFF_StaffFacilityManagementView& A, const FFF_StaffFacilityManagementView& B)
	{
		if (A.bRequiredStaff != B.bRequiredStaff) { return A.bRequiredStaff; }
		const int32 Compare = A.Label.CompareTo(B.Label);
		return Compare == 0 ? A.FacilityId < B.FacilityId : Compare < 0;
	});

	int32 ReadyCount = 0;
	for (const FFF_StaffMember& Member : Roster)
	{
		FFF_StaffMemberManagementView& Row = Result.Members.AddDefaulted_GetRef();
		Row.StaffId = Member.StaffId;
		Row.FacilityId = Member.AssignedFacilityId;
		Row.TrainingLevel = Member.TrainingLevel;
		Row.bIsBase = Member.bIsBaseStaff;
		Row.bUnassigned = !Member.AssignedFacilityId.IsValid();
		Row.PromotionState = GetStaffPromotionState(Member.StaffId);
		FFF_StaffFacilityManagementView* Facility = Result.Facilities.FindByPredicate(
			[&Member](const FFF_StaffFacilityManagementView& Value) { return Value.FacilityId == Member.AssignedFacilityId; });
		Row.FacilityLabel = Facility ? Facility->Label : Row.bUnassigned ? LOCTEXT("NoFacility", "미배정") : LOCTEXT("MissingFacility", "배정 시설 확인 필요");
		if (Row.bUnassigned) { ++Result.Unassigned; } else { ++Result.Assigned; }
		const bool bBusy = Staffing->IsStaffBusy(Member.StaffId);
		if (bBusy)
		{
			// GetStaffState는 몸의 기록이 없을 때도 Idle을 반환하므로 예약된 Idle을 대기로 그리지 않는다.
			const EFF_StaffState State = Bodies ? Bodies->GetStaffState(Member.StaffId) : EFF_StaffState::Idle;
			Row.Duty = State == EFF_StaffState::Returning ? EFF_StaffDuty::Returning
				: State == EFF_StaffState::Moving ? EFF_StaffDuty::Moving
				: State == EFF_StaffState::Working ? EFF_StaffDuty::Working : EFF_StaffDuty::Reserved;
			if (Row.Duty == EFF_StaffDuty::Returning)
			{
				++Result.Returning;
				if (Facility) { ++Facility->Returning; }
			}
			else
			{
				++Result.Busy;
				if (Facility) { ++Facility->Busy; }
			}
		}
		else if (Row.bUnassigned) { Row.Duty = EFF_StaffDuty::Unassigned; }
		else
		{
			FFF_StaffDispatchOrigin Origin;
			const bool bHasOrigin = Staffing->GetStaffDispatchOrigin(Member.StaffId, Origin);
			Row.Duty = bHasOrigin ? EFF_StaffDuty::Ready : EFF_StaffDuty::Unavailable;
			if (bHasOrigin) { ++ReadyCount; if (Facility) { ++Facility->Ready; } }
			else { ++Result.Unavailable; if (Facility) { ++Facility->Unavailable; } }
		}
		Row.DutyLabel = FFStaffManagement::DutyText(Row.Duty);
	}
	// 예약 초기화 방송 중에는 개별 출발점이 남아 있어도 소유자가 출동을 막는다.
	// 그 중간 상태를 대기로 추정하지 않고, 초기화 완료 방송/다음 갱신에서 다시 읽는다.
	if (ReadyCount != Result.Dispatchable || Result.Busy + Result.Returning != Staffing->GetBusyStaffCount())
	{
		return FFF_StaffManagementSnapshot();
	}
	Result.Members.Sort([](const FFF_StaffMemberManagementView& A, const FFF_StaffMemberManagementView& B) { return A.StaffId < B.StaffId; });

	if (const UFF_IncidentSubsystem* Incidents = World->GetSubsystem<UFF_IncidentSubsystem>())
	{
		Result.bIncidentSourceAvailable = true;
		TArray<FFF_IncidentHandle> Handles;
		Incidents->GetActiveHandle(Handles);
		for (const FFF_IncidentHandle Handle : Handles)
		{
			FFF_IncidentView View;
			if (!Incidents->GetIncidentView(Handle, View)) { continue; }
			FFF_StaffIncidentManagementView& Row = Result.Incidents.AddDefaulted_GetRef();
			Row.Handle = Handle;
			Row.Title = View.Title;
			Row.ResponseStage = View.ResponseStage;
			Row.RemainingWorkMinutes = View.RemainingWorkMinutes;
			Row.Status = View.ResponseStage == EFF_IncidentResponseStage::Travelling ? LOCTEXT("IncidentMoving", "현장 이동 중")
				: View.ResponseStage == EFF_IncidentResponseStage::Working
				? FText::Format(LOCTEXT("IncidentWorking", "현장 대응 중 · 남은 {0}분"), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(View.RemainingWorkMinutes))))
				: LOCTEXT("IncidentWaiting", "확인 필요");
		}
	}
	return Result;
}

bool UFF_FestivalViewModelSubsystem::RequestAssignAvailableStaff(FGuid FacilityId)
{
	UWorld* World = GetWorld();
	UFF_StaffFacilitySubsystem* Staffing = World ? World->GetSubsystem<UFF_StaffFacilitySubsystem>() : nullptr;
	return FacilityId.IsValid() && Staffing && FFStaffManagement::CanManage(*this) && Staffing->TryAssignAvailableStaff(FacilityId);
}

bool UFF_FestivalViewModelSubsystem::RequestUnassignAvailableStaff(FGuid FacilityId)
{
	UWorld* World = GetWorld();
	UFF_StaffFacilitySubsystem* Staffing = World ? World->GetSubsystem<UFF_StaffFacilitySubsystem>() : nullptr;
	return FacilityId.IsValid() && Staffing && FFStaffManagement::CanManage(*this) && Staffing->TryUnassignAvailableStaff(FacilityId);
}

FFF_StaffAssignmentResult UFF_FestivalViewModelSubsystem::RequestFillStaffVacancies()
{
	FFF_StaffAssignmentResult Result;
	const FFF_StaffManagementSnapshot Before = GetStaffManagementSnapshot();
	if (!Before.bAvailable || !Before.bCanManage)
	{
		Result.Message = LOCTEXT("SourceUnavailable", "지금은 스태프를 배정할 수 없습니다.");
		return Result;
	}
	if (Before.Unassigned <= 0 || Before.Vacancies <= 0)
	{
		Result.Message = Before.Unassigned <= 0 ? LOCTEXT("NoUnassigned", "배정할 미배정 스태프가 없습니다.")
			: LOCTEXT("NoVacancy", "안내 시설에 빈자리가 없습니다.");
		return Result;
	}
	// 1차: 소유자가 정의한 최소 배정 조건부터 채운다. 메인안내소/1명 등을 UI에 하드코딩하지 않는다.
	const UFF_RequiredFacilitiesSubsystem* Required = GetWorld() ? GetWorld()->GetSubsystem<UFF_RequiredFacilitiesSubsystem>() : nullptr;
	if (Required)
	{
		while (Result.AssignedCount < Before.Unassigned)
		{
			// 재배치 필요 시설 등 필수 조건에서 제외되는 대상까지 UI가 따로 판정하지 않는다.
			const AFFZSFacilityActor* Target = Required->FindStaffAssignmentTarget();
			if (!Target || !RequestAssignAvailableStaff(Target->GetFacilityInstanceId())) { break; }
			++Result.AssignedCount;
		}
	}
	// 2차: 화면에 보이는 시설 순서로 남은 빈자리를 채운다. 매 요청의 최종 판정은 원본 API가 한다.
	for (const FFF_StaffFacilityManagementView& Facility : Before.Facilities)
	{
		while (Result.AssignedCount < Before.Unassigned && RequestAssignAvailableStaff(Facility.FacilityId)) { ++Result.AssignedCount; }
	}
	const FFF_StaffManagementSnapshot After = GetStaffManagementSnapshot();
	Result.Message = Result.AssignedCount > 0
		? FText::Format(LOCTEXT("AssignedResult", "{0}명 배정 완료 · 미배정 {1}명 · 빈자리 {2}개"),
			FText::AsNumber(Result.AssignedCount), FText::AsNumber(After.Unassigned), FText::AsNumber(After.Vacancies))
		: LOCTEXT("AssignmentRejected", "배정 상태가 바뀌었습니다. 시설과 인원 상태를 다시 확인하세요.");
	return Result;
}
#undef LOCTEXT_NAMESPACE
