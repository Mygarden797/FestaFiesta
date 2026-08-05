/**
 * @file FF_FFZSWorldTests.cpp
 * @brief WorldSubsystem의 권위 상태와 여러 Actor가 함께 만드는 통합 계약을 검증한다.
 *
 * 테스트 행렬:
 * 1) Command lifecycle, Queue 이동, Snapshot 복원과 동일 Command replay의 멱등성.
 * 2) Cell의 exactly-one Zone 소유, 회전 보호영역과 경계 접촉 판정.
 * 3) 잘못된 Facility Spec이 Nav/Policy 같은 비싼 단계 전에 중단되는가.
 * 4) 사용자 정의 Zone graph와 고유 Primary Asset Type의 다른 프로젝트 이식성.
 * 5) Schema 4의 1cm Region·CurrentPhase, 재진입 guard, Close/Drain/MovePending 의도 복원.
 *
 * 각 테스트는 transient Editor World를 사용하고 설정 CDO를 RAII로 복구한다. 실제 Dadaepo 맵,
 * Production Nav/Economy Adapter 또는 성능 수치를 검증하는 테스트는 아니다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "Tests/AutomationEditorCommon.h"

#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSQueueComponent.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"

namespace
{
    /** 각 자동화 테스트가 끝난 뒤 Editor/Project 설정을 원래 상태로 유지한다. */
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
            , ProjectZoneIds(Settings->ProjectZoneIds)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
            Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
        }

        ~FScopedZFTestSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bProductionPolicies;
            Settings->bRestrictZoneIdsToProjectPresets = bRestrictZoneIds;
            Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
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
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
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

    const FFFZSCommandResult ReplayResult = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("동일한 CommandId와 Payload가 성공적으로 Replay된다"), ReplayResult.bSucceeded && ReplayResult.bWasDuplicateReplay);
    TestEqual(TEXT("멱등 Replay는 Revision을 증가시키지 않는다"), Subsystem->GetWorldRevision(), int64(1));

    FFFZSCommand PayloadMismatch = InstallFirst;
    ++PayloadMismatch.Placement.OriginCell.X;
    const FFFZSCommandResult MismatchResult = Subsystem->ExecuteCommand(PayloadMismatch);
    TestEqual(TEXT("변경된 Payload로 CommandId를 재사용하면 거부된다"), MismatchResult.FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);

    FFFZSCommand OverlapInstall = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
        ZoneId,
        FirstPlacement,
        1);
    const FFFZSCommandResult OverlapResult = Subsystem->ExecuteCommand(OverlapInstall);
    TestEqual(TEXT("권위 Grid는 겹치는 Footprint를 거부한다"), OverlapResult.FailureReason, EFFZSPlacementFailure::PlacementCollision);

    FFFZSGridPlacement StalePlacement;
    StalePlacement.OriginCell = FIntPoint(7000, 1000); // q0: 기존 70m, 10m 위치를 보존한다.
    const FFFZSCommand StaleInstall = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
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
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::ToiletBasic),
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
    TestEqual(TEXT("점유 중인 Facility는 이동할 수 없다"),
        Subsystem->ExecuteCommand(OccupiedMove).FailureReason,
        EFFZSPlacementFailure::FacilityOccupied);

    const FFFZSCommand EmergencyCloseFirst = MakeFacilityCommand(EFFZSCommandType::EmergencyClose, InstallFirst.FacilityInstanceId, 4);
    TestTrue(TEXT("Emergency Close가 활성 Queue를 취소한다"), Subsystem->ExecuteCommand(EmergencyCloseFirst).bSucceeded);
    TestFalse(TEXT("Emergency Close가 전역 Agent 소유권을 해제한다"), Subsystem->FindAgentQueueFacility(AgentId).IsValid());

    const FFFZSCommand CloseSecond = MakeFacilityCommand(EFFZSCommandType::Close, InstallSecond.FacilityInstanceId, 5);
    TestTrue(TEXT("Normal Close가 성공한다"), Subsystem->ExecuteCommand(CloseSecond).bSucceeded);
    TestEqual(TEXT("비어 있는 Closed Facility는 MovePending 상태가 된다"), SecondFacility->GetFacilityState(), EFFZSFacilityState::MovePending);

    const FFFZSCommand MoveSecond = MakeMoveCommand(InstallSecond.FacilityInstanceId, ZoneId, FIntPoint(4000, 4000), 6);
    const FFFZSCommandResult MoveResult = Subsystem->ExecuteCommand(MoveSecond);
    TestTrue(TEXT("Drain이 끝난 Facility가 트랜잭션으로 이동한다"), MoveResult.bSucceeded);
    SecondFacility = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (!TestNotNull(TEXT("이동된 Facility는 계속 등록되어 있다"), SecondFacility))
    {
        return false;
    }

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
    TestEqual(TEXT("Snapshot Schema가 현재 버전이다"), Snapshot.SchemaVersion, 4);
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
    const FFFZSFacilitySpec FoodTruck = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
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

    FFFZSFacilitySpec InvalidSpec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
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
    Definition->Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Definition->Spec.FacilityDefinitionId = TEXT("Facility.PortablePrimaryAsset");
    const FPrimaryAssetId PrimaryAssetId = Definition->GetPrimaryAssetId();
    TestEqual(TEXT("Facility Definition은 플러그인 고유 Primary Asset Type을 사용한다"),
        PrimaryAssetId.PrimaryAssetType.ToString(), FString(TEXT("FFZSFacilityDefinition")));
    TestEqual(TEXT("Facility Definition ID는 Primary Asset Name으로 유지된다"),
        PrimaryAssetId.PrimaryAssetName, Definition->Spec.FacilityDefinitionId);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSSnapshotPhaseReentrancyAndDrainIntentTest,
    "FF_ZoneSystem.Integration.World.SnapshotPhaseReentrancyAndDrainIntent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// 복원 계약: Schema 4의 1cm Region·Phase/Drain intent와 Command·Restore·Policy 재진입 차단을 함께 검증한다.
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
    Subsystem->SetChangePhase(EFFZSChangePhase::Reconfigure);

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000); // q0: 기존 10m 물리 위치를 보존한다.
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
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
    TestEqual(TEXT("Command Guard는 ExecuteCommand 반환 후 Reset된다"), Subsystem->GetChangePhase(), EFFZSChangePhase::Reconfigure);

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
    TestEqual(TEXT("Snapshot Schema는 Phase와 1cm Region 계약을 포함한다"), DrainingSnapshot.SchemaVersion, 4);
    TestEqual(TEXT("Snapshot이 초기값이 아닌 Phase를 캡처한다"), DrainingSnapshot.CurrentPhase, EFFZSChangePhase::LiveOperation);
    TestEqual(TEXT("Snapshot이 권위 Revision을 캡처한다"), DrainingSnapshot.WorldRevision, int64(3));
    TestTrue(TEXT("Snapshot이 Normal Close의 Drain Intent를 캡처한다"),
        DrainingSnapshot.Facilities.Num() == 1 && DrainingSnapshot.Facilities[0].Queue.bDrainRequested);

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
        AddError(TEXT("Schema 3 마이그레이션 Fixture의 내장 치수는 미터 단위에 맞춰져 있어야 합니다."));
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

    // Schema 3 영수증은 v4 명령 지문이 아니며 런타임이 해석하지 않는 opaque 값이다.
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

        const FFFZSWorldSnapshot MigratedV4Snapshot = Subsystem->CaptureWorldSnapshot();
        if (MigratedV4Snapshot.Facilities.Num() != 1)
        {
            AddError(FString::Printf(TEXT("%s: 마이그레이션된 Facility가 정확히 하나 있어야 합니다."), *CaseLabel));
            return false;
        }
        const FFFZSFacilityInstanceSnapshot& MigratedSaved = MigratedV4Snapshot.Facilities[0];
        TestEqual(*FString::Printf(TEXT("%s: Schema 4로 저장된다"), *CaseLabel),
            MigratedV4Snapshot.SchemaVersion, 4);
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
        TestEqual(*FString::Printf(TEXT("%s: 정규화된 Rotation을 보존한다"), *CaseLabel),
            MigratedSaved.Placement.QuarterTurns, QuarterTurn);
        TestTrue(*FString::Printf(TEXT("%s: 물리적 중심을 보존한다"), *CaseLabel),
            MigratedSaved.WorldTransform.GetLocation().Equals(ExpectedPhysicalCenter, 0.1));
        TestTrue(*FString::Printf(TEXT("%s: 물리적 Rotation을 보존한다"), *CaseLabel),
            MigratedSaved.WorldTransform.GetRotation().AngularDistance(ExpectedPhysicalRotation) <=
                FMath::DegreesToRadians(0.01));

        TestEqual(*FString::Printf(TEXT("%s: Receipt 개수를 보존한다"), *CaseLabel),
            MigratedV4Snapshot.CommandReceipts.Num(),
            LegacyV3BaseSnapshot.CommandReceipts.Num());
        for (const FFFZSCommandReceiptSnapshot& LegacyReceipt : LegacyV3BaseSnapshot.CommandReceipts)
        {
            const FFFZSCommandReceiptSnapshot* MigratedReceipt =
                MigratedV4Snapshot.CommandReceipts.FindByPredicate(
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
            // 같은 구 CommandId를 새 v4 Payload로 재사용하면 opaque 지문을 덮지 않고 충돌로 거부한다.
            FFFZSCommand ReusedCommandId = MakeFacilityCommand(
                EFFZSCommandType::Open, Install.FacilityInstanceId);
            ReusedCommandId.CommandId = LegacyV3BaseSnapshot.CommandReceipts[0].CommandId;
            const int64 RevisionBeforeMismatch = Subsystem->GetWorldRevision();
            const FFFZSCommandResult MismatchResult = Subsystem->ExecuteCommand(ReusedCommandId);
            TestFalse(TEXT("복원된 Schema 3 Receipt는 동일 CommandId의 새 v4 Payload를 거부한다"),
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

    // Schema 4에서도 footprint/expanded는 표현되지만 FoodTruck Queue Y Max만 int32를 넘는 표본이다.
    // 빈 ReservedRects를 정상 "시설 없음"으로 오인하면 점유·Zone 검사를 우회하므로 전체 복원을 거절한다.
    FFFZSWorldSnapshot QueueOverflowV4Snapshot = DrainingSnapshot;
    FFFZSFacilityInstanceSnapshot& QueueOverflowV4Saved = QueueOverflowV4Snapshot.Facilities[0];
    QueueOverflowV4Saved.Placement.OriginCell = FIntPoint(1000, MAX_int32 - 899);
    QueueOverflowV4Saved.Placement.QuarterTurns = 0;
    QueueOverflowV4Saved.WorldTransform = FTransform(
        Zone->GetActorQuat(),
        Zone->GetActorTransform().TransformPosition(FVector(
            1300.0, static_cast<double>(MAX_int32) - 749.0, 0.0)),
        FVector::OneVector);
    TArray<FText> QueueOverflowV4Errors;
    TestFalse(TEXT("Schema 4의 Queue 전용 Overflow는 불완전한 권위 Region을 복원할 수 없다"),
        Subsystem->RestoreWorldSnapshot(QueueOverflowV4Snapshot, QueueOverflowV4Errors));
    TestTrue(TEXT("Schema 4의 Queue 전용 Overflow가 불완전한 압축 Region을 명시한다"),
        QueueOverflowV4Errors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("완전한 압축 1cm 배치 영역"));
        }));
    TestTrue(TEXT("Schema 4의 Queue 전용 Overflow 발생 후 등록된 Facility가 보존된다"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeOverflow);
    TestEqual(TEXT("Schema 4의 Queue 전용 Overflow 발생 후 WorldRevision이 보존된다"),
        Subsystem->GetWorldRevision(), RevisionBeforeOverflow);
    TestEqual(TEXT("Schema 4의 Queue 전용 Overflow 발생 후 Phase가 보존된다"),
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
#endif
