#include "Festival/FF_RequiredFacilitiesSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Staff/FF_StaffFacilitySubsystem.h"

#define LOCTEXT_NAMESPACE "FFRequiredFacilities"

namespace FFRequiredFacilitiesRules
{
	// 설치 개수와 배정 인원이 같은 시설 집합을 세도록 기준을 한 곳에 둔다.
	static bool CountsAsInstalled(const FFFZSFacilityReadModel& Installed)
	{
		const bool bInstalledState = Installed.State == EFFZSFacilityState::Validated
			|| Installed.State == EFFZSFacilityState::Active
			|| Installed.State == EFFZSFacilityState::Closed
			|| Installed.State == EFFZSFacilityState::MovePending;
		return bInstalledState && !Installed.bNeedsRelocation;
	}

	// 배정 요구가 저작상 달성 가능한지. 정원이 없는 시설이나 (월드 상한 × 정원)을 넘는 요구는 저작 오류다.
	static bool IsStaffRequirementAchievable(const FFF_RequiredFacility& Requirement, const FFFZSFacilitySpec& Spec)
	{
		if (Requirement.MinimumAssignedStaff <= 0) { return true; }
		const int32 Capacity = Spec.GetStaffCapacity();
		if (Capacity <= 0) { return false; }
		return Spec.MaxInstancesPerWorld <= 0
			|| static_cast<int64>(Requirement.MinimumAssignedStaff) <= static_cast<int64>(Capacity) * Spec.MaxInstancesPerWorld;
	}
}

void UFF_RequiredFacilitiesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	// 배정 인원은 스태프 경계가 정원 정리까지 마친 값을 읽는다. 그 경계가 먼저 초기화되게 한다.
	Staffing = Collection.InitializeDependency<UFF_StaffFacilitySubsystem>();
	Super::Initialize(Collection);
	ZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	if (Staffing)
	{
		// 배정·해제·고용·정원 정리·불러오기 모두 이 한 신호로 온다. 필수 조건 변경으로 다시 알린다.
		Staffing->OnStaffingChanged.AddUniqueDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	BindProgression();
}

void UFF_RequiredFacilitiesSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// 월드 생성 시 GameInstance가 아직 연결되지 않았어도 플레이 시작 전에 구독한다.
	BindProgression();
	HandleRequirementsChanged();
}

void UFF_RequiredFacilitiesSubsystem::BindProgression()
{
	UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UFF_FestivalProgressionSubsystem* Current = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	if (Progression == Current) { return; }
	if (Progression)
	{
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	Progression = Current;
	if (Progression)
	{
		Progression->OnFestivalDefinitionChanged.AddUniqueDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
}

void UFF_RequiredFacilitiesSubsystem::Deinitialize()
{
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.RemoveDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.RemoveDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	if (Staffing)
	{
		Staffing->OnStaffingChanged.RemoveDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	if (Progression)
	{
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	ZoneSystem = nullptr;
	Staffing = nullptr;
	Progression = nullptr;
	Super::Deinitialize();
}

bool UFF_RequiredFacilitiesSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

void UFF_RequiredFacilitiesSubsystem::HandleWorldChanged(const FFFZSWorldChangeEvent& Event)
{
	HandleRequirementsChanged();
}

void UFF_RequiredFacilitiesSubsystem::HandleRequirementsChanged()
{
	OnRequiredFacilitiesChanged.Broadcast();
	OnRequiredFacilitiesChangedNative.Broadcast();
}

FFF_RequiredFacilitiesStatus UFF_RequiredFacilitiesSubsystem::GetRequiredFacilitiesStatus() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* Current = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const FFFZSReadModel Model = ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel();
	return EvaluateRequirements(Current ? Current->GetDefinition() : nullptr, Model,
		BuildStaffingSnapshot(Model), ZoneSystem != nullptr);
}

FFF_RequiredFacilityStaffing UFF_RequiredFacilitiesSubsystem::BuildStaffingSnapshot(const FFFZSReadModel& Model) const
{
	FFF_RequiredFacilityStaffing Result;
	const UWorld* World = GetWorld();
	const UFF_StaffFacilitySubsystem* Staff = World ? World->GetSubsystem<UFF_StaffFacilitySubsystem>() : nullptr;
	if (!Staff) { return Result; }
	Result.bKnown = true;
	Result.UnassignedStaffCount = Staff->GetUnassignedStaffCount();
	for (const FFFZSFacilityReadModel& Facility : Model.Facilities)
	{
		// 스태프 자리는 Info 시설에만 있다(FFFZSFacilitySpec::GetStaffCapacity). 나머지는 묻지 않는다.
		if (Facility.Category != EFFZSFacilityCategory::Info || !Facility.FacilityInstanceId.IsValid()) { continue; }
		const int32 Assigned = Staff->GetAssignedStaffCount(Facility.FacilityInstanceId);
		if (Assigned > 0) { Result.AssignedStaffByFacility.Add(Facility.FacilityInstanceId, Assigned); }
	}
	return Result;
}

bool UFF_RequiredFacilitiesSubsystem::HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory Category) const
{
	for (const FFF_RequiredFacilityRow& Row : GetRequiredFacilitiesStatus().Facilities)
	{
		if (Row.bDefinitionValid && Row.Category == Category && Row.MissingCount > 0) { return true; }
	}
	return false;
}

bool UFF_RequiredFacilitiesSubsystem::TryGetRequiredFacilityRow(
	FName FacilityDefinitionId, FFF_RequiredFacilityRow& OutRow) const
{
	OutRow = FFF_RequiredFacilityRow();
	for (const FFF_RequiredFacilityRow& Row : GetRequiredFacilitiesStatus().Facilities)
	{
		if (!FacilityDefinitionId.IsNone() && Row.FacilityDefinitionId == FacilityDefinitionId)
		{
			OutRow = Row;
			return true;
		}
	}
	return false;
}

AFFZSFacilityActor* UFF_RequiredFacilitiesSubsystem::FindStaffAssignmentTarget() const
{
	if (!ZoneSystem || !Staffing) { return nullptr; }
	const FFF_RequiredFacilitiesStatus Status = GetRequiredFacilitiesStatus();
	const FFFZSReadModel Model = ZoneSystem->BuildReadModel();
	for (const FFF_RequiredFacilityRow& Row : Status.Facilities)
	{
		if (!Row.bDefinitionValid || Row.MissingCount > 0 || Row.MissingStaffCount <= 0) { continue; }
		for (const FFFZSFacilityReadModel& Installed : Model.Facilities)
		{
			if (Installed.FacilityDefinitionId != Row.FacilityDefinitionId ||
				!FFRequiredFacilitiesRules::CountsAsInstalled(Installed)) { continue; }
			const FGuid Id = Installed.FacilityInstanceId;
			// 대기 인원이 없어도 배정 화면은 열어 부족 이유를 확인할 수 있게 한다.
			if (Staffing->GetFacilityCapacity(Id) > Staffing->GetAssignedStaffCount(Id))
			{
				if (AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(Id)) { return Facility; }
			}
		}
	}
	return nullptr;
}

FFF_RequiredFacilitiesStatus UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(
	const UFF_FestivalDefinition* Definition, const FFFZSReadModel& Model, bool bWorldAvailable)
{
	// 배정 상태를 넘기지 않은 호출자는 배정 조건을 충족으로 보지 않는다 — 모르는 것을 안전하다고 알리지 않는다.
	return EvaluateRequirements(Definition, Model, FFF_RequiredFacilityStaffing(), bWorldAvailable);
}

FFF_RequiredFacilitiesStatus UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(
	const UFF_FestivalDefinition* Definition, const FFFZSReadModel& Model,
	const FFF_RequiredFacilityStaffing& StaffingState, bool bWorldAvailable)
{
	FFF_RequiredFacilitiesStatus Result;
	if (!Definition)
	{
		Result.BlockingReason = LOCTEXT("NoFestival", "축제 설정을 확인할 수 없습니다.");
		return Result;
	}
	Result.bCanProceed = true;
	TSet<FName> Seen;
	TArray<FText> Reasons;
	for (const FFF_RequiredFacility& Requirement : Definition->RequiredSetupFacilities)
	{
		FFF_RequiredFacilityRow Row;
		Row.MinimumCount = Requirement.MinimumCount;
		Row.RequiredStaffCount = FMath::Max(0, Requirement.MinimumAssignedStaff);
		bool bStaffRequirementAchievable = true;
		const UFFZSFacilityDefinition* Facility = Requirement.FacilityDefinition.LoadSynchronous();
		if (Facility)
		{
			Row.FacilityDefinitionId = Facility->Spec.FacilityDefinitionId;
			Row.DisplayName = Facility->Spec.DisplayName;
			Row.Category = Facility->Spec.Category;
			bStaffRequirementAchievable = FFRequiredFacilitiesRules::IsStaffRequirementAchievable(Requirement, Facility->Spec);
			Row.bDefinitionValid = !Row.FacilityDefinitionId.IsNone() && Requirement.MinimumCount > 0
				&& !Seen.Contains(Row.FacilityDefinitionId)
				&& (Facility->Spec.MaxInstancesPerWorld <= 0 || Requirement.MinimumCount <= Facility->Spec.MaxInstancesPerWorld)
				&& bStaffRequirementAchievable;
			Seen.Add(Row.FacilityDefinitionId);
		}
		const FText Name = Row.DisplayName.IsEmpty() ? FText::FromName(Row.FacilityDefinitionId) : Row.DisplayName;
		if (!Row.bDefinitionValid)
		{
			Row.Guidance = (Facility && !bStaffRequirementAchievable)
				? FText::Format(LOCTEXT("InvalidStaffRequirement",
					"{0}: 필수 배정 인원 설정에 오류가 있습니다. Staff Capacity가 있는 Info 시설인지, 배정 인원이 설치 가능한 정원 합을 넘지 않는지 확인하세요."), Name)
				: LOCTEXT("InvalidRequirement", "필수 설치 시설 설정에 오류가 있습니다. 시설 정의와 최소 개수를 확인하세요.");
		}
		else
		{
			// 배정 인원도 설치로 인정된 시설의 것만 센다. 재배치가 필요한 시설은 출동 출발지가 될 수 없다.
			for (const FFFZSFacilityReadModel& Installed : Model.Facilities)
			{
				if (Installed.FacilityDefinitionId != Row.FacilityDefinitionId
					|| !FFRequiredFacilitiesRules::CountsAsInstalled(Installed))
				{
					continue;
				}
				++Row.InstalledCount;
				// 배정 경계를 읽지 못한 상태의 인원은 세지 않는다 — 모르는 값을 충족 근거로 쓰지 않는다.
				const int32* Assigned = StaffingState.bKnown
					? StaffingState.AssignedStaffByFacility.Find(Installed.FacilityInstanceId) : nullptr;
				if (Assigned)
				{
					Row.AssignedStaffCount += FMath::Max(0, *Assigned);
				}
			}
			Row.MissingCount = FMath::Max(0, Row.MinimumCount - Row.InstalledCount);
			Row.MissingStaffCount = FMath::Max(0, Row.RequiredStaffCount - Row.AssignedStaffCount);
			if (Row.MissingCount > 0)
			{
				// 설치 전에도 배정이 필요하다는 것을 미리 알린다. 설치만 하고 끝났다고 오해하지 않게 한다.
				Row.Guidance = Row.RequiresStaffAssignment()
					? FText::Format(LOCTEXT("MissingFacilityThenStaff",
						"{0}: {1}개 추가 설치가 필요합니다. (설치 {2}/{3}) 설치 후 스태프를 {4}명 이상 배정해야 합니다."),
						Name, FText::AsNumber(Row.MissingCount), FText::AsNumber(Row.InstalledCount),
						FText::AsNumber(Row.MinimumCount), FText::AsNumber(Row.RequiredStaffCount))
					: FText::Format(LOCTEXT("MissingFacility", "{0}: {1}개 추가 설치가 필요합니다. (설치 {2}/{3})"),
						Name, FText::AsNumber(Row.MissingCount), FText::AsNumber(Row.InstalledCount), FText::AsNumber(Row.MinimumCount));
			}
			else if (Row.MissingStaffCount > 0)
			{
				if (!StaffingState.bKnown)
				{
					Row.Guidance = FText::Format(LOCTEXT("StaffingUnknown", "{0}: 스태프 배정 상태를 확인할 수 없습니다."), Name);
				}
				else
				{
					// 누를 수 없는 버튼을 안내하지 않는다. 대기 인원이 없으면 고용·해제부터 알린다.
					const FText HowTo = StaffingState.UnassignedStaffCount == 0
						? LOCTEXT("AssignStaffNoneWaiting",
							"대기 중인 스태프가 없습니다. `인력` 탭에서 고용하거나 다른 안내 시설의 스태프를 `- 해제`한 뒤 배정하세요.")
						: LOCTEXT("AssignStaffHowTo", "설치한 시설의 `스태프` 탭에서 `+ 배정`을 누르세요.");
					Row.Guidance = FText::Format(LOCTEXT("MissingStaff", "{0}: 스태프를 {1}명 더 배정해야 합니다. (배정 {2}/{3}) {4}"),
						Name, FText::AsNumber(Row.MissingStaffCount), FText::AsNumber(Row.AssignedStaffCount),
						FText::AsNumber(Row.RequiredStaffCount), HowTo);
				}
			}
		}
		if (!Row.Guidance.IsEmpty()) { Reasons.Add(Row.Guidance); Result.bCanProceed = false; }
		Result.Facilities.Add(Row);
	}
	if (!bWorldAvailable && !Definition->RequiredSetupFacilities.IsEmpty())
	{
		Reasons.Insert(LOCTEXT("NoWorld", "시설 설치 상태를 확인할 수 없습니다."), 0);
		Result.bCanProceed = false;
	}
	Result.BlockingReason = FText::Join(FText::FromString(TEXT("\n")), Reasons);
	return Result;
}

FString UFF_RequiredFacilitiesSubsystem::DebugDescribeRequiredFacilities() const
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const auto* Current = GI ? GI->GetSubsystem<UFF_FestivalProgressionSubsystem>() : nullptr;
	const UFF_FestivalDefinition* Definition = Current ? Current->GetDefinition() : nullptr;
	if (!Definition)
	{
		return TEXT("(축제 정의 없음)");
	}
	if (!ZoneSystem)
	{
		return TEXT("(ZoneSystem 없음)");
	}

	const FFFZSReadModel Model = ZoneSystem->BuildReadModel();
	const FFF_RequiredFacilityStaffing StaffingState = BuildStaffingSnapshot(Model);
	TArray<FString> Lines;
	for (const FFF_RequiredFacility& Requirement : Definition->RequiredSetupFacilities)
	{
		const UFFZSFacilityDefinition* Facility = Requirement.FacilityDefinition.LoadSynchronous();
		const FName Id = Facility ? Facility->Spec.FacilityDefinitionId : NAME_None;
		if (Id.IsNone())
		{
			Lines.Add(TEXT("(정의 로드 실패)"));
			continue;
		}

		TArray<FString> Instances;
		for (const FFFZSFacilityReadModel& Installed : Model.Facilities)
		{
			if (Installed.FacilityDefinitionId == Id)
			{
				Instances.Add(FString::Printf(TEXT("State=%s NeedsRelocation=%s Staff=%d"),
					*UEnum::GetValueAsString(Installed.State),
					Installed.bNeedsRelocation ? TEXT("true") : TEXT("false"),
					StaffingState.AssignedStaffByFacility.FindRef(Installed.FacilityInstanceId)));
			}
		}

		Lines.Add(FString::Printf(TEXT("%s(최소 %d개, 배정 %d명 이상): %s"), *Id.ToString(), Requirement.MinimumCount,
			FMath::Max(0, Requirement.MinimumAssignedStaff),
			Instances.IsEmpty() ? TEXT("월드에 0개") : *FString::Join(Instances, TEXT(" / "))));
	}
	if (!StaffingState.bKnown)
	{
		Lines.Add(TEXT("(스태프 배정 경계 없음)"));
	}
	return Lines.IsEmpty() ? TEXT("(필수 시설 목록 없음)") : FString::Join(Lines, TEXT(" | "));
}

#undef LOCTEXT_NAMESPACE
