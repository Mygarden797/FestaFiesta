// FF_ZoneSystemIntegrationTests.cpp

// Festival Stage 매핑과 Zone-to-Queue 투영·Party façade 수명주기 계약을 자동화 테스트로 고정한다.
// 흐름: 순수 변환 뒤 설치·이동·참가·복원 거부·차단·폐장 취소를 테스트 World의 실제 subsystem으로 실행한다.
// 불변식: Queue ID와 권위 경계는 유지되고 잘못된 입력·복원은 부분 결과를 남기지 않는다.
// 실패/성능: 변경한 개발자 설정은 범위 종료 시 원래 값으로 반드시 복원한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"
#include "Zone/FF_ZoneSystemStageBridgeSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Queue/FF_QueueSubsystem.h"
#include "NPC/Party/FF_ProgramReservationSubsystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "Time/FF_FestivalStageSubsystem.h"

namespace
{
	// 이동·투영 로직만 검증하도록 환경 의존 검사를 잠시 끄고 테스트 종료 때 원래 설정을 복원한다.
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

	// 설치 helper와 같은 revision 계약을 사용하되 기존 GUID를 보존하는 Move 명령을 만든다.
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

	// Game World에서는 프로젝트 Economy 정책도 함께 동작하므로 플러그인 범용 fixture의
	// 레거시 가격 키 대신 실제 DT_FacilityCosts에 존재하는 프로젝트 행을 사용한다.
	FFFZSFacilitySpec MakeZoneIntegrationFoodTruckSpec()
	{
		FFFZSFacilitySpec Spec =
			FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
		Spec.InstallCostKey = TEXT("Cost.Booth.FoodStall.DadaPochaFoodTruck");
		return Spec;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemStageMappingTest,
	"FestaFiesta.ZoneIntegration.Stage.Mapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemStageMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// 대표 Stage의 권한 축약과 실제 Live 진입·이탈 edge를 각각 고정해 두 변환 계약을 분리해 검증한다.
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
	// Phase 값이 같은 PromotionPreview→Live도 운영 edge이므로 별도의 개장 명령이 필요하다.
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
	// Arrange: 실제 WorldSubsystem들이 서로 delegate를 주고받는 Game World를 사용한다.
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
	const FFFZSFacilitySpec FacilitySpec = MakeZoneIntegrationFoodTruckSpec();
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec, ZoneId, InitialPlacement, ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	const FString InstallDiagnostic = FString::Printf(
		TEXT("Install commits before the move (FailureReason=%d, Message=%s)"),
		static_cast<int32>(InstallResult.FailureReason),
		*InstallResult.Message.ToString());
	if (!TestTrue(*InstallDiagnostic, InstallResult.bSucceeded))
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

	// Act: 90도 회전을 포함한 Move가 Zone 이벤트를 통해 기존 Queue 등록을 갱신하게 한다.
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
	// Assert: 외부 참조용 ID는 안정적이고 위치 의존 geometry만 새 시설 Transform을 따라야 한다.
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
	FFFZoneSystemQueueFacadeLifecycleE2ETest,
	"FestaFiesta.ZoneIntegration.Queue.FacadeLifecycleE2E",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueFacadeLifecycleE2ETest::RunTest(const FString& Parameters)
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
	UFF_FestivalStageSubsystem* FestivalStage = World && World->GetGameInstance()
		? World->GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>()
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
	if (!TestNotNull(TEXT("Game test world is created"), World)
		|| !TestNotNull(TEXT("The authoritative Zone subsystem exists"), ZoneSystem)
		|| !TestNotNull(TEXT("The Festival Stage bridge exists"), StageBridge)
		|| !TestNotNull(TEXT("The Zone-to-Queue bridge exists"), QueueBridge)
		|| !TestNotNull(TEXT("The project Queue subsystem exists"), QueueSystem)
		|| !TestNotNull(TEXT("The Festival Stage subsystem exists"), FestivalStage))
	{
		return false;
	}
	// 격리 World는 GameInstanceSubsystem을 WorldSubsystem 뒤에 연결할 수 있으므로, 최초 동기화로
	// Stage delegate 결합까지 완료한 뒤 실제 transition의 동기 전달 계약을 검증한다.
	if (!TestTrue(TEXT("The Stage bridge binds to the test GameInstance"),
		StageBridge->SynchronizeCurrentStage()))
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

	FFFZSGridPlacement Placement;
	Placement.OriginCell = FIntPoint(1000, 1000);
	const FFFZSFacilitySpec FacilitySpec = MakeZoneIntegrationFoodTruckSpec();
	const FFFZSCommand InstallCommand = UFFZSGridLibrary::MakeInstallCommand(
		FacilitySpec, ZoneId, Placement, ZoneSystem->GetWorldRevision());
	const FFFZSCommandResult InstallResult = ZoneSystem->ExecuteCommand(InstallCommand);
	const FString InstallDiagnostic = FString::Printf(
		TEXT("The test Facility installs (FailureReason=%d, Message=%s)"),
		static_cast<int32>(InstallResult.FailureReason),
		*InstallResult.Message.ToString());
	if (!TestTrue(*InstallDiagnostic, InstallResult.bSucceeded))
	{
		return false;
	}
	const FGuid FacilityId = InstallCommand.FacilityInstanceId;
	AFFZSFacilityActor* Facility = ZoneSystem->FindFacility(FacilityId);
	if (!TestNotNull(TEXT("The installed Facility is registered"), Facility)
		|| !TestEqual(TEXT("The Facility opens through the lifecycle bridge"),
			StageBridge->OpenEligibleFacilities(false).SucceededCount, 1))
	{
		return false;
	}

	FestivalStage->RequestTransition(EFF_FestivalStage::Live);
	if (!TestEqual(TEXT("The Stage bridge establishes the Live Zone phase"),
		ZoneSystem->GetChangePhase(), EFFZSChangePhase::LiveOperation))
	{
		return false;
	}
	const int32 QueueFacilityId = QueueBridge->FindQueueFacilityId(FacilityId);
	if (!TestTrue(TEXT("The bridge allocates a project Queue ID"),
		QueueFacilityId != INDEX_NONE)
		|| !TestTrue(TEXT("The projected Queue is registered"),
			QueueSystem->IsFacilityRegistered(QueueFacilityId)))
	{
		return false;
	}

	const FFFZSWorldSnapshot OccupiedSnapshot = ZoneSystem->CaptureWorldSnapshot();
	FFF_JoinRequest JoinRequest;
	JoinRequest.PartyId = 7001;
	JoinRequest.MemberCount = 1;
	// 예약 프로그램은 같은 줄 좌표를 읽지만 일반 서비스 Queue에는 들어가지 않는다.
	UFF_ProgramReservationSubsystem* Reservations = World->GetGameInstance()->GetSubsystem<UFF_ProgramReservationSubsystem>();
	const FGuid ProgramId = Reservations->RegisterProgram(FacilityId, 30.f, 60.f, 1);
	FTransform WaitingTransform;
	TestTrue(TEXT("Reservation reads authored waiting slot"),
		QueueBridge->TryGetProgramWaitingTransform(FacilityId, 0, WaitingTransform));
	TestTrue(TEXT("Reservation slot matches Zone geometry"), WaitingTransform.GetLocation().Equals(
		Facility->GetQueueComponent()->GetQueueSlotWorldLocation(0), 0.1));
	TestFalse(TEXT("Missing slot is not extended outside authored geometry"),
		QueueBridge->TryGetProgramWaitingTransform(FacilityId, MAX_int32, WaitingTransform));
	TestFalse(TEXT("Scheduled facility rejects ordinary Queue joining"),
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest).bAccepted);
	Reservations->ConfirmReservedParty(ProgramId, JoinRequest.PartyId, 1);
	TestFalse(TEXT("Reserved party also bypasses ordinary Queue service"),
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest).bAccepted);
	TestEqual(TEXT("Reading reservation slots never occupies ordinary Queue"),
		QueueBridge->GetZoneFacilityQueueState(FacilityId).OccupiedMemberCount, 0);
	Reservations->RemoveReservation(ProgramId);
	const FFF_QueueJoinResponse FirstJoin =
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest);
	if (!TestTrue(TEXT("The Queue façade accepts a Party for an active Zone Facility"),
		FirstJoin.bAccepted))
	{
		return false;
	}
	TestEqual(TEXT("The façade routes the Party into the projected Queue"),
		QueueBridge->GetZoneFacilityQueueState(FacilityId).OccupiedMemberCount, 1);

	TArray<FText> OccupiedRestoreErrors;
	const bool bOccupiedRestoreSucceeded =
		QueueBridge->RestoreZoneWorldSnapshotAndSynchronize(
			OccupiedSnapshot, OccupiedRestoreErrors);
	if (!TestFalse(TEXT("Snapshot restore rejects an occupied project Queue"),
		bOccupiedRestoreSucceeded))
	{
		return false;
	}
	TestFalse(TEXT("Occupied restore rejection includes a diagnostic"),
		OccupiedRestoreErrors.IsEmpty());
	TestEqual(TEXT("Rejected restore preserves the queued Party"),
		QueueBridge->GetZoneFacilityQueueState(FacilityId).OccupiedMemberCount, 1);

	const UFFZSFacilityQueueComponent* ZoneQueue = Facility->GetQueueComponent();
	if (!TestNotNull(TEXT("The active Facility owns an FFZS Queue"), ZoneQueue)
		|| !TestEqual(TEXT("Project Queue authority leaves the FFZS Queue open"),
			ZoneQueue->GetQueueState(), EFFZSQueueState::Open)
		|| !TestTrue(TEXT("Navigation/Safety can block the FFZS Queue"),
			ZoneSystem->SetFacilityQueueBlocked(FacilityId, true)))
	{
		return false;
	}

	JoinRequest.PartyId = 7002;
	const FFF_QueueJoinResponse BlockedJoin =
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest);
	TestFalse(TEXT("A blocked FFZS Queue rejects another façade Party"),
		BlockedJoin.bAccepted);
	TestEqual(TEXT("Blocked rejection is exposed as a closed Queue"),
		BlockedJoin.Result, EFF_QueueJoinResult::RejectedClosed);
	QueueBridge->Tick(0.0f);
	const FFF_QueueState FailedClosedState =
		QueueBridge->GetZoneFacilityQueueState(FacilityId);
	TestFalse(TEXT("Blocked authority fail-closes the project Queue"),
		FailedClosedState.bIsOpen);
	TestEqual(TEXT("Blocked authority cancels the occupied project Queue"),
		FailedClosedState.OccupiedMemberCount, 0);
	if (!TestTrue(TEXT("The Navigation/Safety block can be cleared"),
		ZoneSystem->SetFacilityQueueBlocked(FacilityId, false)))
	{
		return false;
	}

	JoinRequest.PartyId = 7003;
	const FFF_QueueJoinResponse ClosingPartyJoin =
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest);
	if (!TestTrue(TEXT("The façade reopens an unblocked active Queue on demand"),
		ClosingPartyJoin.bAccepted))
	{
		return false;
	}

	FestivalStage->RequestTransition(EFF_FestivalStage::Closing);
	if (!TestEqual(TEXT("The Stage bridge establishes the Closing Zone phase"),
		ZoneSystem->GetChangePhase(), EFFZSChangePhase::EveningSettlement))
	{
		return false;
	}
	const FFF_ZoneFacilityLifecycleBatchResult CloseResult =
		StageBridge->CloseActiveFacilities(false);
	TestEqual(TEXT("Closing closes the active Facility"), CloseResult.SucceededCount, 1);
	const FFF_QueueState ClosedState =
		QueueBridge->GetZoneFacilityQueueState(FacilityId);
	TestFalse(TEXT("Closing leaves the projected Queue closed"), ClosedState.bIsOpen);
	TestFalse(TEXT("Closing stops the projected Queue accepting Parties"),
		ClosedState.bAcceptingNew);
	TestEqual(TEXT("Non-Live Close cancels only the Facility's occupied Queue"),
		ClosedState.OccupiedMemberCount, 0);

	JoinRequest.PartyId = 7004;
	TestFalse(TEXT("A closed Zone Facility rejects a new façade Party"),
		QueueBridge->TryJoinZoneFacility(FacilityId, JoinRequest).bAccepted);
	const bool bFoundScopedCancellation = QueueSystem->GetQueueLogsRef().ContainsByPredicate(
		[QueueFacilityId](const FFF_QueueLog& Log)
		{
			return Log.FacilityId == QueueFacilityId
				&& Log.PartyId == 7003
				&& Log.ExitType == EFF_QueueExit::Cancelled;
		});
	TestTrue(TEXT("Closing records the scoped Party cancellation"),
		bFoundScopedCancellation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemQueueProjectionTest,
	"FestaFiesta.ZoneIntegration.Queue.Projection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemQueueProjectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	// Legacy 단일 선두 처리 모델로 환산해도 FFZS의 장기 Service 처리율은 보존해야 한다.
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

	// 꺾인 local polyline을 사용해 위치뿐 아니라 구간별 slot 방향도 Transform되는지 확인한다.
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
	// 네 슬롯에 필요한 길이보다 짧은 선은 부분 slot 배열을 노출하지 않고 전체 실패해야 한다.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFProgramFacilityWaitingAssetsTest,
	"FestaFiesta.ZoneIntegration.Queue.ProgramFacilityWaitingAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFProgramFacilityWaitingAssetsTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Booth/PDA_FFZS_Booth_CraftExperience_SandArt.PDA_FFZS_Booth_CraftExperience_SandArt"),
		TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Performance/PDA_FFZS_Performance_ExperienceStage_DanceBattle.PDA_FFZS_Performance_ExperienceStage_DanceBattle")
	};
	for (const TCHAR* Path : Paths)
	{
		const UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, Path);
		if (!TestNotNull(Path, Definition)) { continue; }
		FFF_QueueLaneGeometry Lane;
		TestTrue(TEXT("Program facility has usable authored Queue geometry"),
			UFF_ZoneSystemQueueBridgeSubsystem::BuildProjectQueueLaneGeometry(Definition->Spec, FTransform::Identity, Lane));
		TestTrue(TEXT("Default programs need at most three group waiting slots"), Lane.ExplicitSlotTransforms.Num() >= 3);
		AddInfo(FString::Printf(TEXT("%s: %d waiting slots, spacing %.0fcm"),
			*Definition->GetName(), Lane.ExplicitSlotTransforms.Num(), Lane.SlotSpacingCm));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest,
	"FestaFiesta.ZoneIntegration.Queue.SingleWaitingSlotDegenerateGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZoneSystemSingleWaitingSlotDegenerateGeometryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// 슬롯 하나는 이동 거리가 0이므로 퇴화 선도 허용하고 Service[0]으로 결정적 방향을 정한다.
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
