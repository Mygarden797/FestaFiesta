#include "Festival/FF_RequiredFacilitiesSubsystem.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "DataTables/FF_FacilityCostRow.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Festival/FF_FestivalSettings.h"
#include "Staff/FF_StaffFacilitySubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RequiredFacilitiesRulesTest,
	"FestaFiesta.Festival.RequiredFacilities.CountsAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_RequiredFacilitiesRulesTest::RunTest(const FString& Parameters)
{
	auto* Definition = NewObject<UFF_FestivalDefinition>();
	if (!TestEqual(TEXT("Default requires the main stage and one staff facility"), Definition->RequiredSetupFacilities.Num(), 2))
	{
		return false;
	}
	const auto* Sunset = Definition->RequiredSetupFacilities[0].FacilityDefinition.LoadSynchronous();
	if (!TestNotNull(TEXT("Default Sunset asset loads"), Sunset)) { return false; }
	TestEqual(TEXT("Only exact Sunset required by default"), Sunset->Spec.FacilityDefinitionId,
		FName(TEXT("Performance_MainStage_SunsetMainStage")));
	TestEqual(TEXT("The stage requires installation only"), Definition->RequiredSetupFacilities[0].MinimumAssignedStaff, 0);

	// 스태프가 배정되는 메인안내소 1곳은 설치에 더해 1명 이상 배정까지 요구한다.
	const FFF_RequiredFacility& InfoRequirement = Definition->RequiredSetupFacilities[1];
	const auto* MainInfo = InfoRequirement.FacilityDefinition.LoadSynchronous();
	if (!TestNotNull(TEXT("Default main information asset loads"), MainInfo)) { return false; }
	TestEqual(TEXT("Only exact main information facility required by default"), MainInfo->Spec.FacilityDefinitionId,
		FName(TEXT("Info_GeneralInfo_General")));
	TestTrue(TEXT("The required staff facility is an Info facility"), MainInfo->Spec.Category == EFFZSFacilityCategory::Info);
	TestTrue(TEXT("The required staff facility provides staff slots"), MainInfo->Spec.GetStaffCapacity() > 0);
	TestEqual(TEXT("One main information facility is required"), InfoRequirement.MinimumCount, 1);
	TestEqual(TEXT("At least one staff member must be assigned"), InfoRequirement.MinimumAssignedStaff, 1);
	const auto Defaults = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, FFFZSReadModel());
	TestTrue(TEXT("The default staff requirement is achievable"),
		Defaults.Facilities.IsValidIndex(1) && Defaults.Facilities[1].bDefinitionValid);

	// 아래 설치 개수 규칙은 공연장 한 행만으로 검증한다.
	Definition->RequiredSetupFacilities.SetNum(1);
	Definition->RequiredSetupFacilities[0].MinimumCount = 2;
	// 다른 축제가 같은 시설을 여러 개 요구하는 규칙은 인스턴스 상한과 독립적으로 테스트한다.
	auto* Facility = DuplicateObject<UFFZSFacilityDefinition>(Sunset, Definition);
	Facility->Spec.MaxInstancesPerWorld = 0;
	Definition->RequiredSetupFacilities[0].FacilityDefinition = Facility;
	FFFZSReadModel Model;
	for (EFFZSFacilityState State : { EFFZSFacilityState::Planned, EFFZSFacilityState::Validated,
		EFFZSFacilityState::Active, EFFZSFacilityState::Closed, EFFZSFacilityState::MovePending, EFFZSFacilityState::Removed })
	{
		Model.Facilities.Reset();
		FFFZSFacilityReadModel Row;
		Row.FacilityDefinitionId = Facility->Spec.FacilityDefinitionId;
		Row.State = State;
		Model.Facilities.Add(Row);
		const auto Result = UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model);
		const bool bCounts = State == EFFZSFacilityState::Validated || State == EFFZSFacilityState::Active
			|| State == EFFZSFacilityState::Closed || State == EFFZSFacilityState::MovePending;
		TestEqual(TEXT("Only installed usable lifecycle states count"), Result.Facilities[0].InstalledCount, bCounts ? 1 : 0);
		TestFalse(TEXT("One cannot meet a requirement of two"), Result.bCanProceed);
	}
	Model.Facilities[0].State = EFFZSFacilityState::Validated;
	const FFFZSFacilityReadModel SecondInstance = Model.Facilities[0];
	Model.Facilities.Add(SecondInstance);
	TestTrue(TEXT("Two installed instances satisfy two"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Model.Facilities[1].bNeedsRelocation = true;
	TestFalse(TEXT("Relocation requirement blocks"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Model.Facilities[1].bNeedsRelocation = false;
	Model.Facilities[1].FacilityDefinitionId = TEXT("Performance_Busking_StreetStage");
	TestFalse(TEXT("Busking does not substitute for Sunset"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	const FFF_RequiredFacility DuplicateRequirement = Definition->RequiredSetupFacilities[0];
	Definition->RequiredSetupFacilities.Add(DuplicateRequirement);
	TestFalse(TEXT("Duplicate configuration cannot be satisfied"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).Facilities[1].bDefinitionValid);
	Definition->RequiredSetupFacilities.SetNum(1);
	Facility->Spec.MaxInstancesPerWorld = 1;
	TestFalse(TEXT("Unachievable minimum is an authoring error"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).Facilities[0].bDefinitionValid);
	Facility->Spec.MaxInstancesPerWorld = 0;
	Definition->RequiredSetupFacilities[0].MinimumCount = 0;
	TestFalse(TEXT("Zero minimum must not silently bypass"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Definition->RequiredSetupFacilities[0].MinimumCount = 1;
	Definition->RequiredSetupFacilities[0].FacilityDefinition.Reset();
	TestFalse(TEXT("Empty asset reference blocks"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Definition->RequiredSetupFacilities.Reset();
	TestTrue(TEXT("Explicit empty list opts out"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	TestFalse(TEXT("Missing festival differs from empty list"), UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(nullptr, Model).bCanProceed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RequiredFacilitiesFlowTest,
	"FestaFiesta.Festival.RequiredFacilities.StageGateAndNotifications",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_RequiredFacilitiesFlowTest::RunTest(const FString& Parameters)
{
	auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Nav(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<bool> ZoneIds(Settings->bRestrictZoneIdsToProjectPresets, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game)) { TestWorld.ForwardErrorMessages(this); return false; }
	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GI = World->GetGameInstance();
	auto* Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
	auto* Stage = GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	auto* Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Requirements subsystem created"), Required) || !Zone || !Stage || !Progression) { return false; }
	Required->OnWorldBeginPlay(*World); // FTestWorldWrapper may not call BeginPlay.
	int32 Notifications = 0;
	const FDelegateHandle Handle = Required->OnRequiredFacilitiesChangedNative.AddLambda([&Notifications]() { ++Notifications; });
	ON_SCOPE_EXIT { Required->OnRequiredFacilitiesChangedNative.Remove(Handle); };

	auto* Source = UFF_FestivalSettings::Get()->DefaultFestival.LoadSynchronous();
	if (!TestNotNull(TEXT("Production festival loads"), Source)) { return false; }
	if (TestEqual(TEXT("Existing production asset inherits the stage and staff-facility requirements"),
		Source->RequiredSetupFacilities.Num(), 2))
	{
		TestEqual(TEXT("Production main information facility requires an assigned staff member"),
			Source->RequiredSetupFacilities[1].MinimumAssignedStaff, 1);
	}
	auto* Definition = DuplicateObject<UFF_FestivalDefinition>(Source, GI);
	auto* Facility = NewObject<UFFZSFacilityDefinition>(Definition);
	Facility->Spec = FFZSTest::MakeValidFacilitySpec();
	Facility->Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_RequiredFacilityAutomation");
	Facility->Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall.RequiredFacilityAutomation");
	auto* Costs = NewObject<UDataTable>(Definition);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost; Cost.InstallCost = 100;
	Costs->AddRow(Facility->Spec.InstallCostKey, Cost);
	Definition->FacilityCostTable = Costs;
	Definition->FestivalId = TEXT("RequiredFacilityAutomation");
	Definition->RequiredSetupFacilities.SetNum(1);
	Definition->RequiredSetupFacilities[0].FacilityDefinition = Facility;
	Definition->RequiredSetupFacilities[0].MinimumCount = 1;
	Progression->StartFestival(Definition);
	TestTrue(TEXT("Definition replacement emits refresh even before installations"), Notifications > 0);
	Stage->StartNewFestival();
	FText Reason;
	TestFalse(TEXT("Setup cannot advance with missing requirement"), Stage->CanTransitionTo(EFF_FestivalStage::PromotionPreview, Reason));
	TestFalse(TEXT("Blocking reason is user-readable"), Reason.IsEmpty());
	TestTrue(TEXT("Category badge is requested"), Required->HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory::Booth));
	TestFalse(TEXT("Other category remains clear"), Required->HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory::Toilet));
	Stage->RequestTransition(EFF_FestivalStage::Live);
	TestEqual(TEXT("Direct transition cannot bypass"), Stage->CurrentStage, EFF_FestivalStage::Setup);
	Stage->SkipCurrentStage();
	TestEqual(TEXT("Skip cannot bypass"), Stage->CurrentStage, EFF_FestivalStage::Setup);
	Stage->Tick(100000.f);
	TestEqual(TEXT("Timeout cannot bypass"), Stage->CurrentStage, EFF_FestivalStage::Setup);
	TestEqual(TEXT("Blocked timer never displays negative seconds"), Stage->RemainingTime, 0.f);

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* GameplayZone = World->SpawnActor<AFFZSGameplayZone>(AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	if (!GameplayZone || !GameplayZone->ApplyAcceptedZonePreset(ZoneId) || !Zone->RegisterZone(GameplayZone)) { AddError(TEXT("Test zone unavailable")); return false; }
	FFFZSGridPlacement Placement; Placement.OriginCell = FIntPoint(1000, 1000);
	const auto Install = UFFZSGridLibrary::MakeInstallCommand(Facility->Spec, ZoneId, Placement, Zone->GetWorldRevision());
	const int32 BeforeInstall = Notifications;
	const auto Installed = Zone->ExecuteCommand(Install);
	if (!TestTrue(TEXT("Valid facility install succeeds"), Installed.bSucceeded)) { AddError(Installed.Message.ToString()); return false; }
	TestTrue(TEXT("Install emits refresh"), Notifications > BeforeInstall);
	TestFalse(TEXT("Satisfied category badge clears"), Required->HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory::Booth));
	FFF_RequiredFacilityRow Row;
	TestTrue(TEXT("Card lookup resolves requirement"), Required->TryGetRequiredFacilityRow(Facility->Spec.FacilityDefinitionId, Row));
	TestEqual(TEXT("Card reports installed count"), Row.InstalledCount, 1);
	TestEqual(TEXT("Card reports no missing instances"), Row.MissingCount, 0);
	TestTrue(TEXT("Satisfied setup may advance"), Stage->CanTransitionTo(EFF_FestivalStage::PromotionPreview, Reason));
	// 셋업에는 제한시간이 없다. 조건을 채워도 시간으로는 안 넘어가고 개장(스킵)으로만 넘어간다.
	Stage->Tick(100000.f);
	TestEqual(TEXT("Setup has no time limit"), Stage->CurrentStage, EFF_FestivalStage::Setup);
	Stage->SkipCurrentStage();
	TestNotEqual(TEXT("Satisfied setup advances by skip"), Stage->CurrentStage, EFF_FestivalStage::Setup);

	// 현재 DaySequence는 Setup -> Live다. 철거는 다음 배치 가능 단계에서 검증한다.
	Stage->RequestTransition(EFF_FestivalStage::MorningCheck);
	const auto Snapshot = Zone->CaptureWorldSnapshot();
	// 세 필드를 다 채운다. ExecuteCommand 는 CommandId·TransactionId 중 하나라도 비면
	// DataInvalid("CommandId와 TransactionId가 필요합니다")로 거부한다.
	// Install 은 MakeInstallCommand 가 채워 주지만 Remove 용 헬퍼는 없어서 직접 조립해야 한다
	// (UFF_BuildModeSubsystem::… 및 경제 브리지 테스트와 같은 패턴).
	FFFZSCommand Remove;
	Remove.CommandId = FGuid::NewGuid();
	Remove.TransactionId = FGuid::NewGuid();
	Remove.ExpectedWorldRevision = Zone->GetWorldRevision();
	Remove.Type = EFFZSCommandType::Remove;
	Remove.FacilityInstanceId = Install.FacilityInstanceId;
	const int32 BeforeRemove = Notifications;
	const auto Removed = Zone->ExecuteCommand(Remove);
	if (!TestTrue(TEXT("Required facility can be removed during build phase"), Removed.bSucceeded)) { AddError(Removed.Message.ToString()); return false; }
	TestTrue(TEXT("Remove emits refresh"), Notifications > BeforeRemove);
	TestFalse(TEXT("Removing it blocks Live even after Setup"), Stage->CanTransitionTo(EFF_FestivalStage::Live, Reason));
	TestTrue(TEXT("Category badge returns after removal"), Required->HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory::Booth));
	TArray<FText> Errors;
	const int32 BeforeRestore = Notifications;
	TestTrue(TEXT("Snapshot restore succeeds"), Zone->RestoreWorldSnapshot(Snapshot, Errors));
	TestTrue(TEXT("Restore emits refresh"), Notifications > BeforeRestore);
	TestTrue(TEXT("Restored installation satisfies requirement"), Required->GetRequiredFacilitiesStatus().bCanProceed);

	Definition = DuplicateObject<UFF_FestivalDefinition>(Definition, GI);
	Definition->RequiredSetupFacilities.Reset();
	const int32 BeforeSwitch = Notifications;
	Progression->StartFestival(Definition);
	TestTrue(TEXT("Same-gold festival switch still refreshes"), Notifications > BeforeSwitch);
	TestTrue(TEXT("No-requirements festival is allowed"), Required->GetRequiredFacilitiesStatus().bCanProceed);
	return true;
}

// 스태프 배정 조건의 값 기반 규칙. 설치만으로는 풀리지 않고, 다른 시설 배정이나 모르는 상태로 대신할 수 없다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RequiredFacilitiesStaffRulesTest,
	"FestaFiesta.Festival.RequiredFacilities.StaffAssignmentRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_RequiredFacilitiesStaffRulesTest::RunTest(const FString& Parameters)
{
	auto* Definition = NewObject<UFF_FestivalDefinition>();
	auto* Info = NewObject<UFFZSFacilityDefinition>(Definition);
	Info->Spec.FacilityDefinitionId = TEXT("Info_GeneralInfo_RequiredStaffRules");
	Info->Spec.DisplayName = FText::FromString(TEXT("시험 안내소"));
	Info->Spec.Category = EFFZSFacilityCategory::Info;
	Info->Spec.StaffCapacity = 2;
	Info->Spec.MaxInstancesPerWorld = 0;
	Definition->RequiredSetupFacilities.Reset();
	FFF_RequiredFacility& Requirement = Definition->RequiredSetupFacilities.AddDefaulted_GetRef();
	Requirement.FacilityDefinition = Info;
	Requirement.MinimumCount = 1;
	Requirement.MinimumAssignedStaff = 1;

	FFFZSReadModel Model;
	FFF_RequiredFacilityStaffing StaffingState;
	StaffingState.bKnown = true;
	StaffingState.UnassignedStaffCount = 6;
	const auto Evaluate = [&]()
	{
		return UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model, StaffingState);
	};

	FFF_RequiredFacilitiesStatus Status = Evaluate();
	if (!TestEqual(TEXT("One requirement row"), Status.Facilities.Num(), 1)) { return false; }
	TestTrue(TEXT("Info facility with staff slots may require assignment"), Status.Facilities[0].bDefinitionValid);
	TestFalse(TEXT("Missing installation blocks"), Status.bCanProceed);
	TestEqual(TEXT("Row reports required staff even before installation"), Status.Facilities[0].RequiredStaffCount, 1);
	TestTrue(TEXT("Installation guidance already announces the assignment step"),
		Status.BlockingReason.ToString().Contains(TEXT("배정")));
	TestTrue(TEXT("Catalog installation legend stays while installation is missing"), Status.HasMissingInstallations());

	FFFZSFacilityReadModel& Installed = Model.Facilities.AddDefaulted_GetRef();
	Installed.FacilityInstanceId = FGuid::NewGuid();
	Installed.FacilityDefinitionId = Info->Spec.FacilityDefinitionId;
	Installed.Category = EFFZSFacilityCategory::Info;
	Installed.State = EFFZSFacilityState::Validated;
	const FGuid InstalledId = Installed.FacilityInstanceId;

	Status = Evaluate();
	TestFalse(TEXT("Installation alone does not satisfy an assignment requirement"), Status.bCanProceed);
	TestEqual(TEXT("Installation itself is satisfied"), Status.Facilities[0].MissingCount, 0);
	TestEqual(TEXT("One staff member is still missing"), Status.Facilities[0].MissingStaffCount, 1);
	TestFalse(TEXT("Installation legend closes when only assignment remains"), Status.HasMissingInstallations());
	TestTrue(TEXT("Guidance points to the assign button"), Status.BlockingReason.ToString().Contains(TEXT("+ 배정")));

	StaffingState.UnassignedStaffCount = 0;
	TestTrue(TEXT("Without waiting staff the guidance points to hiring instead of a disabled button"),
		Evaluate().BlockingReason.ToString().Contains(TEXT("`인력`")));
	StaffingState.UnassignedStaffCount = 5;

	StaffingState.AssignedStaffByFacility.Add(FGuid::NewGuid(), 2);
	TestFalse(TEXT("Staff assigned to another facility does not count"), Evaluate().bCanProceed);

	StaffingState.AssignedStaffByFacility.Add(InstalledId, 1);
	Status = Evaluate();
	TestTrue(TEXT("One assigned staff member satisfies the requirement"), Status.bCanProceed);
	TestEqual(TEXT("Assigned staff is reported on the row"), Status.Facilities[0].AssignedStaffCount, 1);
	TestTrue(TEXT("Row is fully satisfied"), Status.Facilities[0].IsSatisfied());
	TestTrue(TEXT("Satisfied requirement has no blocking reason"), Status.BlockingReason.IsEmpty());

	Model.Facilities[0].bNeedsRelocation = true;
	TestFalse(TEXT("Staff at a facility awaiting relocation does not count"), Evaluate().bCanProceed);
	Model.Facilities[0].bNeedsRelocation = false;

	TestFalse(TEXT("Unknown staffing never satisfies an assignment requirement"),
		UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Requirement.MinimumAssignedStaff = 0;
	TestTrue(TEXT("Install-only requirements ignore staffing"),
		UFF_RequiredFacilitiesSubsystem::EvaluateRequirements(Definition, Model).bCanProceed);
	Requirement.MinimumAssignedStaff = 1;

	Info->Spec.Category = EFFZSFacilityCategory::Booth;
	TestFalse(TEXT("A facility without staff slots cannot require assignment"), Evaluate().Facilities[0].bDefinitionValid);
	Info->Spec.Category = EFFZSFacilityCategory::Info;
	Info->Spec.StaffCapacity = 0;
	TestFalse(TEXT("Zero staff capacity cannot require assignment"), Evaluate().Facilities[0].bDefinitionValid);
	TArray<FText> Errors;
	Definition->IsDefinitionValid(Errors);
	TestTrue(TEXT("Authoring validation explains the staff capacity error"),
		Errors.ContainsByPredicate([](const FText& Error) { return Error.ToString().Contains(TEXT("Staff Capacity")); }));
	Info->Spec.StaffCapacity = 2;
	Info->Spec.MaxInstancesPerWorld = 1;
	Requirement.MinimumAssignedStaff = 3;
	TestFalse(TEXT("More staff than the installable capacity is an authoring error"), Evaluate().Facilities[0].bDefinitionValid);
	Requirement.MinimumAssignedStaff = 2;
	TestTrue(TEXT("Exactly the installable capacity is achievable"), Evaluate().Facilities[0].bDefinitionValid);
	return true;
}

// 실제 월드에서 설치 → 배정 → 해제가 필수 조건과 단계 게이트, 갱신 신호에 그대로 반영되는지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RequiredFacilitiesStaffFlowTest,
	"FestaFiesta.Festival.RequiredFacilities.StaffAssignmentFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_RequiredFacilitiesStaffFlowTest::RunTest(const FString& Parameters)
{
	auto* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
	TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
	TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
	TGuardValue<bool> Nav(Settings->bEnableNavigationProjectionChecks, false);
	TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
	TGuardValue<bool> ZoneIds(Settings->bRestrictZoneIdsToProjectPresets, false);
	TGuardValue<EFFZSChangePhase> Phase(Settings->InitialPhase, EFFZSChangePhase::InitialSetup);
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game)) { TestWorld.ForwardErrorMessages(this); return false; }
	UWorld* World = TestWorld.GetTestWorld();
	UGameInstance* GI = World->GetGameInstance();
	auto* Zone = World->GetSubsystem<UFFZSWorldSubsystem>();
	auto* Required = World->GetSubsystem<UFF_RequiredFacilitiesSubsystem>();
	auto* StaffFacilities = World->GetSubsystem<UFF_StaffFacilitySubsystem>();
	auto* Stage = GI->GetSubsystem<UFF_FestivalStageSubsystem>();
	auto* Progression = GI->GetSubsystem<UFF_FestivalProgressionSubsystem>();
	if (!TestNotNull(TEXT("Requirements subsystem created"), Required)
		|| !TestNotNull(TEXT("Staff facility subsystem created"), StaffFacilities)
		|| !Zone || !Stage || !Progression)
	{
		return false;
	}
	Required->OnWorldBeginPlay(*World); // FTestWorldWrapper may not call BeginPlay.
	int32 Notifications = 0;
	const FDelegateHandle Handle = Required->OnRequiredFacilitiesChangedNative.AddLambda([&Notifications]() { ++Notifications; });
	ON_SCOPE_EXIT { Required->OnRequiredFacilitiesChangedNative.Remove(Handle); };

	auto* Definition = NewObject<UFF_FestivalDefinition>(GI);
	auto* Info = NewObject<UFFZSFacilityDefinition>(Definition);
	Info->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Info);
	Info->Spec.FacilityDefinitionId = TEXT("Info_GeneralInfo_RequiredStaffAutomation");
	Info->Spec.InstallCostKey = TEXT("Cost.Info.GeneralInfo.RequiredStaffAutomation");
	Info->Spec.StaffCapacity = 2;
	auto* Costs = NewObject<UDataTable>(Definition);
	Costs->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow Cost; Cost.InstallCost = 100;
	Costs->AddRow(Info->Spec.InstallCostKey, Cost);
	Definition->FacilityCostTable = Costs;
	Definition->FacilityRevenueTable.Reset();
	Definition->FestivalId = TEXT("RequiredStaffAutomation");
	Definition->InitialGold = 10000;
	Definition->InitialStaffCount = 2;
	Definition->RequiredSetupFacilities.Reset();
	FFF_RequiredFacility& Requirement = Definition->RequiredSetupFacilities.AddDefaulted_GetRef();
	Requirement.FacilityDefinition = Info;
	Requirement.MinimumCount = 1;
	Requirement.MinimumAssignedStaff = 1;
	Progression->StartFestival(Definition);
	Stage->StartNewFestival();

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* GameplayZone = World->SpawnActor<AFFZSGameplayZone>(AFFZSGameplayZone::StaticClass(), FTransform::Identity, Spawn);
	const FName ZoneId(TEXT("GZ_DadaPocha"));
	if (!GameplayZone || !GameplayZone->ApplyAcceptedZonePreset(ZoneId) || !Zone->RegisterZone(GameplayZone)) { AddError(TEXT("Test zone unavailable")); return false; }
	FFFZSGridPlacement Placement; Placement.OriginCell = FIntPoint(1000, 1000);
	const auto Install = UFFZSGridLibrary::MakeInstallCommand(Info->Spec, ZoneId, Placement, Zone->GetWorldRevision());
	const auto Installed = Zone->ExecuteCommand(Install);
	if (!TestTrue(TEXT("Required Info facility installs"), Installed.bSucceeded)) { AddError(Installed.Message.ToString()); return false; }

	FFF_RequiredFacilitiesStatus Status = Required->GetRequiredFacilitiesStatus();
	if (!TestEqual(TEXT("One requirement row"), Status.Facilities.Num(), 1)) { return false; }
	TestEqual(TEXT("The installation is counted"), Status.Facilities[0].InstalledCount, 1);
	TestEqual(TEXT("Installation never assigns staff by itself"), Status.Facilities[0].AssignedStaffCount, 0);
	TestFalse(TEXT("An unassigned staff facility blocks the festival"), Status.bCanProceed);
	TestFalse(TEXT("Info tab exclamation clears once installed"),
		Required->HasMissingRequiredFacilitiesInCategory(EFFZSFacilityCategory::Info));
	FText Reason;
	TestFalse(TEXT("Setup cannot advance before assignment"), Stage->CanTransitionTo(EFF_FestivalStage::PromotionPreview, Reason));
	TestTrue(TEXT("The player is told how to assign staff"), Reason.ToString().Contains(TEXT("+ 배정")));
	TestTrue(TEXT("Guidance identifies the staff tab"), Reason.ToString().Contains(TEXT("스태프` 탭")));
	TestEqual(TEXT("The guidance targets the installed required facility"),
		Required->FindStaffAssignmentTarget(), Zone->FindFacility(Install.FacilityInstanceId));

	const int32 BeforeAssign = Notifications;
	if (!TestTrue(TEXT("Waiting staff can be assigned"), StaffFacilities->TryAssignAvailableStaff(Install.FacilityInstanceId))) { return false; }
	TestTrue(TEXT("Assignment emits a requirement refresh"), Notifications > BeforeAssign);
	Status = Required->GetRequiredFacilitiesStatus();
	TestTrue(TEXT("One assigned staff member satisfies the requirement"), Status.bCanProceed);
	TestEqual(TEXT("The assignment is reported"), Status.Facilities[0].AssignedStaffCount, 1);
	TestNull(TEXT("Satisfied staffing has no assignment shortcut target"), Required->FindStaffAssignmentTarget());
	TestTrue(TEXT("Setup may advance after assignment"), Stage->CanTransitionTo(EFF_FestivalStage::PromotionPreview, Reason));

	const int32 BeforeUnassign = Notifications;
	TestTrue(TEXT("The staff member can be released"), StaffFacilities->TryUnassignAvailableStaff(Install.FacilityInstanceId));
	TestTrue(TEXT("Unassignment emits a requirement refresh"), Notifications > BeforeUnassign);
	TestFalse(TEXT("Releasing the only staff member blocks again"), Required->GetRequiredFacilitiesStatus().bCanProceed);
	TestEqual(TEXT("Unassignment makes the shortcut available again"),
		Required->FindStaffAssignmentTarget(), Zone->FindFacility(Install.FacilityInstanceId));
	FFFZSCommand Remove;
	Remove.CommandId = FGuid::NewGuid();
	Remove.TransactionId = FGuid::NewGuid();
	Remove.Type = EFFZSCommandType::Remove;
	Remove.FacilityInstanceId = Install.FacilityInstanceId;
	Remove.ExpectedWorldRevision = Zone->GetWorldRevision();
	TestTrue(TEXT("Required facility can be removed"), Zone->ExecuteCommand(Remove).bSucceeded);
	TestNull(TEXT("Removed facilities cannot remain shortcut targets"), Required->FindStaffAssignmentTarget());
	return true;
}
#endif
