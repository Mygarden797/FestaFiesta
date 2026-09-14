//  FF_FFZSWorldTests.cpp

// WorldSubsystem의 권위 상태와 여러 Actor가 함께 만드는 통합 계약을 검증한다.
// 테스트 행렬:
 // 1) Command lifecycle, Queue 이동, Snapshot 복원과 동일 Command replay의 멱등성.
 // 2) Cell의 exactly-one Zone 소유, 회전 보호영역과 경계 접촉 판정.
 // 3) 잘못된 Facility Spec이 Nav/Policy 같은 비싼 단계 전에 중단되는가.
 // 4) 사용자 정의 Zone graph와 고유 Primary Asset Type의 다른 프로젝트 이식성.
 // 5) Schema 5의 자유 Yaw·1cm Region·CurrentPhase, Schema 3/4 마이그레이션과 복원 재진입 guard.
 // 6) Install·Move·Remove의 InitialSetup·MorningCheck 허용과 그 밖 Phase의 거부.
 // 7) Zone/Facility Read Model의 공간 메타데이터 값 복사 계약.
 // 8) 월드 지점의 등록 Facility 조회와 거리·InstanceId 기반 결정적 겹침 선택.
 // 9) 선택 Proxy의 크기와 전용 Trace/비물리 충돌 계약.
 
 //각 테스트는 transient Editor World를 사용하고 설정 CDO를 RAII로 복구한다. 실제 Dadaepo 맵,
 // Production Nav/Economy Adapter 또는 성능 수치를 검증하는 테스트는 아니다.


#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "Tests/AutomationEditorCommon.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSTestExternalPolicy.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    // 각 자동화 테스트가 끝난 뒤 Editor/Project 설정을 원래 상태로 유지한다.
    class FScopedZFTestSettings
    {
    public:
        FScopedZFTestSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bNavigation(Settings->bEnableNavigationProjectionChecks)
            , bProductionPolicies(Settings->bRequireProductionPolicyDomains)
            , bRestrictZoneIds(Settings->bRestrictZoneIdsToProjectPresets)
            , MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
            , FacilitySelectionPaddingCm(Settings->FacilitySelectionPaddingCm)
            , ProjectZoneIds(Settings->ProjectZoneIds)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
            Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
            Settings->FacilitySelectionPaddingCm =
                UFFZSDeveloperSettings::DefaultFacilitySelectionPaddingCm;
        }

        ~FScopedZFTestSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bProductionPolicies;
            Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
            Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
            Settings->FacilitySelectionPaddingCm = FacilitySelectionPaddingCm;
            Settings->ProjectZoneIds = ProjectZoneIds;
        }

    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround;
        bool bCollision;
        bool bNavigation;
        bool bProductionPolicies;
        bool bRestrictZoneIds;
        int32 MaxEnumeratedCells;
        float FacilitySelectionPaddingCm;
        TArray<FName> ProjectZoneIds;
    };

    AFFZSGameplayZone* SpawnAcceptedZone(UWorld& World, const FName ZoneId, const FTransform& Transform = FTransform::Identity)
    {
        FActorSpawnParameters Parameters;
        Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>(AFFZSGameplayZone::StaticClass(), Transform, Parameters);
        if (Zone)
        {
            Zone->ApplyAcceptedZonePreset(ZoneId);
        }
        return Zone;
    }

    FFFZSCommand MakeFacilityCommand(const EFFZSCommandType Type, const FGuid FacilityId, const int64 ExpectedRevision = -1)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.ExpectedWorldRevision = ExpectedRevision;
        Command.Type = Type;
        Command.FacilityInstanceId = FacilityId;
        return Command;
    }

    FFFZSCommand MakeMoveCommand(const FGuid FacilityId, const FName ZoneId, const FIntPoint OriginCell, const int64 ExpectedRevision)
    {
        FFFZSCommand Command = MakeFacilityCommand(EFFZSCommandType::Move, FacilityId, ExpectedRevision);
        Command.ZoneId = ZoneId;
        Command.Placement.OriginCell = OriginCell;
        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSWorldCommandLifecycleSnapshotTest,
    "FF_ZoneSystem.Integration.World.CommandLifecycleQueueAndSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Arrange→Act→Assert: 설치부터 Queue/Save/Replay까지 한 Facility의 전체 권위 lifecycle을 검증한다.
bool FFFZSWorldCommandLifecycleSnapshotTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("승인된 Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("승인된 ID가 지정되면 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSGridPlacement FirstPlacement;
    FirstPlacement.OriginCell = FIntPoint(1000, 1000); // q0: 기존 10m 위치를 1cm Cell로 보존한다.
    FFFZSCommand InstallFirst = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        FirstPlacement,
        0);

    const FFFZSCommandResult FirstInstallResult = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("Install이 Commit된다"), FirstInstallResult.bSucceeded);
    TestEqual(TEXT("Install은 World Revision을 한 번 증가시킨다"), Subsystem->GetWorldRevision(), int64(1));
    AFFZSFacilityActor* FirstFacility = Subsystem->FindFacility(InstallFirst.FacilityInstanceId);
    if (!TestNotNull(TEXT("설치된 Facility가 등록된다"), FirstFacility))
    {
        return false;
    }

    const FFFZSReadModel InstalledReadModel = Subsystem->BuildReadModel();
    const FFFZSFacilityReadModel* InstalledFacilityModel = InstalledReadModel.Facilities.FindByPredicate(
        [&InstallFirst](const FFFZSFacilityReadModel& Candidate)
        {
            return Candidate.FacilityInstanceId == InstallFirst.FacilityInstanceId;
        });
    if (!TestNotNull(TEXT("설치된 Facility가 Read Model에 포함된다"), InstalledFacilityModel))
    {
        return false;
    }
    TestTrue(TEXT("Facility Read Model은 Actor 대표 월드 위치(cm)를 복사한다"),
        InstalledFacilityModel->WorldLocation.Equals(FirstFacility->GetActorLocation()));
    TestTrue(TEXT("Facility Read Model 위치는 저장 Snapshot Transform의 위치와 같다"),
        InstalledFacilityModel->WorldLocation.Equals(
            FirstFacility->CaptureInstanceSnapshot().WorldTransform.GetLocation()));
    FTransform InstalledApproachWorldTransform;
    TestTrue(TEXT("유효한 Facility는 필수 Entry[0] 월드 접근점을 제공한다"),
        FirstFacility->GetAnchorWorldTransform(
            EFFZSAnchorType::Entry,
            0,
            InstalledApproachWorldTransform));
    TestTrue(TEXT("Facility Read Model은 접근점 유효성을 명시한다"),
        InstalledFacilityModel->bHasApproachWorldTransform);
    TestTrue(TEXT("Facility Read Model 접근점은 Actor의 Entry[0] 월드 Transform과 같다"),
        InstalledFacilityModel->ApproachWorldTransform.Equals(InstalledApproachWorldTransform));

    const FFFZSCommandResult ReplayResult = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("동일한 CommandId와 Payload가 성공적으로 Replay된다"), ReplayResult.bSucceeded && ReplayResult.bWasDuplicateReplay);
    TestEqual(TEXT("멱등 Replay는 Revision을 증가시키지 않는다"), Subsystem->GetWorldRevision(), int64(1));

    FFFZSCommand PayloadMismatch = InstallFirst;
    ++PayloadMismatch.Placement.OriginCell.X;
    const FFFZSCommandResult MismatchResult = Subsystem->ExecuteCommand(PayloadMismatch);
    TestEqual(TEXT("변경된 Payload로 CommandId를 재사용하면 거부된다"), MismatchResult.FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);

    FFFZSCommand OverlapInstall = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        FirstPlacement,
        1);
    const FFFZSCommandResult OverlapResult = Subsystem->ExecuteCommand(OverlapInstall);
    TestEqual(TEXT("권위 Grid는 겹치는 Footprint를 거부한다"), OverlapResult.FailureReason, EFFZSPlacementFailure::PlacementCollision);

    FFFZSGridPlacement StalePlacement;
    StalePlacement.OriginCell = FIntPoint(7000, 1000); // q0: 기존 70m, 10m 위치를 보존한다.
    const FFFZSCommand StaleInstall = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        StalePlacement,
        0);
    const FFFZSCommandResult StaleResult = Subsystem->ExecuteCommand(StaleInstall);
    TestEqual(TEXT("낙관적 동시성 검사는 오래된 Revision을 거부한다"), StaleResult.FailureReason, EFFZSPlacementFailure::StaleWorldRevision);

    const FFFZSCommand OpenFirst = MakeFacilityCommand(EFFZSCommandType::Open, InstallFirst.FacilityInstanceId, 1);
    TestTrue(TEXT("검증된 Facility가 Open된다"), Subsystem->ExecuteCommand(OpenFirst).bSucceeded);

    FFFZSGridPlacement SecondPlacement;
    SecondPlacement.OriginCell = FIntPoint(4000, 1000); // q0: 기존 40m, 10m 위치를 보존한다.
    const FFFZSCommand InstallSecond = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Toilet),
        ZoneId,
        SecondPlacement,
        2);
    const FFFZSCommandResult SecondInstallResult = Subsystem->ExecuteCommand(InstallSecond);
    TestTrue(TEXT("떨어져 있는 Facility가 설치된다"), SecondInstallResult.bSucceeded);
    AFFZSFacilityActor* SecondFacility = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (!TestNotNull(TEXT("두 번째 Facility가 등록된다"), SecondFacility))
    {
        return false;
    }

    const FFFZSCommand OpenSecond = MakeFacilityCommand(EFFZSCommandType::Open, InstallSecond.FacilityInstanceId, 3);
    TestTrue(TEXT("두 번째 Facility가 Open된다"), Subsystem->ExecuteCommand(OpenSecond).bSucceeded);
    TestTrue(TEXT("Navigation/Safety Adapter가 World 권위를 통해 Facility Queue를 Block할 수 있다"),
        Subsystem->SetFacilityQueueBlocked(InstallSecond.FacilityInstanceId, true));
    TestEqual(TEXT("Block된 Facade가 Queue 상태를 변경한다"),
        SecondFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Blocked);
    FGuid BlockedTicket;
    TestEqual(TEXT("Block된 Queue는 새 Agent를 거부한다"),
        Subsystem->JoinFacilityQueue(InstallSecond.FacilityInstanceId, TEXT("Agent.Blocked"), nullptr, BlockedTicket),
        EFFZSQueueJoinResult::NotOpen);
    TestTrue(TEXT("Navigation/Safety Adapter가 World 권위를 통해 Queue를 다시 Open할 수 있다"),
        Subsystem->SetFacilityQueueBlocked(InstallSecond.FacilityInstanceId, false));
    TestEqual(TEXT("Block이 해제된 Queue는 Open 상태로 돌아간다"),
        SecondFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Open);
    TestFalse(TEXT("알 수 없는 Facility는 Block할 수 없다"), Subsystem->SetFacilityQueueBlocked(FGuid::NewGuid(), true));

    const FName AgentId(TEXT("Agent.WorldFlow"));
    FGuid TicketId;
    TestEqual(TEXT("고정 ID는 World 권위를 통해 Join할 수 있다"),
        Subsystem->JoinFacilityQueue(InstallFirst.FacilityInstanceId, AgentId, nullptr, TicketId),
        EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("Queue Ticket이 할당된다"), TicketId.IsValid());
    TestTrue(TEXT("FIFO Slot에 도착한 Agent가 승인된다"),
        Subsystem->ReportQueueSlotArrival(AgentId, FirstFacility->GetQueueComponent()->GetQueueSlotWorldLocation(0)));
    TestEqual(TEXT("Queue Slot 도착 후에만 FIFO 선두가 Service Slot으로 이동한다"),
        FirstFacility->GetQueueComponent()->GetActiveServiceCount(), 1);
    TestTrue(TEXT("Service Anchor 도착 후에만 Service Timer가 시작된다"),
        Subsystem->ReportServicePointArrival(AgentId, FirstFacility->GetQueueComponent()->GetServiceWorldLocation(0)));

    FGuid DuplicateTicket;
    TestEqual(TEXT("하나의 Agent는 두 Facility의 Queue를 동시에 소유할 수 없다"),
        Subsystem->JoinFacilityQueue(InstallSecond.FacilityInstanceId, AgentId, nullptr, DuplicateTicket),
        EFFZSQueueJoinResult::DuplicateAgent);

    const FFFZSCommand OccupiedMove = MakeMoveCommand(InstallFirst.FacilityInstanceId, ZoneId, FIntPoint(1000, 4000), 4);
    const FFFZSPlacementValidation OccupiedMovePreview = Subsystem->ValidatePlacement(OccupiedMove);
    TestTrue(TEXT("점유 중인 Facility의 이동 미리보기도 거부된다"),
        OccupiedMovePreview.Reasons.Contains(EFFZSPlacementFailure::FacilityOccupied));
    TestEqual(TEXT("점유 중인 Facility는 이동할 수 없다"),
        Subsystem->ExecuteCommand(OccupiedMove).FailureReason,
        EFFZSPlacementFailure::FacilityOccupied);

    const FFFZSCommand EmergencyCloseFirst = MakeFacilityCommand(EFFZSCommandType::EmergencyClose, InstallFirst.FacilityInstanceId, 4);
    TestTrue(TEXT("Emergency Close가 활성 Queue를 취소한다"), Subsystem->ExecuteCommand(EmergencyCloseFirst).bSucceeded);
    TestFalse(TEXT("Emergency Close가 전역 Agent 소유권을 해제한다"), Subsystem->FindAgentQueueFacility(AgentId).IsValid());

    const FFFZSCommand CloseSecond = MakeFacilityCommand(EFFZSCommandType::Close, InstallSecond.FacilityInstanceId, 5);
    TestTrue(TEXT("Normal Close가 성공한다"), Subsystem->ExecuteCommand(CloseSecond).bSucceeded);
    TestEqual(TEXT("비어 있는 Closed Facility는 MovePending 상태가 된다"), SecondFacility->GetFacilityState(), EFFZSFacilityState::MovePending);

    FTransform SecondApproachBeforeMove;
    TestTrue(TEXT("이동 전 Facility의 Entry[0] 접근점을 조회할 수 있다"),
        SecondFacility->GetAnchorWorldTransform(
            EFFZSAnchorType::Entry,
            0,
            SecondApproachBeforeMove));
    const FFFZSCommand MoveSecond = MakeMoveCommand(InstallSecond.FacilityInstanceId, ZoneId, FIntPoint(4000, 4000), 6);
    const FFFZSCommandResult MoveResult = Subsystem->ExecuteCommand(MoveSecond);
    TestTrue(TEXT("Drain이 끝난 Facility가 트랜잭션으로 이동한다"), MoveResult.bSucceeded);
    SecondFacility = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (!TestNotNull(TEXT("이동된 Facility는 계속 등록되어 있다"), SecondFacility))
    {
        return false;
    }
    FTransform SecondApproachAfterMove;
    TestTrue(TEXT("이동 후 Facility의 Entry[0] 접근점을 다시 조회할 수 있다"),
        SecondFacility->GetAnchorWorldTransform(
            EFFZSAnchorType::Entry,
            0,
            SecondApproachAfterMove));
    TestFalse(TEXT("시설 이동은 접근점 월드 위치를 갱신한다"),
        SecondApproachAfterMove.GetLocation().Equals(SecondApproachBeforeMove.GetLocation()));

    const FFFZSReadModel MovedReadModel = Subsystem->BuildReadModel();
    const FFFZSFacilityReadModel* MovedFacilityModel = MovedReadModel.Facilities.FindByPredicate(
        [&InstallSecond](const FFFZSFacilityReadModel& Candidate)
        {
            return Candidate.FacilityInstanceId == InstallSecond.FacilityInstanceId;
        });
    if (!TestNotNull(TEXT("이동된 Facility가 새 Read Model에 포함된다"), MovedFacilityModel))
    {
        return false;
    }
    TestTrue(TEXT("이동된 Facility Read Model은 접근점 유효성을 유지한다"),
        MovedFacilityModel->bHasApproachWorldTransform);
    TestTrue(TEXT("이동 뒤 새 Read Model은 갱신된 Entry[0] 월드 Transform을 제공한다"),
        MovedFacilityModel->ApproachWorldTransform.Equals(SecondApproachAfterMove));

    FVector RaisedLocation = SecondFacility->GetActorLocation();
    RaisedLocation.Z = 123.0;
    SecondFacility->SetActorLocation(RaisedLocation, false, nullptr, ETeleportType::TeleportPhysics);
    SecondFacility->SetNeedsRelocation(true);

    const FFFZSCommand ReopenFirst = MakeFacilityCommand(EFFZSCommandType::Open, InstallFirst.FacilityInstanceId, 7);
    TestTrue(TEXT("Closed Facility를 다시 Open할 수 있다"), Subsystem->ExecuteCommand(ReopenFirst).bSucceeded);
    const FName SavedAgentId(TEXT("Agent.SaveRestore"));
    FGuid SavedTicket;
    TestEqual(TEXT("Snapshot 생성 전에 Agent가 Join한다"),
        Subsystem->JoinFacilityQueue(InstallFirst.FacilityInstanceId, SavedAgentId, nullptr, SavedTicket),
        EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("저장된 Agent가 할당된 Queue Slot에 도착한다"),
        Subsystem->ReportQueueSlotArrival(SavedAgentId, FirstFacility->GetQueueComponent()->GetQueueSlotWorldLocation(0)));

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    TestEqual(TEXT("Snapshot Schema가 현재 버전이다"), Snapshot.SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion);
    TestEqual(TEXT("Snapshot이 두 Facility를 모두 캡처한다"), Snapshot.Facilities.Num(), 2);
    TestTrue(TEXT("Snapshot이 영속 Command Receipt를 보존한다"), Snapshot.CommandReceipts.Num() > 0);
    int32 StagedFacilitySpawnCount = 0;
    const FDelegateHandle DestroySecondStagedFacilityHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateLambda([&StagedFacilitySpawnCount](AActor* SpawnedActor)
        {
            if (Cast<AFFZSFacilityActor>(SpawnedActor) && ++StagedFacilitySpawnCount == 2)
            {
                SpawnedActor->Destroy();
            }
        }));
    TArray<FText> InjectedRestoreErrors;
    TestFalse(TEXT("준비 단계 중간의 Spawn 실패는 Snapshot 복원을 거부한다"),
        Subsystem->RestoreWorldSnapshot(Snapshot, InjectedRestoreErrors));
    World->RemoveOnActorSpawnedHandler(DestroySecondStagedFacilityHandle);

    TestEqual(TEXT("실패 주입이 두 번째 준비 Facility까지 도달했다"), StagedFacilitySpawnCount, 2);
    TestTrue(TEXT("준비 중 복원 실패가 오류를 보고한다"), !InjectedRestoreErrors.IsEmpty());
    TestTrue(TEXT("준비 실패 후 첫 번째 기존 Facility Actor가 보존된다"),
        Subsystem->FindFacility(InstallFirst.FacilityInstanceId) == FirstFacility && IsValid(FirstFacility));
    TestTrue(TEXT("준비 실패 후 두 번째 기존 Facility Actor가 보존된다"),
        Subsystem->FindFacility(InstallSecond.FacilityInstanceId) == SecondFacility && IsValid(SecondFacility));
    TestEqual(TEXT("준비 실패 후 World Revision이 보존된다"), Subsystem->GetWorldRevision(), int64(8));
    TestEqual(TEXT("준비 실패 후 전역 Queue 소유권이 보존된다"),
        Subsystem->FindAgentQueueFacility(SavedAgentId), InstallFirst.FacilityInstanceId);
    TestEqual(TEXT("준비 실패 후 영속 Command Receipt가 보존된다"),
        Subsystem->CaptureWorldSnapshot().CommandReceipts.Num(), Snapshot.CommandReceipts.Num());
    int32 LiveFacilityActorCount = 0;
    for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            ++LiveFacilityActorCount;
        }
    }
    TestEqual(TEXT("준비 실패 시 모든 임시 Facility Actor가 제거된다"), LiveFacilityActorCount, 2);

    FFFZSWorldSnapshot InvalidStateSnapshot = Snapshot;
    if (FFFZSFacilityInstanceSnapshot* SavedFirst = InvalidStateSnapshot.Facilities.FindByPredicate(
        [&InstallFirst](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == InstallFirst.FacilityInstanceId; }))
    {
        SavedFirst->State = EFFZSFacilityState::MovePending;
    }
    TArray<FText> InvalidSnapshotErrors;
    TestFalse(TEXT("Snapshot은 호환되지 않는 Facility/Queue 상태를 거부한다"),
        Subsystem->RestoreWorldSnapshot(InvalidStateSnapshot, InvalidSnapshotErrors));
    TestEqual(TEXT("거부된 Snapshot은 Revision을 변경하지 않는다"), Subsystem->GetWorldRevision(), int64(8));

    FFFZSWorldSnapshot InvalidAllowedZoneSnapshot = Snapshot;
    if (FFFZSFacilityInstanceSnapshot* SavedFirst = InvalidAllowedZoneSnapshot.Facilities.FindByPredicate(
        [&InstallFirst](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == InstallFirst.FacilityInstanceId; }))
    {
        SavedFirst->FacilitySpec.AllowedZoneIds = {TEXT("GZ_Operations")};
    }
    InvalidSnapshotErrors.Reset();
    TestFalse(TEXT("Snapshot은 Facility AllowedZoneIds를 강제한다"),
        Subsystem->RestoreWorldSnapshot(InvalidAllowedZoneSnapshot, InvalidSnapshotErrors));

    const FFFZSCommand RemoveSecond = MakeFacilityCommand(EFFZSCommandType::Remove, InstallSecond.FacilityInstanceId, 8);
    TestTrue(TEXT("Snapshot 생성 후 두 번째 Facility를 제거할 수 있다"), Subsystem->ExecuteCommand(RemoveSecond).bSucceeded);
    TestNull(TEXT("제거된 Facility는 더 이상 등록되어 있지 않다"), Subsystem->FindFacility(InstallSecond.FacilityInstanceId));
    const FFFZSCommandResult RemovedFacilityReplay = Subsystem->ExecuteCommand(InstallSecond);
    TestTrue(TEXT("완료된 Receipt는 해당 Facility가 제거된 뒤에도 Replay된다"), RemovedFacilityReplay.bSucceeded && RemovedFacilityReplay.bWasDuplicateReplay);
    TestNull(TEXT("Receipt Replay에서 제거된 Facility 참조는 null로 해석된다"), RemovedFacilityReplay.FacilityActor);

    TArray<FText> RestoreErrors;
    TestTrue(TEXT("유효한 World Snapshot이 원자적으로 복원된다"), Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors));
    TestTrue(TEXT("Snapshot 복원은 검증 오류를 내지 않는다"), RestoreErrors.IsEmpty());
    TestEqual(TEXT("Snapshot 복원이 권위 Revision을 복원한다"), Subsystem->GetWorldRevision(), int64(8));

    AFFZSFacilityActor* RestoredSecond = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (TestNotNull(TEXT("제거된 Facility가 Snapshot에서 다시 생성된다"), RestoredSecond))
    {
        TestTrue(TEXT("지면 보정된 Z 값이 정확히 보존된다"), FMath::IsNearlyEqual(RestoredSecond->GetActorLocation().Z, 123.0, 0.01));
        TestTrue(TEXT("GIS 재배치 Flag가 보존된다"), RestoredSecond->NeedsRelocation());
        const FFFZSFacilitySpec& RestoredSpec = RestoredSecond->GetFacilitySpec();
        const float PaddingCm = GetDefault<UFFZSDeveloperSettings>()->FacilitySelectionPaddingCm;
        const FVector ExpectedRestoredProxyExtent(
            RestoredSpec.FootprintSizeCells.X * UFFZSDeveloperSettings::DefaultPlacementCellSizeCm * 0.5f + PaddingCm,
            RestoredSpec.FootprintSizeCells.Y * UFFZSDeveloperSettings::DefaultPlacementCellSizeCm * 0.5f + PaddingCm,
            RestoredSpec.PlacementObstacleHeightCm * 0.5f);
        TestTrue(TEXT("Snapshot 복원도 저장된 footprint에서 선택 Proxy를 다시 파생한다"),
            RestoredSecond->SelectionProxy->GetUnscaledBoxExtent().Equals(
                ExpectedRestoredProxyExtent,
                KINDA_SMALL_NUMBER));
    }
    TestEqual(TEXT("Queue 소유권이 Snapshot에서 재구축된다"),
        Subsystem->FindAgentQueueFacility(SavedAgentId),
        InstallFirst.FacilityInstanceId);

    const FFFZSCommandResult RestoredReplay = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("Command 멱등성이 저장/복원 후에도 유지된다"), RestoredReplay.bSucceeded && RestoredReplay.bWasDuplicateReplay);
    TestEqual(TEXT("복원 후 Replay는 현재 Revision을 변경하지 않는다"), Subsystem->GetWorldRevision(), int64(8));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSWorldFacilityLookupAtWorldPointTest,
    "FF_ZoneSystem.Integration.World.FacilityLookupAtWorldPoint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSWorldFacilityLookupAtWorldPointTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    AFFZSFacilityActor* UninitializedFacility = World->SpawnActor<AFFZSFacilityActor>();
    if (!TestNotNull(TEXT("미초기화 시설 Actor"), UninitializedFacility) ||
        !TestNotNull(TEXT("미초기화 시설 선택 Proxy"),
            UninitializedFacility ? UninitializedFacility->SelectionProxy.Get() : nullptr))
    {
        return false;
    }
    TestEqual(TEXT("유효 Spec 전에는 선택 Proxy 충돌이 꺼져 있다"),
        UninitializedFacility->SelectionProxy->GetCollisionEnabled(),
        ECollisionEnabled::NoCollision);
    TestTrue(TEXT("유효 Spec 전에는 선택 Proxy 크기가 0이다"),
        UninitializedFacility->SelectionProxy->GetUnscaledBoxExtent().IsNearlyZero());
    UninitializedFacility->Destroy();

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("조회 테스트 Zone"), Zone) ||
        !TestTrue(TEXT("조회 테스트 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    const FGuid HigherFacilityId(0, 0, 0, 2);
    const FGuid LowerFacilityId(0, 0, 0, 1);
    FFFZSGridPlacement HigherPlacement;
    HigherPlacement.OriginCell = FIntPoint(1000, 1000);
    FFFZSCommand InstallHigher = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        HigherPlacement,
        0);
    InstallHigher.FacilityInstanceId = HigherFacilityId;
    if (!TestTrue(TEXT("높은 InstanceId Facility가 먼저 설치된다"),
        Subsystem->ExecuteCommand(InstallHigher).bSucceeded))
    {
        return false;
    }

    FFFZSGridPlacement LowerPlacement;
    LowerPlacement.OriginCell = FIntPoint(4000, 1000);
    FFFZSCommand InstallLower = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::Performance),
        ZoneId,
        LowerPlacement,
        1);
    InstallLower.FacilityInstanceId = LowerFacilityId;
    if (!TestTrue(TEXT("낮은 InstanceId Facility가 나중에 설치된다"),
        Subsystem->ExecuteCommand(InstallLower).bSucceeded))
    {
        return false;
    }

    AFFZSFacilityActor* HigherFacility = Subsystem->FindFacility(HigherFacilityId);
    AFFZSFacilityActor* LowerFacility = Subsystem->FindFacility(LowerFacilityId);
    if (!TestNotNull(TEXT("높은 InstanceId Facility"), HigherFacility) ||
        !TestNotNull(TEXT("낮은 InstanceId Facility"), LowerFacility))
    {
        return false;
    }

    UBoxComponent* SelectionProxy = HigherFacility->SelectionProxy;
    if (!TestNotNull(TEXT("시설 선택 Proxy"), SelectionProxy))
    {
        return false;
    }

    TestEqual(TEXT("메시가 없는 시설에는 플레이어 충돌이 없다"),
        HigherFacility->VisualMesh->GetCollisionEnabled(),
        ECollisionEnabled::NoCollision);
    TestEqual(TEXT("비주얼 메시의 충돌 ObjectType은 플레이어 전용 시설 채널이다"),
        HigherFacility->VisualMesh->GetCollisionObjectType(), FFZSCollision::FacilityPlayerObjectChannel);
    TestEqual(TEXT("비주얼 메시가 시설 선택 Trace를 가로채지 않는다"),
        HigherFacility->VisualMesh->GetCollisionResponseToChannel(FFZSCollision::FacilitySelectionTraceChannel), ECR_Ignore);
    TestFalse(TEXT("비주얼 메시가 Navigation에 영향을 주지 않는다"),
        HigherFacility->VisualMesh->CanEverAffectNavigation());
    TestEqual(TEXT("선택 Proxy는 QueryOnly다"),
        SelectionProxy->GetCollisionEnabled(),
        ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("선택 Proxy ObjectType은 WorldDynamic이다"),
        SelectionProxy->GetCollisionObjectType(),
        ECC_WorldDynamic);
    TestEqual(TEXT("공개 선택 채널 상수는 프로젝트 FacilitySelection 채널에 매핑된다"),
        UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(
            static_cast<int32>(FFZSCollision::FacilitySelectionTraceChannel)),
        FName(TEXT("FacilitySelection")));
    TestFalse(TEXT("선택 Proxy는 Overlap 이벤트를 만들지 않는다"),
        SelectionProxy->GetGenerateOverlapEvents());
    TestFalse(TEXT("선택 Proxy는 Navigation에 영향을 주지 않는다"),
        SelectionProxy->CanEverAffectNavigation());
    TestFalse(TEXT("선택 Proxy는 물리를 시뮬레이션하지 않는다"),
        SelectionProxy->IsSimulatingPhysics());
    TestFalse(TEXT("선택 Proxy는 중력을 사용하지 않는다"),
        SelectionProxy->IsGravityEnabled());
    TestEqual(TEXT("선택 표시 기본 상태는 숨김이다"),
        HigherFacility->GetSelectionIndicatorState(),
        EFFZSFacilitySelectionIndicatorState::Hidden);
    TestTrue(TEXT("선택 Proxy는 기본적으로 게임에서 숨겨진다"),
        SelectionProxy->bHiddenInGame);
    TestFalse(TEXT("선택 표시는 에디터 Actor 선택에 의존하지 않는다"),
        SelectionProxy->bDrawOnlyIfSelected);
    TestTrue(TEXT("선택 Proxy의 Component visible flag는 유지된다"),
        SelectionProxy->GetVisibleFlag());

    const FVector IndicatorExtentBefore = SelectionProxy->GetUnscaledBoxExtent();
    const FVector IndicatorLocationBefore = SelectionProxy->GetRelativeLocation();
    HigherFacility->SetSelectionIndicatorState(
        EFFZSFacilitySelectionIndicatorState::Hovered);
    TestEqual(TEXT("Hover는 선택 표시 상태로 기록된다"),
        HigherFacility->GetSelectionIndicatorState(),
        EFFZSFacilitySelectionIndicatorState::Hovered);
    TestFalse(TEXT("Hover된 실제 클릭 Box는 게임에 표시된다"),
        SelectionProxy->bHiddenInGame);
    TestEqual(TEXT("Live 선택 표시는 시설 정보 UI Accent 색을 쓴다"),
        SelectionProxy->ShapeColor,
        FColor(20, 242, 224));
    HigherFacility->SetSelectionIndicatorState(
        EFFZSFacilitySelectionIndicatorState::Selected);
    TestEqual(TEXT("정보 패널에서 선택된 시설은 Selected 표시가 우선한다"),
        HigherFacility->GetSelectionIndicatorState(),
        EFFZSFacilitySelectionIndicatorState::Selected);
    TestTrue(TEXT("표시 상태 변경은 선택 Box 크기를 바꾸지 않는다"),
        SelectionProxy->GetUnscaledBoxExtent().Equals(
            IndicatorExtentBefore,
            KINDA_SMALL_NUMBER));
    TestTrue(TEXT("표시 상태 변경은 선택 Box 위치를 바꾸지 않는다"),
        SelectionProxy->GetRelativeLocation().Equals(
            IndicatorLocationBefore,
            KINDA_SMALL_NUMBER));
    TestEqual(TEXT("표시 상태 변경은 QueryOnly 계약을 유지한다"),
        SelectionProxy->GetCollisionEnabled(),
        ECollisionEnabled::QueryOnly);
    HigherFacility->SetSelectionIndicatorState(
        EFFZSFacilitySelectionIndicatorState::Hidden);
    TestTrue(TEXT("표시 해제 시 실제 클릭 Box가 다시 숨겨진다"),
        SelectionProxy->bHiddenInGame);

    for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ECC_MAX); ++ChannelIndex)
    {
        const ECollisionChannel Channel = static_cast<ECollisionChannel>(ChannelIndex);
        const ECollisionResponse ExpectedResponse =
            Channel == FFZSCollision::FacilitySelectionTraceChannel ? ECR_Block : ECR_Ignore;
        TestEqual(
            *FString::Printf(TEXT("선택 Proxy 채널 %d 응답"), ChannelIndex),
            SelectionProxy->GetCollisionResponseToChannel(Channel),
            ExpectedResponse);
    }

    const FFFZSFacilitySpec& InstalledSpec = HigherFacility->GetFacilitySpec();
    const float CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const float SelectionPaddingCm = GetDefault<UFFZSDeveloperSettings>()->FacilitySelectionPaddingCm;
    constexpr float ExpectedSelectionPaddingPerSideCm = 75.0f;
    TestEqual(TEXT("프로젝트 선택 여백 정책은 XY 각 변 75cm다"),
        SelectionPaddingCm,
        ExpectedSelectionPaddingPerSideCm);
    const FVector ExpectedFootprintExtent(
        InstalledSpec.FootprintSizeCells.X * CellSizeCm * 0.5f,
        InstalledSpec.FootprintSizeCells.Y * CellSizeCm * 0.5f,
        InstalledSpec.PlacementObstacleHeightCm * 0.5f);
    const FVector ExpectedProxyExtent(
        ExpectedFootprintExtent.X + SelectionPaddingCm,
        ExpectedFootprintExtent.Y + SelectionPaddingCm,
        ExpectedFootprintExtent.Z);
    TestTrue(TEXT("Footprint 표현은 권위 시설 크기를 그대로 유지한다"),
        HigherFacility->FootprintBounds->GetUnscaledBoxExtent().Equals(
            ExpectedFootprintExtent,
            KINDA_SMALL_NUMBER));
    TestTrue(TEXT("선택 Proxy는 footprint와 XY 75cm 여백, 시설 높이를 반영한다"),
        SelectionProxy->GetUnscaledBoxExtent().Equals(ExpectedProxyExtent, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("선택 Proxy 바닥은 시설 원점에 맞는다"),
        SelectionProxy->GetRelativeLocation().Equals(
            FVector(0.0f, 0.0f, ExpectedProxyExtent.Z),
            KINDA_SMALL_NUMBER));

    const FFFZSFacilitySpec& LowerSpec = LowerFacility->GetFacilitySpec();
    const FVector ExpectedLowerProxyExtent(
        LowerSpec.FootprintSizeCells.X * CellSizeCm * 0.5f + SelectionPaddingCm,
        LowerSpec.FootprintSizeCells.Y * CellSizeCm * 0.5f + SelectionPaddingCm,
        LowerSpec.PlacementObstacleHeightCm * 0.5f);
    TestTrue(TEXT("다른 footprint 시설은 자체 크기로 선택 Proxy를 파생한다"),
        LowerFacility->SelectionProxy->GetUnscaledBoxExtent().Equals(
            ExpectedLowerProxyExtent,
            KINDA_SMALL_NUMBER));
    TestTrue(TEXT("90도 회전 시설의 선택 Proxy는 Actor 회전을 그대로 따른다"),
        LowerFacility->SelectionProxy->GetComponentQuat().Equals(
            LowerFacility->GetActorQuat(),
            KINDA_SMALL_NUMBER));

    const FVector PaddedTracePoint = SelectionProxy->GetComponentTransform().TransformPosition(
        FVector(ExpectedFootprintExtent.X + SelectionPaddingCm * 0.5f, 0.0f, 0.0f));
    FHitResult PaddedSelectionHit;
    TestTrue(TEXT("footprint 밖이지만 75cm 선택 여백 안인 지점은 Proxy가 맞는다"),
        World->LineTraceSingleByChannel(
            PaddedSelectionHit,
            PaddedTracePoint + FVector(0.0, 0.0, ExpectedProxyExtent.Z + 100.0),
            PaddedTracePoint - FVector(0.0, 0.0, ExpectedProxyExtent.Z + 100.0),
            FFZSCollision::FacilitySelectionTraceChannel));
    TestTrue(TEXT("선택 여백 Trace 결과는 해당 Facility다"),
        PaddedSelectionHit.GetActor() == HigherFacility);
    TestFalse(TEXT("선택 여백 지점은 권위 footprint 내부로 간주하지 않는다"),
        HigherFacility->IsWorldPointInsideFootprint(PaddedTracePoint));

    const FVector OutsideTracePoint = SelectionProxy->GetComponentTransform().TransformPosition(
        FVector(ExpectedProxyExtent.X + 5.0f, 0.0f, 0.0f));
    FHitResult OutsideSelectionHit;
    TestFalse(TEXT("75cm 선택 여백 밖의 지점은 Proxy가 맞지 않는다"),
        World->LineTraceSingleByChannel(
            OutsideSelectionHit,
            OutsideTracePoint + FVector(0.0, 0.0, ExpectedProxyExtent.Z + 100.0),
            OutsideTracePoint - FVector(0.0, 0.0, ExpectedProxyExtent.Z + 100.0),
            FFZSCollision::FacilitySelectionTraceChannel));

    const FVector ProxyCenter = SelectionProxy->GetComponentLocation();
    const FVector TraceStart = ProxyCenter + FVector(0.0, 0.0, ExpectedProxyExtent.Z + 100.0);

    AActor* VisibilityBlockerActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("표준 WorldStatic 차단 Actor"), VisibilityBlockerActor))
    {
        return false;
    }
    UBoxComponent* VisibilityBlocker = NewObject<UBoxComponent>(
        VisibilityBlockerActor,
        TEXT("FacilitySelectionConfigBlocker"));
    if (!TestNotNull(TEXT("표준 BlockAll Box"), VisibilityBlocker))
    {
        return false;
    }
    VisibilityBlockerActor->SetRootComponent(VisibilityBlocker);
    VisibilityBlockerActor->AddInstanceComponent(VisibilityBlocker);
    VisibilityBlocker->SetBoxExtent(FVector(50.0f, 50.0f, 10.0f));
    VisibilityBlocker->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    VisibilityBlocker->SetGenerateOverlapEvents(false);
    VisibilityBlocker->SetCanEverAffectNavigation(false);
    VisibilityBlocker->RegisterComponent();
    VisibilityBlockerActor->SetActorLocation(
        ProxyCenter + FVector(0.0, 0.0, ExpectedProxyExtent.Z + 50.0));

    TestEqual(TEXT("BlockAll Box는 WorldStatic이다"),
        VisibilityBlocker->GetCollisionObjectType(),
        ECC_WorldStatic);
    TestEqual(TEXT("기본 BlockAll은 FacilitySelection 채널을 무시한다"),
        VisibilityBlocker->GetCollisionResponseToChannel(
            FFZSCollision::FacilitySelectionTraceChannel),
        ECR_Ignore);
    TestEqual(TEXT("기본 BlockAll은 Visibility 채널을 막는다"),
        VisibilityBlocker->GetCollisionResponseToChannel(ECC_Visibility),
        ECR_Block);

    FHitResult SelectionHit;
    TestTrue(TEXT("선택 전용 Trace는 BlockAll을 통과해 Proxy를 맞힌다"),
        World->LineTraceSingleByChannel(
            SelectionHit,
            TraceStart,
            ProxyCenter,
            FFZSCollision::FacilitySelectionTraceChannel));
    TestTrue(TEXT("선택 전용 Trace 결과는 시설 Actor다"),
        SelectionHit.GetActor() == HigherFacility);
    TestTrue(TEXT("선택 전용 Trace 결과 컴포넌트는 Proxy다"),
        SelectionHit.GetComponent() == SelectionProxy);

    FHitResult VisibilityHit;
    TestTrue(TEXT("Visibility Trace는 선택 Proxy 앞 BlockAll에서 멈춘다"),
        World->LineTraceSingleByChannel(
            VisibilityHit,
            TraceStart,
            ProxyCenter,
            ECC_Visibility));
    TestTrue(TEXT("Visibility Trace 결과 Actor는 외부 차단 Actor다"),
        VisibilityHit.GetActor() == VisibilityBlockerActor);
    TestTrue(TEXT("Visibility Trace 결과 컴포넌트는 BlockAll Box다"),
        VisibilityHit.GetComponent() == VisibilityBlocker);
    VisibilityBlockerActor->Destroy();

    const FVector QueryPoint = HigherFacility->GetActorLocation();
    TestTrue(TEXT("ZoneId가 없으면 월드 위치에서 Zone을 해석해 Facility를 찾는다"),
        Subsystem->FindFacilityAtWorldPoint(QueryPoint) == HigherFacility);
    TestTrue(TEXT("명시한 ZoneId에서도 Facility를 찾는다"),
        Subsystem->FindFacilityAtWorldPoint(QueryPoint, ZoneId) == HigherFacility);
    TestNull(TEXT("다른 ZoneId는 Facility를 반환하지 않는다"),
        Subsystem->FindFacilityAtWorldPoint(QueryPoint, TEXT("GZ_SunsetLounge")));
    TestNull(TEXT("등록 Zone 안의 빈 지점은 Facility를 반환하지 않는다"),
        Subsystem->FindFacilityAtWorldPoint(FVector(9000.0, 9000.0, 0.0)));

    // 권위 배치는 겹침을 허용하지 않으므로, 조회의 방어적 결정 규칙만 검증하도록 표현 Actor를
    // 직접 옮긴다. FacilitiesById 등록과 Grid 점유는 유지되어 TMap 순회 순서와 무관한 결과를 확인한다.
    LowerFacility->SetActorLocation(QueryPoint + FVector(100.0, 0.0, 0.0));
    TestTrue(TEXT("겹치는 footprint에서는 더 가까운 Facility를 우선한다"),
        Subsystem->FindFacilityAtWorldPoint(QueryPoint, ZoneId) == HigherFacility);

    LowerFacility->SetActorLocation(QueryPoint);
    TestTrue(TEXT("거리까지 같으면 InstanceId 문자열 오름차순으로 결정한다"),
        Subsystem->FindFacilityAtWorldPoint(QueryPoint, ZoneId) == LowerFacility);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSWorldChangePhasePermissionsTest,
    "FF_ZoneSystem.Integration.World.ChangePhasePermissions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Phase 계약: 세 월드 변경 명령은 InitialSetup·MorningCheck에서만 성공하고 다른 운영 단계에서는 실패한다.
bool FFFZSWorldChangePhasePermissionsTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("승인된 Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("Phase 테스트 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    const auto MakeInstallAt = [ZoneId](const FIntPoint OriginCell)
    {
        FFFZSGridPlacement Placement;
        Placement.OriginCell = OriginCell;
        return UFFZSGridLibrary::MakeInstallCommand(
            FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
            ZoneId,
            Placement,
            -1);
    };

    TestEqual(TEXT("Subsystem은 InitialSetup에서 시작한다"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::InitialSetup);
    const FFFZSCommand InitialInstall = MakeInstallAt(FIntPoint(1000, 1000));
    TestTrue(TEXT("InitialSetup에서 Install이 허용된다"),
        Subsystem->ExecuteCommand(InitialInstall).bSucceeded);
    TestTrue(TEXT("InitialSetup에서 Move가 허용된다"),
        Subsystem->ExecuteCommand(
            MakeMoveCommand(InitialInstall.FacilityInstanceId, ZoneId, FIntPoint(1000, 4000), -1)).bSucceeded);
    TestTrue(TEXT("InitialSetup에서 Remove가 허용된다"),
        Subsystem->ExecuteCommand(
            MakeFacilityCommand(EFFZSCommandType::Remove, InitialInstall.FacilityInstanceId)).bSucceeded);

    TestTrue(TEXT("MorningCheck로 전환된다"),
        Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck));
    const FFFZSCommand MorningInstall = MakeInstallAt(FIntPoint(4000, 1000));
    TestTrue(TEXT("MorningCheck에서 Install 배치 미리검증이 허용된다"),
        Subsystem->ValidatePlacement(MorningInstall).bIsValid);
    TestTrue(TEXT("MorningCheck에서 Install이 허용된다"),
        Subsystem->ExecuteCommand(MorningInstall).bSucceeded);
    TestTrue(TEXT("MorningCheck에서 Move가 허용된다"),
        Subsystem->ExecuteCommand(
            MakeMoveCommand(MorningInstall.FacilityInstanceId, ZoneId, FIntPoint(4000, 4000), -1)).bSucceeded);

    TestTrue(TEXT("LiveOperation으로 전환된다"),
        Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation));
    const FFFZSCommand LiveInstall = MakeInstallAt(FIntPoint(7000, 1000));
    const FFFZSPlacementValidation LiveValidation = Subsystem->ValidatePlacement(LiveInstall);
    TestTrue(TEXT("LiveOperation에서 Install 배치 미리검증이 WrongStage로 거부된다"),
        LiveValidation.Reasons.Contains(EFFZSPlacementFailure::WrongStage));
    TestEqual(TEXT("LiveOperation에서 Install이 거부된다"),
        Subsystem->ExecuteCommand(LiveInstall).FailureReason, EFFZSPlacementFailure::WrongStage);
    TestEqual(TEXT("LiveOperation에서 Move가 거부된다"),
        Subsystem->ExecuteCommand(
            MakeMoveCommand(MorningInstall.FacilityInstanceId, ZoneId, FIntPoint(7000, 4000), -1)).FailureReason,
        EFFZSPlacementFailure::WrongStage);
    TestEqual(TEXT("LiveOperation에서 Remove가 거부된다"),
        Subsystem->ExecuteCommand(
            MakeFacilityCommand(EFFZSCommandType::Remove, MorningInstall.FacilityInstanceId)).FailureReason,
        EFFZSPlacementFailure::WrongStage);

    TestTrue(TEXT("EveningSettlement로 전환된다"),
        Subsystem->SetChangePhase(EFFZSChangePhase::EveningSettlement));
    const FFFZSCommand EveningInstall = MakeInstallAt(FIntPoint(7000, 7000));
    TestEqual(TEXT("EveningSettlement에서 Install이 거부된다"),
        Subsystem->ExecuteCommand(EveningInstall).FailureReason, EFFZSPlacementFailure::WrongStage);
    TestEqual(TEXT("EveningSettlement에서 Move가 거부된다"),
        Subsystem->ExecuteCommand(
            MakeMoveCommand(MorningInstall.FacilityInstanceId, ZoneId, FIntPoint(4000, 7000), -1)).FailureReason,
        EFFZSPlacementFailure::WrongStage);
    TestEqual(TEXT("EveningSettlement에서 Remove가 거부된다"),
        Subsystem->ExecuteCommand(
            MakeFacilityCommand(EFFZSCommandType::Remove, MorningInstall.FacilityInstanceId)).FailureReason,
        EFFZSPlacementFailure::WrongStage);

    TestTrue(TEXT("MorningCheck로 다시 전환된다"),
        Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck));
    TestTrue(TEXT("MorningCheck에서 Remove가 허용된다"),
        Subsystem->ExecuteCommand(
            MakeFacilityCommand(EFFZSCommandType::Remove, MorningInstall.FacilityInstanceId)).bSucceeded);
    TestFalse(TEXT("제거된 이전 Phase 값은 설정할 수 없다"),
        Subsystem->SetChangePhase(static_cast<EFFZSChangePhase>(1)));
    TestEqual(TEXT("잘못된 Phase 설정은 현재 MorningCheck를 보존한다"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSWorldZoneExclusivityProtectedAreaTest,
    "FF_ZoneSystem.Integration.World.ZoneExclusivityAndProtectedArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Geometry 계약: Cell은 정확히 한 Zone에 속해야 하며 회전 OBB와 접촉 경계를 일관되게 거절한다.
bool FFFZSWorldZoneExclusivityProtectedAreaTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    AFFZSGameplayZone* OverlappingZone = SpawnAcceptedZone(*World, TEXT("GZ_DadaYajang"));
    if (!TestNotNull(TEXT("기본 Zone"), Zone) || !TestNotNull(TEXT("겹치는 Zone"), OverlappingZone))
    {
        return false;
    }
    TestTrue(TEXT("기본 Zone이 등록된다"), Subsystem->RegisterZone(Zone));
    TestTrue(TEXT("서로 다른 ID를 가진 겹치는 Zone이 검증을 위해 등록된다"), Subsystem->RegisterZone(OverlappingZone));

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000); // q0: 기존 10m 물리 위치를 보존한다.
    const FFFZSFacilitySpec FoodTruck = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    const FFFZSCommand AmbiguousInstall = UFFZSGridLibrary::MakeInstallCommand(FoodTruck, ZoneId, Placement, 0);
    TestEqual(TEXT("모든 Reserved Sample은 정확히 하나의 Zone에 속해야 한다"),
        Subsystem->ExecuteCommand(AmbiguousInstall).FailureReason,
        EFFZSPlacementFailure::OutsideZone);

    const FFFZSPlacementValidation AmbiguousValidation = Subsystem->ValidatePlacement(AmbiguousInstall);
    if (!TestTrue(TEXT("모호성 검증도 압축된 Reserved Region을 보고한다"), !AmbiguousValidation.ReservedRects.IsEmpty()))
    {
        return false;
    }
    int32 MaxReservedCellExclusiveX = AmbiguousValidation.ReservedRects[0].Max.X;
    for (const FFFZSCellRect& Rect : AmbiguousValidation.ReservedRects)
    {
        MaxReservedCellExclusiveX = FMath::Max(MaxReservedCellExclusiveX, Rect.Max.X);
    }

    OverlappingZone->SetActorTransform(FTransform(
        FRotator::ZeroRotator,
        FVector(MaxReservedCellExclusiveX * UFFZSDeveloperSettings::DefaultPlacementCellSizeCm, 0.0, 0.0)));
    TestTrue(TEXT("Reserved Cell 경계에만 닿는 인접 Zone은 내부 겹침이 아니다"),
        Subsystem->ValidatePlacement(AmbiguousInstall).bIsValid);

    OverlappingZone->SetActorTransform(FTransform(FRotator(0.0, 90.0, 0.0), FVector(10000.0, 0.0, 0.0)));
    const FFFZSPlacementValidation RotatedOverlapValidation = Subsystem->ValidatePlacement(AmbiguousInstall);
    TestFalse(TEXT("직각 Yaw로 회전했지만 World Geometry가 일치하는 Zone은 여전히 모호하다"),
        RotatedOverlapValidation.bIsValid);
    TestTrue(TEXT("회전된 Zone의 모호성은 OutsideZone 실패 원인을 유지한다"),
        RotatedOverlapValidation.Reasons.Contains(EFFZSPlacementFailure::OutsideZone));

    Subsystem->UnregisterZone(OverlappingZone);
    OverlappingZone->Destroy();

    FActorSpawnParameters ProtectedParameters;
    ProtectedParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSProtectedArea* ProtectedArea = World->SpawnActor<AFFZSProtectedArea>(
        AFFZSProtectedArea::StaticClass(),
        FTransform(FRotator::ZeroRotator, FVector(1050.0, 1050.0, 100.0)),
        ProtectedParameters);
    if (!TestNotNull(TEXT("보호 구역"), ProtectedArea))
    {
        return false;
    }
    const UFFZSDeveloperSettings* ProtectedSettings = GetDefault<UFFZSDeveloperSettings>();
    ProtectedArea->RequiredClearWidthCm = 0.0f;
    ProtectedArea->AreaType = EFFZSProtectedAreaType::GeneralRoute;
    TestEqual(TEXT("General Route 폭 0은 Project Settings 값을 상속한다"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->GeneralRouteWidthCm);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::EmergencyServiceRoute;
    TestEqual(TEXT("Emergency Route 폭 0은 Project Settings 값을 상속한다"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->EmergencyServiceRouteWidthCm);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::AccessibilityRoute;
    TestEqual(TEXT("Accessibility Route 폭 0은 Project Settings 값을 상속한다"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->AccessibilityRouteWidthCm);
    ProtectedArea->RequiredClearWidthCm = 275.0f;
    TestEqual(TEXT("양수인 Actor별 보호 폭이 Project Settings 값을 덮어쓴다"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), 275.0f);
    ProtectedArea->ProtectedAreaId = TEXT("PA_TestNoBuild");
    ProtectedArea->LinkedZoneId = ZoneId;
    ProtectedArea->AreaType = EFFZSProtectedAreaType::NoBuild;
    ProtectedArea->RequiredClearWidthCm = 0.0f;
    ProtectedArea->AreaBounds->SetBoxExtent(FVector(40.0, 40.0, 100.0));
    Subsystem->RegisterProtectedArea(ProtectedArea);

    const FFFZSCommand ProtectedInstall = UFFZSGridLibrary::MakeInstallCommand(FoodTruck, ZoneId, Placement, 0);
    TestEqual(TEXT("건축 금지 Volume은 교차하는 Facility를 거부한다"),
        Subsystem->ExecuteCommand(ProtectedInstall).FailureReason,
        EFFZSPlacementFailure::ProtectedRouteBlocked);
    TestEqual(TEXT("거부된 Command는 World Revision을 증가시키지 않는다"), Subsystem->GetWorldRevision(), int64(0));

    Subsystem->UnregisterProtectedArea(ProtectedArea);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::GeneralRoute;
    ProtectedArea->RequiredClearWidthCm = 300.0f;
    ProtectedArea->AreaBounds->SetBoxExtent(FVector(40.0, 300.0, 100.0));
    ProtectedArea->SetActorRotation(FRotator(0.0, 45.0, 0.0));
    TArray<FText> ProtectedErrors;
    TestTrue(TEXT("Yaw 회전된 보호 구역 데이터는 계속 유효하다"), ProtectedArea->ValidateProtectedAreaData(ProtectedErrors));

    const FVector ProtectedCenter = ProtectedArea->GetActorLocation();
    const FVector LocalShortAxis = ProtectedArea->GetActorForwardVector();
    const FVector ExpandedPoint = ProtectedCenter + LocalShortAxis * 120.0;
    TestTrue(TEXT("필수 폭이 회전된 Local 짧은 축을 확장한다"),
        ProtectedArea->IntersectsWorldBox(FBox(ExpandedPoint - FVector(5.0, 5.0, 10.0), ExpandedPoint + FVector(5.0, 5.0, 10.0))));

    const FVector AabbOnlyPoint = ProtectedCenter + FVector(300.0, 0.0, 0.0);
    TestFalse(TEXT("회전된 OBB는 World AABB의 오탐을 피한다"),
        ProtectedArea->IntersectsWorldBox(FBox(AabbOnlyPoint - FVector(5.0, 5.0, 10.0), AabbOnlyPoint + FVector(5.0, 5.0, 10.0))));

    ProtectedArea->SetActorRotation(FRotator(5.0, 45.0, 0.0));
    ProtectedErrors.Reset();
    TestFalse(TEXT("보호 구역은 Pitch/Roll을 거부한다"), ProtectedArea->ValidateProtectedAreaData(ProtectedErrors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSInvalidFacilitySpecStopsBeforeNavigationTest,
    "FF_ZoneSystem.Integration.World.InvalidFacilitySpecStopsBeforeNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 비용 순서 계약: 잘못된 정적 데이터는 Nav와 외부 Policy 전에 fail-fast해야 한다.
bool FFFZSInvalidFacilitySpecStopsBeforeNavigationTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    GetMutableDefault<UFFZSDeveloperSettings>()->bEnableNavigationProjectionChecks = true;

    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("승인된 Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSFacilitySpec InvalidSpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    InvalidSpec.QueueSplinePointsCm.Reset();
    const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(InvalidSpec, ZoneId, FFFZSGridPlacement(), 0);
    const FFFZSPlacementValidation Validation = Subsystem->ValidatePlacement(Command);

    TestFalse(TEXT("유효하지 않은 Facility Spec이 거부된다"), Validation.bIsValid);
    TestEqual(TEXT("검증은 데이터 오류 하나에서 중단된다"), Validation.Reasons.Num(), 1);
    TestTrue(TEXT("실패 원인은 DataInvalid이다"), Validation.Reasons.Contains(EFFZSPlacementFailure::DataInvalid));
    TestTrue(TEXT("Footprint Cell은 계산되지 않는다"), Validation.FootprintCells.IsEmpty());
    TestTrue(TEXT("Queue Cell은 계산되지 않는다"), Validation.QueueCells.IsEmpty());
    TestTrue(TEXT("Reserved Cell은 계산되지 않는다"), Validation.ReservedCells.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSPortableZoneGraphAndPrimaryAssetTest,
    "FF_ZoneSystem.Integration.World.PortableZoneGraphAndPrimaryAssetType",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 이식성 계약: Festa 고정 ID 제한을 끈 사용자 graph와 고유 Primary Asset Type을 검증한다.
bool FFFZSPortableZoneGraphAndPrimaryAssetTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    Settings->bRestrictZoneIdsToProjectPresets = false;
    Settings->ProjectZoneIds = {TEXT("GZ_NotRequiredInPortableMode")};

    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("이식성 자동화 테스트 World"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("이식성 World Subsystem"), Subsystem))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSGameplayZone* CustomZone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
    if (!TestNotNull(TEXT("사용자 정의 이식성 Zone"), CustomZone))
    {
        return false;
    }
    CustomZone->ZoneId = TEXT("Custom_Main");
    TestTrue(TEXT("비제한 모드에서는 Festa가 아닌 Zone ID도 등록된다"), Subsystem->RegisterZone(CustomZone));
    TestFalse(TEXT("등록된 Zone은 Preset으로 Registry Key를 바꿀 수 없다"),
        CustomZone->ApplyAcceptedZonePreset(TEXT("GZ_Operations")));
    TestEqual(TEXT("거부된 Preset은 등록된 Zone ID를 유지한다"), CustomZone->ZoneId, FName(TEXT("Custom_Main")));
    TestTrue(TEXT("기존 Registry Key로 동일한 Zone이 조회된다"),
        Subsystem->FindZone(TEXT("Custom_Main")) == CustomZone);
    TestNull(TEXT("거부된 Preset은 오래된 교체 Key를 생성하지 않는다"),
        Subsystem->FindZone(TEXT("GZ_Operations")));

    TArray<FText> GraphErrors;
    TestTrue(TEXT("비제한 모드는 등록된 Graph만 검증한다"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestTrue(TEXT("이식 가능한 등록 Graph에는 Festa Zone 누락 오류가 없다"), GraphErrors.IsEmpty());

    Settings->bRestrictZoneIdsToProjectPresets = true;
    Settings->ProjectZoneIds = {TEXT("Custom_Main"), TEXT("Custom_Secondary")};
    GraphErrors.Reset();
    TestFalse(TEXT("제한 모드는 설정된 모든 Project Zone ID를 요구한다"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestEqual(TEXT("설정된 누락 Zone만 보고된다"), GraphErrors.Num(), 1);

    AFFZSGameplayZone* SecondaryZone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(20000.0, 0.0, 0.0)), SpawnParameters);
    if (!TestNotNull(TEXT("두 번째 설정 Zone"), SecondaryZone))
    {
        return false;
    }
    SecondaryZone->ZoneId = TEXT("Custom_Secondary");
    TestTrue(TEXT("제한 모드는 Project Zone ID 목록의 ID를 등록한다"), Subsystem->RegisterZone(SecondaryZone));
    GraphErrors.Reset();
    TestTrue(TEXT("설정된 이식 가능 Zone Graph는 Festa ID 없이도 검증된다"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestTrue(TEXT("설정된 이식 가능 Graph는 오류를 내지 않는다"), GraphErrors.IsEmpty());

    UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
    Definition->Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Definition->Spec.FacilityDefinitionId = TEXT("Facility.PortablePrimaryAsset");
    const FPrimaryAssetId PrimaryAssetId = Definition->GetPrimaryAssetId();
    TestEqual(TEXT("Facility Definition은 플러그인 고유 Primary Asset Type을 사용한다"),
        PrimaryAssetId.PrimaryAssetType.ToString(), FString(TEXT("FFZSFacilityDefinition")));
    TestEqual(TEXT("Facility Definition ID는 Primary Asset Name으로 유지된다"),
        PrimaryAssetId.PrimaryAssetName, Definition->Spec.FacilityDefinitionId);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSSchema5FreeYawSnapshotRestoreTest,
    "FF_ZoneSystem.Integration.World.Schema5FreeYawSnapshotRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 자유각 설치부터 schema 5 캡처와 Actor 교체 복원까지 권위 Yaw/Transform을 한 흐름으로 검증한다.
bool FFFZSSchema5FreeYawSnapshotRestoreTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자유 Yaw 자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("자유 Yaw 권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("자유 Yaw 대상 Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("자유 Yaw 대상 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(3000, 3000);
    Placement.YawCentidegrees = 3725;
    Placement.QuarterTurns = 0;
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        Placement,
        0);
    TestEqual(TEXT("Install 명령이 37.25도 Yaw를 정규형으로 유지한다"),
        Install.Placement.YawCentidegrees, 3725);
    TestEqual(TEXT("Install 명령에는 legacy QuarterTurns가 남지 않는다"),
        Install.Placement.QuarterTurns, 0);

    const FFFZSCommandResult InstallResult = Subsystem->ExecuteCommand(Install);
    if (!TestTrue(TEXT("37.25도 Facility 설치가 Commit된다"), InstallResult.bSucceeded))
    {
        return false;
    }

    AFFZSFacilityActor* InstalledFacility =
        Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("37.25도 Facility Actor가 등록된다"), InstalledFacility))
    {
        return false;
    }
    TestEqual(TEXT("설치된 Actor가 37.25도 Placement를 보존한다"),
        InstalledFacility->GetGridPlacement().YawCentidegrees, 3725);
    const FQuat ExpectedWorldRotation = Zone->GetActorQuat() * FQuat(
        FVector::UpVector, FMath::DegreesToRadians(37.25));
    TestTrue(TEXT("설치된 Actor가 정확한 37.25도 World Yaw를 사용한다"),
        InstalledFacility->GetActorQuat().AngularDistance(ExpectedWorldRotation) <=
            FMath::DegreesToRadians(0.01));

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    if (!TestEqual(TEXT("자유 Yaw Capture는 현재 schema를 사용한다"), Snapshot.SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion) ||
        !TestEqual(TEXT("자유 Yaw Capture는 Facility 하나를 저장한다"), Snapshot.Facilities.Num(), 1))
    {
        return false;
    }
    const FFFZSFacilityInstanceSnapshot& SavedFacility = Snapshot.Facilities[0];
    TestEqual(TEXT("Capture가 37.25도 centidegree를 보존한다"),
        SavedFacility.Placement.YawCentidegrees, 3725);
    TestEqual(TEXT("Capture가 legacy QuarterTurns를 비운다"),
        SavedFacility.Placement.QuarterTurns, 0);
    TestTrue(TEXT("Capture Transform이 설치된 Actor Transform과 같다"),
        SavedFacility.WorldTransform.Equals(InstalledFacility->GetActorTransform(), 0.001));

    InstalledFacility->AddActorWorldOffset(FVector(0.0, 0.0, 123.0), false, nullptr,
        ETeleportType::TeleportPhysics);
    TestFalse(TEXT("복원 전 변형으로 저장 Transform과 현재 Actor를 구분한다"),
        InstalledFacility->GetActorTransform().Equals(SavedFacility.WorldTransform, 0.001));

    TArray<FText> RestoreErrors;
    if (!TestTrue(TEXT("schema 5 자유 Yaw Snapshot이 원자적으로 복원된다"),
        Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors)))
    {
        return false;
    }
    TestTrue(TEXT("schema 5 자유 Yaw 복원은 오류를 내지 않는다"), RestoreErrors.IsEmpty());

    AFFZSFacilityActor* RestoredFacility =
        Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("복원된 자유 Yaw Facility Actor"), RestoredFacility))
    {
        return false;
    }
    TestTrue(TEXT("원자적 Restore가 기존 Actor를 교체한다"), RestoredFacility != InstalledFacility);
    TestEqual(TEXT("복원된 Placement가 37.25도 centidegree를 보존한다"),
        RestoredFacility->GetGridPlacement().YawCentidegrees, 3725);
    TestEqual(TEXT("복원된 Placement는 legacy QuarterTurns를 비운다"),
        RestoredFacility->GetGridPlacement().QuarterTurns, 0);
    TestTrue(TEXT("복원된 Actor Transform이 Capture 값과 같다"),
        RestoredFacility->GetActorTransform().Equals(SavedFacility.WorldTransform, 0.001));
    TestTrue(TEXT("복원된 Actor가 정확한 37.25도 World Yaw를 유지한다"),
        RestoredFacility->GetActorQuat().AngularDistance(ExpectedWorldRotation) <=
            FMath::DegreesToRadians(0.01));
    TestEqual(TEXT("Restore가 저장된 WorldRevision을 보존한다"),
        Subsystem->GetWorldRevision(), Snapshot.WorldRevision);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSSnapshotPhaseReentrancyAndDrainIntentTest,
    "FF_ZoneSystem.Integration.World.SnapshotPhaseReentrancyAndDrainIntent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 복원 계약: Schema 5 자유 Yaw와 Schema 3/4 마이그레이션, Phase/Drain intent 및 재진입 차단을 함께 검증한다.
bool FFFZSSnapshotPhaseReentrancyAndDrainIntentTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("권위 World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("승인된 Gameplay Zone"), Zone))
    {
        return false;
    }

    // Schema 3 음수 Origin도 실제 복원 성공 경로에서 검증할 수 있도록 이 테스트 전용 Zone을
    // 원점 양쪽 100 m로 넓힌다. 제품 Preset이나 다른 테스트의 Zone 경계는 변경하지 않는다.
    USplineComponent* BoundarySpline = Zone->FindComponentByClass<USplineComponent>();
    if (!TestNotNull(TEXT("Gameplay Zone 경계 Spline"), BoundarySpline))
    {
        return false;
    }
    const FVector BoundaryPoints[] = {
        FVector(-10000.0, -10000.0, 0.0),
        FVector(10000.0, -10000.0, 0.0),
        FVector(10000.0, 10000.0, 0.0),
        FVector(-10000.0, 10000.0, 0.0)
    };
    for (int32 PointIndex = 0; PointIndex < UE_ARRAY_COUNT(BoundaryPoints); ++PointIndex)
    {
        BoundarySpline->SetLocationAtSplinePoint(
            PointIndex, BoundaryPoints[PointIndex], ESplineCoordinateSpace::Local, false);
    }
    BoundarySpline->UpdateSpline();
    if (!TestTrue(TEXT("Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    // 명령 Callback(Blueprint Policy Callback 포함)에서 시도한 복원은
    // 명령 Guard가 활성화된 동안 반드시 실패해야 하며 권위 상태를 변경해서는 안 된다.
    Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation);
    const FFFZSWorldSnapshot EmptyLiveSnapshot = Subsystem->CaptureWorldSnapshot();
    Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck);

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000); // q0: 기존 10m 물리 위치를 보존한다.
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck),
        ZoneId,
        Placement,
        0);

    bool bRestoreCallbackReached = false;
    bool bReentrantRestoreSucceeded = true;
    bool bReentrantRestorePreservedState = false;
    TArray<FText> ReentrantRestoreErrors;
    const FDelegateHandle RestoreDuringCommandHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateLambda(
            [Subsystem, &EmptyLiveSnapshot, &Install, &bRestoreCallbackReached,
             &bReentrantRestoreSucceeded, &bReentrantRestorePreservedState, &ReentrantRestoreErrors](AActor* SpawnedActor)
            {
                if (bRestoreCallbackReached || !Cast<AFFZSFacilityActor>(SpawnedActor))
                {
                    return;
                }
                bRestoreCallbackReached = true;
                const int64 RevisionBefore = Subsystem->GetWorldRevision();
                const EFFZSChangePhase PhaseBefore = Subsystem->GetChangePhase();
                const AFFZSFacilityActor* RegisteredBefore = Subsystem->FindFacility(Install.FacilityInstanceId);
                bReentrantRestoreSucceeded = Subsystem->RestoreWorldSnapshot(EmptyLiveSnapshot, ReentrantRestoreErrors);
                bReentrantRestorePreservedState =
                    Subsystem->GetWorldRevision() == RevisionBefore &&
                    Subsystem->GetChangePhase() == PhaseBefore &&
                    RegisteredBefore == nullptr &&
                    Subsystem->FindFacility(Install.FacilityInstanceId) == nullptr;
            }));

    const FFFZSCommandResult InstallResult = Subsystem->ExecuteCommand(Install);
    World->RemoveOnActorSpawnedHandler(RestoreDuringCommandHandle);
    TestTrue(TEXT("Facility Spawn이 재진입 Restore Callback에 도달한다"), bRestoreCallbackReached);
    TestFalse(TEXT("Command 실행 중에는 Restore가 거부된다"), bReentrantRestoreSucceeded);
    TestTrue(TEXT("거부된 재진입 Restore가 충돌 원인을 설명한다"), !ReentrantRestoreErrors.IsEmpty());
    TestTrue(TEXT("거부된 재진입 Restore는 권위 상태를 변경하지 않는다"), bReentrantRestorePreservedState);
    TestTrue(TEXT("외부 Install은 거부된 Callback 후에도 Commit된다"), InstallResult.bSucceeded);
    TestEqual(TEXT("Command Guard는 ExecuteCommand 반환 후 Reset된다"), Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);

    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("설치된 Facility"), Facility))
    {
        return false;
    }
    const FFFZSCommand Open = MakeFacilityCommand(EFFZSCommandType::Open, Install.FacilityInstanceId, 1);
    TestTrue(TEXT("Command Guard 해제 후 후속 Command가 성공한다"), Subsystem->ExecuteCommand(Open).bSucceeded);

    const FName DrainingAgentId(TEXT("Agent.RestoredDrain"));
    FGuid DrainingTicket;
    TestEqual(TEXT("Normal Close 전에 Agent가 Join한다"),
        Subsystem->JoinFacilityQueue(Install.FacilityInstanceId, DrainingAgentId, nullptr, DrainingTicket),
        EFFZSQueueJoinResult::Joined);
    const FFFZSCommand Close = MakeFacilityCommand(EFFZSCommandType::Close, Install.FacilityInstanceId, 2);
    TestTrue(TEXT("Normal Close가 Drain을 시작한다"), Subsystem->ExecuteCommand(Close).bSucceeded);
    TestEqual(TEXT("점유 중인 Facility는 Closed 상태를 유지한다"), Facility->GetFacilityState(), EFFZSFacilityState::Closed);
    TestEqual(TEXT("점유 상태의 Normal Close는 Draining Queue 상태를 사용한다"),
        Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Draining);

    Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation);
    const FFFZSWorldSnapshot DrainingSnapshot = Subsystem->CaptureWorldSnapshot();
    TestEqual(TEXT("Snapshot Schema는 자유 Yaw와 1cm Region 계약을 포함한다"), DrainingSnapshot.SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion);
    TestEqual(TEXT("Snapshot이 초기값이 아닌 Phase를 캡처한다"), DrainingSnapshot.CurrentPhase, EFFZSChangePhase::LiveOperation);
    TestEqual(TEXT("Snapshot이 권위 Revision을 캡처한다"), DrainingSnapshot.WorldRevision, int64(3));
    TestTrue(TEXT("Snapshot이 Normal Close의 Drain Intent를 캡처한다"),
        DrainingSnapshot.Facilities.Num() == 1 && DrainingSnapshot.Facilities[0].Queue.bDrainRequested);

    if (DrainingSnapshot.Facilities.Num() != 1)
    {
        AddError(TEXT("Schema 5 Yaw 및 마이그레이션 Fixture에는 Facility가 정확히 하나 있어야 합니다."));
        return false;
    }

    // 현재 Schema는 archive 경계에서도 정규형만 허용한다. 실패한 입력이 기존 Actor나
    // Revision/Phase를 조금이라도 바꾸면 원자적 Restore 계약 위반이다.
    AFFZSFacilityActor* FacilityBeforeInvalidYaw =
        Subsystem->FindFacility(Install.FacilityInstanceId);
    const int64 RevisionBeforeInvalidYaw = Subsystem->GetWorldRevision();
    const EFFZSChangePhase PhaseBeforeInvalidYaw = Subsystem->GetChangePhase();
    const int32 InvalidSchema5YawCases[] = { -1, 36000 };
    for (const int32 InvalidYaw : InvalidSchema5YawCases)
    {
        FFFZSWorldSnapshot InvalidYawSnapshot = DrainingSnapshot;
        InvalidYawSnapshot.Facilities[0].Placement.YawCentidegrees = InvalidYaw;
        InvalidYawSnapshot.Facilities[0].Placement.QuarterTurns = 0;
        TArray<FText> InvalidYawErrors;
        const FString CaseLabel = FString::Printf(TEXT("Schema 5 Yaw %d"), InvalidYaw);
        TestFalse(*FString::Printf(TEXT("%s: 비정규 자유 Yaw를 거부한다"), *CaseLabel),
            Subsystem->RestoreWorldSnapshot(InvalidYawSnapshot, InvalidYawErrors));
        TestTrue(*FString::Printf(TEXT("%s: 거부 사유를 반환한다"), *CaseLabel),
            !InvalidYawErrors.IsEmpty());
        TestTrue(*FString::Printf(TEXT("%s: 기존 Facility Actor를 보존한다"), *CaseLabel),
            Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeInvalidYaw);
        TestEqual(*FString::Printf(TEXT("%s: WorldRevision을 보존한다"), *CaseLabel),
            Subsystem->GetWorldRevision(), RevisionBeforeInvalidYaw);
        TestEqual(*FString::Printf(TEXT("%s: Phase를 보존한다"), *CaseLabel),
            Subsystem->GetChangePhase(), PhaseBeforeInvalidYaw);
    }

    // Schema 4는 QuarterTurns만 권위였고 1cm 좌표/Transform은 이미 현재 단위였다.
    // q0..q3 모두 기존 물리 Transform을 유지한 채 Schema 5 centidegree로 승격되어야 한다.
    FFFZSWorldSnapshot LegacyV4BaseSnapshot = DrainingSnapshot;
    LegacyV4BaseSnapshot.SchemaVersion = 4;
    for (int32 QuarterTurn = 0; QuarterTurn < 4; ++QuarterTurn)
    {
        FFFZSWorldSnapshot LegacyV4Snapshot = LegacyV4BaseSnapshot;
        FFFZSFacilityInstanceSnapshot& LegacySaved = LegacyV4Snapshot.Facilities[0];
        LegacySaved.Placement.YawCentidegrees = 0;
        LegacySaved.Placement.QuarterTurns = QuarterTurn;

        const TArray<FFFZSCellRect> LegacyFootprintRects =
            UFFZSGridLibrary::GetFootprintRects(LegacySaved.FacilitySpec, LegacySaved.Placement);
        if (LegacyFootprintRects.Num() != 1)
        {
            AddError(FString::Printf(
                TEXT("Schema 4 q%d Fixture의 Footprint가 정확히 한 Rect여야 합니다."), QuarterTurn));
            return false;
        }
        const FFFZSCellRect& FootprintRect = LegacyFootprintRects[0];
        const double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
        const FVector ExpectedPhysicalCenter = Zone->GetActorTransform().TransformPosition(FVector(
            (static_cast<double>(FootprintRect.Min.X) + FootprintRect.Max.X) * 0.5 * CellSizeCm,
            (static_cast<double>(FootprintRect.Min.Y) + FootprintRect.Max.Y) * 0.5 * CellSizeCm,
            0.0));
        const FQuat ExpectedPhysicalRotation = Zone->GetActorQuat() *
            FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0 * QuarterTurn));
        const FTransform ExpectedPhysicalTransform(
            ExpectedPhysicalRotation, ExpectedPhysicalCenter, FVector::OneVector);
        LegacySaved.WorldTransform = ExpectedPhysicalTransform;

        TArray<FText> LegacyV4Errors;
        const FString CaseLabel = FString::Printf(TEXT("Schema 4 q%d"), QuarterTurn);
        if (!TestTrue(*FString::Printf(TEXT("%s: Schema 5로 원자적 마이그레이션한다"), *CaseLabel),
            Subsystem->RestoreWorldSnapshot(LegacyV4Snapshot, LegacyV4Errors)))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("%s: 마이그레이션 오류가 없다"), *CaseLabel),
            LegacyV4Errors.IsEmpty());

        const FFFZSWorldSnapshot MigratedV5Snapshot = Subsystem->CaptureWorldSnapshot();
        if (MigratedV5Snapshot.Facilities.Num() != 1)
        {
            AddError(FString::Printf(TEXT("%s: 복원된 Facility가 정확히 하나 있어야 합니다."), *CaseLabel));
            return false;
        }
        const FFFZSFacilityInstanceSnapshot& MigratedSaved = MigratedV5Snapshot.Facilities[0];
        TestEqual(*FString::Printf(TEXT("%s: Schema 5로 다시 캡처된다"), *CaseLabel),
            MigratedV5Snapshot.SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion);
        TestEqual(*FString::Printf(TEXT("%s: 정확한 centidegree Yaw로 승격된다"), *CaseLabel),
            MigratedSaved.Placement.YawCentidegrees, QuarterTurn * 9000);
        TestEqual(*FString::Printf(TEXT("%s: legacy QuarterTurns를 비운다"), *CaseLabel),
            MigratedSaved.Placement.QuarterTurns, 0);
        TestTrue(*FString::Printf(TEXT("%s: 저장 Transform을 보존한다"), *CaseLabel),
            MigratedSaved.WorldTransform.Equals(ExpectedPhysicalTransform, 0.001));

        const AFFZSFacilityActor* MigratedActor =
            Subsystem->FindFacility(Install.FacilityInstanceId);
        TestTrue(*FString::Printf(TEXT("%s: Actor Transform도 보존한다"), *CaseLabel),
            MigratedActor && MigratedActor->GetActorTransform().Equals(ExpectedPhysicalTransform, 0.001));
    }

    // 실제 Schema 3(1 Cell=100 cm) 표본을 만든다. 원래 cm로 저장되던 spline/anchor/Transform은
    // 그대로 두고 Cell 기반 footprint/clearance/queue만 구 단위로 되돌린다.
    FFFZSWorldSnapshot LegacyV3BaseSnapshot = DrainingSnapshot;
    LegacyV3BaseSnapshot.SchemaVersion = 3;
    if (LegacyV3BaseSnapshot.Facilities.Num() != 1)
    {
        AddError(TEXT("Schema 3 마이그레이션 Fixture에는 Facility가 정확히 하나 있어야 합니다."));
        return false;
    }

    FFFZSFacilityInstanceSnapshot& LegacyBaseSaved = LegacyV3BaseSnapshot.Facilities[0];
    FFFZSFacilitySpec& LegacyBaseSpec = LegacyBaseSaved.FacilitySpec;
    if (LegacyBaseSpec.FootprintSizeCells.X % 100 != 0 ||
        LegacyBaseSpec.FootprintSizeCells.Y % 100 != 0 ||
        LegacyBaseSpec.ClearanceCells % 100 != 0)
    {
        AddError(TEXT("Schema 3 마이그레이션 Fixture의 치수는 미터 단위에 맞춰져 있어야 합니다."));
        return false;
    }

    TArray<FIntPoint> LegacyQueueCells;
    for (const FFFZSCellRect& Rect : LegacyBaseSpec.QueueReservedRects)
    {
        if (Rect.Min.X % 100 != 0 || Rect.Min.Y % 100 != 0 ||
            Rect.Max.X % 100 != 0 || Rect.Max.Y % 100 != 0)
        {
            AddError(TEXT("Schema 3 마이그레이션 Fixture의 Queue 사각형은 미터 단위에 맞춰져 있어야 합니다."));
            return false;
        }
        for (int32 X = Rect.Min.X; X < Rect.Max.X; X += 100)
        {
            for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; Y += 100)
            {
                LegacyQueueCells.Emplace(X / 100, Y / 100);
            }
        }
    }
    LegacyBaseSpec.SchemaVersion = 1;
    LegacyBaseSpec.FootprintSizeCells /= 100;
    LegacyBaseSpec.ClearanceCells /= 100;
    LegacyBaseSpec.QueueReservedCells = MoveTemp(LegacyQueueCells);
    LegacyBaseSpec.QueueReservedRects.Reset();

    // Schema 3 영수증은 v5 명령 지문이 아니며 런타임이 해석하지 않는 opaque 값이다.
    for (int32 ReceiptIndex = 0; ReceiptIndex < LegacyV3BaseSnapshot.CommandReceipts.Num(); ++ReceiptIndex)
    {
        FFFZSCommandReceiptSnapshot& Receipt = LegacyV3BaseSnapshot.CommandReceipts[ReceiptIndex];
        Receipt.PayloadFingerprint = FString::Printf(
            TEXT("ZFCommand.v3.LegacyOpaque.%d.%s"),
            ReceiptIndex,
            *Receipt.CommandId.ToString());
    }
    if (!TestFalse(TEXT("Schema 3 Fixture에 Opaque Command Receipt가 포함되어 있다"),
        LegacyV3BaseSnapshot.CommandReceipts.IsEmpty()))
    {
        return false;
    }

    // q0/q1/q2는 양수 구 Origin, q3은 음수 구 Origin을 사용한다. q1/q2 X와 q2/q3 Y의
    // +99 cm 보정이 구 RotateCell의 물리 경계를 그대로 보존해야 한다.
    const FIntPoint LegacyOrigins[] = {
        FIntPoint(10, 10),
        FIntPoint(10, 10),
        FIntPoint(10, 10),
        FIntPoint(-10, -10)
    };
    const FIntPoint ExpectedMigratedOrigins[] = {
        FIntPoint(1000, 1000),
        FIntPoint(1099, 1000),
        FIntPoint(1099, 1099),
        FIntPoint(-1000, -901)
    };
    const FVector ExpectedLocalCentersCm[] = {
        FVector(1300.0, 1150.0, 0.0),
        FVector(950.0, 1300.0, 0.0),
        FVector(800.0, 950.0, 0.0),
        FVector(-850.0, -1200.0, 0.0)
    };

    for (int32 QuarterTurn = 0; QuarterTurn < 4; ++QuarterTurn)
    {
        FFFZSWorldSnapshot LegacyV3Snapshot = LegacyV3BaseSnapshot;
        FFFZSFacilityInstanceSnapshot& LegacySaved = LegacyV3Snapshot.Facilities[0];
        LegacySaved.Placement.OriginCell = LegacyOrigins[QuarterTurn];
        LegacySaved.Placement.YawCentidegrees = 0;
        LegacySaved.Placement.QuarterTurns = QuarterTurn;

        const FVector ExpectedPhysicalCenter = Zone->GetActorTransform().TransformPosition(
            ExpectedLocalCentersCm[QuarterTurn]);
        const FQuat ExpectedPhysicalRotation = Zone->GetActorQuat() *
            FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0 * QuarterTurn));
        LegacySaved.WorldTransform = FTransform(
            ExpectedPhysicalRotation, ExpectedPhysicalCenter, FVector::OneVector);

        TArray<FText> LegacyV3Errors;
        const FString CaseLabel = FString::Printf(TEXT("Schema 3 q%d"), QuarterTurn);
        if (!TestTrue(*FString::Printf(TEXT("%s: 1cm 계약으로 원자적 마이그레이션에 성공한다"), *CaseLabel),
            Subsystem->RestoreWorldSnapshot(LegacyV3Snapshot, LegacyV3Errors)))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("%s: 마이그레이션은 오류를 내지 않는다"), *CaseLabel),
            LegacyV3Errors.IsEmpty());

        const FFFZSWorldSnapshot MigratedV5Snapshot = Subsystem->CaptureWorldSnapshot();
        if (MigratedV5Snapshot.Facilities.Num() != 1)
        {
            AddError(FString::Printf(TEXT("%s: 마이그레이션된 Facility가 정확히 하나 있어야 합니다."), *CaseLabel));
            return false;
        }
        const FFFZSFacilityInstanceSnapshot& MigratedSaved = MigratedV5Snapshot.Facilities[0];
        TestEqual(*FString::Printf(TEXT("%s: Schema 5로 저장된다"), *CaseLabel),
            MigratedV5Snapshot.SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion);
        TestEqual(*FString::Printf(TEXT("%s: FacilitySpec은 Schema 2를 사용한다"), *CaseLabel),
            MigratedSaved.FacilitySpec.SchemaVersion, 2);
        TestEqual(*FString::Printf(TEXT("%s: 6m x 3m Footprint를 보존한다"), *CaseLabel),
            MigratedSaved.FacilitySpec.FootprintSizeCells, FIntPoint(600, 300));
        TestEqual(*FString::Printf(TEXT("%s: 1m Clearance를 보존한다"), *CaseLabel),
            MigratedSaved.FacilitySpec.ClearanceCells, 100);
        TestTrue(*FString::Printf(TEXT("%s: Legacy Queue Cell을 제거한다"), *CaseLabel),
            MigratedSaved.FacilitySpec.QueueReservedCells.IsEmpty());

        const TArray<FFFZSCellRect>& MigratedQueueRects =
            MigratedSaved.FacilitySpec.QueueReservedRects;
        TestEqual(*FString::Printf(TEXT("%s: Queue Cell을 정확한 세 Band로 압축한다"), *CaseLabel),
            MigratedQueueRects.Num(), 3);
        TestEqual(*FString::Printf(TEXT("%s: 정확히 160,000 cm2인 Queue 합집합을 보존한다"), *CaseLabel),
            UFFZSGridLibrary::GetCellRectArea(MigratedQueueRects), static_cast<int64>(160'000));
        TestTrue(*FString::Printf(TEXT("%s: 왼쪽 Queue Band를 보존한다"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(0, 300), FIntPoint(100, 800))));
        TestTrue(*FString::Printf(TEXT("%s: 오른쪽 Queue Band를 보존한다"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(500, 300), FIntPoint(600, 800))));
        TestTrue(*FString::Printf(TEXT("%s: 위쪽 Queue Band를 보존한다"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(0, 800), FIntPoint(600, 900))));

        TestEqual(*FString::Printf(TEXT("%s: 정확히 보정된 Origin을 적용한다"), *CaseLabel),
            MigratedSaved.Placement.OriginCell, ExpectedMigratedOrigins[QuarterTurn]);
        TestEqual(*FString::Printf(TEXT("%s: 정확한 centidegree Yaw로 승격된다"), *CaseLabel),
            MigratedSaved.Placement.YawCentidegrees, QuarterTurn * 9000);
        TestEqual(*FString::Printf(TEXT("%s: legacy QuarterTurns를 비운다"), *CaseLabel),
            MigratedSaved.Placement.QuarterTurns, 0);
        TestTrue(*FString::Printf(TEXT("%s: 물리적 중심을 보존한다"), *CaseLabel),
            MigratedSaved.WorldTransform.GetLocation().Equals(ExpectedPhysicalCenter, 0.1));
        TestTrue(*FString::Printf(TEXT("%s: 물리적 Rotation을 보존한다"), *CaseLabel),
            MigratedSaved.WorldTransform.GetRotation().AngularDistance(ExpectedPhysicalRotation) <=
                FMath::DegreesToRadians(0.01));

        TestEqual(*FString::Printf(TEXT("%s: Receipt 개수를 보존한다"), *CaseLabel),
            MigratedV5Snapshot.CommandReceipts.Num(),
            LegacyV3BaseSnapshot.CommandReceipts.Num());
        for (const FFFZSCommandReceiptSnapshot& LegacyReceipt : LegacyV3BaseSnapshot.CommandReceipts)
        {
            const FFFZSCommandReceiptSnapshot* MigratedReceipt =
                MigratedV5Snapshot.CommandReceipts.FindByPredicate(
                    [&LegacyReceipt](const FFFZSCommandReceiptSnapshot& Candidate)
                    {
                        return Candidate.CommandId == LegacyReceipt.CommandId;
                    });
            TestTrue(*FString::Printf(TEXT("%s: Receipt %s를 보존한다"),
                *CaseLabel, *LegacyReceipt.CommandId.ToString()),
                MigratedReceipt != nullptr &&
                MigratedReceipt->PayloadFingerprint == LegacyReceipt.PayloadFingerprint);
        }

        if (QuarterTurn == 0)
        {
            // 같은 구 CommandId를 새 v5 Payload로 재사용하면 opaque 지문을 덮지 않고 충돌로 거부한다.
            FFFZSCommand ReusedCommandId = MakeFacilityCommand(
                EFFZSCommandType::Open, Install.FacilityInstanceId);
            ReusedCommandId.CommandId = LegacyV3BaseSnapshot.CommandReceipts[0].CommandId;
            const int64 RevisionBeforeMismatch = Subsystem->GetWorldRevision();
            const FFFZSCommandResult MismatchResult = Subsystem->ExecuteCommand(ReusedCommandId);
            TestFalse(TEXT("복원된 Schema 3 Receipt는 동일 CommandId의 새 v5 Payload를 거부한다"),
                MismatchResult.bSucceeded);
            TestEqual(TEXT("Opaque Legacy Receipt 불일치는 전용 실패 코드를 사용한다"),
                MismatchResult.FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);
            TestEqual(TEXT("Receipt 불일치는 WorldRevision을 변경하지 않는다"),
                Subsystem->GetWorldRevision(), RevisionBeforeMismatch);
        }
    }

    // x*100은 int32에 들어오지만 q1의 +99 cm 보정만 넘치는 경계를 별도로 거부해야 한다.
    FFFZSWorldSnapshot AdjustedOriginOverflowSnapshot = LegacyV3BaseSnapshot;
    AdjustedOriginOverflowSnapshot.Facilities[0].Placement.OriginCell =
        FIntPoint(21'474'836, 10);
    AdjustedOriginOverflowSnapshot.Facilities[0].Placement.QuarterTurns = 1;
    AFFZSFacilityActor* FacilityBeforeOverflow =
        Subsystem->FindFacility(Install.FacilityInstanceId);
    const int64 RevisionBeforeOverflow = Subsystem->GetWorldRevision();
    const EFFZSChangePhase PhaseBeforeOverflow = Subsystem->GetChangePhase();
    TArray<FText> AdjustedOriginOverflowErrors;
    TestFalse(TEXT("Schema 3의 보정된 Origin Overflow는 준비 전에 거부된다"),
        Subsystem->RestoreWorldSnapshot(
            AdjustedOriginOverflowSnapshot, AdjustedOriginOverflowErrors));
    TestTrue(TEXT("보정된 Origin Overflow가 정확한 마이그레이션 경계를 보고한다"),
        AdjustedOriginOverflowErrors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("회전 원점 보정 후"));
        }));
    TestTrue(TEXT("보정된 Origin Overflow 발생 후 등록된 Facility가 보존된다"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeOverflow);
    TestEqual(TEXT("보정된 Origin Overflow 발생 후 WorldRevision이 보존된다"),
        Subsystem->GetWorldRevision(), RevisionBeforeOverflow);
    TestEqual(TEXT("보정된 Origin Overflow 발생 후 Phase가 보존된다"),
        Subsystem->GetChangePhase(), PhaseBeforeOverflow);

    // Schema 5에서도 footprint/expanded는 표현되지만 FoodTruck Queue Y Max만 int32를 넘는 표본이다.
    // 빈 권위 Region을 정상 "시설 없음"으로 오인하면 점유·Zone 검사를 우회하므로 전체 복원을 거절한다.
    FFFZSWorldSnapshot QueueOverflowV5Snapshot = DrainingSnapshot;
    FFFZSFacilityInstanceSnapshot& QueueOverflowV5Saved = QueueOverflowV5Snapshot.Facilities[0];
    QueueOverflowV5Saved.Placement.OriginCell = FIntPoint(1000, MAX_int32 - 899);
    QueueOverflowV5Saved.Placement.YawCentidegrees = 0;
    QueueOverflowV5Saved.Placement.QuarterTurns = 0;
    QueueOverflowV5Saved.WorldTransform = FTransform(
        Zone->GetActorQuat(),
        Zone->GetActorTransform().TransformPosition(FVector(
            1300.0, static_cast<double>(MAX_int32) - 749.0, 0.0)),
        FVector::OneVector);
    TArray<FText> QueueOverflowV5Errors;
    TestFalse(TEXT("Schema 5의 Queue 전용 Overflow는 불완전한 권위 Region을 복원할 수 없다"),
        Subsystem->RestoreWorldSnapshot(QueueOverflowV5Snapshot, QueueOverflowV5Errors));
    TestTrue(TEXT("Schema 5의 Queue 전용 Overflow가 불완전한 압축 Region을 명시한다"),
        QueueOverflowV5Errors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("완전한 압축 1cm 배치 영역"));
        }));
    TestTrue(TEXT("Schema 5의 Queue 전용 Overflow 발생 후 등록된 Facility가 보존된다"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeOverflow);
    TestEqual(TEXT("Schema 5의 Queue 전용 Overflow 발생 후 WorldRevision이 보존된다"),
        Subsystem->GetWorldRevision(), RevisionBeforeOverflow);
    TestEqual(TEXT("Schema 5의 Queue 전용 Overflow 발생 후 Phase가 보존된다"),
        Subsystem->GetChangePhase(), PhaseBeforeOverflow);

    Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck);
    AFFZSFacilityActor* FacilityBeforeRejectedRestore = Subsystem->FindFacility(Install.FacilityInstanceId);
    FFFZSWorldSnapshot LegacySnapshot = DrainingSnapshot;
    LegacySnapshot.SchemaVersion = 2;
    TArray<FText> LegacyErrors;
    TestFalse(TEXT("Schema 2 Snapshot은 명시적으로 거부된다"), Subsystem->RestoreWorldSnapshot(LegacySnapshot, LegacyErrors));
    TestTrue(TEXT("Schema 2 거부 사유가 누락된 CurrentPhase를 설명한다"),
        LegacyErrors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("CurrentPhase"));
        }));
    TestTrue(TEXT("거부된 Legacy Snapshot은 Facility 식별자를 보존한다"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeRejectedRestore);
    TestEqual(TEXT("거부된 Legacy Snapshot은 Phase를 보존한다"), Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);
    TestEqual(TEXT("거부된 Legacy Snapshot은 Revision을 보존한다"), Subsystem->GetWorldRevision(), int64(3));
    TestEqual(TEXT("거부된 Legacy Snapshot은 Queue 소유권을 보존한다"),
        Subsystem->FindAgentQueueFacility(DrainingAgentId), Install.FacilityInstanceId);

    // Game Thread 밖의 복원 요청은 Subsystem/World 상태를 읽기 전에 거부되어야 한다.
    const TPair<bool, int32> OffThreadRestore = Async(EAsyncExecution::ThreadPool,
        [Subsystem, DrainingSnapshot]()
        {
            TArray<FText> Errors;
            const bool bSucceeded = Subsystem->RestoreWorldSnapshot(DrainingSnapshot, Errors);
            return TPair<bool, int32>(bSucceeded, Errors.Num());
        }).Get();
    TestFalse(TEXT("Game Thread 밖의 Restore는 거부된다"), OffThreadRestore.Key);
    TestTrue(TEXT("Game Thread 밖의 Restore가 오류를 보고한다"), OffThreadRestore.Value > 0);
    TestTrue(TEXT("다른 Thread의 요청을 거부한 뒤 Facility 식별자가 보존된다"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeRejectedRestore);
    TestEqual(TEXT("다른 Thread의 요청을 거부한 뒤 Phase가 보존된다"), Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);

    FFFZSWorldSnapshot InvalidPhaseSnapshot = DrainingSnapshot;
    InvalidPhaseSnapshot.CurrentPhase = static_cast<EFFZSChangePhase>(255);
    TArray<FText> InvalidPhaseErrors;
    TestFalse(TEXT("Snapshot은 유효하지 않은 Phase Enum을 거부한다"),
        Subsystem->RestoreWorldSnapshot(InvalidPhaseSnapshot, InvalidPhaseErrors));
    TestEqual(TEXT("유효하지 않은 Phase를 거부한 뒤 현재 Phase가 보존된다"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);

    // 복원이 Actor를 준비하는 동안 시도된 명령은 기존 World를 변경하지 않고 실패해야 한다.
    const FFFZSCommand ReentrantOpen = MakeFacilityCommand(EFFZSCommandType::Open, Install.FacilityInstanceId, 3);
    FFFZSCommandResult ReentrantCommandResult;
    bool bCommandCallbackReached = false;
    bool bReentrantCommandPreservedState = false;
    const int32 ReceiptCountBeforeRestore = DrainingSnapshot.CommandReceipts.Num();
    const FDelegateHandle CommandDuringRestoreHandle = World->AddOnActorSpawnedHandler(
        FOnActorSpawned::FDelegate::CreateLambda(
            [Subsystem, &ReentrantOpen, FacilityBeforeRejectedRestore, DrainingAgentId, ReceiptCountBeforeRestore,
             &ReentrantCommandResult, &bCommandCallbackReached, &bReentrantCommandPreservedState](AActor* SpawnedActor)
            {
                if (bCommandCallbackReached || !Cast<AFFZSFacilityActor>(SpawnedActor))
                {
                    return;
                }
                bCommandCallbackReached = true;
                ReentrantCommandResult = Subsystem->ExecuteCommand(ReentrantOpen);
                bReentrantCommandPreservedState =
                    !ReentrantCommandResult.bSucceeded &&
                    Subsystem->GetWorldRevision() == 3 &&
                    Subsystem->GetChangePhase() == EFFZSChangePhase::MorningCheck &&
                    Subsystem->FindFacility(ReentrantOpen.FacilityInstanceId) == FacilityBeforeRejectedRestore &&
                    FacilityBeforeRejectedRestore->GetFacilityState() == EFFZSFacilityState::Closed &&
                    FacilityBeforeRejectedRestore->GetQueueComponent()->GetQueueState() == EFFZSQueueState::Draining &&
                    Subsystem->FindAgentQueueFacility(DrainingAgentId) == ReentrantOpen.FacilityInstanceId &&
                    Subsystem->CaptureWorldSnapshot().CommandReceipts.Num() == ReceiptCountBeforeRestore;
            }));

    TArray<FText> RestoreErrors;
    TestTrue(TEXT("앞선 검증 실패 후에도 유효한 Snapshot은 복원된다"),
        Subsystem->RestoreWorldSnapshot(DrainingSnapshot, RestoreErrors));
    World->RemoveOnActorSpawnedHandler(CommandDuringRestoreHandle);
    TestTrue(TEXT("준비된 Facility Spawn이 재진입 Command Callback에 도달한다"), bCommandCallbackReached);
    TestEqual(TEXT("Restore 진행 중에는 Command가 거부된다"),
        ReentrantCommandResult.FailureReason, EFFZSPlacementFailure::InternalError);
    TestTrue(TEXT("거부된 재진입 Command는 기존 권위 상태를 변경하지 않는다"), bReentrantCommandPreservedState);
    TestTrue(TEXT("성공한 Restore는 오류를 내지 않는다"), RestoreErrors.IsEmpty());
    TestEqual(TEXT("Restore가 저장된 LiveOperation Phase를 원자적으로 적용한다"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::LiveOperation);
    TestEqual(TEXT("Restore가 저장된 Revision을 원자적으로 적용한다"), Subsystem->GetWorldRevision(), int64(3));

    AFFZSFacilityActor* RestoredFacility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("복원된 Facility"), RestoredFacility))
    {
        return false;
    }
    TestTrue(TEXT("성공한 Restore가 기존 Facility Actor를 교체한다"), RestoredFacility != FacilityBeforeRejectedRestore);
    TestEqual(TEXT("복원된 Normal Close Facility는 Drain 중 Closed 상태를 유지한다"),
        RestoredFacility->GetFacilityState(), EFFZSFacilityState::Closed);
    TestEqual(TEXT("복원된 Queue는 Draining 상태를 유지한다"),
        RestoredFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Draining);
    TestTrue(TEXT("Restore가 Drain 후 MovePending으로 전환할 임시 Intent를 재구성한다"),
        RestoredFacility->bSetMovePendingWhenDrained);

    TestTrue(TEXT("복원된 Agent는 World 권위를 통해 Leave할 수 있다"),
        Subsystem->LeaveCurrentFacilityQueue(DrainingAgentId, EFFZSQueueReleaseReason::RemovedBySystem));
    TestEqual(TEXT("마지막 Agent를 제거하면 복원된 Draining Queue가 닫힌다"),
        RestoredFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
    World->Tick(ELevelTick::LEVELTICK_All, 0.2f);
    TestEqual(TEXT("복원된 Normal Close Drain은 MovePending 상태로 끝난다"),
        RestoredFacility->GetFacilityState(), EFFZSFacilityState::MovePending);

    const FFFZSCommand ReopenAfterRestore = MakeFacilityCommand(EFFZSCommandType::Open, Install.FacilityInstanceId, 3);
    TestTrue(TEXT("성공한 Restore 후 Restore Guard가 해제된다"),
        Subsystem->ExecuteCommand(ReopenAfterRestore).bSucceeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSZoneReadModelSpatialMetadataTest,
    "FF_ZoneSystem.Integration.World.ZoneReadModelSpatialMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Read Model 계약: Actor의 로컬 그래프·앵커는 값 복사하고 면적은 수용량 Override가 아닌 경계에서 산출한다.
bool FFFZSZoneReadModelSpatialMetadataTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Read Model 자동화 테스트 World"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Read Model World Subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("Read Model 대상 Gameplay Zone"), Zone))
    {
        return false;
    }

    const TArray<FName> ExpectedNeighborZoneIds = {
        TEXT("GZ_SunsetLounge"),
        TEXT("GZ_FleaMarket")
    };
    const TArray<FTransform> ExpectedEntryExitAnchors = {
        FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 250.0, 0.0)),
        FTransform(FRotator(0.0, 180.0, 0.0), FVector(1900.0, 750.0, 50.0))
    };
    Zone->NeighborZoneIds = ExpectedNeighborZoneIds;
    Zone->EntryExitAnchors = ExpectedEntryExitAnchors;

    // 20m x 10m 경계는 200m²다. 수용량용 공공 면적 Override와 의도적으로 다르게 둔다.
    USplineComponent* BoundarySpline = Zone->BoundarySpline;
    if (!TestNotNull(TEXT("Read Model 대상 Zone 경계 Spline"), BoundarySpline))
    {
        return false;
    }
    const FVector BoundaryPoints[] = {
        FVector(0.0, 0.0, 0.0),
        FVector(2000.0, 0.0, 0.0),
        FVector(2000.0, 1000.0, 0.0),
        FVector(0.0, 1000.0, 0.0)
    };
    for (int32 PointIndex = 0; PointIndex < UE_ARRAY_COUNT(BoundaryPoints); ++PointIndex)
    {
        BoundarySpline->SetLocationAtSplinePoint(
            PointIndex, BoundaryPoints[PointIndex], ESplineCoordinateSpace::Local, false);
    }
    BoundarySpline->UpdateSpline();
    Zone->UsablePublicAreaSquareMeters = 777.0f;

    if (!TestTrue(TEXT("Read Model 대상 Zone이 등록된다"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    constexpr float ExpectedAreaSquareMeters = 200.0f;
    TestTrue(TEXT("20m x 10m 경계 면적은 200m²다"),
        FMath::IsNearlyEqual(Zone->GetBoundaryAreaSquareMeters(), ExpectedAreaSquareMeters));

    const FFFZSReadModel ReadModel = Subsystem->BuildReadModel();
    TestEqual(TEXT("등록한 Zone 하나가 Read Model에 포함된다"), ReadModel.Zones.Num(), 1);
    const FFFZSZoneReadModel* ZoneReadModel = ReadModel.Zones.FindByPredicate(
        [ZoneId](const FFFZSZoneReadModel& Candidate)
        {
            return Candidate.ZoneId == ZoneId;
        });
    if (!TestNotNull(TEXT("Zone ID로 Read Model을 찾는다"), ZoneReadModel))
    {
        return false;
    }

    TestTrue(TEXT("Read Model 면적은 경계 함수 결과를 그대로 사용한다"),
        FMath::IsNearlyEqual(ZoneReadModel->AreaSquareMeters, Zone->GetBoundaryAreaSquareMeters()));
    TestTrue(TEXT("Read Model 면적은 200m²다"),
        FMath::IsNearlyEqual(ZoneReadModel->AreaSquareMeters, ExpectedAreaSquareMeters));
    TestFalse(TEXT("Read Model 면적은 SoftCapacity용 공공 면적 Override가 아니다"),
        FMath::IsNearlyEqual(ZoneReadModel->AreaSquareMeters, Zone->UsablePublicAreaSquareMeters));

    Zone->NeighborZoneIds.Reset();
    Zone->EntryExitAnchors.Reset();
    TestTrue(TEXT("Actor의 그래프 배열을 변경했다"), Zone->NeighborZoneIds.IsEmpty());
    TestTrue(TEXT("Actor의 출입 기준점 배열을 변경했다"), Zone->EntryExitAnchors.IsEmpty());

    TestEqual(TEXT("Read Model은 이웃 ID 배열을 값 복사한다"),
        ZoneReadModel->NeighborZoneIds.Num(), ExpectedNeighborZoneIds.Num());
    for (int32 Index = 0;
         Index < FMath::Min(ZoneReadModel->NeighborZoneIds.Num(), ExpectedNeighborZoneIds.Num());
         ++Index)
    {
        TestEqual(TEXT("Read Model의 이웃 ID와 순서가 보존된다"),
            ZoneReadModel->NeighborZoneIds[Index], ExpectedNeighborZoneIds[Index]);
    }

    TestEqual(TEXT("Read Model은 출입 기준점 배열을 값 복사한다"),
        ZoneReadModel->EntryExitAnchors.Num(), ExpectedEntryExitAnchors.Num());
    for (int32 Index = 0;
         Index < FMath::Min(ZoneReadModel->EntryExitAnchors.Num(), ExpectedEntryExitAnchors.Num());
         ++Index)
    {
        TestTrue(TEXT("Read Model의 출입 기준점 Transform과 순서가 보존된다"),
            ZoneReadModel->EntryExitAnchors[Index].Equals(ExpectedEntryExitAnchors[Index]));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityUpgradeTransactionTest,
    "FF_ZoneSystem.Integration.World.FacilityUpgradeTransactionAndPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityUpgradeTransactionTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Upgrade test world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("Upgrade subsystem"), Subsystem) ||
        !TestNotNull(TEXT("Upgrade zone"), Zone) ||
        !TestTrue(TEXT("Register upgrade zone"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Spec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Level2;
    FFFZSFacilityUpgradeTier Level3;
    Level2.RevenueMultiplier = 1.0f;
    Level3.RevenueMultiplier = 1.0f;
    Level3.TargetLevel = 3;
    Level3.CostMultiplier = 0.6f;
    Level3.ServiceDurationMultiplier = 0.65f;
    Spec.UpgradeTiers = { Level2, Level3 };
    FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        Spec, ZoneId, FFFZSGridPlacement{FIntPoint(1000, 1000), 0}, -1);
    const FString OriginalInstallFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Install);
    Install.TargetUpgradeLevel = 99;
    Install.UpgradeServiceDurationMultiplier = 0.01f;
    TestEqual(TEXT("Non-upgrade commands preserve legacy receipt bytes"),
        UFFZSGridLibrary::BuildCommandFingerprint(Install), OriginalInstallFingerprint);
    if (!TestTrue(TEXT("Install succeeds without applying injected upgrade fields"),
        Subsystem->ExecuteCommand(Install).bSucceeded))
    {
        return false;
    }
    const FGuid FacilityId = Install.FacilityInstanceId;
    AFFZSFacilityActor* Facility = Subsystem->FindFacility(FacilityId);
    if (!TestNotNull(TEXT("Installed upgrade subject"), Facility))
    {
        return false;
    }
    TestEqual(TEXT("Installation always starts at level one"), Facility->GetUpgradeLevel(), 1);
    TestEqual(TEXT("Installation always starts with original performance"),
        Facility->GetUpgradeServiceDurationMultiplier(), 1.0f);

    const FFFZSCommand OtherInstall = UFFZSGridLibrary::MakeInstallCommand(
        Spec, ZoneId, FFFZSGridPlacement{FIntPoint(4000, 1000), 0}, -1);
    if (!TestTrue(TEXT("Install another copy of same facility type"), Subsystem->ExecuteCommand(OtherInstall).bSucceeded))
    {
        return false;
    }
    FFFZSWorldSnapshot LegacySnapshot = Subsystem->CaptureWorldSnapshot();
    LegacySnapshot.SchemaVersion = 5;
    const auto UpgradeCommand = [FacilityId](const int32 Level, const float Multiplier)
    {
        FFFZSCommand Command = MakeFacilityCommand(EFFZSCommandType::Upgrade, FacilityId);
        Command.TargetUpgradeLevel = Level;
        Command.UpgradeServiceDurationMultiplier = Multiplier;
        return Command;
    };

    const int64 BeforeUpgradeRevision = Subsystem->GetWorldRevision();
    TestEqual(TEXT("Skipping an upgrade level is rejected"),
        Subsystem->ExecuteCommand(UpgradeCommand(3, 0.65f)).FailureReason, EFFZSPlacementFailure::DataInvalid);
    TestEqual(TEXT("Nonpositive multiplier is rejected"),
        Subsystem->ExecuteCommand(UpgradeCommand(2, 0.0f)).FailureReason, EFFZSPlacementFailure::DataInvalid);
    TestEqual(TEXT("An unauthored multiplier is rejected"),
        Subsystem->ExecuteCommand(UpgradeCommand(2, 1.0f)).FailureReason, EFFZSPlacementFailure::DataInvalid);
    TestEqual(TEXT("Rejected upgrade leaves revision unchanged"), Subsystem->GetWorldRevision(), BeforeUpgradeRevision);

    const FFFZSCommand Upgrade2 = UpgradeCommand(2, 0.8f);
    const FFFZSCommandResult Upgraded = Subsystem->ExecuteCommand(Upgrade2);
    TestTrue(TEXT("Upgrade commits successfully"), Upgraded.bSucceeded);
    TestTrue(TEXT("Upgrade retains the actor and identity"), Upgraded.FacilityActor == Facility);
    TestEqual(TEXT("Upgrade increments revision once"), Subsystem->GetWorldRevision(), BeforeUpgradeRevision + 1);
    TestEqual(TEXT("Upgrade publishes level two"), Facility->GetUpgradeLevel(), 2);
    TestEqual(TEXT("Base specification remains immutable"),
        Facility->GetFacilitySpec().QueueRules.ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds);
    TestEqual(TEXT("Queue runtime uses the improved service duration"),
        Facility->GetQueueComponent()->GetRules().ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds * 0.8f);
    TestEqual(TEXT("Another instance keeps its original service duration"),
        Subsystem->FindFacility(OtherInstall.FacilityInstanceId)->GetEffectiveQueueRules().ServiceDurationSeconds,
        Spec.QueueRules.ServiceDurationSeconds);
    TestEqual(TEXT("Read model publishes effective service duration"),
        Facility->GetReadModel().EffectiveQueueRules.ServiceDurationSeconds,
        Facility->GetQueueComponent()->GetRules().ServiceDurationSeconds);
    const FFFZSCommandResult Replay = Subsystem->ExecuteCommand(Upgrade2);
    TestTrue(TEXT("Repeated click reuses committed receipt"), Replay.bSucceeded && Replay.bWasDuplicateReplay);
    TestEqual(TEXT("Repeated click does not upgrade twice"), Facility->GetUpgradeLevel(), 2);
    FFFZSCommand Mismatch = Upgrade2;
    Mismatch.UpgradeServiceDurationMultiplier = 0.7f;
    TestEqual(TEXT("Changed multiplier cannot reuse an existing command ID"),
        Subsystem->ExecuteCommand(Mismatch).FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);
    Mismatch = Upgrade2;
    Mismatch.TargetUpgradeLevel = 3;
    TestEqual(TEXT("Changed target level cannot reuse an existing command ID"),
        Subsystem->ExecuteCommand(Mismatch).FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);

    TestTrue(TEXT("Open upgraded facility"),
        Subsystem->ExecuteCommand(MakeFacilityCommand(EFFZSCommandType::Open, FacilityId)).bSucceeded);
    TestEqual(TEXT("Open facility cannot be upgraded"),
        Subsystem->ExecuteCommand(UpgradeCommand(3, 0.65f)).FailureReason, EFFZSPlacementFailure::InvalidFacilityState);
    FGuid Ticket;
    TestEqual(TEXT("Guest joins upgraded queue"),
        Subsystem->JoinFacilityQueue(FacilityId, TEXT("Upgrade.Guest"), nullptr, Ticket), EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("Close starts draining occupied upgraded facility"),
        Subsystem->ExecuteCommand(MakeFacilityCommand(EFFZSCommandType::Close, FacilityId)).bSucceeded);
    TestEqual(TEXT("Draining guests block upgrade"),
        Subsystem->ExecuteCommand(UpgradeCommand(3, 0.65f)).FailureReason, EFFZSPlacementFailure::FacilityOccupied);
    TestTrue(TEXT("Clear synthetic guest"),
        Subsystem->ExecuteCommand(MakeFacilityCommand(EFFZSCommandType::EmergencyClose, FacilityId)).bSucceeded);
    TestTrue(TEXT("Enter live operation"), Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation));
    TestEqual(TEXT("Live operation blocks upgrade"),
        Subsystem->ExecuteCommand(UpgradeCommand(3, 0.65f)).FailureReason, EFFZSPlacementFailure::WrongStage);
    TestTrue(TEXT("Enter morning check"), Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck));
    TestTrue(TEXT("Move upgraded facility"),
        Subsystem->ExecuteCommand(MakeMoveCommand(FacilityId, ZoneId, FIntPoint(1000, 4000), -1)).bSucceeded);
    TestEqual(TEXT("Move preserves upgrade"), Facility->GetUpgradeLevel(), 2);

    FFFZSWorldSnapshot Saved = Subsystem->CaptureWorldSnapshot();
    FFFZSFacilityInstanceSnapshot* SavedFacility = Saved.Facilities.FindByPredicate(
        [FacilityId](const FFFZSFacilityInstanceSnapshot& Candidate) { return Candidate.FacilityInstanceId == FacilityId; });
    if (!TestNotNull(TEXT("Saved upgraded instance"), SavedFacility))
    {
        return false;
    }
    SavedFacility->Queue.CompletedCount = 7;
    SavedFacility->Queue.TotalCompletedWaitSeconds = 14.0f;
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Restore upgraded snapshot"), Subsystem->RestoreWorldSnapshot(Saved, Errors)))
    {
        return false;
    }
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Restore retains the original base"),
        Facility->GetFacilitySpec().QueueRules.ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds);
    TestEqual(TEXT("Restore applies the modifier only once"),
        Facility->GetEffectiveQueueRules().ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds * 0.8f);
    TestTrue(TEXT("Upgrade receipt remains idempotent after load"), Subsystem->ExecuteCommand(Upgrade2).bWasDuplicateReplay);

    FFFZSWorldSnapshot Version6 = Saved;
    Version6.SchemaVersion = 6;
    for (FFFZSFacilityInstanceSnapshot& LegacyFacility : Version6.Facilities)
    {
        LegacyFacility.FacilitySpec.bUsesFacilityLevels = false;
        LegacyFacility.FacilitySpec.UpgradeTiers.Reset();
        LegacyFacility.UpgradeSizeMultiplier = 3.0f; // A v6 archive never owned this modern field.
        LegacyFacility.UpgradeRevenueMultiplier = 5.0f;
    }
    if (!TestTrue(TEXT("Schema six preserves already paid upgrades"), Subsystem->RestoreWorldSnapshot(Version6, Errors)))
    {
        return false;
    }
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Schema six retains paid level"), Facility->GetUpgradeLevel(), 2);
    TestEqual(TEXT("Schema six retains paid duration"), Facility->GetUpgradeServiceDurationMultiplier(), 0.8f);
    TestEqual(TEXT("Schema six defaults size to the original building"), Facility->GetUpgradeSizeMultiplier(), 1.0f);
    TestEqual(TEXT("Schema six defaults revenue to its original value"), Facility->GetUpgradeRevenueMultiplier(), 1.0f);
    TestTrue(TEXT("Schema six paid level remains visible"), Facility->GetBaseFacilitySpec().bUsesFacilityLevels);
    TestTrue(TEXT("Schema six upgrade receipt is still replayable"), Subsystem->ExecuteCommand(Upgrade2).bWasDuplicateReplay);
    if (!TestTrue(TEXT("Restore authored schema seven fixture"), Subsystem->RestoreWorldSnapshot(Saved, Errors)))
    {
        return false;
    }
    Facility = Subsystem->FindFacility(FacilityId);

    UFFZSTestExternalPolicy* Economy = NewObject<UFFZSTestExternalPolicy>();
    Economy->PolicyName = TEXT("Upgrade.Economy");
    Economy->PolicyDomain = EFFZSPolicyDomain::Economy;
    UFFZSTestExternalPolicy* LateReject = NewObject<UFFZSTestExternalPolicy>();
    LateReject->PolicyName = TEXT("Upgrade.LateReject");
    LateReject->PolicyDomain = EFFZSPolicyDomain::SimulationTime;
    LateReject->bAllowTentativeApply = false;
    TestTrue(TEXT("Register economy transaction probe"), Subsystem->RegisterExternalPolicy(Economy));
    TestTrue(TEXT("Register late rejecting policy"), Subsystem->RegisterExternalPolicy(LateReject));
    const int64 BeforeRejectedRevision = Subsystem->GetWorldRevision();
    TestEqual(TEXT("Later policy can reject upgrade"),
        Subsystem->ExecuteCommand(UpgradeCommand(3, 0.65f)).FailureReason, EFFZSPlacementFailure::ExternalPolicyRejected);
    TestEqual(TEXT("Economy reservation is rolled back"), Economy->RollbackCount, 1);
    TestEqual(TEXT("Rejecting policy is also rolled back"), LateReject->RollbackCount, 1);
    TestEqual(TEXT("Failed transaction keeps level unchanged"), Facility->GetUpgradeLevel(), 2);
    TestEqual(TEXT("Failed transaction keeps revision unchanged"), Subsystem->GetWorldRevision(), BeforeRejectedRevision);
    LateReject->bAllowTentativeApply = true;
    const FFFZSCommand Upgrade3 = UpgradeCommand(3, 0.65f);
    TestTrue(TEXT("Morning upgrade succeeds after all policies agree"), Subsystem->ExecuteCommand(Upgrade3).bSucceeded);
    TestEqual(TEXT("Successful upgrade commits payment exactly once"), Economy->CommitCount, 1);
    TestEqual(TEXT("Level three uses original base, not compounded level two"),
        Facility->GetQueueComponent()->GetRules().ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds * 0.65f);
    TestEqual(TEXT("Upgrade preserves completed guest statistics"), Facility->GetQueueComponent()->GetCompletedCount(), 7);
    Subsystem->ExecuteCommand(Upgrade3);
    TestEqual(TEXT("Replay never charges again"), Economy->CommitCount, 1);

    FFFZSWorldSnapshot Corrupt = Subsystem->CaptureWorldSnapshot();
    Corrupt.Facilities[0].UpgradeServiceDurationMultiplier = 0.0f;
    TestFalse(TEXT("Corrupt saved multiplier is rejected atomically"), Subsystem->RestoreWorldSnapshot(Corrupt, Errors));
    TestTrue(TEXT("Failed restore keeps the current actor"), Subsystem->FindFacility(FacilityId) == Facility);
    // The old schema never owned upgrade fields: ignore even stray modern values in a legacy fixture.
    for (FFFZSFacilityInstanceSnapshot& LegacyFacility : LegacySnapshot.Facilities)
    {
        LegacyFacility.UpgradeLevel = 99;
        LegacyFacility.UpgradeServiceDurationMultiplier = 0.01f;
    }
    if (!TestTrue(TEXT("Schema five saves migrate to level one"), Subsystem->RestoreWorldSnapshot(LegacySnapshot, Errors)))
    {
        return false;
    }
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Legacy facility is level one"), Facility->GetUpgradeLevel(), 1);
    TestEqual(TEXT("Legacy facility retains original service duration"),
        Facility->GetQueueComponent()->GetRules().ServiceDurationSeconds, Spec.QueueRules.ServiceDurationSeconds);
    TestEqual(TEXT("Migrated save writes current schema"), Subsystem->CaptureWorldSnapshot().SchemaVersion, FFFZSWorldSnapshot::CurrentSchemaVersion);
    TestTrue(TEXT("Legacy install receipt remains replayable after migration"), Subsystem->ExecuteCommand(Install).bWasDuplicateReplay);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityFlexibleUpgradeTransactionTest,
    "FF_ZoneSystem.Integration.World.FacilityFlexibleUpgradeLevelsAndValues",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityFlexibleUpgradeTransactionTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Flexible upgrade world"), World)) return false;
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!Subsystem || !Zone || !Subsystem->RegisterZone(Zone)) return false;
    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Base.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Level20;
    Level20.TargetLevel = 20;
    Level20.CostMultiplier = 0.f;
    Level20.ServiceDurationMultiplier = 1.f;
    Level20.RevenueMultiplier = 0.f;
    FFFZSFacilityUpgradeTier Level42 = Level20;
    Level42.TargetLevel = 42;
    Level42.CostMultiplier = 50.f;
    Level42.ServiceDurationMultiplier = 3.f;
    Level42.RevenueMultiplier = 1000.f;
    FFFZSFacilityUpgradeTier Last = Level42;
    Last.TargetLevel = MAX_int32;
    Base.UpgradeTiers = { Last, Level42, Level20 };
    const FFFZSCommandResult Installed = Subsystem->ExecuteCommand(UFFZSGridLibrary::MakeInstallCommand(
        Base, ZoneId, FFFZSGridPlacement{FIntPoint(1000, 1000), 0}, -1));
    if (!TestTrue(TEXT("Unordered high-level configuration installs"), Installed.bSucceeded)) return false;
    const FGuid FacilityId = Installed.FacilityActor->GetFacilityInstanceId();
    const auto Upgrade = [FacilityId](const FFFZSFacilityUpgradeTier& Tier)
    {
        FFFZSCommand Command = MakeFacilityCommand(EFFZSCommandType::Upgrade, FacilityId);
        Command.TargetUpgradeLevel = Tier.TargetLevel;
        Command.UpgradeServiceDurationMultiplier = Tier.ServiceDurationMultiplier;
        Command.UpgradeSizeMultiplier = Tier.SizeMultiplier;
        Command.UpgradeRevenueMultiplier = Tier.RevenueMultiplier;
        return Command;
    };
    TestFalse(TEXT("Cannot skip the smallest larger authored level"), Subsystem->ExecuteCommand(Upgrade(Level42)).bSucceeded);
    TestTrue(TEXT("Sparse next-level geometry can be previewed"), Subsystem->ValidateUpgradePlacement(FacilityId, 1.f).bIsValid);
    if (!TestTrue(TEXT("First authored tier can lower revenue to zero"), Subsystem->ExecuteCommand(Upgrade(Level20)).bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Sparse target is used instead of current level plus one"), Facility->GetUpgradeLevel(), 20);
    TestEqual(TEXT("Zero paid revenue is retained"), Facility->GetUpgradeRevenueMultiplier(), 0.f);
    const FFFZSCommand Paid42 = Upgrade(Level42);
    if (!TestTrue(TEXT("A later tier may slow service and exceed old revenue caps"), Subsystem->ExecuteCommand(Paid42).bSucceeded)) return false;
    TestEqual(TEXT("Duration increase is applied from the base"), Facility->GetEffectiveQueueRules().ServiceDurationSeconds, Base.QueueRules.ServiceDurationSeconds * 3.f);
    TestEqual(TEXT("Revenue above the former limit is retained"), Facility->GetUpgradeRevenueMultiplier(), 1000.f);
    TestTrue(TEXT("Flexible upgrade still has idempotent receipts"), Subsystem->ExecuteCommand(Paid42).bWasDuplicateReplay);
    FFFZSCommand Changed = Paid42;
    Changed.UpgradeRevenueMultiplier = 1001.f;
    TestEqual(TEXT("Changed payload still cannot reuse a receipt"), Subsystem->ExecuteCommand(Changed).FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);
    const FFFZSWorldSnapshot Saved = Subsystem->CaptureWorldSnapshot();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Sparse high-level paid state restores"), Subsystem->RestoreWorldSnapshot(Saved, Errors))) return false;
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Restored sparse level"), Facility->GetUpgradeLevel(), 42);
    TestEqual(TEXT("Restored unrestricted revenue"), Facility->GetUpgradeRevenueMultiplier(), 1000.f);
    TestEqual(TEXT("Restored duration is not compounded"), Facility->GetEffectiveQueueRules().ServiceDurationSeconds, Base.QueueRules.ServiceDurationSeconds * 3.f);
    TestTrue(TEXT("Receipt survives unrestricted-state restore"), Subsystem->ExecuteCommand(Paid42).bWasDuplicateReplay);
    if (!TestTrue(TEXT("A tier with unchanged effects may advance to the final authored level"), Subsystem->ExecuteCommand(Upgrade(Last)).bSucceeded)) return false;
    TestEqual(TEXT("Highest representable level applies without arithmetic overflow"), Facility->GetUpgradeLevel(), MAX_int32);
    TestFalse(TEXT("No next tier is returned at the highest level"), Subsystem->ValidateUpgradePlacement(FacilityId, 1.f).bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityRevenueUpgradeTransactionTest,
    "FF_ZoneSystem.Integration.World.FacilityRevenueUpgradeTransactionAndSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityRevenueUpgradeTransactionTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Revenue upgrade world"), World)) return false;
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!Subsystem || !Zone || !Subsystem->RegisterZone(Zone)) return false;
    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Base.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Level2;
    Level2.ServiceDurationMultiplier = 1.0f;
    FFFZSFacilityUpgradeTier Level3 = Level2;
    Level3.TargetLevel = 3;
    Level3.RevenueMultiplier = 2.0f;
    Base.UpgradeTiers = { Level2, Level3 };
    FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        Base, ZoneId, FFFZSGridPlacement{FIntPoint(1000, 1000), 0}, -1);
    Install.UpgradeRevenueMultiplier = 100.0f;
    const FFFZSCommandResult Installed = Subsystem->ExecuteCommand(Install);
    if (!TestTrue(TEXT("Revenue upgrade subject installs"), Installed.bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Installed.FacilityActor.Get();
    const FGuid FacilityId = Facility->GetFacilityInstanceId();
    TestEqual(TEXT("Install cannot inject a paid revenue boost"), Facility->GetUpgradeRevenueMultiplier(), 1.0f);
    const auto Upgrade = [FacilityId](const int32 Level, const float Revenue)
    {
        FFFZSCommand Command = MakeFacilityCommand(EFFZSCommandType::Upgrade, FacilityId);
        Command.TargetUpgradeLevel = Level;
        Command.UpgradeRevenueMultiplier = Revenue;
        return Command;
    };
    TestFalse(TEXT("An unauthored revenue increase cannot be purchased"), Subsystem->ExecuteCommand(Upgrade(2, 3.0f)).bSucceeded);
    UFFZSTestExternalPolicy* Policy = NewObject<UFFZSTestExternalPolicy>();
    Policy->PolicyName = TEXT("Upgrade.Revenue");
    Policy->bAllowTentativeApply = false;
    if (!TestTrue(TEXT("Register revenue policy"), Subsystem->RegisterExternalPolicy(Policy))) return false;
    const int64 OriginalRevision = Subsystem->GetWorldRevision();
    TestFalse(TEXT("A revenue upgrade can be rolled back"), Subsystem->ExecuteCommand(Upgrade(2, 1.5f)).bSucceeded);
    TestEqual(TEXT("Rejected revenue boost rolls back the policy"), Policy->RollbackCount, 1);
    TestEqual(TEXT("Rejected revenue boost leaves paid revenue unchanged"), Facility->GetUpgradeRevenueMultiplier(), 1.0f);
    TestEqual(TEXT("Rejected revenue boost leaves revision unchanged"), Subsystem->GetWorldRevision(), OriginalRevision);
    Policy->bAllowTentativeApply = true;
    const FFFZSCommand Paid = Upgrade(2, 1.5f);
    if (!TestTrue(TEXT("Revenue-only upgrade succeeds with unchanged service duration"), Subsystem->ExecuteCommand(Paid).bSucceeded)) return false;
    TestEqual(TEXT("Paid revenue is stored as an absolute multiplier"), Facility->GetUpgradeRevenueMultiplier(), 1.5f);
    TestEqual(TEXT("Revenue-only upgrade keeps service duration"), Facility->GetEffectiveQueueRules().ServiceDurationSeconds, Base.QueueRules.ServiceDurationSeconds);
    TestEqual(TEXT("Read model publishes paid revenue"), Facility->GetReadModel().UpgradeRevenueMultiplier, 1.5f);
    TestTrue(TEXT("Repeated revenue upgrade is idempotent"), Subsystem->ExecuteCommand(Paid).bWasDuplicateReplay);
    TestEqual(TEXT("Repeated revenue upgrade commits policy once"), Policy->CommitCount, 1);
    FFFZSCommand Tampered = Paid;
    Tampered.UpgradeRevenueMultiplier = 2.0f;
    TestEqual(TEXT("A receipt cannot be replayed with changed revenue"), Subsystem->ExecuteCommand(Tampered).FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);
    const FFFZSWorldSnapshot Saved = Subsystem->CaptureWorldSnapshot();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Paid revenue survives snapshot restore"), Subsystem->RestoreWorldSnapshot(Saved, Errors))) return false;
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Restore does not compound revenue"), Facility->GetUpgradeRevenueMultiplier(), 1.5f);
    TestTrue(TEXT("Revenue receipt survives restore"), Subsystem->ExecuteCommand(Paid).bWasDuplicateReplay);
    if (!TestTrue(TEXT("Next revenue tier can keep the same duration"), Subsystem->ExecuteCommand(Upgrade(3, 2.0f)).bSucceeded)) return false;
    TestEqual(TEXT("Next revenue tier stays relative to Lv1"), Facility->GetUpgradeRevenueMultiplier(), 2.0f);
    FFFZSWorldSnapshot InvalidSaved = Subsystem->CaptureWorldSnapshot();
    InvalidSaved.Facilities[0].UpgradeRevenueMultiplier = -1.0f;
    TestFalse(TEXT("Unsafe saved revenue is rejected atomically"), Subsystem->RestoreWorldSnapshot(InvalidSaved, Errors));
    TestTrue(TEXT("Rejected saved revenue leaves the committed actor"), Subsystem->FindFacility(FacilityId) == Facility);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSFacilityUpgradeSizeTransactionTest,
    "FF_ZoneSystem.Integration.World.FacilityUpgradeSizeCollisionRollbackAndSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityUpgradeSizeTransactionTest::RunTest(const FString& Parameters)
{
    FScopedZFTestSettings ScopedSettings;
    GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility = 10'000'000;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Size upgrade test world"), World)) return false;
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!Subsystem || !Zone || !Subsystem->RegisterZone(Zone)) return false;

    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Base.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Tier;
    Tier.SizeMultiplier = 2.f;
    Tier.RevenueMultiplier = 1.f;
    Base.UpgradeTiers = { Tier };
    const auto InstallAt = [Subsystem, ZoneId, &Base](const FIntPoint Origin)
    {
        return Subsystem->ExecuteCommand(UFFZSGridLibrary::MakeInstallCommand(
            Base, ZoneId, FFFZSGridPlacement{Origin, 0}, -1));
    };
    const FFFZSCommandResult Installed = InstallAt(FIntPoint(1000, 1000));
    const FFFZSCommandResult Neighbor = InstallAt(FIntPoint(2000, 1000));
    const FFFZSCommandResult Edge = InstallAt(FIntPoint(9000, 9000));
    if (!TestTrue(TEXT("Three initial facilities fit their original footprints"),
        Installed.bSucceeded && Neighbor.bSucceeded && Edge.bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Installed.FacilityActor.Get();
    const FGuid FacilityId = Facility->GetFacilityInstanceId();
    const auto Upgrade = [](const FGuid Id)
    {
        FFFZSCommand Command = MakeFacilityCommand(EFFZSCommandType::Upgrade, Id);
        Command.TargetUpgradeLevel = 2;
        Command.UpgradeServiceDurationMultiplier = 0.8f;
        Command.UpgradeSizeMultiplier = 2.f;
        return Command;
    };
    const FTransform OriginalTransform = Facility->GetActorTransform();
    const int64 BeforeFailure = Subsystem->GetWorldRevision();
    const FFFZSPlacementValidation Collision = Subsystem->ValidateUpgradePlacement(FacilityId, 2.f);
    TestFalse(TEXT("Enlargement preview rejects neighboring occupied space"), Collision.bIsValid);
    TestTrue(TEXT("Enlargement reports occupied geometry"),
        Collision.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision) ||
        Collision.Reasons.Contains(EFFZSPlacementFailure::InsufficientClearance) ||
        Collision.Reasons.Contains(EFFZSPlacementFailure::QueueFootprintBlocked));
    TestFalse(TEXT("Colliding enlargement cannot commit"), Subsystem->ExecuteCommand(Upgrade(FacilityId)).bSucceeded);
    TestEqual(TEXT("Colliding enlargement preserves revision"), Subsystem->GetWorldRevision(), BeforeFailure);
    TestEqual(TEXT("Colliding enlargement preserves dimensions"), Facility->GetFacilitySpec().FootprintSizeCells, Base.FootprintSizeCells);
    TestTrue(TEXT("Colliding enlargement preserves actor placement"), Facility->GetActorTransform().Equals(OriginalTransform));
    const FFFZSPlacementValidation Outside = Subsystem->ValidateUpgradePlacement(Edge.FacilityActor->GetFacilityInstanceId(), 2.f);
    TestTrue(TEXT("Enlargement validates zone boundaries against new geometry"), Outside.Reasons.Contains(EFFZSPlacementFailure::OutsideZone));

    if (!TestTrue(TEXT("Remove neighboring blocker"), Subsystem->ExecuteCommand(
        MakeFacilityCommand(EFFZSCommandType::Remove, Neighbor.FacilityActor->GetFacilityInstanceId())).bSucceeded)) return false;
    UFFZSTestExternalPolicy* RejectingPolicy = NewObject<UFFZSTestExternalPolicy>();
    RejectingPolicy->PolicyName = TEXT("Upgrade.SizeReject");
    RejectingPolicy->bAllowTentativeApply = false;
    Subsystem->RegisterExternalPolicy(RejectingPolicy);
    const int64 BeforeRollback = Subsystem->GetWorldRevision();
    TestFalse(TEXT("Late policy can reject a spatially valid enlargement"), Subsystem->ExecuteCommand(Upgrade(FacilityId)).bSucceeded);
    TestEqual(TEXT("Rejected size transaction invokes rollback"), RejectingPolicy->RollbackCount, 1);
    TestEqual(TEXT("Rollback preserves the old dimensions"), Facility->GetFacilitySpec().FootprintSizeCells, Base.FootprintSizeCells);
    TestEqual(TEXT("Rollback preserves the original revision"), Subsystem->GetWorldRevision(), BeforeRollback);
    Subsystem->UnregisterExternalPolicy(RejectingPolicy);
    if (!TestTrue(TEXT("Enlargement succeeds after space and policies allow it"), Subsystem->ExecuteCommand(Upgrade(FacilityId)).bSucceeded)) return false;
    TestEqual(TEXT("The authoritative footprint doubles"), Facility->GetFacilitySpec().FootprintSizeCells, FIntPoint(1200, 600));
    TestEqual(TEXT("Original installation footprint is retained"), Facility->GetBaseFacilitySpec().FootprintSizeCells, Base.FootprintSizeCells);
    TestEqual(TEXT("The paid physical size is stored"), Facility->GetUpgradeSizeMultiplier(), 2.f);
    TestTrue(TEXT("Actor centre updates while placement origin stays fixed"),
        Facility->GetActorLocation().Equals(OriginalTransform.GetLocation() + FVector(300, 150, 0)));
    TestFalse(TEXT("New authoritative occupancy blocks an install in the enlarged area"), InstallAt(FIntPoint(2000, 1000)).bSucceeded);

    const FFFZSWorldSnapshot Saved = Subsystem->CaptureWorldSnapshot();
    const FTransform EnlargedTransform = Facility->GetActorTransform();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Enlarged world snapshot restores"), Subsystem->RestoreWorldSnapshot(Saved, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    Facility = Subsystem->FindFacility(FacilityId);
    TestEqual(TEXT("Restore applies saved size only once"), Facility->GetFacilitySpec().FootprintSizeCells, FIntPoint(1200, 600));
    TestEqual(TEXT("Restore retains the original base dimensions"), Facility->GetBaseFacilitySpec().FootprintSizeCells, Base.FootprintSizeCells);
    TestTrue(TEXT("Restore validates and keeps the enlarged centre"), Facility->GetActorTransform().Equals(EnlargedTransform));
    TestFalse(TEXT("Restored occupancy covers the enlarged area"), InstallAt(FIntPoint(2000, 1000)).bSucceeded);

    FFFZSFacilitySpec NoLevels = Base;
    NoLevels.bUsesFacilityLevels = false;
    const FFFZSCommandResult Disabled = Subsystem->ExecuteCommand(UFFZSGridLibrary::MakeInstallCommand(
        NoLevels, ZoneId, FFFZSGridPlacement{FIntPoint(5000, 1000), 0}, -1));
    if (!TestTrue(TEXT("Disabled level facility retains its authored tiers"), Disabled.bSucceeded)) return false;
    TestFalse(TEXT("Raw upgrade cannot bypass disabled levels"),
        Subsystem->ExecuteCommand(Upgrade(Disabled.FacilityActor->GetFacilityInstanceId())).bSucceeded);
    NoLevels.bUsesFacilityLevels = true;
    NoLevels.UpgradeTiers.Reset();
    const FFFZSCommandResult LevelOne = Subsystem->ExecuteCommand(UFFZSGridLibrary::MakeInstallCommand(
        NoLevels, ZoneId, FFFZSGridPlacement{FIntPoint(7000, 1000), 0}, -1));
    if (!TestTrue(TEXT("Explicit Lv1-only facility installs"), LevelOne.bSucceeded)) return false;
    TestFalse(TEXT("Lv1-only facility cannot purchase an unauthored tier"),
        Subsystem->ExecuteCommand(Upgrade(LevelOne.FacilityActor->GetFacilityInstanceId())).bSucceeded);
    return true;
}

#endif
