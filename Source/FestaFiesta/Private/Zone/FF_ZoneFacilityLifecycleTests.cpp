// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

namespace
{
	class FScopedLifecycleTestSettings
	{
	public:
		FScopedLifecycleTestSettings()
			: Settings(GetMutableDefault<UFFZSDeveloperSettings>())
			, bGroundChecks(Settings->bEnableGroundChecks)
			, bWorldCollisionChecks(Settings->bEnableWorldCollisionChecks)
			, bNavigationChecks(Settings->bEnableNavigationProjectionChecks)
			, bProductionPolicies(Settings->bRequireProductionPolicyDomains)
			, bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
			, InitialPhase(Settings->InitialPhase)
			, MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
		{
			Settings->bEnableGroundChecks = false;
			Settings->bEnableWorldCollisionChecks = false;
			Settings->bEnableNavigationProjectionChecks = false;
			Settings->bRequireProductionPolicyDomains = false;
			Settings->bRestrictZoneIdsToProjectPresets = false;
			Settings->InitialPhase = EFFZSChangePhase::InitialSetup;
			Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
		}

		~FScopedLifecycleTestSettings()
		{
			Settings->bEnableGroundChecks = bGroundChecks;
			Settings->bEnableWorldCollisionChecks = bWorldCollisionChecks;
			Settings->bEnableNavigationProjectionChecks = bNavigationChecks;
			Settings->bRequireProductionPolicyDomains = bProductionPolicies;
			Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
			Settings->InitialPhase = InitialPhase;
			Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
		}

	private:
		UFFZSDeveloperSettings* Settings;
		bool bGroundChecks;
		bool bWorldCollisionChecks;
		bool bNavigationChecks;
		bool bProductionPolicies;
		bool bRestrictZoneIds;
		EFFZSChangePhase InitialPhase;
		int32 MaxEnumeratedCells;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneFacilityLifecycleBatchTest,
	"FestaFiesta.ZoneIntegration.Stage.FacilityLifecycleBatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneFacilityLifecycleBatchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedLifecycleTestSettings SettingsGuard;
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game))
	{
		TestWorld.ForwardErrorMessages(this);
		return false;
	}

	UWorld* World = TestWorld.GetTestWorld();
	UFFZSWorldSubsystem* ZoneSystem = World
		? World->GetSubsystem<UFFZSWorldSubsystem>()
		: nullptr;
	UFF_ZoneSystemStageBridgeSubsystem* StageBridge = World
		? World->GetSubsystem<UFF_ZoneSystemStageBridgeSubsystem>()
		: nullptr;
	UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World
		? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>()
		: nullptr;
	UFF_QueueSubsystem* QueueSystem = World
		? World->GetSubsystem<UFF_QueueSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Game test world가 생성된다"), World)
		|| !TestNotNull(TEXT("Zone 권위 subsystem이 존재한다"), ZoneSystem)
		|| !TestNotNull(TEXT("Stage bridge가 존재한다"), StageBridge)
		|| !TestNotNull(TEXT("Queue bridge가 존재한다"), QueueBridge)
		|| !TestNotNull(TEXT("Project Queue subsystem이 존재한다"), QueueSystem))
	{
		return false;
	}

	const FName ZoneId(TEXT("GZ_DadaPocha"));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
	if (!TestNotNull(TEXT("Gameplay Zone을 생성한다"), Zone)
		|| !TestTrue(TEXT("Zone preset을 적용한다"), Zone->ApplyAcceptedZonePreset(ZoneId))
		|| !TestTrue(TEXT("Gameplay Zone을 등록한다"), ZoneSystem->RegisterZone(Zone)))
	{
		return false;
	}

	const FFFZSFacilitySpec FacilitySpec =
		FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	TArray<FGuid> FacilityIds;
	for (const FIntPoint Origin : {FIntPoint(1000, 1000), FIntPoint(5000, 1000)})
	{
		FFFZSGridPlacement Placement;
		Placement.OriginCell = Origin;
		const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
			FacilitySpec,
			ZoneId,
			Placement,
			ZoneSystem->GetWorldRevision());
		const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
		if (!TestTrue(TEXT("시설 설치가 성공한다"), InstallResult.bSucceeded))
		{
			return false;
		}
		FacilityIds.Add(InstallCommand.FacilityInstanceId);
	}

	for (const FGuid FacilityId : FacilityIds)
	{
		AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityId);
		const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(FacilityId);
		if (!TestNotNull(TEXT("설치된 시설이 등록된다"), Facility)
			|| !TestTrue(TEXT("Project Queue ID가 배정된다"), QueueFacilityId != INDEX_NONE))
		{
			return false;
		}
		TestEqual(TEXT("설치 직후 시설은 Validated다"),
			Facility->GetFacilityState(), EFFZSFacilityState::Validated);
		TestEqual(TEXT("설치 직후 FFZS Queue는 닫혀 있다"),
			Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
		TestTrue(TEXT("설치 직후 Project Queue는 등록돼 있다"),
			QueueSystem->IsFacilityRegistered(QueueFacilityId));
		TestFalse(TEXT("설치 직후 Project Queue는 닫혀 있다"),
			QueueSystem->GetQueueState(QueueFacilityId).bIsOpen);
	}

	const int64 RevisionBeforeOpen = ZoneSystem->GetWorldRevision();
	const FFF_ZoneFacilityLifecycleBatchResult OpenResult =
		StageBridge->OpenEligibleFacilities(false);
	TestEqual(TEXT("Validated 시설 두 개를 개장 대상으로 잡는다"), OpenResult.AttemptedCount, 2);
	TestEqual(TEXT("두 시설 개장이 성공한다"), OpenResult.SucceededCount, 2);
	TestEqual(TEXT("개장 실패가 없다"), OpenResult.FailedCount, 0);
	TestEqual(TEXT("성공 개장 수만큼 revision이 증가한다"),
		ZoneSystem->GetWorldRevision(), RevisionBeforeOpen + 2);

	for (const FGuid FacilityId : FacilityIds)
	{
		const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityId);
		const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(FacilityId);
		TestEqual(TEXT("개장된 시설은 Active다"),
			Facility->GetFacilityState(), EFFZSFacilityState::Active);
		TestEqual(TEXT("개장된 FFZS Queue는 Open이다"),
			Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Open);
		const FFF_QueueState ProjectState = QueueSystem->GetQueueState(QueueFacilityId);
		TestTrue(TEXT("개장된 Project Queue가 열린다"), ProjectState.bIsOpen);
		TestTrue(TEXT("개장된 Project Queue가 신규 참가를 받는다"),
			ProjectState.bAcceptingNew);
	}

	const int64 RevisionBeforeDuplicateOpen = ZoneSystem->GetWorldRevision();
	const FFF_ZoneFacilityLifecycleBatchResult DuplicateOpen =
		StageBridge->OpenEligibleFacilities(false);
	TestEqual(TEXT("이미 Active인 시설은 재개장하지 않는다"), DuplicateOpen.AttemptedCount, 0);
	TestEqual(TEXT("중복 개장이 revision을 바꾸지 않는다"),
		ZoneSystem->GetWorldRevision(), RevisionBeforeDuplicateOpen);

	const FFF_ZoneFacilityLifecycleBatchResult CloseResult =
		StageBridge->CloseActiveFacilities(false);
	TestEqual(TEXT("Active 시설 두 개를 폐쇄한다"), CloseResult.AttemptedCount, 2);
	TestEqual(TEXT("두 시설 정상 폐쇄가 성공한다"), CloseResult.SucceededCount, 2);
	for (const FGuid FacilityId : FacilityIds)
	{
		const AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityId);
		const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(FacilityId);
		TestEqual(TEXT("빈 시설 정상 폐쇄는 다음날 재개장 가능한 MovePending이다"),
			Facility->GetFacilityState(), EFFZSFacilityState::MovePending);
		TestEqual(TEXT("정상 폐쇄 후 FFZS Queue는 닫혀 있다"),
			Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
		TestFalse(TEXT("정상 폐쇄 후 Project Queue는 닫혀 있다"),
			QueueSystem->GetQueueState(QueueFacilityId).bIsOpen);
	}

	TestEqual(TEXT("MovePending 시설은 다음 개장 대상이다"),
		StageBridge->OpenEligibleFacilities(false).SucceededCount, 2);
	TestEqual(TEXT("두 Active 시설 긴급 폐쇄가 성공한다"),
		StageBridge->CloseActiveFacilities(true).SucceededCount, 2);
	for (const FGuid FacilityId : FacilityIds)
	{
		TestEqual(TEXT("긴급 폐쇄는 Closed 의도를 남긴다"),
			ZoneSystem->FindFacility(FacilityId)->GetFacilityState(),
			EFFZSFacilityState::Closed);
	}

	const FFF_ZoneFacilityLifecycleBatchResult SafeAutomaticEquivalent =
		StageBridge->OpenEligibleFacilities(false);
	TestEqual(TEXT("자동 개장 정책은 Closed를 보존한다"),
		SafeAutomaticEquivalent.AttemptedCount, 0);
	TestEqual(TEXT("수동 테스트 개장은 Closed도 명시적으로 다시 연다"),
		StageBridge->OpenEligibleFacilities(true).SucceededCount, 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

