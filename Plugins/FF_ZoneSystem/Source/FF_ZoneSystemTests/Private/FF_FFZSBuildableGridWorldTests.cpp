#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSBuildGridSurface.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    struct FScopedBuildGridSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        TGuardValue<bool> Ground{Settings->bEnableGroundChecks, false};
        TGuardValue<bool> Collision{Settings->bEnableWorldCollisionChecks, false};
        TGuardValue<bool> Navigation{Settings->bEnableNavigationProjectionChecks, false};
        TGuardValue<bool> Policies{Settings->bRequireProductionPolicyDomains, false};
        TGuardValue<bool> Presets{Settings->bRestrictZoneIdsToProjectPresets, false};
    };

    AFFZSGameplayZone* MakeGridZone(UWorld& World, const FName Id, const TArray<FVector>& Polygon)
    {
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!Zone)
        {
            return nullptr;
        }
        Zone->ZoneId = Id;
        Zone->BoundarySpline->ClearSplinePoints(false);
        for (const FVector& Point : Polygon)
        {
            Zone->BoundarySpline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        }
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
        }
        Zone->BoundarySpline->SetClosedLoop(true, true);
        return Zone;
    }

    bool IsWorldCellVisible(const FFFZSBuildableGridMask& Mask, const FVector2D& LocalPoint)
    {
        return Mask.IsBuildable(
            FMath::FloorToInt((LocalPoint.X - Mask.GetMinLocalCm().X) / Mask.GetStepCm()),
            FMath::FloorToInt((LocalPoint.Y - Mask.GetMinLocalCm().Y) / Mask.GetStepCm()));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridWorldOwnershipTest,
    "FF_ZoneSystem.Integration.BuildableGrid.ZoneOwnershipProtectionAndCategory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridWorldOwnershipTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World subsystem exists"), Subsystem))
    {
        return false;
    }
    const FName ZoneId(TEXT("GZ_BuildGrid"));
    AFFZSGameplayZone* Zone = MakeGridZone(*World, ZoneId,
        {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0), FVector(0, 1000, 0)});
    if (!TestTrue(TEXT("The lot registers"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }
    FFFZSCommand Command;
    Command.ZoneId = ZoneId;
    FFFZSBuildableGridMask Mask;
    const auto Rebuild = [&]
    {
        return Subsystem->BuildPlacementGridMask(Command, FVector2D(-100.0), FVector2D(1100.0), 100, 0.0, Mask);
    };
    TestTrue(TEXT("Overview does not require a selected facility"), Rebuild());
    TestEqual(TEXT("Only the 100 complete cells inside the lot are visible"), Mask.GetBuildableCellCount(), 100);
    TestFalse(TEXT("Outside boundary cells remain dark"), IsWorldCellVisible(Mask, FVector2D(-50, 50)));

    AFFZSGameplayZone* OverlapZone = MakeGridZone(*World, TEXT("GZ_BuildGridOverlap"),
        {FVector(600, 0, 0), FVector(800, 0, 0), FVector(800, 1000, 0), FVector(600, 1000, 0)});
    TestTrue(TEXT("Overlapping second zone registers"), Subsystem->RegisterZone(OverlapZone));
    Rebuild();
    TestEqual(TEXT("Ambiguous ownership removes exactly twenty cells"), Mask.GetBuildableCellCount(), 80);
    TestFalse(TEXT("Shared zone interior is hidden"), IsWorldCellVisible(Mask, FVector2D(650, 50)));
    TestTrue(TEXT("A cell merely touching the other zone stays available"), IsWorldCellVisible(Mask, FVector2D(550, 50)));

    AFFZSProtectedArea* Protected = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Protected area exists"), Protected))
    {
        return false;
    }
    Protected->ProtectedAreaId = TEXT("PA_BuildGrid");
    Protected->LinkedZoneId = ZoneId;
    Protected->AreaType = EFFZSProtectedAreaType::NoBuild;
    Protected->AreaBounds->SetBoxExtent(FVector(45, 45, 100));
    Protected->SetActorLocation(FVector(450, 450, 100));
    Protected->SetActorRotation(FRotator(0, 45, 0));
    TestTrue(TEXT("Rotated protection registers"), Subsystem->RegisterProtectedArea(Protected));
    Rebuild();
    TestFalse(TEXT("Rotated protection hides its interior"), IsWorldCellVisible(Mask, FVector2D(450, 450)));
    TestTrue(TEXT("A distant buildable cell stays visible"), IsWorldCellVisible(Mask, FVector2D(150, 150)));
    const int32 ProtectedCellCount = Mask.GetBuildableCellCount();
    Subsystem->BuildPlacementGridMask(Command, FVector2D(-100.0), FVector2D(1100.0), 100, 600.0, Mask);
    TestEqual(TEXT("A camera hit on a high roof cannot reveal protected ground"),
        Mask.GetBuildableCellCount(), ProtectedCellCount);
    // An empty zone reference means global protection in the authority contract; it is not
    // malformed data. Check its footprint before exercising genuinely invalid fields.
    Protected->LinkedZoneId = NAME_None;
    TArray<FText> ProtectionErrors;
    TestTrue(TEXT("Global protection is valid authored data"), Protected->ValidateProtectedAreaData(ProtectionErrors));
    Rebuild();
    TestEqual(TEXT("Global protection preserves the same blocked footprint"), Mask.GetBuildableCellCount(), ProtectedCellCount);
    TestFalse(TEXT("Global protection still hides its interior"), IsWorldCellVisible(Mask, FVector2D(450, 450)));

    Protected->ProtectedAreaId = NAME_None;
    TestFalse(TEXT("A missing protected area ID is genuinely invalid data"),
        Protected->ValidateProtectedAreaData(ProtectionErrors));
    Rebuild();
    TestEqual(TEXT("Malformed registered protection fails closed"), Mask.GetBuildableCellCount(), 0);
    Protected->ProtectedAreaId = TEXT("PA_BuildGrid");
    Protected->LinkedZoneId = TEXT("GZ_MissingBuildGrid");
    Rebuild();
    TestEqual(TEXT("A nonexistent protection zone reference also fails closed"), Mask.GetBuildableCellCount(), 0);
    Protected->LinkedZoneId = ZoneId;
    Subsystem->UnregisterProtectedArea(Protected);

    Zone->bAllowAllFacilityCategories = false;
    Zone->AllowedFacilityCategories.Reset();
    Rebuild();
    TestEqual(TEXT("An all-category prohibition hides overview"), Mask.GetBuildableCellCount(), 0);
    Zone->AllowedFacilityCategories.Add(EFFZSFacilityCategory::Toilet);
    Command.FacilitySpec = FFZSTest::MakeValidFacilitySpec();
    Rebuild();
    TestEqual(TEXT("The selected category prohibition also hides the grid"), Mask.GetBuildableCellCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridWorldReservationTest,
    "FF_ZoneSystem.Integration.BuildableGrid.ReservationsAndMoveIgnore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridWorldReservationTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World subsystem exists"), Subsystem))
    {
        return false;
    }
    const FName ZoneId(TEXT("GZ_BuildGridReservations"));
    AFFZSGameplayZone* Zone = MakeGridZone(*World, ZoneId,
        {FVector(0, 0, 0), FVector(10000, 0, 0), FVector(10000, 10000, 0), FVector(0, 10000, 0)});
    if (!TestTrue(TEXT("The lot registers"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(2000, 2000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(
        FFZSTest::MakeValidFacilitySpec(), ZoneId, Placement);
    const FFFZSCommandResult Installed = Subsystem->ExecuteCommand(Install);
    if (!TestTrue(TEXT("The facility installs using authority"), Installed.bSucceeded))
    {
        return false;
    }
    FFFZSCommand Overview;
    Overview.ZoneId = ZoneId;
    FFFZSBuildableGridMask Mask;
    Subsystem->BuildPlacementGridMask(Overview, FVector2D(1000.0), FVector2D(5000.0), 100, 0.0, Mask);
    TestFalse(TEXT("The placed footprint has no grid"), IsWorldCellVisible(Mask, FVector2D(2050, 2050)));
    TestFalse(TEXT("Safety clearance also has no grid"), IsWorldCellVisible(Mask, FVector2D(1950, 2050)));
    const FFFZSPlacementValidation Validation = Subsystem->ValidatePlacement(Install);
    for (const FFFZSOrientedRect& QueueRect : Validation.QueueOrientedRects)
    {
        TestFalse(TEXT("Reserved queue space is hidden"), IsWorldCellVisible(Mask, QueueRect.CenterCell));
    }
    const int32 OccupiedCount = Mask.GetBuildableCellCount();
    Overview.Type = EFFZSCommandType::Move;
    Overview.FacilityInstanceId = Install.FacilityInstanceId;
    Subsystem->BuildPlacementGridMask(Overview, FVector2D(1000.0), FVector2D(5000.0), 100, 0.0, Mask);
    TestTrue(TEXT("Moving a facility releases its own footprint in the preview mask"),
        IsWorldCellVisible(Mask, FVector2D(2050, 2050)));
    TestTrue(TEXT("Ignoring the moving reservation restores available cells"), Mask.GetBuildableCellCount() > OccupiedCount);
    TestEqual(TEXT("Queries never mutate the authority revision"), Subsystem->GetWorldRevision(), int64(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridWorldConcavityAndObstacleTest,
    "FF_ZoneSystem.Integration.BuildableGrid.ConcaveBoundaryAndPhysicalObstacle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridWorldConcavityAndObstacleTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridSettings Settings;
    Settings.Settings->bEnableWorldCollisionChecks = true;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World subsystem exists"), Subsystem))
    {
        return false;
    }
    const FName ZoneId(TEXT("GZ_BuildGridConcave"));
    AFFZSGameplayZone* Zone = MakeGridZone(*World, ZoneId,
        {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0),
         FVector(480, 1000, 0), FVector(480, 520, 0), FVector(460, 520, 0),
         FVector(460, 1000, 0), FVector(0, 1000, 0)});
    if (!TestTrue(TEXT("The concave lot registers"), Subsystem->RegisterZone(Zone)))
    {
        return false;
    }
    FFFZSCommand Overview;
    Overview.ZoneId = ZoneId;
    FFFZSBuildableGridMask Mask;
    Subsystem->BuildPlacementGridMask(Overview, FVector2D::ZeroVector, FVector2D(1000.0), 100, 0.0, Mask);
    TestFalse(TEXT("A thin notch hides the whole intersected cell even when all corners are inside"),
        IsWorldCellVisible(Mask, FVector2D(450, 550)));

    AActor* Obstacle = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Obstacle actor exists"), Obstacle))
    {
        return false;
    }
    UBoxComponent* Box = NewObject<UBoxComponent>(Obstacle);
    Obstacle->SetRootComponent(Box);
    Obstacle->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(35, 35, 100));
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionResponseToAllChannels(ECR_Ignore);
    Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Box->RegisterComponent();
    Obstacle->SetActorLocation(FVector(150, 150, 100));
    Subsystem->BuildPlacementGridMask(Overview, FVector2D::ZeroVector, FVector2D(1000.0), 100, 0.0, Mask);
    TestFalse(TEXT("An unregistered physical prop hides the intersecting cell"),
        IsWorldCellVisible(Mask, FVector2D(150, 150)));
    TestTrue(TEXT("A nearby cell outside the prop remains visible"),
        IsWorldCellVisible(Mask, FVector2D(250, 150)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSBuildGridMeshGroundTest,
    "FF_ZoneSystem.Integration.BuildableGrid.MeshGroundAndElevatedObstacle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildGridMeshGroundTest::RunTest(const FString& Parameters)
{
    FScopedBuildGridSettings Settings;
    Settings.Settings->bEnableGroundChecks = true;
    Settings.Settings->bEnableWorldCollisionChecks = true;
    TGuardValue<float> Slope(Settings.Settings->MaxGroundSlopeDegrees, 5.0f);
    TGuardValue<float> Variance(Settings.Settings->MaxGroundHeightVarianceCm, 20.0f);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("World system"), System)) { return false; }
    AFFZSGameplayZone* Zone = MakeGridZone(*World, TEXT("GZ_MeshGrid"),
        {FVector(0, 0, 0), FVector(1000, 0, 0), FVector(1000, 1000, 0), FVector(0, 1000, 0)});
    if (!TestTrue(TEXT("Zone registers"), System->RegisterZone(Zone))) { return false; }
    const auto SpawnBox = [World](const FVector& Location, const FVector& Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Actor->AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    };
    AActor* Floor = SpawnBox(FVector(500, 500, 190), FVector(1200, 1200, 10));
    Floor->SetActorRotation(FRotator(1, 0, 0));
    SpawnBox(FVector(150, 150, 300), FVector(35, 35, 100));
    FFFZSCommand Command;
    Command.ZoneId = Zone->ZoneId;
    FFFZSBuildableGridMask Mask;
    System->BuildPlacementGridMask(Command, FVector2D::ZeroVector, FVector2D(1000), 100, 600.0, Mask);
    FFFZSBuildGridSurfaceCache Surface;
    Surface.Update(*World, Zone->GetActorTransform(), 600.0, Mask);
    TestTrue(TEXT("A gently tilted non-Landscape floor remains buildable"), Mask.GetBuildableCellCount() >= 90);
    TestTrue(TEXT("A cell away from the obstacle stays visible on the elevated floor"),
        IsWorldCellVisible(Mask, FVector2D(550, 550)));
    TestFalse(TEXT("A small prop is clipped at real ground height despite high cursor reference"),
        IsWorldCellVisible(Mask, FVector2D(150, 150)));
    TestTrue(TEXT("Rendered support height follows the elevated floor"),
        Surface.GetWorldPoint(FVector2D(500, 500)).Z > 190.0);
    return true;
}

#endif
