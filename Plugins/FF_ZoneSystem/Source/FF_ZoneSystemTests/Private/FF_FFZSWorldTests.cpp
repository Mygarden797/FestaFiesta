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
    /** Keeps editor/project configuration unchanged after each automation test. */
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
    if (!TestNotNull(TEXT("Automation world"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Authoritative world subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("Accepted Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("Zone registers once its accepted ID is assigned"), Subsystem->RegisterZone(Zone)))
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
    TestTrue(TEXT("Install commits"), FirstInstallResult.bSucceeded);
    TestEqual(TEXT("Install advances world revision once"), Subsystem->GetWorldRevision(), int64(1));
    AFFZSFacilityActor* FirstFacility = Subsystem->FindFacility(InstallFirst.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed Facility is registered"), FirstFacility))
    {
        return false;
    }

    const FFFZSCommandResult ReplayResult = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("Same CommandId and payload replays successfully"), ReplayResult.bSucceeded && ReplayResult.bWasDuplicateReplay);
    TestEqual(TEXT("Idempotent replay does not advance revision"), Subsystem->GetWorldRevision(), int64(1));

    FFFZSCommand PayloadMismatch = InstallFirst;
    ++PayloadMismatch.Placement.OriginCell.X;
    const FFFZSCommandResult MismatchResult = Subsystem->ExecuteCommand(PayloadMismatch);
    TestEqual(TEXT("Reused CommandId with changed payload is rejected"), MismatchResult.FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);

    FFFZSCommand OverlapInstall = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
        ZoneId,
        FirstPlacement,
        1);
    const FFFZSCommandResult OverlapResult = Subsystem->ExecuteCommand(OverlapInstall);
    TestEqual(TEXT("Authoritative grid rejects overlapping footprint"), OverlapResult.FailureReason, EFFZSPlacementFailure::PlacementCollision);

    FFFZSGridPlacement StalePlacement;
    StalePlacement.OriginCell = FIntPoint(7000, 1000); // q0: 기존 70m, 10m 위치를 보존한다.
    const FFFZSCommand StaleInstall = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic),
        ZoneId,
        StalePlacement,
        0);
    const FFFZSCommandResult StaleResult = Subsystem->ExecuteCommand(StaleInstall);
    TestEqual(TEXT("Optimistic concurrency rejects a stale revision"), StaleResult.FailureReason, EFFZSPlacementFailure::StaleWorldRevision);

    const FFFZSCommand OpenFirst = MakeFacilityCommand(EFFZSCommandType::Open, InstallFirst.FacilityInstanceId, 1);
    TestTrue(TEXT("Validated Facility opens"), Subsystem->ExecuteCommand(OpenFirst).bSucceeded);

    FFFZSGridPlacement SecondPlacement;
    SecondPlacement.OriginCell = FIntPoint(4000, 1000); // q0: 기존 40m, 10m 위치를 보존한다.
    const FFFZSCommand InstallSecond = UFFZSGridLibrary::MakeInstallCommand(
        UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::ToiletBasic),
        ZoneId,
        SecondPlacement,
        2);
    const FFFZSCommandResult SecondInstallResult = Subsystem->ExecuteCommand(InstallSecond);
    TestTrue(TEXT("A separated Facility installs"), SecondInstallResult.bSucceeded);
    AFFZSFacilityActor* SecondFacility = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (!TestNotNull(TEXT("Second Facility is registered"), SecondFacility))
    {
        return false;
    }

    const FFFZSCommand OpenSecond = MakeFacilityCommand(EFFZSCommandType::Open, InstallSecond.FacilityInstanceId, 3);
    TestTrue(TEXT("Second Facility opens"), Subsystem->ExecuteCommand(OpenSecond).bSucceeded);
    TestTrue(TEXT("Navigation/Safety adapter can block a Facility Queue through world authority"),
        Subsystem->SetFacilityQueueBlocked(InstallSecond.FacilityInstanceId, true));
    TestEqual(TEXT("Blocked facade changes the Queue state"),
        SecondFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Blocked);
    FGuid BlockedTicket;
    TestEqual(TEXT("A blocked Queue rejects new Agents"),
        Subsystem->JoinFacilityQueue(InstallSecond.FacilityInstanceId, TEXT("Agent.Blocked"), nullptr, BlockedTicket),
        EFFZSQueueJoinResult::NotOpen);
    TestTrue(TEXT("Navigation/Safety adapter can reopen the Queue through world authority"),
        Subsystem->SetFacilityQueueBlocked(InstallSecond.FacilityInstanceId, false));
    TestEqual(TEXT("Unblocked Queue returns to Open"),
        SecondFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Open);
    TestFalse(TEXT("Unknown Facility cannot be blocked"), Subsystem->SetFacilityQueueBlocked(FGuid::NewGuid(), true));

    const FName AgentId(TEXT("Agent.WorldFlow"));
    FGuid TicketId;
    TestEqual(TEXT("Stable ID can join through world authority"),
        Subsystem->JoinFacilityQueue(InstallFirst.FacilityInstanceId, AgentId, nullptr, TicketId),
        EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("Queue ticket is assigned"), TicketId.IsValid());
    TestTrue(TEXT("Agent arrival at FIFO slot is accepted"),
        Subsystem->ReportQueueSlotArrival(AgentId, FirstFacility->GetQueueComponent()->GetQueueSlotWorldLocation(0)));
    TestEqual(TEXT("FIFO head transfers to a service slot only after queue-slot arrival"),
        FirstFacility->GetQueueComponent()->GetActiveServiceCount(), 1);
    TestTrue(TEXT("Service timer is armed only after Service-anchor arrival"),
        Subsystem->ReportServicePointArrival(AgentId, FirstFacility->GetQueueComponent()->GetServiceWorldLocation(0)));

    FGuid DuplicateTicket;
    TestEqual(TEXT("One Agent cannot own queues at two Facilities"),
        Subsystem->JoinFacilityQueue(InstallSecond.FacilityInstanceId, AgentId, nullptr, DuplicateTicket),
        EFFZSQueueJoinResult::DuplicateAgent);

    const FFFZSCommand OccupiedMove = MakeMoveCommand(InstallFirst.FacilityInstanceId, ZoneId, FIntPoint(1000, 4000), 4);
    TestEqual(TEXT("Occupied Facility cannot move"),
        Subsystem->ExecuteCommand(OccupiedMove).FailureReason,
        EFFZSPlacementFailure::FacilityOccupied);

    const FFFZSCommand EmergencyCloseFirst = MakeFacilityCommand(EFFZSCommandType::EmergencyClose, InstallFirst.FacilityInstanceId, 4);
    TestTrue(TEXT("Emergency close cancels the active queue"), Subsystem->ExecuteCommand(EmergencyCloseFirst).bSucceeded);
    TestFalse(TEXT("Emergency close releases global Agent ownership"), Subsystem->FindAgentQueueFacility(AgentId).IsValid());

    const FFFZSCommand CloseSecond = MakeFacilityCommand(EFFZSCommandType::Close, InstallSecond.FacilityInstanceId, 5);
    TestTrue(TEXT("Normal close succeeds"), Subsystem->ExecuteCommand(CloseSecond).bSucceeded);
    TestEqual(TEXT("An empty closed Facility becomes MovePending"), SecondFacility->GetFacilityState(), EFFZSFacilityState::MovePending);

    const FFFZSCommand MoveSecond = MakeMoveCommand(InstallSecond.FacilityInstanceId, ZoneId, FIntPoint(4000, 4000), 6);
    const FFFZSCommandResult MoveResult = Subsystem->ExecuteCommand(MoveSecond);
    TestTrue(TEXT("Drained Facility moves transactionally"), MoveResult.bSucceeded);
    SecondFacility = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (!TestNotNull(TEXT("Moved Facility remains registered"), SecondFacility))
    {
        return false;
    }

    FVector RaisedLocation = SecondFacility->GetActorLocation();
    RaisedLocation.Z = 123.0;
    SecondFacility->SetActorLocation(RaisedLocation, false, nullptr, ETeleportType::TeleportPhysics);
    SecondFacility->SetNeedsRelocation(true);

    const FFFZSCommand ReopenFirst = MakeFacilityCommand(EFFZSCommandType::Open, InstallFirst.FacilityInstanceId, 7);
    TestTrue(TEXT("Closed Facility can reopen"), Subsystem->ExecuteCommand(ReopenFirst).bSucceeded);
    const FName SavedAgentId(TEXT("Agent.SaveRestore"));
    FGuid SavedTicket;
    TestEqual(TEXT("Agent joins before snapshot"),
        Subsystem->JoinFacilityQueue(InstallFirst.FacilityInstanceId, SavedAgentId, nullptr, SavedTicket),
        EFFZSQueueJoinResult::Joined);
    TestTrue(TEXT("Saved Agent reaches its assigned queue slot"),
        Subsystem->ReportQueueSlotArrival(SavedAgentId, FirstFacility->GetQueueComponent()->GetQueueSlotWorldLocation(0)));

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    TestEqual(TEXT("Snapshot schema is current"), Snapshot.SchemaVersion, 4);
    TestEqual(TEXT("Snapshot captures both Facilities"), Snapshot.Facilities.Num(), 2);
    TestTrue(TEXT("Snapshot persists durable command receipts"), Snapshot.CommandReceipts.Num() > 0);
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
    TestFalse(TEXT("A mid-stage spawn failure rejects snapshot restore"),
        Subsystem->RestoreWorldSnapshot(Snapshot, InjectedRestoreErrors));
    World->RemoveOnActorSpawnedHandler(DestroySecondStagedFacilityHandle);

    TestEqual(TEXT("Failure injection reached the second staged Facility"), StagedFacilitySpawnCount, 2);
    TestTrue(TEXT("A staged restore failure reports an error"), !InjectedRestoreErrors.IsEmpty());
    TestTrue(TEXT("Failed staging preserves the first live Facility Actor"),
        Subsystem->FindFacility(InstallFirst.FacilityInstanceId) == FirstFacility && IsValid(FirstFacility));
    TestTrue(TEXT("Failed staging preserves the second live Facility Actor"),
        Subsystem->FindFacility(InstallSecond.FacilityInstanceId) == SecondFacility && IsValid(SecondFacility));
    TestEqual(TEXT("Failed staging preserves world revision"), Subsystem->GetWorldRevision(), int64(8));
    TestEqual(TEXT("Failed staging preserves global Queue ownership"),
        Subsystem->FindAgentQueueFacility(SavedAgentId), InstallFirst.FacilityInstanceId);
    TestEqual(TEXT("Failed staging preserves durable command receipts"),
        Subsystem->CaptureWorldSnapshot().CommandReceipts.Num(), Snapshot.CommandReceipts.Num());
    int32 LiveFacilityActorCount = 0;
    for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
    {
        if (IsValid(*It))
        {
            ++LiveFacilityActorCount;
        }
    }
    TestEqual(TEXT("Failed staging destroys every temporary Facility Actor"), LiveFacilityActorCount, 2);

    FFFZSWorldSnapshot InvalidStateSnapshot = Snapshot;
    if (FFFZSFacilityInstanceSnapshot* SavedFirst = InvalidStateSnapshot.Facilities.FindByPredicate(
        [&InstallFirst](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == InstallFirst.FacilityInstanceId; }))
    {
        SavedFirst->State = EFFZSFacilityState::MovePending;
    }
    TArray<FText> InvalidSnapshotErrors;
    TestFalse(TEXT("Snapshot rejects incompatible Facility/Queue states"),
        Subsystem->RestoreWorldSnapshot(InvalidStateSnapshot, InvalidSnapshotErrors));
    TestEqual(TEXT("Rejected snapshot leaves revision untouched"), Subsystem->GetWorldRevision(), int64(8));

    FFFZSWorldSnapshot InvalidAllowedZoneSnapshot = Snapshot;
    if (FFFZSFacilityInstanceSnapshot* SavedFirst = InvalidAllowedZoneSnapshot.Facilities.FindByPredicate(
        [&InstallFirst](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == InstallFirst.FacilityInstanceId; }))
    {
        SavedFirst->FacilitySpec.AllowedZoneIds = {TEXT("GZ_Operations")};
    }
    InvalidSnapshotErrors.Reset();
    TestFalse(TEXT("Snapshot enforces Facility AllowedZoneIds"),
        Subsystem->RestoreWorldSnapshot(InvalidAllowedZoneSnapshot, InvalidSnapshotErrors));

    const FFFZSCommand RemoveSecond = MakeFacilityCommand(EFFZSCommandType::Remove, InstallSecond.FacilityInstanceId, 8);
    TestTrue(TEXT("Second Facility can be removed after snapshot"), Subsystem->ExecuteCommand(RemoveSecond).bSucceeded);
    TestNull(TEXT("Removed Facility is no longer registered"), Subsystem->FindFacility(InstallSecond.FacilityInstanceId));
    const FFFZSCommandResult RemovedFacilityReplay = Subsystem->ExecuteCommand(InstallSecond);
    TestTrue(TEXT("Completed receipt still replays after its Facility is removed"), RemovedFacilityReplay.bSucceeded && RemovedFacilityReplay.bWasDuplicateReplay);
    TestNull(TEXT("Receipt replay resolves a removed Facility reference to null"), RemovedFacilityReplay.FacilityActor);

    TArray<FText> RestoreErrors;
    TestTrue(TEXT("Valid world snapshot restores atomically"), Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors));
    TestTrue(TEXT("Snapshot restore emits no validation errors"), RestoreErrors.IsEmpty());
    TestEqual(TEXT("Snapshot restore restores its authoritative revision"), Subsystem->GetWorldRevision(), int64(8));

    AFFZSFacilityActor* RestoredSecond = Subsystem->FindFacility(InstallSecond.FacilityInstanceId);
    if (TestNotNull(TEXT("Removed Facility is recreated from snapshot"), RestoredSecond))
    {
        TestTrue(TEXT("Ground-adjusted Z is preserved exactly"), FMath::IsNearlyEqual(RestoredSecond->GetActorLocation().Z, 123.0, 0.01));
        TestTrue(TEXT("GIS relocation flag is preserved"), RestoredSecond->NeedsRelocation());
    }
    TestEqual(TEXT("Queue ownership is rebuilt from snapshot"),
        Subsystem->FindAgentQueueFacility(SavedAgentId),
        InstallFirst.FacilityInstanceId);

    const FFFZSCommandResult RestoredReplay = Subsystem->ExecuteCommand(InstallFirst);
    TestTrue(TEXT("Command idempotency survives save/restore"), RestoredReplay.bSucceeded && RestoredReplay.bWasDuplicateReplay);
    TestEqual(TEXT("Restored replay does not change current revision"), Subsystem->GetWorldRevision(), int64(8));
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
    if (!TestNotNull(TEXT("Automation world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Authoritative world subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    AFFZSGameplayZone* OverlappingZone = SpawnAcceptedZone(*World, TEXT("GZ_DadaYajang"));
    if (!TestNotNull(TEXT("Primary Zone"), Zone) || !TestNotNull(TEXT("Overlapping Zone"), OverlappingZone))
    {
        return false;
    }
    TestTrue(TEXT("Primary Zone registers"), Subsystem->RegisterZone(Zone));
    TestTrue(TEXT("Distinct-ID overlapping Zone registers for validation"), Subsystem->RegisterZone(OverlappingZone));

    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000); // q0: 기존 10m 물리 위치를 보존한다.
    const FFFZSFacilitySpec FoodTruck = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    const FFFZSCommand AmbiguousInstall = UFFZSGridLibrary::MakeInstallCommand(FoodTruck, ZoneId, Placement, 0);
    TestEqual(TEXT("Every reserved sample must belong to exactly one Zone"),
        Subsystem->ExecuteCommand(AmbiguousInstall).FailureReason,
        EFFZSPlacementFailure::OutsideZone);

    const FFFZSPlacementValidation AmbiguousValidation = Subsystem->ValidatePlacement(AmbiguousInstall);
    if (!TestTrue(TEXT("Ambiguous validation still reports its compressed reserved regions"), !AmbiguousValidation.ReservedRects.IsEmpty()))
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
    TestTrue(TEXT("A neighboring Zone touching only the reserved-cell boundary is not an interior overlap"),
        Subsystem->ValidatePlacement(AmbiguousInstall).bIsValid);

    OverlappingZone->SetActorTransform(FTransform(FRotator(0.0, 90.0, 0.0), FVector(10000.0, 0.0, 0.0)));
    const FFFZSPlacementValidation RotatedOverlapValidation = Subsystem->ValidatePlacement(AmbiguousInstall);
    TestFalse(TEXT("A cardinal-yaw rotated Zone with coincident world geometry remains ambiguous"),
        RotatedOverlapValidation.bIsValid);
    TestTrue(TEXT("Rotated Zone ambiguity preserves the OutsideZone failure reason"),
        RotatedOverlapValidation.Reasons.Contains(EFFZSPlacementFailure::OutsideZone));

    Subsystem->UnregisterZone(OverlappingZone);
    OverlappingZone->Destroy();

    FActorSpawnParameters ProtectedParameters;
    ProtectedParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSProtectedArea* ProtectedArea = World->SpawnActor<AFFZSProtectedArea>(
        AFFZSProtectedArea::StaticClass(),
        FTransform(FRotator::ZeroRotator, FVector(1050.0, 1050.0, 100.0)),
        ProtectedParameters);
    if (!TestNotNull(TEXT("Protected Area"), ProtectedArea))
    {
        return false;
    }
    const UFFZSDeveloperSettings* ProtectedSettings = GetDefault<UFFZSDeveloperSettings>();
    ProtectedArea->RequiredClearWidthCm = 0.0f;
    ProtectedArea->AreaType = EFFZSProtectedAreaType::GeneralRoute;
    TestEqual(TEXT("General Route width zero inherits Project Settings"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->GeneralRouteWidthCm);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::EmergencyServiceRoute;
    TestEqual(TEXT("Emergency Route width zero inherits Project Settings"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->EmergencyServiceRouteWidthCm);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::AccessibilityRoute;
    TestEqual(TEXT("Accessibility Route width zero inherits Project Settings"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), ProtectedSettings->AccessibilityRouteWidthCm);
    ProtectedArea->RequiredClearWidthCm = 275.0f;
    TestEqual(TEXT("A positive per-Actor protected width overrides Project Settings"),
        ProtectedArea->GetEffectiveRequiredClearWidthCm(), 275.0f);
    ProtectedArea->ProtectedAreaId = TEXT("PA_TestNoBuild");
    ProtectedArea->LinkedZoneId = ZoneId;
    ProtectedArea->AreaType = EFFZSProtectedAreaType::NoBuild;
    ProtectedArea->RequiredClearWidthCm = 0.0f;
    ProtectedArea->AreaBounds->SetBoxExtent(FVector(40.0, 40.0, 100.0));
    Subsystem->RegisterProtectedArea(ProtectedArea);

    const FFFZSCommand ProtectedInstall = UFFZSGridLibrary::MakeInstallCommand(FoodTruck, ZoneId, Placement, 0);
    TestEqual(TEXT("No-build volume rejects an intersecting Facility"),
        Subsystem->ExecuteCommand(ProtectedInstall).FailureReason,
        EFFZSPlacementFailure::ProtectedRouteBlocked);
    TestEqual(TEXT("Rejected commands do not advance world revision"), Subsystem->GetWorldRevision(), int64(0));

    Subsystem->UnregisterProtectedArea(ProtectedArea);
    ProtectedArea->AreaType = EFFZSProtectedAreaType::GeneralRoute;
    ProtectedArea->RequiredClearWidthCm = 300.0f;
    ProtectedArea->AreaBounds->SetBoxExtent(FVector(40.0, 300.0, 100.0));
    ProtectedArea->SetActorRotation(FRotator(0.0, 45.0, 0.0));
    TArray<FText> ProtectedErrors;
    TestTrue(TEXT("Yaw-rotated Protected Area data remains valid"), ProtectedArea->ValidateProtectedAreaData(ProtectedErrors));

    const FVector ProtectedCenter = ProtectedArea->GetActorLocation();
    const FVector LocalShortAxis = ProtectedArea->GetActorForwardVector();
    const FVector ExpandedPoint = ProtectedCenter + LocalShortAxis * 120.0;
    TestTrue(TEXT("Required width expands the rotated local short axis"),
        ProtectedArea->IntersectsWorldBox(FBox(ExpandedPoint - FVector(5.0, 5.0, 10.0), ExpandedPoint + FVector(5.0, 5.0, 10.0))));

    const FVector AabbOnlyPoint = ProtectedCenter + FVector(300.0, 0.0, 0.0);
    TestFalse(TEXT("Rotated OBB avoids world-AABB false positives"),
        ProtectedArea->IntersectsWorldBox(FBox(AabbOnlyPoint - FVector(5.0, 5.0, 10.0), AabbOnlyPoint + FVector(5.0, 5.0, 10.0))));

    ProtectedArea->SetActorRotation(FRotator(5.0, 45.0, 0.0));
    ProtectedErrors.Reset();
    TestFalse(TEXT("Protected Areas reject pitch/roll"), ProtectedArea->ValidateProtectedAreaData(ProtectedErrors));
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
    if (!TestNotNull(TEXT("Automation world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Authoritative world subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("Accepted Gameplay Zone"), Zone) ||
        !TestTrue(TEXT("Zone registers"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    FFFZSFacilitySpec InvalidSpec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    InvalidSpec.QueueSplinePointsCm.Reset();
    const FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(InvalidSpec, ZoneId, FFFZSGridPlacement(), 0);
    const FFFZSPlacementValidation Validation = Subsystem->ValidatePlacement(Command);

    TestFalse(TEXT("Invalid Facility spec is rejected"), Validation.bIsValid);
    TestEqual(TEXT("Validation stops at one data error"), Validation.Reasons.Num(), 1);
    TestTrue(TEXT("Failure reason is DataInvalid"), Validation.Reasons.Contains(EFFZSPlacementFailure::DataInvalid));
    TestTrue(TEXT("Footprint cells are not calculated"), Validation.FootprintCells.IsEmpty());
    TestTrue(TEXT("Queue cells are not calculated"), Validation.QueueCells.IsEmpty());
    TestTrue(TEXT("Reserved cells are not calculated"), Validation.ReservedCells.IsEmpty());
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
    if (!TestNotNull(TEXT("Portable automation world"), World))
    {
        return false;
    }
    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Portable world subsystem"), Subsystem))
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSGameplayZone* CustomZone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
    if (!TestNotNull(TEXT("Custom portable Zone"), CustomZone))
    {
        return false;
    }
    CustomZone->ZoneId = TEXT("Custom_Main");
    TestTrue(TEXT("Unrestricted mode registers a non-Festa Zone ID"), Subsystem->RegisterZone(CustomZone));
    TestFalse(TEXT("A registered Zone cannot replace its registry key through a preset"),
        CustomZone->ApplyAcceptedZonePreset(TEXT("GZ_Operations")));
    TestEqual(TEXT("Rejected preset keeps the registered Zone ID"), CustomZone->ZoneId, FName(TEXT("Custom_Main")));
    TestTrue(TEXT("Original registry key still resolves to the same Zone"),
        Subsystem->FindZone(TEXT("Custom_Main")) == CustomZone);
    TestNull(TEXT("Rejected preset does not create a stale replacement key"),
        Subsystem->FindZone(TEXT("GZ_Operations")));

    TArray<FText> GraphErrors;
    TestTrue(TEXT("Unrestricted mode validates only the registered graph"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestTrue(TEXT("Portable registered graph has no Festa missing-Zone errors"), GraphErrors.IsEmpty());

    Settings->bRestrictZoneIdsToProjectPresets = true;
    Settings->ProjectZoneIds = {TEXT("Custom_Main"), TEXT("Custom_Secondary")};
    GraphErrors.Reset();
    TestFalse(TEXT("Restricted mode requires every configured Project Zone ID"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestEqual(TEXT("Only the configured missing Zone is reported"), GraphErrors.Num(), 1);

    AFFZSGameplayZone* SecondaryZone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform(FRotator::ZeroRotator, FVector(20000.0, 0.0, 0.0)), SpawnParameters);
    if (!TestNotNull(TEXT("Second configured Zone"), SecondaryZone))
    {
        return false;
    }
    SecondaryZone->ZoneId = TEXT("Custom_Secondary");
    TestTrue(TEXT("Restricted mode registers an ID from Project Zone Ids"), Subsystem->RegisterZone(SecondaryZone));
    GraphErrors.Reset();
    TestTrue(TEXT("Configured portable Zone graph validates without Festa IDs"), Subsystem->ValidateZoneGraph(GraphErrors));
    TestTrue(TEXT("Configured portable graph emits no errors"), GraphErrors.IsEmpty());

    UFFZSFacilityDefinition* Definition = NewObject<UFFZSFacilityDefinition>();
    Definition->Spec = UFFZSGridLibrary::MakeBuiltInFacilitySpec(EFFZSBuiltInFacilityPreset::FoodTruckBasic);
    Definition->Spec.FacilityDefinitionId = TEXT("Facility.PortablePrimaryAsset");
    const FPrimaryAssetId PrimaryAssetId = Definition->GetPrimaryAssetId();
    TestEqual(TEXT("Facility definitions use the plugin-unique Primary Asset type"),
        PrimaryAssetId.PrimaryAssetType.ToString(), FString(TEXT("FFZSFacilityDefinition")));
    TestEqual(TEXT("Facility Definition ID remains the Primary Asset name"),
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
    if (!TestNotNull(TEXT("Automation world"), World))
    {
        return false;
    }

    UFFZSWorldSubsystem* Subsystem = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Authoritative world subsystem"), Subsystem))
    {
        return false;
    }

    const FName ZoneId(TEXT("GZ_DadaPocha"));
    AFFZSGameplayZone* Zone = SpawnAcceptedZone(*World, ZoneId);
    if (!TestNotNull(TEXT("Accepted Gameplay Zone"), Zone))
    {
        return false;
    }

    // Schema 3 음수 Origin도 실제 복원 성공 경로에서 검증할 수 있도록 이 테스트 전용 Zone을
    // 원점 양쪽 100 m로 넓힌다. 제품 Preset이나 다른 테스트의 Zone 경계는 변경하지 않는다.
    USplineComponent* BoundarySpline = Zone->FindComponentByClass<USplineComponent>();
    if (!TestNotNull(TEXT("Gameplay Zone boundary spline"), BoundarySpline))
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
    if (!TestTrue(TEXT("Zone registers"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }

    // A restore attempted from any command callback (including a Blueprint policy callback)
    // must fail while the command guard is active and must not alter authoritative state.
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
    TestTrue(TEXT("Facility spawn reaches the reentrant restore callback"), bRestoreCallbackReached);
    TestFalse(TEXT("Restore is rejected while a command is in flight"), bReentrantRestoreSucceeded);
    TestTrue(TEXT("Rejected reentrant restore explains the conflict"), !ReentrantRestoreErrors.IsEmpty());
    TestTrue(TEXT("Rejected reentrant restore leaves authoritative state unchanged"), bReentrantRestorePreservedState);
    TestTrue(TEXT("Outer Install still commits after the rejected callback"), InstallResult.bSucceeded);
    TestEqual(TEXT("Command guard resets after ExecuteCommand returns"), Subsystem->GetChangePhase(), EFFZSChangePhase::Reconfigure);

    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed Facility"), Facility))
    {
        return false;
    }
    const FFFZSCommand Open = MakeFacilityCommand(EFFZSCommandType::Open, Install.FacilityInstanceId, 1);
    TestTrue(TEXT("A later command succeeds after the command guard unwinds"), Subsystem->ExecuteCommand(Open).bSucceeded);

    const FName DrainingAgentId(TEXT("Agent.RestoredDrain"));
    FGuid DrainingTicket;
    TestEqual(TEXT("Agent joins before normal close"),
        Subsystem->JoinFacilityQueue(Install.FacilityInstanceId, DrainingAgentId, nullptr, DrainingTicket),
        EFFZSQueueJoinResult::Joined);
    const FFFZSCommand Close = MakeFacilityCommand(EFFZSCommandType::Close, Install.FacilityInstanceId, 2);
    TestTrue(TEXT("Normal Close starts a drain"), Subsystem->ExecuteCommand(Close).bSucceeded);
    TestEqual(TEXT("Facility remains Closed while occupied"), Facility->GetFacilityState(), EFFZSFacilityState::Closed);
    TestEqual(TEXT("Occupied normal Close uses Draining Queue state"),
        Facility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Draining);

    Subsystem->SetChangePhase(EFFZSChangePhase::LiveOperation);
    const FFFZSWorldSnapshot DrainingSnapshot = Subsystem->CaptureWorldSnapshot();
    TestEqual(TEXT("Snapshot schema includes phase and the 1cm Region contract"), DrainingSnapshot.SchemaVersion, 4);
    TestEqual(TEXT("Snapshot captures non-initial phase"), DrainingSnapshot.CurrentPhase, EFFZSChangePhase::LiveOperation);
    TestEqual(TEXT("Snapshot captures authoritative revision"), DrainingSnapshot.WorldRevision, int64(3));
    TestTrue(TEXT("Snapshot captures normal-close drain intent"),
        DrainingSnapshot.Facilities.Num() == 1 && DrainingSnapshot.Facilities[0].Queue.bDrainRequested);

    // 실제 Schema 3(1 Cell=100 cm) 표본을 만든다. 원래 cm로 저장되던 spline/anchor/Transform은
    // 그대로 두고 Cell 기반 footprint/clearance/queue만 구 단위로 되돌린다.
    FFFZSWorldSnapshot LegacyV3BaseSnapshot = DrainingSnapshot;
    LegacyV3BaseSnapshot.SchemaVersion = 3;
    if (LegacyV3BaseSnapshot.Facilities.Num() != 1)
    {
        AddError(TEXT("Schema 3 migration fixture requires exactly one Facility."));
        return false;
    }

    FFFZSFacilityInstanceSnapshot& LegacyBaseSaved = LegacyV3BaseSnapshot.Facilities[0];
    FFFZSFacilitySpec& LegacyBaseSpec = LegacyBaseSaved.FacilitySpec;
    if (LegacyBaseSpec.FootprintSizeCells.X % 100 != 0 ||
        LegacyBaseSpec.FootprintSizeCells.Y % 100 != 0 ||
        LegacyBaseSpec.ClearanceCells % 100 != 0)
    {
        AddError(TEXT("Schema 3 migration fixture requires meter-aligned built-in dimensions."));
        return false;
    }

    TArray<FIntPoint> LegacyQueueCells;
    for (const FFFZSCellRect& Rect : LegacyBaseSpec.QueueReservedRects)
    {
        if (Rect.Min.X % 100 != 0 || Rect.Min.Y % 100 != 0 ||
            Rect.Max.X % 100 != 0 || Rect.Max.Y % 100 != 0)
        {
            AddError(TEXT("Schema 3 migration fixture requires meter-aligned Queue rectangles."));
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
    if (!TestFalse(TEXT("Schema 3 fixture contains opaque command receipts"),
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
        if (!TestTrue(*FString::Printf(TEXT("%s migrates atomically to the 1cm contract"), *CaseLabel),
            Subsystem->RestoreWorldSnapshot(LegacyV3Snapshot, LegacyV3Errors)))
        {
            return false;
        }
        TestTrue(*FString::Printf(TEXT("%s migration emits no errors"), *CaseLabel),
            LegacyV3Errors.IsEmpty());

        const FFFZSWorldSnapshot MigratedV4Snapshot = Subsystem->CaptureWorldSnapshot();
        if (MigratedV4Snapshot.Facilities.Num() != 1)
        {
            AddError(FString::Printf(TEXT("%s must contain exactly one migrated Facility."), *CaseLabel));
            return false;
        }
        const FFFZSFacilityInstanceSnapshot& MigratedSaved = MigratedV4Snapshot.Facilities[0];
        TestEqual(*FString::Printf(TEXT("%s saves as Schema 4"), *CaseLabel),
            MigratedV4Snapshot.SchemaVersion, 4);
        TestEqual(*FString::Printf(TEXT("%s FacilitySpec uses Schema 2"), *CaseLabel),
            MigratedSaved.FacilitySpec.SchemaVersion, 2);
        TestEqual(*FString::Printf(TEXT("%s preserves the 6m x 3m footprint"), *CaseLabel),
            MigratedSaved.FacilitySpec.FootprintSizeCells, FIntPoint(600, 300));
        TestEqual(*FString::Printf(TEXT("%s preserves the 1m clearance"), *CaseLabel),
            MigratedSaved.FacilitySpec.ClearanceCells, 100);
        TestTrue(*FString::Printf(TEXT("%s removes legacy Queue cells"), *CaseLabel),
            MigratedSaved.FacilitySpec.QueueReservedCells.IsEmpty());

        const TArray<FFFZSCellRect>& MigratedQueueRects =
            MigratedSaved.FacilitySpec.QueueReservedRects;
        TestEqual(*FString::Printf(TEXT("%s compresses Queue cells into three exact bands"), *CaseLabel),
            MigratedQueueRects.Num(), 3);
        TestEqual(*FString::Printf(TEXT("%s preserves the exact 160,000 cm2 Queue union"), *CaseLabel),
            UFFZSGridLibrary::GetCellRectArea(MigratedQueueRects), static_cast<int64>(160'000));
        TestTrue(*FString::Printf(TEXT("%s preserves the left Queue band"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(0, 300), FIntPoint(100, 800))));
        TestTrue(*FString::Printf(TEXT("%s preserves the right Queue band"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(500, 300), FIntPoint(600, 800))));
        TestTrue(*FString::Printf(TEXT("%s preserves the top Queue band"), *CaseLabel),
            MigratedQueueRects.Contains(
                FFFZSCellRect(FIntPoint(0, 800), FIntPoint(600, 900))));

        TestEqual(*FString::Printf(TEXT("%s applies the exact corrected Origin"), *CaseLabel),
            MigratedSaved.Placement.OriginCell, ExpectedMigratedOrigins[QuarterTurn]);
        TestEqual(*FString::Printf(TEXT("%s preserves its normalized rotation"), *CaseLabel),
            MigratedSaved.Placement.QuarterTurns, QuarterTurn);
        TestTrue(*FString::Printf(TEXT("%s preserves the physical center"), *CaseLabel),
            MigratedSaved.WorldTransform.GetLocation().Equals(ExpectedPhysicalCenter, 0.1));
        TestTrue(*FString::Printf(TEXT("%s preserves the physical rotation"), *CaseLabel),
            MigratedSaved.WorldTransform.GetRotation().AngularDistance(ExpectedPhysicalRotation) <=
                FMath::DegreesToRadians(0.01));

        TestEqual(*FString::Printf(TEXT("%s preserves the receipt count"), *CaseLabel),
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
            TestTrue(*FString::Printf(TEXT("%s preserves receipt %s"),
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
            TestFalse(TEXT("A restored Schema 3 receipt rejects a new v4 payload with the same CommandId"),
                MismatchResult.bSucceeded);
            TestEqual(TEXT("Opaque legacy receipt mismatch has the dedicated failure code"),
                MismatchResult.FailureReason, EFFZSPlacementFailure::DuplicatePayloadMismatch);
            TestEqual(TEXT("Receipt mismatch does not mutate WorldRevision"),
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
    TestFalse(TEXT("Schema 3 adjusted-Origin overflow is rejected before staging"),
        Subsystem->RestoreWorldSnapshot(
            AdjustedOriginOverflowSnapshot, AdjustedOriginOverflowErrors));
    TestTrue(TEXT("Adjusted-Origin overflow reports its precise migration boundary"),
        AdjustedOriginOverflowErrors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("after rotation-origin correction"));
        }));
    TestTrue(TEXT("Adjusted-Origin overflow preserves the registered Facility"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeOverflow);
    TestEqual(TEXT("Adjusted-Origin overflow preserves WorldRevision"),
        Subsystem->GetWorldRevision(), RevisionBeforeOverflow);
    TestEqual(TEXT("Adjusted-Origin overflow preserves phase"),
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
    TestFalse(TEXT("Schema 4 Queue-only overflow cannot restore a partial authority Region"),
        Subsystem->RestoreWorldSnapshot(QueueOverflowV4Snapshot, QueueOverflowV4Errors));
    TestTrue(TEXT("Schema 4 Queue-only overflow names the incomplete compressed Region"),
        QueueOverflowV4Errors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("complete compressed 1 cm placement region"));
        }));
    TestTrue(TEXT("Schema 4 Queue-only overflow preserves the registered Facility"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeOverflow);
    TestEqual(TEXT("Schema 4 Queue-only overflow preserves WorldRevision"),
        Subsystem->GetWorldRevision(), RevisionBeforeOverflow);
    TestEqual(TEXT("Schema 4 Queue-only overflow preserves phase"),
        Subsystem->GetChangePhase(), PhaseBeforeOverflow);

    Subsystem->SetChangePhase(EFFZSChangePhase::MorningCheck);
    AFFZSFacilityActor* FacilityBeforeRejectedRestore = Subsystem->FindFacility(Install.FacilityInstanceId);
    FFFZSWorldSnapshot LegacySnapshot = DrainingSnapshot;
    LegacySnapshot.SchemaVersion = 2;
    TArray<FText> LegacyErrors;
    TestFalse(TEXT("Schema 2 snapshot is explicitly rejected"), Subsystem->RestoreWorldSnapshot(LegacySnapshot, LegacyErrors));
    TestTrue(TEXT("Schema 2 rejection explains the missing CurrentPhase"),
        LegacyErrors.ContainsByPredicate([](const FText& Error)
        {
            return Error.ToString().Contains(TEXT("CurrentPhase"));
        }));
    TestTrue(TEXT("Rejected legacy snapshot preserves Facility identity"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeRejectedRestore);
    TestEqual(TEXT("Rejected legacy snapshot preserves phase"), Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);
    TestEqual(TEXT("Rejected legacy snapshot preserves revision"), Subsystem->GetWorldRevision(), int64(3));
    TestEqual(TEXT("Rejected legacy snapshot preserves Queue ownership"),
        Subsystem->FindAgentQueueFacility(DrainingAgentId), Install.FacilityInstanceId);

    // Off-thread restore must reject before reading subsystem/world state.
    const TPair<bool, int32> OffThreadRestore = Async(EAsyncExecution::ThreadPool,
        [Subsystem, DrainingSnapshot]()
        {
            TArray<FText> Errors;
            const bool bSucceeded = Subsystem->RestoreWorldSnapshot(DrainingSnapshot, Errors);
            return TPair<bool, int32>(bSucceeded, Errors.Num());
        }).Get();
    TestFalse(TEXT("Off-Game-Thread restore is rejected"), OffThreadRestore.Key);
    TestTrue(TEXT("Off-Game-Thread restore reports an error"), OffThreadRestore.Value > 0);
    TestTrue(TEXT("Off-thread rejection preserves Facility identity"),
        Subsystem->FindFacility(Install.FacilityInstanceId) == FacilityBeforeRejectedRestore);
    TestEqual(TEXT("Off-thread rejection preserves phase"), Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);

    FFFZSWorldSnapshot InvalidPhaseSnapshot = DrainingSnapshot;
    InvalidPhaseSnapshot.CurrentPhase = static_cast<EFFZSChangePhase>(255);
    TArray<FText> InvalidPhaseErrors;
    TestFalse(TEXT("Snapshot rejects an invalid phase enum"),
        Subsystem->RestoreWorldSnapshot(InvalidPhaseSnapshot, InvalidPhaseErrors));
    TestEqual(TEXT("Invalid phase rejection preserves current phase"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::MorningCheck);

    // A command attempted while restore stages an Actor must fail without mutating the old world.
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
    TestTrue(TEXT("Valid snapshot restores after prior validation failures"),
        Subsystem->RestoreWorldSnapshot(DrainingSnapshot, RestoreErrors));
    World->RemoveOnActorSpawnedHandler(CommandDuringRestoreHandle);
    TestTrue(TEXT("Staged Facility spawn reaches the reentrant command callback"), bCommandCallbackReached);
    TestEqual(TEXT("Command is rejected while restore is in progress"),
        ReentrantCommandResult.FailureReason, EFFZSPlacementFailure::InternalError);
    TestTrue(TEXT("Rejected reentrant command leaves old authoritative state unchanged"), bReentrantCommandPreservedState);
    TestTrue(TEXT("Successful restore emits no errors"), RestoreErrors.IsEmpty());
    TestEqual(TEXT("Restore applies the saved LiveOperation phase atomically"),
        Subsystem->GetChangePhase(), EFFZSChangePhase::LiveOperation);
    TestEqual(TEXT("Restore applies the saved revision atomically"), Subsystem->GetWorldRevision(), int64(3));

    AFFZSFacilityActor* RestoredFacility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Restored Facility"), RestoredFacility))
    {
        return false;
    }
    TestTrue(TEXT("Successful restore replaces the old Facility Actor"), RestoredFacility != FacilityBeforeRejectedRestore);
    TestEqual(TEXT("Restored normal-close Facility remains Closed while draining"),
        RestoredFacility->GetFacilityState(), EFFZSFacilityState::Closed);
    TestEqual(TEXT("Restored Queue remains Draining"),
        RestoredFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Draining);
    TestTrue(TEXT("Restore reconstructs the transient MovePending-after-drain intent"),
        RestoredFacility->bSetMovePendingWhenDrained);

    TestTrue(TEXT("Restored Agent can leave through world authority"),
        Subsystem->LeaveCurrentFacilityQueue(DrainingAgentId, EFFZSQueueReleaseReason::RemovedBySystem));
    TestEqual(TEXT("Removing the final Agent closes the restored draining Queue"),
        RestoredFacility->GetQueueComponent()->GetQueueState(), EFFZSQueueState::Closed);
    World->Tick(ELevelTick::LEVELTICK_All, 0.2f);
    TestEqual(TEXT("Restored normal-close drain finishes in MovePending"),
        RestoredFacility->GetFacilityState(), EFFZSFacilityState::MovePending);

    const FFFZSCommand ReopenAfterRestore = MakeFacilityCommand(EFFZSCommandType::Open, Install.FacilityInstanceId, 3);
    TestTrue(TEXT("Restore guard unwinds after successful restore"),
        Subsystem->ExecuteCommand(ReopenAfterRestore).bSucceeded);
    return true;
}
#endif