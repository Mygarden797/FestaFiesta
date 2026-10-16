#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSQueueComponent.h"
#include "Components/SplineComponent.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FFZSTestExternalPolicy.h"
#include "FF_FFZSConstructionHistoryTestPolicy.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "FF_FFZSProjectZoneTestFixture.h"

namespace
{
    struct FConstructionTestSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        TGuardValue<bool> Ground{Settings->bEnableGroundChecks, false};
        TGuardValue<bool> Collision{Settings->bEnableWorldCollisionChecks, false};
        TGuardValue<bool> Navigation{Settings->bEnableNavigationProjectionChecks, false};
        TGuardValue<bool> Policies{Settings->bRequireProductionPolicyDomains, false};
    };
    UFFZSWorldSubsystem* CreateConstructionWorld(FAutomationTestBase& Test)
    {
        UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
        if (!Test.TestNotNull(TEXT("Construction fixture world"), World)) return nullptr;
        UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
        AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
        if (!Test.TestNotNull(TEXT("Construction fixture subsystem"), Zones) ||
            !FFZSTest::ApplyZonePreset(Test, Zone) ||
            !Test.TestTrue(TEXT("Construction fixture zone registers"), Zones->RegisterZone(Zone))) return nullptr;
        Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
        return Zones;
    }
    FFFZSCommand InstallAt(FIntPoint Position)
    {
        FFFZSGridPlacement Placement;
        Placement.OriginCell = Position;
        return UFFZSGridLibrary::MakeInstallCommand(FFZSTest::MakeValidFacilitySpec(), FFZSTest::ProjectZoneId(), Placement, -1);
    }
    FFFZSCommand Edit(const FFFZSCommand& Install, EFFZSCommandType Type, FIntPoint Position, int32 Yaw = 0)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.Type = Type;
        Command.FacilityInstanceId = Install.FacilityInstanceId;
        Command.ZoneId = Install.ZoneId;
        Command.Placement.OriginCell = Position;
        Command.Placement.YawCentidegrees = Yaw;
        return Command;
    }
    FFFZSCommand StrokeTileAt(FIntPoint Position, FGuid TransactionId)
    {
        FFFZSFacilitySpec Spec;
        Spec.FacilityDefinitionId = TEXT("Tile_HistoryStrokeFixture");
        Spec.DisplayName = FText::FromString(TEXT("History stroke tile"));
        Spec.Category = EFFZSFacilityCategory::Tile;
        Spec.FootprintSizeCells = FIntPoint(100, 100);
        Spec.ClearanceCells = 0;
        Spec.QueueRules.MaxWaitingAgents = 0;
        Spec.QueueRules.ServiceCapacity = 0;
        FFFZSGridPlacement Placement;
        Placement.OriginCell = Position;
        FFFZSCommand Command = UFFZSGridLibrary::MakeInstallCommand(Spec, FFZSTest::ProjectZoneId(), Placement, -1);
        Command.TransactionId = TransactionId;
        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSConstructionUndoRedoTest,
    "FF_ZoneSystem.Construction.History.SingleEditsIdentityRevisionAndBranch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSConstructionUndoRedoTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Construction world"), Zones)) return false;
    const FFFZSCommand Install = InstallAt(FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Install"), Zones->ExecuteCommand(Install).bSucceeded)) return false;
    FText Error;
    TestTrue(TEXT("Undo install"), Zones->UndoConstruction(Error));
    TestNull(TEXT("Undo removes its facility"), Zones->FindFacility(Install.FacilityInstanceId));
    const int64 UndoRevision = Zones->GetWorldRevision();
    TestTrue(TEXT("Original receipt is retained"), Zones->ExecuteCommand(Install).bWasDuplicateReplay);
    TestNull(TEXT("Receipt replay cannot recreate an undone facility"), Zones->FindFacility(Install.FacilityInstanceId));
    if (!TestTrue(TEXT("Redo install"), Zones->RedoConstruction(Error))) return false;
    TestTrue(TEXT("Undo and redo revisions only advance"), Zones->GetWorldRevision() > UndoRevision);
    TestTrue(TEXT("Replayed install remains directly undoable after queue canonicalization"), Zones->UndoConstruction(Error));
    TestTrue(TEXT("Repeated redo remains available"), Zones->RedoConstruction(Error));
    FFFZSCommand Move = Edit(Install, EFFZSCommandType::Move, FIntPoint(4000, 3000), 9000);
    if (!TestTrue(TEXT("Move and rotate"), Zones->ExecuteCommand(Move).bSucceeded)) return false;
    TestTrue(TEXT("Undo move"), Zones->UndoConstruction(Error));
    AFFZSFacilityActor* Restored = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Restored stable ID"), Restored)) return false;
    TestEqual(TEXT("Position restored"), Restored->GetGridPlacement().OriginCell, Install.Placement.OriginCell);
    TestEqual(TEXT("Rotation restored"), Restored->GetGridPlacement().YawCentidegrees, 0);
    TestTrue(TEXT("Older install entry stays undoable after moving backwards through history"), Zones->UndoConstruction(Error));
    TestNull(TEXT("Multi-step undo reaches empty world"), Zones->FindFacility(Install.FacilityInstanceId));
    TestTrue(TEXT("Redo older install"), Zones->RedoConstruction(Error));
    TestTrue(TEXT("Redo move"), Zones->RedoConstruction(Error));
    const FFFZSCommand Remove = Edit(Install, EFFZSCommandType::Remove, FIntPoint::ZeroValue);
    TestTrue(TEXT("Remove"), Zones->ExecuteCommand(Remove).bSucceeded);
    TestTrue(TEXT("Undo remove"), Zones->UndoConstruction(Error));
    Restored = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Removed facility resurrected with same ID"), Restored)) return false;
    TestEqual(TEXT("Removed position preserved"), Restored->GetGridPlacement().OriginCell, Move.Placement.OriginCell);
    FFFZSCommand Branch = Edit(Install, EFFZSCommandType::Move, FIntPoint(1000, 4000));
    TestTrue(TEXT("New edit after undo"), Zones->ExecuteCommand(Branch).bSucceeded);
    TestFalse(TEXT("New edit clears redo branch"), Zones->CanRedoConstruction());
    const FFFZSWorldSnapshot Save = Zones->CaptureWorldSnapshot();
    TArray<FText> Errors;
    TestTrue(TEXT("Restore save"), Zones->RestoreWorldSnapshot(Save, Errors));
    TestFalse(TEXT("Save restore clears history"), Zones->CanUndoConstruction());
    TestTrue(TEXT("Move after loading"), Zones->ExecuteCommand(Edit(Install, EFFZSCommandType::Move, FIntPoint(2000, 2000))).bSucceeded);
    Zones->SetChangePhase(EFFZSChangePhase::LiveOperation);
    TestFalse(TEXT("Operation cannot undo"), Zones->UndoConstruction(Error));
    Zones->SetChangePhase(EFFZSChangePhase::MorningCheck);
    TestFalse(TEXT("Old phase history is discarded"), Zones->CanUndoConstruction());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSConstructionBatchAtomicTest,
    "FF_ZoneSystem.Construction.History.AtomicGroupsSwapRollbackAndIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSConstructionBatchAtomicTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Construction world"), Zones)) return false;
    const FFFZSCommand A = InstallAt(FIntPoint(1000, 1000));
    const FFFZSCommand B = InstallAt(FIntPoint(4000, 1000));
    const FFFZSCommand Unrelated = InstallAt(FIntPoint(1000, 5000));
    if (!TestTrue(TEXT("Install A"), Zones->ExecuteCommand(A).bSucceeded)
        || !TestTrue(TEXT("Install B"), Zones->ExecuteCommand(B).bSucceeded)
        || !TestTrue(TEXT("Install unrelated"), Zones->ExecuteCommand(Unrelated).bSucceeded)) return false;
    AFFZSFacilityActor* UnrelatedActor = Zones->FindFacility(Unrelated.FacilityInstanceId);
    Zones->ClearConstructionHistory();
    TArray<FFFZSCommand> Commands{
        Edit(A, EFFZSCommandType::Move, B.Placement.OriginCell),
        Edit(B, EFFZSCommandType::Move, A.Placement.OriginCell)};
    Commands[1].TransactionId = Commands[0].TransactionId;
    FText Error;
    TestTrue(TEXT("Preview allows crossing original group positions"), Zones->ValidateConstructionBatch(Commands, Error));
    if (!TestTrue(TEXT("Atomic swap"), Zones->ExecuteConstructionBatch(Commands).bSucceeded)) return false;
    TestEqual(TEXT("A at B location"), Zones->FindFacility(A.FacilityInstanceId)->GetGridPlacement().OriginCell, B.Placement.OriginCell);
    TestEqual(TEXT("B at A location"), Zones->FindFacility(B.FacilityInstanceId)->GetGridPlacement().OriginCell, A.Placement.OriginCell);
    TestTrue(TEXT("Entire group duplicate replay is harmless"), Zones->ExecuteConstructionBatch(Commands).bWasDuplicateReplay);
    TestTrue(TEXT("Group is one undo step"), Zones->UndoConstruction(Error));
    TestFalse(TEXT("No member-level history remains"), Zones->CanUndoConstruction());
    TestEqual(TEXT("A original position restored"), Zones->FindFacility(A.FacilityInstanceId)->GetGridPlacement().OriginCell, A.Placement.OriginCell);
    TestTrue(TEXT("Unrelated actor identity survives undo"), Zones->FindFacility(Unrelated.FacilityInstanceId) == UnrelatedActor);
    const int64 BeforeFailure = Zones->GetWorldRevision();
    Commands[0].CommandId = FGuid::NewGuid();
    Commands[1].CommandId = FGuid::NewGuid();
    Commands[1].Placement.OriginCell = FIntPoint(999999, 999999);
    TestFalse(TEXT("Invalid second member rejects whole group"), Zones->ExecuteConstructionBatch(Commands).bSucceeded);
    TestEqual(TEXT("Failed group keeps revision"), Zones->GetWorldRevision(), BeforeFailure);
    TestEqual(TEXT("Valid first member also stays put"), Zones->FindFacility(A.FacilityInstanceId)->GetGridPlacement().OriginCell, A.Placement.OriginCell);
    TestTrue(TEXT("Failed group preserves redo"), Zones->CanRedoConstruction());
    UFFZSTestExternalPolicy* Policy = NewObject<UFFZSTestExternalPolicy>(Zones);
    Policy->bAllowTentativeApply = false;
    Zones->RegisterExternalPolicy(Policy);
    Commands[1].Placement.OriginCell = A.Placement.OriginCell;
    TestFalse(TEXT("Tentative policy failure rejects group"), Zones->ExecuteConstructionBatch(Commands).bSucceeded);
    TestEqual(TEXT("No policy commits on rejected group"), Policy->CommitCount, 0);
    TestEqual(TEXT("Failing policy rolled back"), Policy->RollbackCount, 1);
    TestEqual(TEXT("Policy rejection preserves revision"), Zones->GetWorldRevision(), BeforeFailure);
    Zones->UnregisterExternalPolicy(Policy);
    const bool bRedone = Zones->RedoConstruction(Error);
    TestTrue(*FString::Printf(TEXT("Redo still available after rejection: %s"), *Error.ToString()), bRedone);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSConstructionPlacementStrokeTest,
    "FF_ZoneSystem.Construction.History.PlacementStrokeGroupingAndSessionBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSConstructionPlacementStrokeTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Stroke world"), Zones)) return false;
    const FGuid TransactionId = FGuid::NewGuid();
    const FFFZSCommand A = StrokeTileAt(FIntPoint(1000, 1000), TransactionId);
    const FFFZSCommand B = StrokeTileAt(FIntPoint(1200, 1000), TransactionId);
    FText Error;
    TestFalse(TEXT("Invalid transaction cannot begin a stroke"), Zones->BeginConstructionPlacementStroke(FGuid()));
    TestTrue(TEXT("Press begins stroke"), Zones->BeginConstructionPlacementStroke(TransactionId));
    TestFalse(TEXT("Nested begin cannot change the active session"), Zones->BeginConstructionPlacementStroke(FGuid::NewGuid()));
    FFFZSCommand InvalidOrigin = StrokeTileAt(FIntPoint(-1000, -1000), TransactionId);
    TestFalse(TEXT("Invalid initial candidate is skipped"), Zones->ExecuteCommand(InvalidOrigin).bSucceeded);
    TestTrue(TEXT("Rejected origin preserves session"), Zones->IsConstructionPlacementStrokeActive(TransactionId));
    if (!TestTrue(TEXT("First successful stroke install"), Zones->ExecuteCommand(A).bSucceeded)) return false;
    TestTrue(TEXT("Duplicate receipt does not create another history item"), Zones->ExecuteCommand(A).bWasDuplicateReplay);
    const FFFZSCommand Collision = StrokeTileAt(A.Placement.OriginCell, TransactionId);
    TestFalse(TEXT("Rejected intermediate candidate is skipped"), Zones->ExecuteCommand(Collision).bSucceeded);
    if (!TestTrue(TEXT("Second successful stroke install"), Zones->ExecuteCommand(B).bSucceeded)) return false;
    Zones->EndConstructionPlacementStroke(FGuid::NewGuid());
    TestTrue(TEXT("Wrong owner cannot close stroke"), Zones->IsConstructionPlacementStrokeActive(TransactionId));
    Zones->EndConstructionPlacementStroke(TransactionId);
    TestFalse(TEXT("Release closes stroke"), Zones->IsConstructionPlacementStrokeActive(TransactionId));

    const FFFZSCommand Independent = StrokeTileAt(FIntPoint(1600, 1000), TransactionId);
    TestTrue(TEXT("Same transaction without begin installs independently"), Zones->ExecuteCommand(Independent).bSucceeded);
    TestTrue(TEXT("Undo independent install"), Zones->UndoConstruction(Error));
    TestNull(TEXT("Only independent install removed"), Zones->FindFacility(Independent.FacilityInstanceId));
    TestNotNull(TEXT("Previous stroke survives first undo"), Zones->FindFacility(A.FacilityInstanceId));
    TestTrue(TEXT("Undo all successful stroke installs once"), Zones->UndoConstruction(Error));
    TestNull(TEXT("First stroke member removed"), Zones->FindFacility(A.FacilityInstanceId));
    TestNull(TEXT("Second stroke member removed"), Zones->FindFacility(B.FacilityInstanceId));
    TestFalse(TEXT("Skipped candidates and duplicates leave no history"), Zones->CanUndoConstruction());
    TestTrue(TEXT("Redo stroke once"), Zones->RedoConstruction(Error));
    TestNotNull(TEXT("First stroke member restored"), Zones->FindFacility(A.FacilityInstanceId));
    TestNotNull(TEXT("Second stroke member restored"), Zones->FindFacility(B.FacilityInstanceId));

    TestTrue(TEXT("Empty press begins"), Zones->BeginConstructionPlacementStroke(TransactionId));
    Zones->EndConstructionPlacementStroke(TransactionId);
    TestTrue(TEXT("Empty press preserves redo branch"), Zones->CanRedoConstruction());
    TestTrue(TEXT("Reused transaction begins a fresh stroke generation"), Zones->BeginConstructionPlacementStroke(TransactionId));
    const FFFZSCommand Later = StrokeTileAt(FIntPoint(1800, 1000), TransactionId);
    TestTrue(TEXT("New stroke install"), Zones->ExecuteCommand(Later).bSucceeded);
    Zones->EndConstructionPlacementStroke(TransactionId);
    TestTrue(TEXT("Undo newer session once"), Zones->UndoConstruction(Error));
    TestNull(TEXT("New session removed"), Zones->FindFacility(Later.FacilityInstanceId));
    TestNotNull(TEXT("Reused transaction cannot merge earlier session"), Zones->FindFacility(A.FacilityInstanceId));

    TestTrue(TEXT("Interrupted press begins"), Zones->BeginConstructionPlacementStroke(TransactionId));
    const FFFZSCommand Interrupted = StrokeTileAt(FIntPoint(2000, 1000), TransactionId);
    TestTrue(TEXT("Interrupted stroke member installs"), Zones->ExecuteCommand(Interrupted).bSucceeded);
    const FFFZSCommand OtherEdit = StrokeTileAt(FIntPoint(2200, 1000), FGuid::NewGuid());
    TestTrue(TEXT("Other successful edit"), Zones->ExecuteCommand(OtherEdit).bSucceeded);
    TestFalse(TEXT("Other edit closes stroke"), Zones->IsConstructionPlacementStrokeActive(TransactionId));
    TestTrue(TEXT("Undo only other edit"), Zones->UndoConstruction(Error));
    TestNotNull(TEXT("Interrupted stroke remains independently undoable"), Zones->FindFacility(Interrupted.FacilityInstanceId));
    TestTrue(TEXT("Clear-bound stroke begins"), Zones->BeginConstructionPlacementStroke(TransactionId));
    Zones->ClearConstructionHistory();
    TestFalse(TEXT("Clear closes session"), Zones->IsConstructionPlacementStrokeActive(TransactionId));
    TestFalse(TEXT("Clear discards undo history"), Zones->CanUndoConstruction());
    Zones->SetChangePhase(EFFZSChangePhase::LiveOperation);
    TestFalse(TEXT("Operation cannot begin a construction stroke"), Zones->BeginConstructionPlacementStroke(TransactionId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSConstructionLargePlacementStrokeTest,
    "FF_ZoneSystem.Construction.History.LargePlacementStrokeAtomicReplayAndPolicyReceipts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSConstructionLargePlacementStrokeTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Large stroke world"), Zones)) return false;
    const FFFZSCommand Unrelated = StrokeTileAt(FIntPoint(7000, 7000), FGuid::NewGuid());
    if (!TestTrue(TEXT("Unrelated tile installs"), Zones->ExecuteCommand(Unrelated).bSucceeded)) return false;
    AFFZSFacilityActor* UnrelatedActor = Zones->FindFacility(Unrelated.FacilityInstanceId);
    Zones->ClearConstructionHistory();
    const FGuid TransactionId = FGuid::NewGuid();
    if (!TestTrue(TEXT("Large stroke begins"), Zones->BeginConstructionPlacementStroke(TransactionId))) return false;
    TArray<FFFZSCommand> Commands;
    TArray<AFFZSFacilityActor*> OriginalActors;
    for (int32 Index = 0; Index < 129; ++Index)
    {
        const FFFZSCommand Command = StrokeTileAt(FIntPoint(1000 + Index % 16 * 200, 1000 + Index / 16 * 200), TransactionId);
        if (!TestTrue(*FString::Printf(TEXT("Large stroke member %d installs"), Index), Zones->ExecuteCommand(Command).bSucceeded)) return false;
        Commands.Add(Command);
        OriginalActors.Add(Zones->FindFacility(Command.FacilityInstanceId));
    }
    Zones->EndConstructionPlacementStroke(TransactionId);
    FText Error;
    TestFalse(TEXT("Public preview still rejects groups above 128"), Zones->ValidateConstructionBatch(Commands, Error));
    TestFalse(TEXT("Public execute still rejects groups above 128"), Zones->ExecuteConstructionBatch(Commands).bSucceeded);

    UFFZSConstructionHistoryTestPolicy* Policy = NewObject<UFFZSConstructionHistoryTestPolicy>(Zones);
    Policy->Zones = Zones;
    Policy->RejectTentativeCall = 65;
    Zones->RegisterExternalPolicy(Policy);
    const int64 BeforeUndoRevision = Zones->GetWorldRevision();
    TestFalse(TEXT("Late policy rejection aborts the full undo"), Zones->UndoConstruction(Error));
    TestEqual(TEXT("Rejected undo keeps revision"), Zones->GetWorldRevision(), BeforeUndoRevision);
    TestEqual(TEXT("Full stroke is visible to aggregate policy validation"), Policy->ObservedBatchSize, 129);
    TestEqual(TEXT("Rejected undo commits no member"), Policy->CommitCalls, 0);
    TestEqual(TEXT("Rejected member and all earlier reservations roll back"), Policy->RollbackCalls, 65);
    for (int32 Index = 0; Index < Commands.Num(); ++Index)
        TestTrue(*FString::Printf(TEXT("Rejected undo preserves actor %d"), Index), Zones->FindFacility(Commands[Index].FacilityInstanceId) == OriginalActors[Index]);
    TestTrue(TEXT("Rejected undo remains retryable"), Zones->CanUndoConstruction());
    TestFalse(TEXT("Rejected undo does not create redo"), Zones->CanRedoConstruction());

    Policy->RejectTentativeCall = INDEX_NONE;
    Policy->TentativeCalls = Policy->RollbackCalls = 0;
    if (!TestTrue(*FString::Printf(TEXT("One undo removes all 129: %s"), *Error.ToString()), Zones->UndoConstruction(Error))) return false;
    TestEqual(TEXT("Every original receipt is exposed for exact refunds"), Policy->UndoSources.Num(), 129);
    for (const FFFZSCommand& Command : Commands)
    {
        TestNull(TEXT("Stroke member removed"), Zones->FindFacility(Command.FacilityInstanceId));
        TestTrue(TEXT("Undo maps to original installation receipt"), Policy->UndoSources.Contains(Command.CommandId));
    }
    TestFalse(TEXT("Large stroke occupies exactly one history slot"), Zones->CanUndoConstruction());
    TestTrue(TEXT("Unrelated actor identity survives undo"), Zones->FindFacility(Unrelated.FacilityInstanceId) == UnrelatedActor);

    Policy->RejectTentativeCall = 65;
    Policy->TentativeCalls = Policy->CommitCalls = Policy->RollbackCalls = 0;
    const int64 BeforeRedoRevision = Zones->GetWorldRevision();
    TestFalse(TEXT("Late redo rejection aborts all staged installs"), Zones->RedoConstruction(Error));
    TestEqual(TEXT("Rejected redo keeps revision"), Zones->GetWorldRevision(), BeforeRedoRevision);
    TestEqual(TEXT("Rejected redo commits no member"), Policy->CommitCalls, 0);
    TestEqual(TEXT("Rejected redo rolls back all reservations"), Policy->RollbackCalls, 65);
    for (const FFFZSCommand& Command : Commands) TestNull(TEXT("Rejected redo leaves no partial install"), Zones->FindFacility(Command.FacilityInstanceId));
    TestEqual(TEXT("Rejected redo leaves only unrelated actor registered"), Zones->GetPlacedFacilityCount(), 1);
    TestTrue(TEXT("Rejected redo remains retryable"), Zones->CanRedoConstruction());

    Policy->RejectTentativeCall = INDEX_NONE;
    Policy->TentativeCalls = Policy->RollbackCalls = 0;
    if (!TestTrue(TEXT("One redo restores all 129"), Zones->RedoConstruction(Error))) return false;
    TestEqual(TEXT("Every original receipt is exposed for exact recharging"), Policy->RedoSources.Num(), 129);
    for (const FFFZSCommand& Command : Commands)
    {
        AFFZSFacilityActor* Facility = Zones->FindFacility(Command.FacilityInstanceId);
        if (TestNotNull(TEXT("Stable facility ID restored"), Facility))
            TestEqual(TEXT("Placement restored"), Facility->GetGridPlacement().OriginCell, Command.Placement.OriginCell);
        TestTrue(TEXT("Redo maps to original installation receipt"), Policy->RedoSources.Contains(Command.CommandId));
    }
    TestTrue(TEXT("Unrelated actor identity survives redo"), Zones->FindFacility(Unrelated.FacilityInstanceId) == UnrelatedActor);
    TestTrue(TEXT("Large stroke supports another complete undo"), Zones->UndoConstruction(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSConstructionPlacementStrokeReentrantEndTest,
    "FF_ZoneSystem.Construction.History.PlacementStrokeCallbackEndIncludesInFlightInstall",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSConstructionPlacementStrokeReentrantEndTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Callback stroke world"), Zones)) return false;
    const FGuid TransactionId = FGuid::NewGuid();
    Zones->BeginConstructionPlacementStroke(TransactionId);
    const FFFZSCommand A = StrokeTileAt(FIntPoint(1000, 1000), TransactionId);
    const FFFZSCommand B = StrokeTileAt(FIntPoint(1200, 1000), TransactionId);
    if (!TestTrue(TEXT("First stroke member installs"), Zones->ExecuteCommand(A).bSucceeded)) return false;
    UFFZSConstructionHistoryTestPolicy* Policy = NewObject<UFFZSConstructionHistoryTestPolicy>(Zones);
    Policy->Zones = Zones;
    Policy->StrokeToEndOnPreValidate = TransactionId;
    Zones->RegisterExternalPolicy(Policy);
    if (!TestTrue(TEXT("Callback closes during second member validation"), Zones->ExecuteCommand(B).bSucceeded)) return false;
    TestFalse(TEXT("End takes effect before command returns"), Zones->IsConstructionPlacementStrokeActive(TransactionId));
    TestFalse(TEXT("Callback cannot open another session"), Policy->bCallbackBeginSucceeded);
    FText Error;
    TestTrue(TEXT("One undo still includes the in-flight installation"), Zones->UndoConstruction(Error));
    TestNull(TEXT("Earlier stroke member removed"), Zones->FindFacility(A.FacilityInstanceId));
    TestNull(TEXT("In-flight stroke member removed"), Zones->FindFacility(B.FacilityInstanceId));
    TestFalse(TEXT("Callback end did not split history"), Zones->CanUndoConstruction());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSettingsHistoryTest,
    "FF_ZoneSystem.Construction.QueueSettings.EditUndoSaveAndAtomicReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueSettingsHistoryTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Queue settings world"), Zones)) return false;
    const FFFZSCommand A = InstallAt(FIntPoint(1000, 1000));
    const FFFZSCommand B = InstallAt(FIntPoint(5000, 1000));
    if (!TestTrue(TEXT("Legacy facility A installs"), Zones->ExecuteCommand(A).bSucceeded) ||
        !TestTrue(TEXT("Legacy facility B installs"), Zones->ExecuteCommand(B).bSucceeded)) return false;
    Zones->ClearConstructionHistory();
    FFFZSQueueSettings Custom;
    Custom.bEnabled = true;
    Custom.MaxWaitingTeams = 8;
    Custom.SlotSpacingCm = 150.f;
    Custom.LaneWidthCm = 300.f;
    FText Error;
    if (!TestTrue(TEXT("One facility accepts custom settings"), Zones->SetFacilityQueueSettings(A.FacilityInstanceId, Custom, EFFZSQueueLayout::Left, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    AFFZSFacilityActor* Facility = Zones->FindFacility(A.FacilityInstanceId);
    TestTrue(TEXT("User settings are retained in the instance base"), Facility->GetBaseFacilitySpec().QueueSettings == Custom);
    TestEqual(TEXT("Runtime queue uses the new team limit"), Facility->GetQueueComponent()->GetRules().MaxWaitingAgents, 8);
    TestEqual(TEXT("Runtime queue uses the new spacing"), Facility->GetQueueComponent()->GetRules().SlotSpacingCm, 150.f);
    TestTrue(TEXT("Undo custom settings"), Zones->UndoConstruction(Error));
    TestFalse(TEXT("Undo restores legacy authored layout"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestFalse(TEXT("One settings action creates one history entry"), Zones->CanUndoConstruction());
    TestTrue(TEXT("Redo custom settings"), Zones->RedoConstruction(Error));
    TestTrue(TEXT("Changing only shape preserves custom settings"), Zones->SetFacilityQueueLayout(A.FacilityInstanceId, EFFZSQueueLayout::Right, Error));
    TestTrue(TEXT("The custom settings survive a shape change"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    FFFZSCommand MoveSettings = Edit(A, EFFZSCommandType::Move, FIntPoint(3000, 5000), 0);
    MoveSettings.bChangeQueueSettings = true;
    MoveSettings.QueueSettings = Custom;
    MoveSettings.QueueSettings.MaxWaitingTeams = 10;
    if (!TestTrue(TEXT("Move can atomically change settings while preserving shape"), Zones->ExecuteCommand(MoveSettings).bSucceeded)) return false;
    Custom = MoveSettings.QueueSettings;
    TestTrue(TEXT("Move commits the player settings"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    TestEqual(TEXT("Settings-only Move preserves the layout"), Zones->FindFacility(A.FacilityInstanceId)->GetQueueLayout(), EFFZSQueueLayout::Right);
    const FFFZSWorldSnapshot Snapshot = Zones->CaptureWorldSnapshot();
    TArray<FText> Errors;
    TestTrue(TEXT("Custom settings save restores"), Zones->RestoreWorldSnapshot(Snapshot, Errors));
    TestTrue(TEXT("Snapshot preserves custom settings"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    Zones->ClearConstructionHistory();
    UFFZSTestExternalPolicy* RejectPolicy = NewObject<UFFZSTestExternalPolicy>(Zones);
    RejectPolicy->bAllowTentativeApply = false;
    Zones->RegisterExternalPolicy(RejectPolicy);
    const int64 BeforePolicyFailure = Zones->GetWorldRevision();
    TestFalse(TEXT("Policy rejection aborts the whole settings reset"), Zones->ResetAllFacilityQueueSettingsToDefaults().bSucceeded);
    TestEqual(TEXT("Policy rejection leaves revision unchanged"), Zones->GetWorldRevision(), BeforePolicyFailure);
    TestTrue(TEXT("Policy rejection preserves custom A"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    TestFalse(TEXT("Policy rejection preserves legacy B"), Zones->FindFacility(B.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestFalse(TEXT("Rejected reset creates no undo record"), Zones->CanUndoConstruction());
    Zones->UnregisterExternalPolicy(RejectPolicy);
    const FFFZSCommandResult Reset = Zones->ResetAllFacilityQueueSettingsToDefaults();
    if (!TestTrue(*FString::Printf(TEXT("Whole park settings reset: %s"), *Reset.Message.ToString()), Reset.bSucceeded)) return false;
    FFFZSQueueSettings Defaults;
    Defaults.bEnabled = true;
    for (const FGuid Id : {A.FacilityInstanceId, B.FacilityInstanceId})
    {
        TestTrue(TEXT("Reset applies common settings to each service facility"), Zones->FindFacility(Id)->GetBaseFacilitySpec().QueueSettings == Defaults);
        TestEqual(TEXT("Reset uses the common straight layout"), Zones->FindFacility(Id)->GetQueueLayout(), EFFZSQueueLayout::Authored);
    }
    TestTrue(TEXT("One undo restores the entire reset"), Zones->UndoConstruction(Error));
    TestTrue(TEXT("Undo reset restores custom A"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    TestFalse(TEXT("Undo reset restores legacy B"), Zones->FindFacility(B.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestFalse(TEXT("Reset has no member-level undo entries"), Zones->CanUndoConstruction());
    TestTrue(TEXT("One redo reapplies reset"), Zones->RedoConstruction(Error));
    const int64 BeforeNoop = Zones->GetWorldRevision();
    TestTrue(TEXT("Reset already common settings is harmless"), Zones->ResetAllFacilityQueueSettingsToDefaults().bSucceeded);
    TestEqual(TEXT("No-op reset creates no world mutation"), Zones->GetWorldRevision(), BeforeNoop);
    TestTrue(TEXT("Return to mixed settings"), Zones->UndoConstruction(Error));
    const FFFZSCommand Open = Edit(B, EFFZSCommandType::Open, B.Placement.OriginCell);
    TestTrue(TEXT("One affected facility opens"), Zones->ExecuteCommand(Open).bSucceeded);
    const int64 BeforeRejectedReset = Zones->GetWorldRevision();
    TestFalse(TEXT("An active facility rejects the full reset"), Zones->ResetAllFacilityQueueSettingsToDefaults().bSucceeded);
    TestEqual(TEXT("Rejected reset leaves revision unchanged"), Zones->GetWorldRevision(), BeforeRejectedReset);
    TestTrue(TEXT("Rejected reset preserves another facility's custom settings"), Zones->FindFacility(A.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings == Custom);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSettingsLargeResetTest,
    "FF_ZoneSystem.Construction.QueueSettings.ResetAbovePublicBatchLimit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSQueueSettingsLargeResetTest::RunTest(const FString& Parameters)
{
    FConstructionTestSettings Settings;
    UFFZSWorldSubsystem* Zones = CreateConstructionWorld(*this);
    if (!TestNotNull(TEXT("Large queue settings world"), Zones)) return false;
    AFFZSGameplayZone* Zone = Zones->FindZone(FFZSTest::ProjectZoneId());
    if (!TestNotNull(TEXT("Large queue test zone"), Zone)) return false;
    Zones->UnregisterZone(Zone);
    Zone->BoundarySpline->ClearSplinePoints(false);
    for (const FVector& Point : {FVector(0, 0, 0), FVector(100000, 0, 0), FVector(100000, 100000, 0), FVector(0, 100000, 0)})
    {
        const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
        Zone->BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    if (!TestTrue(TEXT("Large test boundary registers"), Zones->RegisterZone(Zone))) return false;
    TArray<FFFZSCommand> Installs;
    for (int32 Index = 0; Index < 129; ++Index)
    {
        const FFFZSCommand Command = InstallAt(FIntPoint(1000 + Index % 13 * 4000, 1000 + Index / 13 * 4000));
        if (!TestTrue(*FString::Printf(TEXT("Legacy queue member %d installs"), Index), Zones->ExecuteCommand(Command).bSucceeded)) return false;
        Installs.Add(Command);
    }
    Zones->ClearConstructionHistory();
    FText Error;
    TestFalse(TEXT("Public batch limit remains 128"), Zones->ValidateConstructionBatch(Installs, Error));
    const FFFZSCommandResult Reset = Zones->ResetAllFacilityQueueSettingsToDefaults();
    if (!TestTrue(*FString::Printf(TEXT("129 facilities reset atomically: %s"), *Reset.Message.ToString()), Reset.bSucceeded)) return false;
    for (const FFFZSCommand& Install : Installs)
        TestTrue(TEXT("Every reset member has common settings"), Zones->FindFacility(Install.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestTrue(TEXT("One undo restores all 129 legacy queues"), Zones->UndoConstruction(Error));
    for (const FFFZSCommand& Install : Installs)
        TestFalse(TEXT("Undo preserves each original facility ID and legacy settings"), Zones->FindFacility(Install.FacilityInstanceId)->GetBaseFacilitySpec().QueueSettings.bEnabled);
    TestFalse(TEXT("Large reset creates only one undo entry"), Zones->CanUndoConstruction());
    TestTrue(TEXT("One redo restores all 129 common queues"), Zones->RedoConstruction(Error));
    return true;
}

#endif
