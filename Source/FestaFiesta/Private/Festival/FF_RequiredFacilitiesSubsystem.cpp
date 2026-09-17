#include "Festival/FF_RequiredFacilitiesSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSWorldSubsystem.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

#define LOCTEXT_NAMESPACE "FFRequiredFacilities"

void UFF_RequiredFacilitiesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFFZSWorldSubsystem>();
	Super::Initialize(Collection);
	ZoneSystem = GetWorld()->GetSubsystem<UFFZSWorldSubsystem>();
	if (ZoneSystem)
	{
		ZoneSystem->OnWorldChanged.AddUniqueDynamic(this, &ThisClass::HandleWorldChanged);
		ZoneSystem->OnWorldSnapshotRestored.AddUniqueDynamic(this, &ThisClass::HandleRequirementsChanged);
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
	if (Progression)
	{
		Progression->OnFestivalDefinitionChanged.RemoveDynamic(this, &ThisClass::HandleRequirementsChanged);
	}
	ZoneSystem = nullptr;
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
	return EvaluateRequirements(Current ? Current->GetDefinition() : nullptr,
		ZoneSystem ? ZoneSystem->BuildReadModel() : FFFZSReadModel(), ZoneSystem != nullptr);
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

FFF_RequiredFacilitiesStatus UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(
	const UFF_FestivalDefinition* Definition, const FFFZSReadModel& Model, bool bWorldAvailable)
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
		const UFFZSFacilityDefinition* Facility = Requirement.FacilityDefinition.LoadSynchronous();
		if (Facility)
		{
			Row.FacilityDefinitionId = Facility->Spec.FacilityDefinitionId;
			Row.DisplayName = Facility->Spec.DisplayName;
			Row.Category = Facility->Spec.Category;
			Row.bDefinitionValid = !Row.FacilityDefinitionId.IsNone() && Requirement.MinimumCount > 0
				&& !Seen.Contains(Row.FacilityDefinitionId)
				&& (Facility->Spec.MaxInstancesPerWorld <= 0 || Requirement.MinimumCount <= Facility->Spec.MaxInstancesPerWorld);
			Seen.Add(Row.FacilityDefinitionId);
		}
		if (!Row.bDefinitionValid)
		{
			Row.Guidance = LOCTEXT("InvalidRequirement", "필수 설치 시설 설정에 오류가 있습니다. 시설 정의와 최소 개수를 확인하세요.");
		}
		else
		{
			for (const FFFZSFacilityReadModel& Installed : Model.Facilities)
			{
				const bool bInstalled = Installed.State == EFFZSFacilityState::Validated
					|| Installed.State == EFFZSFacilityState::Active
					|| Installed.State == EFFZSFacilityState::Closed
					|| Installed.State == EFFZSFacilityState::MovePending;
				if (Installed.FacilityDefinitionId == Row.FacilityDefinitionId && bInstalled && !Installed.bNeedsRelocation)
				{
					++Row.InstalledCount;
				}
			}
			Row.MissingCount = FMath::Max(0, Row.MinimumCount - Row.InstalledCount);
			if (Row.MissingCount > 0)
			{
				const FText Name = Row.DisplayName.IsEmpty() ? FText::FromName(Row.FacilityDefinitionId) : Row.DisplayName;
				Row.Guidance = FText::Format(LOCTEXT("MissingFacility", "{0}: {1}개 추가 설치가 필요합니다. (설치 {2}/{3})"),
					Name, FText::AsNumber(Row.MissingCount), FText::AsNumber(Row.InstalledCount), FText::AsNumber(Row.MinimumCount));
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
				Instances.Add(FString::Printf(TEXT("State=%s NeedsRelocation=%s"),
					*UEnum::GetValueAsString(Installed.State),
					Installed.bNeedsRelocation ? TEXT("true") : TEXT("false")));
			}
		}

		Lines.Add(FString::Printf(TEXT("%s(최소 %d개): %s"), *Id.ToString(), Requirement.MinimumCount,
			Instances.IsEmpty() ? TEXT("월드에 0개") : *FString::Join(Instances, TEXT(" / "))));
	}
	return Lines.IsEmpty() ? TEXT("(필수 시설 목록 없음)") : FString::Join(Lines, TEXT(" | "));
}

#undef LOCTEXT_NAMESPACE
