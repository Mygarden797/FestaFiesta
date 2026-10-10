// FF_BuildNoRoadPlacementTests.cpp
// 역할: BuildNoRoadPlacement 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.Placement.NoRoadRequiredByProjectDefault).

// Product placement contract: roads and baked navigation are optional for
// construction; authored zone, protected-area and occupancy rules still apply.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_ProjectZoneTestFixture.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"
#include "NavigationSystem.h"
#include "Tests/AutomationCommon.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFBuildNoRoadPlacementTest,
    "FestaFiesta.Building.Placement.NoRoadRequiredByProjectDefault",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFFBuildNoRoadPlacementTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    bool bConfiguredNavigationChecks = true;
    if (!TestTrue(TEXT("The project explicitly configures its navigation placement policy"),
            GConfig->GetBool(TEXT("/Script/FF_ZoneSystem.FFZSDeveloperSettings"),
                TEXT("bEnableNavigationProjectionChecks"), bConfiguredNavigationChecks, GGameIni)) ||
        !TestFalse(TEXT("Project configuration permits construction before paths are connected"),
            bConfiguredNavigationChecks) ||
        !TestFalse(TEXT("Loaded production defaults permit construction without navigation"),
            Settings->bEnableNavigationProjectionChecks) ||
        !TestTrue(TEXT("Production ground checks remain enabled"), Settings->bEnableGroundChecks) ||
        !TestTrue(TEXT("Production world collision checks remain enabled"), Settings->bEnableWorldCollisionChecks))
    {
        return false;
    }

    // This empty spatial-authority fixture has no terrain or physics obstacles.
    // Read the real navigation default above and leave it untouched for success
    // cases. Only absent terrain/physics are excluded, with automatic restoration.
    TGuardValue<bool> GroundGuard(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> CollisionGuard(Settings->bEnableWorldCollisionChecks, false);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::EditorPreview))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Isolated world"), World) ||
        !TestNotNull(TEXT("Real placement and occupancy authority"), Zones))
    {
        return false;
    }

    // Keep a real NavigationSystem with no NavData. A null system would not
    // exercise the route gate when the separate fail-if-unavailable setting is
    // false. No navigation result is mocked and no navmesh is built here.
    UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Navigation)
    {
        Navigation = NewObject<UNavigationSystemV1>(World);
        World->SetNavigationSystem(Navigation);
    }
    if (!TestNull(TEXT("The fixture has no baked navigation data"),
            Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate)))
    {
        return false;
    }

    const FName ZoneId(FFProjectZoneTest::ZoneId());
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Construction zone"), Zone) ||
        !TestTrue(TEXT("An accepted production zone ID is used"), Zone->ApplyAcceptedZonePreset(ZoneId)) ||
        !TestTrue(TEXT("Zone registration succeeds"), Zones->RegisterZone(Zone)))
    {
        return false;
    }
    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    TArray<FText> SpecErrors;
    if (!TestTrue(TEXT("The service facility fixture is valid"), Spec.IsValid(SpecErrors)) ||
        !TestTrue(TEXT("The fixture has actual service and queue targets"),
            Spec.Category == EFFZSFacilityCategory::Booth && Spec.QueueRules.ServiceCapacity > 0 &&
            Spec.QueueRules.MaxWaitingAgents > 0 && !Spec.Anchors.IsEmpty()))
    {
        return false;
    }
    const auto MakeInstall = [&](const FIntPoint Origin)
    {
        FFFZSGridPlacement Placement;
        Placement.OriginCell = Origin;
        return UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement, Zones->GetWorldRevision());
    };
    const auto MakeMove = [&](const FGuid InstanceId, const FIntPoint Origin)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.ExpectedWorldRevision = Zones->GetWorldRevision();
        Command.Type = EFFZSCommandType::Move;
        Command.FacilityInstanceId = InstanceId;
        Command.ZoneId = ZoneId;
        Command.Placement.OriginCell = Origin;
        return Command;
    };
    const auto ExpectRejected = [&](const TCHAR* Label, const FFFZSCommand& Command,
        const EFFZSPlacementFailure Reason)
    {
        const int64 RevisionBefore = Zones->GetWorldRevision();
        const FFFZSPlacementValidation Preview = Zones->ValidatePlacement(Command);
        TestFalse(*FString::Printf(TEXT("%s: preview rejects"), Label), Preview.bIsValid);
        TestTrue(*FString::Printf(TEXT("%s: preview preserves the precise reason"), Label),
            Preview.Reasons.Contains(Reason));
        const FFFZSCommandResult Result = Zones->ExecuteCommand(Command);
        TestFalse(*FString::Printf(TEXT("%s: authoritative command rejects"), Label), Result.bSucceeded);
        TestEqual(*FString::Printf(TEXT("%s: command preserves the precise reason"), Label),
            Result.FailureReason, Reason);
        TestEqual(*FString::Printf(TEXT("%s: rejection does not mutate the world"), Label),
            Zones->GetWorldRevision(), RevisionBefore);
    };

    const FFFZSCommand FirstInstall = MakeInstall(FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("A service facility installs without any road or navigation mesh"),
            Zones->ExecuteCommand(FirstInstall).bSucceeded))
    {
        return false;
    }
    const FIntPoint MovedOrigin(3500, 1000);
    if (!TestTrue(TEXT("The same facility moves without a connected path"),
            Zones->ExecuteCommand(MakeMove(FirstInstall.FacilityInstanceId, MovedOrigin)).bSucceeded))
    {
        return false;
    }

    // Change only the navigation gate. These exact otherwise-valid destinations
    // must now fail, proving the success cases actually depend on project policy.
    const FIntPoint FreeOrigin(6000, 1000);
    TestTrue(TEXT("The control installation site is valid with the project default"),
        Zones->ValidatePlacement(MakeInstall(FreeOrigin)).bIsValid);
    TestTrue(TEXT("The control move site is valid with the project default"),
        Zones->ValidatePlacement(MakeMove(FirstInstall.FacilityInstanceId, FreeOrigin)).bIsValid);
    {
        TGuardValue<bool> NavigationGuard(Settings->bEnableNavigationProjectionChecks, true);
        ExpectRejected(TEXT("Enabling navigation rejects an unconnected installation"),
            MakeInstall(FreeOrigin), EFFZSPlacementFailure::RequiredRouteDisconnected);
        ExpectRejected(TEXT("Enabling navigation rejects an unconnected move"),
            MakeMove(FirstInstall.FacilityInstanceId, FreeOrigin), EFFZSPlacementFailure::RequiredRouteDisconnected);
    }
    TestFalse(TEXT("Navigation sensitivity control restores the project default"),
        Settings->bEnableNavigationProjectionChecks);

    const FIntPoint OccupiedOrigin(6000, 6000);
    if (!TestTrue(TEXT("An independent second facility installs without roads"),
            Zones->ExecuteCommand(MakeInstall(OccupiedOrigin)).bSucceeded))
    {
        return false;
    }
    ExpectRejected(TEXT("An installation cannot overlap an existing facility"),
        MakeInstall(OccupiedOrigin), EFFZSPlacementFailure::PlacementCollision);
    ExpectRejected(TEXT("A move cannot overlap an existing facility"),
        MakeMove(FirstInstall.FacilityInstanceId, OccupiedOrigin), EFFZSPlacementFailure::PlacementCollision);
    const FIntPoint OutsideOrigin(10500, 1000);
    ExpectRejected(TEXT("An installation cannot leave the construction zone"),
        MakeInstall(OutsideOrigin), EFFZSPlacementFailure::OutsideZone);
    ExpectRejected(TEXT("A move cannot leave the construction zone"),
        MakeMove(FirstInstall.FacilityInstanceId, OutsideOrigin), EFFZSPlacementFailure::OutsideZone);

    AFFZSProtectedArea* NoBuild = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Explicit NoBuild area"), NoBuild)) return false;
    NoBuild->ProtectedAreaId = TEXT("NoRoadPlacement_NoBuild");
    NoBuild->LinkedZoneId = ZoneId;
    NoBuild->AreaType = EFFZSProtectedAreaType::NoBuild;
    NoBuild->SetActorLocation(FVector(6050.0, 1050.0, 100.0));
    NoBuild->AreaBounds->SetBoxExtent(FVector(40.0, 40.0, 100.0));
    Zones->RegisterProtectedArea(NoBuild);
    ExpectRejected(TEXT("NoBuild still rejects installation at the formerly valid site"),
        MakeInstall(FreeOrigin), EFFZSPlacementFailure::ProtectedRouteBlocked);
    ExpectRejected(TEXT("NoBuild still rejects moving into the formerly valid site"),
        MakeMove(FirstInstall.FacilityInstanceId, FreeOrigin), EFFZSPlacementFailure::ProtectedRouteBlocked);

    TestEqual(TEXT("Rejected operations leave exactly the two successful installations"),
        Zones->CountFacilitiesByDefinitionId(Spec.FacilityDefinitionId), 2);
    const AFFZSFacilityActor* First = Zones->FindFacility(FirstInstall.FacilityInstanceId);
    if (TestNotNull(TEXT("The original facility remains registered"), First))
    {
        TestEqual(TEXT("Rejected moves preserve its successful placement"),
            First->GetGridPlacement().OriginCell, MovedOrigin);
    }
    int32 RoadCount = 0;
    for (TActorIterator<AFFZSFacilityActor> It(World); It; ++It)
    {
        RoadCount += It->GetFacilitySpec().Category == EFFZSFacilityCategory::Road ? 1 : 0;
    }
    TestEqual(TEXT("No road tile was installed anywhere in the fixture"), RoadCount, 0);
    TestNull(TEXT("No navigation data was generated during construction"),
        Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate));
    return true;
}

#endif
