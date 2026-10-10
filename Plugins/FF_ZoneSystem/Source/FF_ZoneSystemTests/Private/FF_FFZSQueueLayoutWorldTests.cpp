#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSSaveGameTestObject.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Kismet/GameplayStatics.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "FF_FFZSProjectZoneTestFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueLayoutWorldTest,
    "FF_ZoneSystem.Integration.QueueLayout.SaveHistoryAndObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueLayoutWorldTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> Ground(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> Collision(Settings->bEnableWorldCollisionChecks, false);
    TGuardValue<bool> Navigation(Settings->bEnableNavigationProjectionChecks, false);
    TGuardValue<bool> Policies(Settings->bRequireProductionPolicyDomains, false);
    TGuardValue<int32> Budget(Settings->MaxEnumeratedCellsPerFacility, 10'000'000);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World subsystem"), Zones)) return false;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!FFZSTest::ApplyZonePreset(*this, Zone)) return false;
    Zone->bAllowAllFacilityCategories = true;
    if (!TestTrue(TEXT("Zone registers"), Zones->RegisterZone(Zone))) return false;
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000);
    FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(FFZSTest::MakeValidFacilitySpec(), Zone->ZoneId, Placement);
    if (!TestTrue(TEXT("Facility installs"), Zones->ExecuteCommand(Install).bSucceeded)) return false;
    AFFZSFacilityActor* Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed facility"), Facility)) return false;
    const FFFZSFacilitySpec Original = Facility->GetBaseFacilitySpec();
    FText Error;
    TestTrue(TEXT("Preparation permits queue editing"), Zones->SetFacilityQueueLayout(Install.FacilityInstanceId, EFFZSQueueLayout::Right, Error));
    TestEqual(TEXT("Selected shape is stored on the instance"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);
    TestTrue(TEXT("History includes queue shape edits"), Zones->UndoConstruction(Error));
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    TestTrue(TEXT("Undo restores authored geometry"), Facility && Facility->GetQueueLayout() == EFFZSQueueLayout::Authored);
    TestTrue(TEXT("Redo restores selected queue shape"), Zones->RedoConstruction(Error));
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Facility after history"), Facility)) return false;
    TestEqual(TEXT("Redo shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);
    TestTrue(TEXT("Authored base path survives repeated editing"), Facility->GetBaseFacilitySpec().QueueSplinePointsCm == Original.QueueSplinePointsCm);

    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot = Zones->CaptureWorldSnapshot();
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("SaveGame serializes selected queue shape"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    const UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("SaveGame reloads"), Loaded)) return false;
    TArray<FText> RestoreErrors;
    TestTrue(TEXT("Saved world restores selected layout"), Zones->RestoreWorldSnapshot(Loaded->Snapshot, RestoreErrors));
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Restored facility"), Facility)) return false;
    TestEqual(TEXT("Selected shape persists through SaveGame archive"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);

    FFFZSFacilitySpec Blocker;
    Blocker.FacilityDefinitionId = TEXT("QueueLayoutBlocker");
    Blocker.DisplayName = FText::FromString(TEXT("Queue layout blocker"));
    Blocker.Category = EFFZSFacilityCategory::Lighting;
    Blocker.FootprintSizeCells = FIntPoint(50, 50);
    Blocker.ClearanceCells = 0;
    Blocker.QueueRules.MaxWaitingAgents = 0;
    Blocker.QueueRules.ServiceCapacity = 0;
    Placement.OriginCell = FIntPoint(500, 1500);
    const FFFZSCommand BlockerInstall = UFFZSGridLibrary::MakeInstallCommand(Blocker, Zone->ZoneId, Placement);
    TestTrue(TEXT("Independent blocker installs outside the current queue"), Zones->ExecuteCommand(BlockerInstall).bSucceeded);
    const int64 RevisionBeforeRejectedEdit = Zones->GetWorldRevision();
    TestFalse(TEXT("New lane cannot overlap another facility"), Zones->SetFacilityQueueLayout(Install.FacilityInstanceId, EFFZSQueueLayout::Left, Error));
    TestEqual(TEXT("Rejected edit leaves the existing layout unchanged"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);
    TestEqual(TEXT("Rejected edit leaves world revision unchanged"), Zones->GetWorldRevision(), RevisionBeforeRejectedEdit);

    // A real NavMesh is deliberately absent: required projection/connectivity must reject,
    // rather than treating generated slot positions as sufficient proof of NPC reachability.
    {
        TGuardValue<bool> RequireNav(Settings->bEnableNavigationProjectionChecks, true);
        TGuardValue<bool> FailUnavailable(Settings->bFailWhenNavigationSystemIsUnavailable, true);
        TestFalse(TEXT("Missing navigation cannot approve a new layout"),
            Zones->SetFacilityQueueLayout(Install.FacilityInstanceId, EFFZSQueueLayout::UShape, Error));
    }
    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = Zone->ZoneId;
    Move.Placement.OriginCell = FIntPoint(2800, 2000);
    Move.bChangeQueueLayout = true;
    Move.QueueLayout = EFFZSQueueLayout::Left;
    const FFFZSGridPlacement BeforeMove = Facility->GetGridPlacement();
    TestTrue(TEXT("A combined move+shape draft validates"), Zones->ValidatePlacement(Move).bIsValid);
    TestEqual(TEXT("Validation cannot mutate the selected original shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);
    TestTrue(TEXT("Move and shape commit atomically"), Zones->ExecuteCommand(Move).bSucceeded);
    TestEqual(TEXT("Move commits requested shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);
    TestTrue(TEXT("Combined move+shape has one undo step"), Zones->UndoConstruction(Error));
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Facility survives combined undo"), Facility)) return false;
    TestEqual(TEXT("Undo restores original shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Right);
    TestTrue(TEXT("Undo restores original placement"), Facility->GetGridPlacement() == BeforeMove);
    TestTrue(TEXT("Redo restores combined move and shape"), Zones->RedoConstruction(Error));
    Facility = Zones->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Facility survives combined redo"), Facility)) return false;
    TestEqual(TEXT("Redo restores new shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);
    TestTrue(TEXT("Redo restores new placement"), Facility->GetGridPlacement() == Move.Placement);
    Move.CommandId = FGuid::NewGuid();
    Move.QueueLayout = static_cast<EFFZSQueueLayout>(255);
    Move.Placement.OriginCell = FIntPoint(2000, 3000);
    TestFalse(TEXT("Unsupported shape rejects the entire move"), Zones->ExecuteCommand(Move).bSucceeded);
    TestEqual(TEXT("Rejected move keeps original queue shape"), Facility->GetQueueLayout(), EFFZSQueueLayout::Left);
    TestTrue(TEXT("Rejected move keeps original position"), Facility->GetGridPlacement().OriginCell == FIntPoint(2800, 2000));
    TestTrue(TEXT("Operation begins"), Zones->SetChangePhase(EFFZSChangePhase::LiveOperation));
    TestFalse(TEXT("Live operation cannot change queue shapes"), Zones->SetFacilityQueueLayout(Install.FacilityInstanceId, EFFZSQueueLayout::Authored, Error));
    return true;
}

#endif
