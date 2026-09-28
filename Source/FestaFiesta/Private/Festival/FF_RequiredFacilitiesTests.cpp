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
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_RequiredFacilitiesRulesTest,
	"FestaFiesta.Festival.RequiredFacilities.CountsAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FFF_RequiredFacilitiesRulesTest::RunTest(const FString& Parameters)
{
	auto* Definition = NewObject<UFF_FestivalDefinition>();
	TestEqual(TEXT("Default requires one facility type"), Definition->RequiredSetupFacilities.Num(), 1);
	const auto* Sunset = Definition->RequiredSetupFacilities[0].FacilityDefinition.LoadSynchronous();
	if (!TestNotNull(TEXT("Default Sunset asset loads"), Sunset)) { return false; }
	TestEqual(TEXT("Only exact Sunset required by default"), Sunset->Spec.FacilityDefinitionId,
		FName(TEXT("Performance_MainStage_SunsetMainStage")));

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
	TestEqual(TEXT("Existing production asset inherits Sunset requirement"), Source->RequiredSetupFacilities.Num(), 1);
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
#endif
