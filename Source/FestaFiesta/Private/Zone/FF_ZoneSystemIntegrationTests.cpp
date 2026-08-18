// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
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

namespace
{
	class FScopedQueueMoveTestSettings
	{
	public:
		FScopedQueueMoveTestSettings()
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

		~FScopedQueueMoveTestSettings()
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

	FFFZSCommand MakeQueueMoveCommand(
		const FGuid FacilityInstanceId,
		const FFFZSFacilitySpec& FacilitySpec,
		const FName ZoneId,
		const FFFZSGridPlacement& Placement,
		const int64 ExpectedWorldRevision)
	{
		FFFZSCommand Command;
		Command.CommandId = FGuid::NewGuid();
		Command.TransactionId = FGuid::NewGuid();
		Command.ExpectedWorldRevision = ExpectedWorldRevision;
		Command.Type = EFFZSCommandType::Move;
		Command.FacilityInstanceId = FacilityInstanceId;
		Command.FacilitySpec = FacilitySpec;
		Command.ZoneId = ZoneId;
		Command.Placement = Placement;
		return Command;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemStageMappingTest,
	"FestaFiesta.ZoneIntegration.Stage.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemStageMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	using FStageBridge = UFF_ZoneSystemStageBridgeSubsystem;
	TestEqual(TEXT("Setup은 최초 배치 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Setup),
		EFFZSChangePhase::InitialSetup);
	TestEqual(TEXT("MorningCheck는 점검 배치 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::MorningCheck),
		EFFZSChangePhase::MorningCheck);
	TestEqual(TEXT("PromotionPreview는 배치를 잠근다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::PromotionPreview),
		EFFZSChangePhase::LiveOperation);
	TestEqual(TEXT("Live는 라이브 운영 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Live),
		EFFZSChangePhase::LiveOperation);
	TestEqual(TEXT("Closing은 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Closing),
		EFFZSChangePhase::EveningSettlement);
	TestEqual(TEXT("Report는 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Report),
		EFFZSChangePhase::EveningSettlement);
	TestEqual(TEXT("Finale는 정산 단계다"),
		FStageBridge::ToZoneChangePhase(EFF_FestivalStage::Finale),
		EFFZSChangePhase::EveningSettlement);

	EFFZSCommandType LifecycleCommand = EFFZSCommandType::Install;
	TestTrue(TEXT("PromotionPreview에서 Live 진입은 같은 FFZS phase여도 자동 개장 edge다"),
		FStageBridge::TryGetAutomaticLifecycleCommand(
			EFF_FestivalStage::PromotionPreview,
			EFF_FestivalStage::Live,
			LifecycleCommand));
	TestEqual(TEXT("Live 진입 edge는 Open을 만든다"),
		LifecycleCommand, EFFZSCommandType::Open);
	TestTrue(TEXT("Live에서 Closing 진입은 자동 폐쇄 edge다"),
		FStageBridge::TryGetAutomaticLifecycleCommand(
			EFF_FestivalStage::Live,
			EFF_FestivalStage::Closing,
			LifecycleCommand));
	TestEqual(TEXT("Live 이탈 edge는 Close를 만든다"),
		LifecycleCommand, EFFZSCommandType::Close);
	TestFalse(TEXT("중복 Live 요청은 자동 개폐를 다시 만들지 않는다"),
		FStageBridge::TryGetAutomaticLifecycleCommand(
			EFF_FestivalStage::Live,
			EFF_FestivalStage::Live,
			LifecycleCommand));
	TestFalse(TEXT("비 Live 단계 사이 전환은 자동 개폐 대상이 아니다"),
		FStageBridge::TryGetAutomaticLifecycleCommand(
			EFF_FestivalStage::Setup,
			EFF_FestivalStage::MorningCheck,
			LifecycleCommand));

	TestTrue(TEXT("Stage 브리지는 FFZS Policy 구현이다"),
		UFF_ZoneSystemStageBridgeSubsystem::StaticClass()->ImplementsInterface(
			UFFZSExternalPolicy::StaticClass()));
	TestTrue(TEXT("Queue 브리지는 FFZS Policy 구현이다"),
		UFF_ZoneSystemQueueBridgeSubsystem::StaticClass()->ImplementsInterface(
			UFFZSExternalPolicy::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueMoveRefreshesProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.MoveRefreshesProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueMoveRefreshesProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FScopedQueueMoveTestSettings SettingsGuard;
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
	UFF_QueueSubsystem* QueueSystem = World
		? World->GetSubsystem<UFF_QueueSubsystem>()
		: nullptr;
	UFF_ZoneSystemQueueBridgeSubsystem* QueueBridge = World
		? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Game test world is created"), World)
		|| !TestNotNull(TEXT("The authoritative Zone subsystem exists"), ZoneSystem)
		|| !TestNotNull(TEXT("The project Queue subsystem exists"), QueueSystem)
		|| !TestNotNull(TEXT("The Zone-to-Queue bridge exists"), QueueBridge))
	{
		return false;
	}

	const FName ZoneId(TEXT("GZ_DadaPocha"));
	FActorSpawnParameters ZoneSpawnParameters;
	ZoneSpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
		AFFZSGameplayZone::StaticClass(), FTransform::Identity, ZoneSpawnParameters);
	if (!TestNotNull(TEXT("A Gameplay Zone is spawned"), Zone)
		|| !TestTrue(TEXT("The accepted Zone preset is applied"),
			Zone->ApplyAcceptedZonePreset(ZoneId))
		|| !TestTrue(TEXT("The Gameplay Zone is registered"),
			ZoneSystem->RegisterZone(Zone)))
	{
		return false;
	}

	FFFZSGridPlacement InitialPlacement;
	InitialPlacement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSFacilitySpec FacilitySpec =
		FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec, ZoneId, InitialPlacement, ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	if (!TestTrue(TEXT("Install commits before the move"), InstallResult.bSucceeded))
	{
		return false;
	}

	AFFZSFacilityActor* Facility =
		ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId);
	if (!TestNotNull(TEXT("The installed Facility remains registered"), Facility)
		|| !TestNotNull(TEXT("The installed Facility owns a Zone queue component"),
			Facility ? Facility->GetQueueComponent() : nullptr))
	{
		return false;
	}

	const int32 QueueFacilityIdBefore =
		QueueBridge->FindQueueFacilityId(InstallCommand.FacilityInstanceId);
	if (!TestTrue(TEXT("Install allocates a project Queue ID"),
		QueueFacilityIdBefore != INDEX_NONE)
		|| !TestTrue(TEXT("Install registers the projected project Queue"),
			QueueSystem->IsFacilityRegistered(QueueFacilityIdBefore)))
	{
		return false;
	}
	const FVector ExitLocationBefore =
		QueueSystem->GetFacilityExitLocation(QueueFacilityIdBefore);

	FFFZSGridPlacement MovedPlacement;
	MovedPlacement.OriginCell = FIntPoint(5000, 1000);
	MovedPlacement.QuarterTurns = 1;
	const FFFZSCommand MoveCommand = MakeQueueMoveCommand(
		InstallCommand.FacilityInstanceId,
		FacilitySpec,
		ZoneId,
		MovedPlacement,
		ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult MoveResult = ZoneSystem->ExecuteCommand(MoveCommand);
	if (!TestTrue(TEXT("Move commits through the Zone authority"), MoveResult.bSucceeded))
	{
		return false;
	}

	const int32 QueueFacilityIdAfter =
		QueueBridge->FindQueueFacilityId(InstallCommand.FacilityInstanceId);
	TestEqual(TEXT("Move preserves the stable project Queue ID"),
		QueueFacilityIdAfter, QueueFacilityIdBefore);
	TestTrue(TEXT("Move leaves the refreshed project Queue registered"),
		QueueSystem->IsFacilityRegistered(QueueFacilityIdAfter));

	Facility = ZoneSystem->FindFacility(InstallCommand.FacilityInstanceId);
	if (!TestNotNull(TEXT("Move preserves the authoritative Facility instance"), Facility)
		|| !TestNotNull(TEXT("The moved Facility still owns a Zone queue component"),
			Facility ? Facility->GetQueueComponent() : nullptr))
	{
		return false;
	}
	const FVector ExpectedExitLocation =
		Facility->GetQueueComponent()->GetExitWorldLocation(0);
	const FVector ExitLocationAfter =
		QueueSystem->GetFacilityExitLocation(QueueFacilityIdAfter);
	TestFalse(TEXT("Move changes the projected Queue exit location"),
		ExitLocationAfter.Equals(ExitLocationBefore, 0.1));
	TestTrue(TEXT("The project Queue exit is refreshed from the moved Facility"),
		ExitLocationAfter.Equals(ExpectedExitLocation, 0.1));

	const FFF_QueueState QueueState =
		QueueSystem->GetQueueState(QueueFacilityIdAfter);
	TestFalse(TEXT("A moved Validated Facility keeps the project Queue closed"),
		QueueState.bIsOpen);
	TestFalse(TEXT("A moved Validated Facility does not accept new Parties"),
		QueueState.bAcceptingNew);
	TestEqual(TEXT("The closed project Queue remains empty after Move"),
		QueueState.OccupiedMemberCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFFZSQueueRules Rules;
	Rules.MaxWaitingAgents = 12;
	Rules.ServiceCapacity = 4;
	Rules.ServiceDurationSeconds = 8.0f;
	Rules.AbandonAfterSeconds = 30.0f;
	const FVector ExitLocation(100.0, 200.0, 300.0);
	const FFF_QueueConfig Config =
		UFF_ZoneSystemQueueBridgeSubsystem::ToProjectQueueConfig(Rules, ExitLocation);

	TestEqual(TEXT("물리 waiting 슬롯 수를 넘겨 배정하지 않는다"), Config.MaxCapacityMembers, 12);
	TestEqual(TEXT("Legacy 선두 Party 모델은 단일 처리 슬롯을 쓴다"), Config.ConcurrentServiceSlots, 1);
	TestEqual(TEXT("4개 창구 처리율을 2초/인원으로 환산한다"), Config.ServiceSecondsPerMember, 2.0f);
	TestEqual(TEXT("Legacy 호환 처리율은 ServiceCapacity / Duration을 보존한다"),
		static_cast<float>(Config.ConcurrentServiceSlots) / Config.ServiceSecondsPerMember,
		0.5f,
		0.001f);
	TestEqual(TEXT("Exit Anchor 위치를 전달한다"), Config.ExitLocation, ExitLocation);
	TestFalse(TEXT("도착 통지 연결 전에는 서비스 정지를 만들지 않는다"), Config.bRequireArrivalConfirmation);

	using FQueueBridge = UFF_ZoneSystemQueueBridgeSubsystem;
	TestEqual(TEXT("시설 포기 제한이 Party 제한보다 빠르면 시설 값을 쓴다"),
		FQueueBridge::ToProjectMaxWaitSeconds(60.0f, 30.0f), 30.0f);
	TestEqual(TEXT("Party 제한이 더 빠르면 Party 값을 보존한다"),
		FQueueBridge::ToProjectMaxWaitSeconds(10.0f, 30.0f), 10.0f);
	TestEqual(TEXT("무제한 Party도 시설 제한을 따른다"),
		FQueueBridge::ToProjectMaxWaitSeconds(-1.0f, 30.0f), 30.0f);
	TestEqual(TEXT("시설 제한 0은 자동 포기를 끈다"),
		FQueueBridge::ToProjectMaxWaitSeconds(600.0f, 0.0f), -1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueExplicitGeometryProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.ExplicitGeometryProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueExplicitGeometryProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 4;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = {
		FVector(0.0, 0.0, 0.0),
		FVector(100.0, 0.0, 0.0),
		FVector(100.0, 200.0, 0.0)
	};
	const FTransform FacilityTransform(
		FRotator(0.0f, 90.0f, 0.0f),
		FVector(1000.0, 2000.0, 50.0));

	FFF_QueueLaneGeometry Lane;
	TestTrue(TEXT("A valid frozen facility spec projects into the Party Queue lane"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FacilityTransform, Lane));
	TestEqual(TEXT("Every authored polyline point is preserved"),
		Lane.PolylinePoints.Num(), Spec.QueueSplinePointsCm.Num());
	TestEqual(TEXT("Exactly MaxWaitingAgents explicit slots are generated"),
		Lane.ExplicitSlotTransforms.Num(), Spec.QueueRules.MaxWaitingAgents);

	const TArray<FVector> ExpectedLocalSlots = {
		FVector(0.0, 0.0, 0.0),
		FVector(100.0, 0.0, 0.0),
		FVector(100.0, 100.0, 0.0),
		FVector(100.0, 200.0, 0.0)
	};
	for (int32 SlotIndex = 0;
		SlotIndex < FMath::Min(ExpectedLocalSlots.Num(), Lane.ExplicitSlotTransforms.Num());
		++SlotIndex)
	{
		TestTrue(*FString::Printf(TEXT("Slot %d follows the full transformed polyline"), SlotIndex),
			Lane.ExplicitSlotTransforms[SlotIndex].GetLocation().Equals(
				FacilityTransform.TransformPosition(ExpectedLocalSlots[SlotIndex]),
				UE_KINDA_SMALL_NUMBER));
	}
	if (Lane.ExplicitSlotTransforms.Num() == 4)
	{
		TestEqual(TEXT("The bend gives later slots a distinct world-space facing"),
			Lane.ExplicitSlotTransforms[2].Rotator().Yaw, 0.0, 0.01);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueShortGeometryFailsClosedTest,
	"FestaFiesta.ZoneIntegration.Queue.ShortGeometryFailsClosed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueShortGeometryFailsClosedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 4;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = { FVector::ZeroVector, FVector(200.0, 0.0, 0.0) };

	FFF_QueueLaneGeometry Lane;
	TestFalse(TEXT("A path too short for all physical waiting slots is rejected"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FTransform::Identity, Lane));
	TestTrue(TEXT("Failed projection leaves no partial explicit geometry"),
		Lane.ExplicitSlotTransforms.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest,
	"FestaFiesta.ZoneIntegration.Queue.SingleWaitingSlotDegenerateGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.QueueRules.MaxWaitingAgents = 1;
	Spec.QueueRules.SlotSpacingCm = 100.0f;
	Spec.QueueSplinePointsCm = { FVector(25.0, 50.0, 0.0), FVector(25.0, 50.0, 0.0) };

	FFFZSAnchorDefinition Service;
	Service.Type = EFFZSAnchorType::Service;
	Service.Index = 0;
	Service.LocalTransform.SetLocation(FVector(25.0, 0.0, 0.0));
	Spec.Anchors.Add(Service);

	FFF_QueueLaneGeometry Lane;
	TestTrue(TEXT("A valid one-slot queue does not require a non-zero spline tangent"),
		UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(
			Spec, FTransform::Identity, Lane));
	TestEqual(TEXT("Exactly one explicit waiting slot is projected"),
		Lane.ExplicitSlotTransforms.Num(), 1);
	if (Lane.ExplicitSlotTransforms.Num() == 1)
	{
		TestTrue(TEXT("The single slot preserves the authored position"),
			Lane.ExplicitSlotTransforms[0].GetLocation().Equals(
				Spec.QueueSplinePointsCm[0], UE_KINDA_SMALL_NUMBER));
		TestEqual(TEXT("Facing is derived deterministically from Service[0]"),
			Lane.ExplicitSlotTransforms[0].Rotator().Yaw, -90.0, 0.01);
	}
	return true;
}

#endif
