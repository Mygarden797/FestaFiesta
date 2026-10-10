#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "FF_FFZSTestExternalPolicy.h"
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
#endif
