// Verify terrain support is not reported as a paving obstruction, while real obstacles remain blocked.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "FF_FFZSProjectZoneTestFixture.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSProtectedArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"

namespace
{
    struct FScopedTerrainPlacementSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        TGuardValue<bool> Ground{Settings->bEnableGroundChecks, true};
        TGuardValue<bool> Collision{Settings->bEnableWorldCollisionChecks, true};
        TGuardValue<bool> Navigation{Settings->bEnableNavigationProjectionChecks, false};
        TGuardValue<bool> Policies{Settings->bRequireProductionPolicyDomains, false};
        TGuardValue<bool> EverySample{Settings->bRequireEveryGroundSample, true};
        TGuardValue<float> SampleSpacing{Settings->GroundSampleSpacingCm, 100.0f};
        TGuardValue<float> Slope{Settings->MaxGroundSlopeDegrees, 5.0f};
        TGuardValue<float> Variance{Settings->MaxGroundHeightVarianceCm, 20.0f};
        TGuardValue<float> TileSlope{Settings->MaxTileGroundSlopeDegrees, 89.0f};
        TGuardValue<float> TileVariance{Settings->MaxTileGroundHeightVarianceCm, 2000.0f};
    };

    UFFZSWorldSubsystem* PrepareTerrainWorld(FAutomationTestBase& Test, UWorld& World)
    {
        UFFZSWorldSubsystem* System = World.GetSubsystem<UFFZSWorldSubsystem>();
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!Test.TestNotNull(TEXT("Terrain fixture subsystem"), System) ||
            !FFZSTest::ApplyZonePreset(Test, Zone)) return nullptr;
        Zone->bAllowAllFacilityCategories = true;
        return Test.TestTrue(TEXT("Terrain fixture zone registers"), System->RegisterZone(Zone)) ? System : nullptr;
    }

    FFFZSCommand MakeTerrainPaving(const EFFZSFacilityCategory Category)
    {
        FFFZSFacilitySpec Spec;
        Spec.FacilityDefinitionId = Category == EFFZSFacilityCategory::Road
            ? TEXT("Road_TerrainRegression") : TEXT("Tile_TerrainRegression");
        Spec.DisplayName = FText::FromString(TEXT("Terrain paving"));
        Spec.Category = Category;
        Spec.FootprintSizeCells = FIntPoint(400, 400);
        Spec.ClearanceCells = 0;
        Spec.PlacementObstacleHeightCm = 8.0f;
        Spec.QueueRules.MaxWaitingAgents = 0;
        Spec.QueueRules.ServiceCapacity = 0;
        if (Spec.UsesServiceQueue())
        {
            // A small service fixture keeps its independently validated queue beside
            // the 4 m footprint, away from the audience fixtures in front of it.
            Spec.QueueRules.MaxWaitingAgents = 1;
            Spec.QueueRules.ServiceCapacity = 1;
            Spec.QueueRules.SlotSpacingCm = 100.0f;
            Spec.QueueRules.SlotArrivalToleranceCm = 25.0f;
            Spec.QueueReservedRects = { FFFZSCellRect(FIntPoint(400, 100), FIntPoint(600, 200)) };
            Spec.QueueSplinePointsCm = { FVector(250, -50, 0), FVector(350, -50, 0) };
            for (const auto& Pair : { TPair<EFFZSAnchorType, FVector>(EFFZSAnchorType::Entry, FVector(350, -50, 0)),
                TPair<EFFZSAnchorType, FVector>(EFFZSAnchorType::Service, FVector(200, -50, 0)),
                TPair<EFFZSAnchorType, FVector>(EFFZSAnchorType::Exit, FVector(200, 100, 0)) })
            {
                FFFZSAnchorDefinition& Anchor = Spec.Anchors.AddDefaulted_GetRef();
                Anchor.Type = Pair.Key;
                Anchor.LocalTransform.SetLocation(Pair.Value);
            }
        }
        FFFZSGridPlacement Placement;
        Placement.OriginCell = FIntPoint(1000, 1000);
        return UFFZSGridLibrary::MakeInstallCommand(Spec, FFZSTest::ProjectZoneId(), Placement);
    }

    UBoxComponent* AddTerrainBox(AActor& Actor, const FVector& Location, const FVector& Extent)
    {
        UBoxComponent* Box = NewObject<UBoxComponent>(&Actor);
        if (!Actor.GetRootComponent()) Actor.SetRootComponent(Box);
        Actor.AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->SetCanEverAffectNavigation(false);
        Box->RegisterComponent();
        Box->SetWorldLocation(Location);
        return Box;
    }

    FFFZSCommand TerrainEditCommand(const EFFZSCommandType Type, const FGuid FacilityId,
        const FIntPoint Origin = FIntPoint(1000, 1000))
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.Type = Type;
        Command.FacilityInstanceId = FacilityId;
        Command.ZoneId = FFZSTest::ProjectZoneId();
        Command.Placement.OriginCell = Origin;
        return Command;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileFoundationHeightTest,
    "FF_ZoneSystem.Tile.FoundationHeightAndIndependentLimits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileFoundationHeightTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Foundation height world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Foundation terrain"), Terrain)) return false;
    UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(2000, 2000, 10));
    const FFFZSCommand Tile = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    const FFFZSCommand Road = MakeTerrainPaving(EFFZSFacilityCategory::Road);

    for (const double Pitch : {2.0, 8.0})
    {
        Floor->SetWorldRotation(FRotator(Pitch, 0, 0));
        double HighestCorner = -TNumericLimits<double>::Max();
        for (const double X : {1000.0, 1400.0})
        {
            for (const double Y : {1000.0, 1400.0})
            {
                FHitResult Hit;
                if (!TestTrue(TEXT("Fixture has terrain at each footprint corner"), World->LineTraceSingleByChannel(
                    Hit, FVector(X, Y, 1000), FVector(X, Y, -1000), ECC_Visibility))) return false;
                HighestCorner = FMath::Max(HighestCorner, Hit.ImpactPoint.Z);
            }
        }
        const FFFZSPlacementValidation Validation = System->ValidatePlacement(Tile);
        TestTrue(TEXT("Tile levels terrain beyond ordinary building limits"), Validation.bIsValid);
        TestTrue(TEXT("Foundation clears the true high corner, beyond the last center sample"),
            Validation.PlacementTransform.GetLocation().Z >= HighestCorner - 0.1);
        TestTrue(TEXT("Foundation uses local footprint terrain rather than the whole floor bounds"),
            Validation.PlacementTransform.GetLocation().Z <= HighestCorner + 1.0);
        TestTrue(TEXT("Foundation stays horizontal"), Validation.PlacementTransform.GetRotation().Equals(FQuat::Identity));
    }

    {
        TGuardValue<float> StrictTileSlope(Settings.Settings->MaxTileGroundSlopeDegrees, 5.0f);
        const FFFZSPlacementValidation Strict = System->ValidatePlacement(Tile);
        TestFalse(TEXT("An explicit tighter tile slope limit is enforced"), Strict.bIsValid);
        TestTrue(TEXT("Tile slope rejection preserves its diagnostic"), Strict.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
    }
    Floor->SetWorldRotation(FRotator(4, 0, 0));
    {
        TGuardValue<float> StrictTileVariance(Settings.Settings->MaxTileGroundHeightVarianceCm, 20.0f);
        const FFFZSPlacementValidation Strict = System->ValidatePlacement(Tile);
        TestFalse(TEXT("An explicit tighter tile height limit is enforced"), Strict.bIsValid);
        TestTrue(TEXT("Tile height rejection preserves its diagnostic"), Strict.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
    }

    Floor->SetWorldRotation(FRotator(2, 0, 0));
    FHitResult CenterHit;
    if (!TestTrue(TEXT("Road reference center intersects the plane"), World->LineTraceSingleByChannel(
        CenterHit, FVector(1200, 1200, 1000), FVector(1200, 1200, -1000), ECC_Visibility))) return false;
    const FFFZSPlacementValidation RoadValidation = System->ValidatePlacement(Road);
    TestTrue(TEXT("Ordinary road remains placeable on gentle terrain"), RoadValidation.bIsValid);
    TestTrue(TEXT("Road still uses mean terrain height, not the tile's highest support height"),
        FMath::IsNearlyEqual(RoadValidation.PlacementTransform.GetLocation().Z, CenterHit.ImpactPoint.Z, 0.1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileFoundationBoundariesTest,
    "FF_ZoneSystem.Tile.FoundationRetainsGroundAndProtectedBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileFoundationBoundariesTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Tile boundary world"), System)) return false;
    const FFFZSCommand Tile = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    const FFFZSPlacementValidation Empty = System->ValidatePlacement(Tile);
    TestFalse(TEXT("Tile does not create a foundation over empty space"), Empty.bIsValid);
    TestTrue(TEXT("Empty space remains missing ground"), Empty.Reasons.Contains(EFFZSPlacementFailure::InvalidGround));

    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Tile boundary terrain"), Terrain)) return false;
    UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1100, 1200, -10), FVector(90, 2000, 10));
    const FFFZSPlacementValidation Partial = System->ValidatePlacement(Tile);
    TestFalse(TEXT("One supported portion does not bypass mandatory ground samples"), Partial.bIsValid);
    TestTrue(TEXT("Partially missing ground retains the ground failure"), Partial.Reasons.Contains(EFFZSPlacementFailure::InvalidGround));
    {
        TGuardValue<bool> OptionalOrdinarySamples(Settings.Settings->bRequireEveryGroundSample, false);
        TestTrue(TEXT("Tile support remains mandatory when ordinary partial support is allowed"),
            System->ValidatePlacement(Tile).Reasons.Contains(EFFZSPlacementFailure::InvalidGround));
    }
    Floor->SetBoxExtent(FVector(2000, 2000, 10));
    TestTrue(TEXT("Full ground makes the same tile valid"), System->ValidatePlacement(Tile).bIsValid);

    FFFZSCommand Outside = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    Outside.Placement.OriginCell = FIntPoint(-1000, -1000);
    TestTrue(TEXT("Tile does not bypass the zone boundary"),
        System->ValidatePlacement(Outside).Reasons.Contains(EFFZSPlacementFailure::OutsideZone));

    AFFZSProtectedArea* Protected = World->SpawnActor<AFFZSProtectedArea>();
    if (!TestNotNull(TEXT("Protected tile area"), Protected)) return false;
    Protected->ProtectedAreaId = TEXT("PA_TileFoundationRegression");
    Protected->LinkedZoneId = Tile.ZoneId;
    Protected->AreaType = EFFZSProtectedAreaType::NoBuild;
    Protected->AreaBounds->SetBoxExtent(FVector(250, 250, 1000));
    Protected->SetActorLocation(FVector(1200, 1200, 0));
    if (!TestTrue(TEXT("Protected tile area registers"), System->RegisterProtectedArea(Protected))) return false;
    const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Tile);
    TestFalse(TEXT("Tile does not bypass protected construction areas"), Blocked.bIsValid);
    TestTrue(TEXT("Protected placement preserves its diagnostic"), Blocked.Reasons.Contains(EFFZSPlacementFailure::ProtectedRouteBlocked));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileTraceBandTest,
    "FF_ZoneSystem.Tile.FoundationNearTraceBandEdges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileTraceBandTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    TGuardValue<float> TraceAbove(Settings.Settings->GroundTraceAboveCm, 1000.0f);
    TGuardValue<float> TraceBelow(Settings.Settings->GroundTraceBelowCm, 1000.0f);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Tile trace-band world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Tile trace-band terrain"), Terrain)) return false;
    UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1200, 1200, 989.8), FVector(2000, 2000, 10));
    const FFFZSCommand Tile = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    for (const double Height : {999.8, -999.8})
    {
        Floor->SetWorldLocation(FVector(1200, 1200, Height - 10.0));
        const FFFZSPlacementValidation Result = System->ValidatePlacement(Tile);
        TestTrue(TEXT("A ground hit inside either trace boundary remains valid for the footprint sweep"), Result.bIsValid);
        TestFalse(TEXT("The finite sweep thickness neither starts inside high ground nor misses low ground"),
            Result.Reasons.Contains(EFFZSPlacementFailure::InvalidGround));
        TestTrue(TEXT("Near-limit foundations preserve the actual ground height"),
            FMath::IsNearlyEqual(Result.PlacementTransform.GetLocation().Z, Height, 0.1));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileAuthoredTerrainTest,
    "FF_ZoneSystem.Tile.AuthoredTerrainSeamsAndUnsampledPeaks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileAuthoredTerrainTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Authored modular terrain world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Authored modular terrain actor"), Terrain)) return false;
    // Independent mesh sections meet at X=1210; neither component owns every ground sample.
    const FName TerrainTag(TEXT("FFZS_TileGround"));
    UBoxComponent* LowFloor = AddTerrainBox(*Terrain, FVector(1010, 1200, -10), FVector(200, 400, 10));
    UBoxComponent* HighFloor = AddTerrainBox(*Terrain, FVector(1410, 1200, 50), FVector(200, 400, 10));
    LowFloor->ComponentTags.Add(TerrainTag);
    HighFloor->ComponentTags.Add(TerrainTag);
    const FFFZSCommand Tile = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    const FFFZSPlacementValidation Seam = System->ValidatePlacement(Tile);
    TestTrue(TEXT("Tagged modular ground supports a tile across separate component seams"), Seam.bIsValid);
    TestTrue(TEXT("The higher component defines the flat foundation height"),
        FMath::IsNearlyEqual(Seam.PlacementTransform.GetLocation().Z, 60.0, 0.1));
    HighFloor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    const FFFZSPlacementValidation MissingSection = System->ValidatePlacement(Tile);
    TestFalse(TEXT("Terrain tags do not excuse a missing modular ground section"), MissingSection.bIsValid);
    TestTrue(TEXT("The missing tagged section retains its ground diagnostic"),
        MissingSection.Reasons.Contains(EFFZSPlacementFailure::InvalidGround));
    HighFloor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

    AActor* PeakActor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Authored terrain peak actor"), PeakActor)) return false;
    UBoxComponent* Peak = AddTerrainBox(*PeakActor, FVector(1125, 1125, 80), FVector(10, 10, 80));
    // This 20 cm wide peak is strictly between the 100 cm point-sampling lattice.
    const FFFZSPlacementValidation Untagged = System->ValidatePlacement(Tile);
    TestFalse(TEXT("A separate untagged peak still counts as an obstruction"), Untagged.bIsValid);
    TestTrue(TEXT("Untagged geometry retains its collision diagnostic"),
        Untagged.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));

    for (const bool bTagActor : {false, true})
    {
        Peak->ComponentTags.Remove(TerrainTag);
        PeakActor->Tags.Remove(TerrainTag);
        if (bTagActor) PeakActor->Tags.Add(TerrainTag);
        else Peak->ComponentTags.Add(TerrainTag);
        const FString Label = bTagActor ? TEXT("Actor terrain tag: ") : TEXT("Component terrain tag: ");
        const FFFZSPlacementValidation Tagged = System->ValidatePlacement(Tile);
        TestTrue(Label + TEXT("explicit ground classification admits an unsampled terrain peak"), Tagged.bIsValid);
        TestTrue(Label + TEXT("the swept peak raises the entire tile above its 160 cm top"),
            FMath::IsNearlyEqual(Tagged.PlacementTransform.GetLocation().Z, 160.0, 0.1));
        {
            TGuardValue<float> HeightLimit(Settings.Settings->MaxTileGroundHeightVarianceCm, 100.0f);
            const FFFZSPlacementValidation TooHigh = System->ValidatePlacement(Tile);
            TestFalse(Label + TEXT("authored terrain still obeys the tile height limit"), TooHigh.bIsValid);
            TestTrue(Label + TEXT("the unsampled peak participates in height-variance validation"),
                TooHigh.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSTileAnalyticalFoundationTest,
    "FF_ZoneSystem.Tile.AnalyticalFoundationAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTileAnalyticalFoundationTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Analytical tile world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Analytical tile terrain"), Terrain)) return false;
    UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(4000, 4000, 10));
    Floor->SetWorldRotation(FRotator(8, 0, 0));
    const FFFZSCommand Tile = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    FFFZSFacilitySpec BuildingSpec = Tile.FacilitySpec;
    BuildingSpec.FacilityDefinitionId = TEXT("Lighting_TileFoundationRegression");
    BuildingSpec.Category = EFFZSFacilityCategory::Lighting;
    BuildingSpec.PlacementObstacleHeightCm = 200;
    FFFZSCommand Building = UFFZSGridLibrary::MakeInstallCommand(BuildingSpec, Tile.ZoneId, Tile.Placement);
    const FFFZSPlacementValidation Bare = System->ValidatePlacement(Building);
    TestFalse(TEXT("The ordinary building cannot use this steep bare terrain"), Bare.bIsValid);
    TestTrue(TEXT("The control building really fails its terrain slope"), Bare.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
    if (!TestTrue(TEXT("A mesh-free tile installs on the uneven terrain"), System->ExecuteCommand(Tile).bSucceeded)) return false;
    AFFZSFacilityActor* TileActor = System->FindFacility(Tile.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed tile"), TileActor)) return false;
    TestNull(TEXT("The support fixture has no loaded display mesh"), TileActor->VisualMesh->GetStaticMesh());
    const double TileTop = TileActor->GetActorLocation().Z + Tile.FacilitySpec.PlacementObstacleHeightCm;
    const FFFZSPlacementValidation Supported = System->ValidatePlacement(Building);
    TestTrue(TEXT("Registered tile geometry supports the building before its mesh loads"), Supported.bIsValid);
    TestTrue(TEXT("Supported building rests on the analytical tile top"),
        FMath::IsNearlyEqual(Supported.PlacementTransform.GetLocation().Z, TileTop, 0.1));

    FFFZSGridPlacement PartialPlacement = Tile.Placement;
    PartialPlacement.OriginCell.X += 200;
    const FFFZSCommand Partial = UFFZSGridLibrary::MakeInstallCommand(BuildingSpec, Tile.ZoneId, PartialPlacement);
    TestFalse(TEXT("A partially overlapping tile does not exempt uncovered steep terrain"), System->ValidatePlacement(Partial).bIsValid);
    if (!TestTrue(TEXT("The supported building installs"), System->ExecuteCommand(Building).bSucceeded)) return false;
    TestFalse(TEXT("Supporting foundation cannot be moved out from under its building"), System->ExecuteCommand(
        TerrainEditCommand(EFFZSCommandType::Move, Tile.FacilityInstanceId, FIntPoint(2000, 1000))).bSucceeded);
    TestFalse(TEXT("Supporting foundation cannot be removed"), System->ExecuteCommand(
        TerrainEditCommand(EFFZSCommandType::Remove, Tile.FacilityInstanceId)).bSucceeded);

    const FFFZSWorldSnapshot Snapshot = System->CaptureWorldSnapshot();
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Steep-terrain tile and building restore together"), System->RestoreWorldSnapshot(Snapshot, Errors))) return false;
    const AFFZSFacilityActor* RestoredTile = System->FindFacility(Tile.FacilityInstanceId);
    const AFFZSFacilityActor* RestoredBuilding = System->FindFacility(Building.FacilityInstanceId);
    if (!TestNotNull(TEXT("Restored tile"), RestoredTile) || !TestNotNull(TEXT("Restored supported building"), RestoredBuilding)) return false;
    TestTrue(TEXT("Restored building preserves its supported height"),
        FMath::IsNearlyEqual(RestoredBuilding->GetActorLocation().Z, TileTop, 0.1));
    TestTrue(TEXT("A restored supported building still validates on the mesh-free tile"), System->ValidatePlacement(
        TerrainEditCommand(EFFZSCommandType::Move, Building.FacilityInstanceId)).bIsValid);
    TestTrue(TEXT("Building can be removed first"), System->ExecuteCommand(
        TerrainEditCommand(EFFZSCommandType::Remove, Building.FacilityInstanceId)).bSucceeded);
    TestTrue(TEXT("Foundation can be removed after its building"), System->ExecuteCommand(
        TerrainEditCommand(EFFZSCommandType::Remove, Tile.FacilityInstanceId)).bSucceeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadTerrainSupportTest,
    "FF_ZoneSystem.Road.TerrainSupportAndObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadTerrainSupportTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    for (const EFFZSFacilityCategory Category : {EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile})
    {
        const FString Label = Category == EFFZSFacilityCategory::Road ? TEXT("Road: ") : TEXT("Tile: ");
        UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
        UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
        if (!TestNotNull(Label + TEXT("terrain world"), System)) return false;
        AActor* Terrain = World->SpawnActor<AActor>();
        if (!TestNotNull(Label + TEXT("terrain actor"), Terrain)) return false;
        UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(2000, 2000, 10));
        Floor->SetWorldRotation(FRotator(2, 0, 0));
        const FFFZSCommand Command = MakeTerrainPaving(Category);

        // At 2 degrees the 400 cm footprint has only 10.5 cm sampled height variance.
        // Its higher edge nevertheless crosses the old mean-ground + 3 cm overlap box.
        const FFFZSPlacementValidation Gentle = System->ValidatePlacement(Command);
        TestFalse(Label + TEXT("gentle terrain satisfies the slope limit"),
            Gentle.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
        TestFalse(Label + TEXT("gentle terrain satisfies the height variance limit"),
            Gentle.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
        TestFalse(Label + TEXT("supporting ground is not a world obstruction"),
            Gentle.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        TestTrue(Label + TEXT("preview accepts gently sloping ground"), Gentle.bIsValid);

        // Tiles level terrain using their own limits; roads retain the ordinary terrain limits.
        Floor->SetWorldRotation(FRotator(8, 0, 0));
        const FFFZSPlacementValidation Steep = System->ValidatePlacement(Command);
        TestEqual(Label + TEXT("steep ground follows category-specific limits"),
            Steep.bIsValid, Category == EFFZSFacilityCategory::Tile);
        TestEqual(Label + TEXT("only the road reports the ordinary slope limit"),
            Steep.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope), Category == EFFZSFacilityCategory::Road);
        Floor->SetWorldRotation(FRotator(4, 0, 0));
        const FFFZSPlacementValidation Uneven = System->ValidatePlacement(Command);
        TestEqual(Label + TEXT("uneven ground follows category-specific height limits"),
            Uneven.bIsValid, Category == EFFZSFacilityCategory::Tile);
        TestEqual(Label + TEXT("only the road retains the ordinary height variance failure"),
            Uneven.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded), Category == EFFZSFacilityCategory::Road);
        TestFalse(Label + TEXT("four degree fixture is within the slope limit"),
            Uneven.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
        Floor->SetWorldRotation(FRotator(2, 0, 0));

        // This small obstacle is between the ground samples and shares the floor's actor.
        // Ignoring an entire support actor would incorrectly allow paving through it.
        UBoxComponent* Obstacle = AddTerrainBox(*Terrain, FVector(1200, 1200, 30), FVector(15, 15, 30));
        const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Command);
        TestFalse(Label + TEXT("another component on the support actor blocks placement"), Blocked.bIsValid);
        TestTrue(Label + TEXT("the real obstacle reports collision"),
            Blocked.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        TestFalse(Label + TEXT("execution also rejects the real obstacle"), System->ExecuteCommand(Command).bSucceeded);
        Obstacle->DestroyComponent();

        // Failed command receipts must not mask the result after removing the obstacle.
        const FFFZSCommand ClearCommand = MakeTerrainPaving(Category);
        TestTrue(Label + TEXT("preview accepts terrain after obstacle removal"),
            System->ValidatePlacement(ClearCommand).bIsValid);
        TestTrue(Label + TEXT("installation succeeds on the same gently sloping terrain"),
            System->ExecuteCommand(ClearCommand).bSucceeded);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadTerrainInstanceTest,
    "FF_ZoneSystem.Road.TerrainSupportInstanceIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadTerrainInstanceTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Instanced terrain collision mesh"), Cube)) return false;
    TArray<UStaticMesh*> MeshesToFinish{Cube};
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Instanced terrain world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Instanced terrain actor"), Terrain)) return false;
    UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(Terrain);
    Terrain->SetRootComponent(Instances);
    Terrain->AddInstanceComponent(Instances);
    Instances->SetStaticMesh(Cube);
    Instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Instances->SetCollisionObjectType(ECC_WorldStatic);
    Instances->SetCollisionResponseToAllChannels(ECR_Ignore);
    Instances->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Instances->SetCanEverAffectNavigation(false);
    Instances->RegisterComponent();
    Instances->AddInstance(FTransform(FRotator(2, 0, 0), FVector(1200, 1200, -10), FVector(40, 40, 0.2)), true);
    const FFFZSCommand Command = MakeTerrainPaving(EFFZSFacilityCategory::Road);
    TestTrue(TEXT("An instanced gently sloping floor supports paving"), System->ValidatePlacement(Command).bIsValid);

    const int32 ObstacleIndex = Instances->AddInstance(
        FTransform(FRotator::ZeroRotator, FVector(1200, 1200, 30), FVector(0.3, 0.3, 0.6)), true);
    const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Command);
    TestFalse(TEXT("Another instance in the support component blocks placement"), Blocked.bIsValid);
    TestTrue(TEXT("The separate instance reports a real obstruction"),
        Blocked.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestTrue(TEXT("Remove only the obstacle instance"), Instances->RemoveInstance(ObstacleIndex));
    TestTrue(TEXT("Instanced terrain installs after its obstacle is removed"), System->ExecuteCommand(Command).bSucceeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadMergedTerrainObstacleTest,
    "FF_ZoneSystem.Road.TerrainSupportMergedObstacle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadMergedTerrainObstacleTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Merged terrain source mesh"), Cube)) return false;
    TArray<UStaticMesh*> MeshesToFinish{Cube};
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Merged terrain world"), System)) return false;
    UStaticMesh* MergedMesh = DuplicateObject<UStaticMesh>(Cube, GetTransientPackage());
    UBodySetup* Body = MergedMesh ? MergedMesh->GetBodySetup() : nullptr;
    if (!TestNotNull(TEXT("Transient compound collision body"), Body) ||
        !TestTrue(TEXT("The fixture never modifies the engine asset's collision"), Body != Cube->GetBodySetup())) return false;
    Body->CollisionTraceFlag = CTF_UseSimpleAsComplex;
    Body->AggGeom.EmptyElements();
    FKBoxElem FloorShape;
    FloorShape.X = 4000.0f;
    FloorShape.Y = 4000.0f;
    FloorShape.Z = 20.0f;
    Body->AggGeom.BoxElems.Add(FloorShape);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();

    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Merged terrain actor"), Terrain)) return false;
    UStaticMeshComponent* Surface = NewObject<UStaticMeshComponent>(Terrain);
    Terrain->SetRootComponent(Surface);
    Terrain->AddInstanceComponent(Surface);
    Surface->SetStaticMesh(MergedMesh);
    Surface->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Surface->SetCollisionObjectType(ECC_WorldStatic);
    Surface->SetCollisionResponseToAllChannels(ECR_Ignore);
    Surface->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Surface->SetCanEverAffectNavigation(false);
    Surface->RegisterComponent();
    Surface->SetWorldLocationAndRotation(FVector(1200, 1200, -10), FRotator(2, 0, 0));
    const FFFZSCommand Command = MakeTerrainPaving(EFFZSFacilityCategory::Road);
    const FFFZSCommand TileCommand = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
    FFFZSCommand BuildingCommand = MakeTerrainPaving(EFFZSFacilityCategory::Performance);
    BuildingCommand.FacilitySpec.PlacementObstacleHeightCm = 250.0f;
    TestTrue(TEXT("A merged-mesh floor alone supports paving"), System->ValidatePlacement(Command).bIsValid);
    TestTrue(TEXT("A merged-mesh floor alone supports a level tile"), System->ValidatePlacement(TileCommand).bIsValid);
    TestTrue(TEXT("A merged-mesh floor alone supports an ordinary stage"), System->ValidatePlacement(BuildingCommand).bIsValid);
    FKBoxElem OverheadShape;
    OverheadShape.Center = FVector(0, 0, 269);
    OverheadShape.X = 30.0f;
    OverheadShape.Y = 30.0f;
    OverheadShape.Z = 8.0f;
    Body->AggGeom.BoxElems.Add(OverheadShape);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    Surface->RecreatePhysicsState();
    TestTrue(TEXT("Terrain recheck cannot extend a building's collision above its original top"),
        System->ValidatePlacement(BuildingCommand).bIsValid);
    Body->AggGeom.BoxElems.RemoveAt(1);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    Surface->RecreatePhysicsState();
    {
        TGuardValue<bool> GroundDisabled(Settings.Settings->bEnableGroundChecks, false);
        const FFFZSPlacementValidation Unverified = System->ValidatePlacement(Command);
        TestFalse(TEXT("Disabling ground sampling grants no support exemption"), Unverified.bIsValid);
        TestTrue(TEXT("Unverified intersecting geometry retains ordinary collision validation"),
            Unverified.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    }

    // A narrow static-mesh ridge lies between both center samples and boundary samples.
    // Static mesh collision cannot distinguish a ridge from a prop merged into terrain:
    // accepting the height or conservatively rejecting it are both safe, clipping is not.
    FKBoxElem RidgeShape;
    RidgeShape.Center = FVector(25, 25, 16);
    RidgeShape.X = 10.0f;
    RidgeShape.Y = 10.0f;
    RidgeShape.Z = 12.0f;
    Body->AggGeom.BoxElems.Add(RidgeShape);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    Surface->RecreatePhysicsState();
    const FVector RidgeProbe = Surface->GetComponentTransform().TransformPosition(FVector(25, 25, 22));
    FHitResult RidgeHit;
    if (!TestTrue(TEXT("The between-sample ridge has physical support above the plane"), World->LineTraceSingleByChannel(
        RidgeHit, FVector(RidgeProbe.X, RidgeProbe.Y, 1000), FVector(RidgeProbe.X, RidgeProbe.Y, -1000), ECC_Visibility))) return false;
    const FFFZSPlacementValidation Ridge = System->ValidatePlacement(TileCommand);
    TestTrue(TEXT("An accepted tile never intersects a ridge missed by point samples"),
        !Ridge.bIsValid || Ridge.PlacementTransform.GetLocation().Z >= RidgeHit.ImpactPoint.Z - 0.1);
    if (!Ridge.bIsValid)
    {
        TestTrue(TEXT("Ambiguous static-mesh ridge is rejected as an obstruction"),
            Ridge.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    }
    Body->AggGeom.BoxElems.RemoveAt(1);

    // A narrow wall between sample centers shares the floor's component AND physics body.
    // Support identity alone must never allow a merged wall through the original paving box.
    FKBoxElem WallShape;
    WallShape.Center = FVector(0, 0, 40);
    WallShape.X = 30.0f;
    WallShape.Y = 30.0f;
    WallShape.Z = 60.0f;
    Body->AggGeom.BoxElems.Add(WallShape);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    Surface->RecreatePhysicsState();
    const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Command);
    TestFalse(TEXT("A wall merged into the supporting floor blocks placement"), Blocked.bIsValid);
    TestTrue(TEXT("The merged wall is reported as a world collision"),
        Blocked.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestFalse(TEXT("The merged wall fixture does not rely on excess slope"),
        Blocked.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
    TestFalse(TEXT("The merged wall fixture does not rely on height variance"),
        Blocked.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
    TestFalse(TEXT("Installation rejects the merged wall"), System->ExecuteCommand(Command).bSucceeded);
    const FFFZSPlacementValidation BlockedBuilding = System->ValidatePlacement(BuildingCommand);
    TestFalse(TEXT("An ordinary stage cannot ignore a wall merged into its terrain"), BlockedBuilding.bIsValid);
    TestTrue(TEXT("The stage retains the merged wall collision"),
        BlockedBuilding.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestTrue(TEXT("Merged obstacle diagnostics describe the actual large collision body"),
        BlockedBuilding.Obstructions.ContainsByPredicate([](const FFFZSPlacementObstruction& Obstruction)
            { return Obstruction.WorldHalfExtent.X > 1000.0; }));
    const FFFZSPlacementValidation BlockedTile = System->ValidatePlacement(TileCommand);
    TestFalse(TEXT("Tile height leveling cannot conceal a wall merged into terrain"), BlockedTile.bIsValid);
    TestTrue(TEXT("Tile reports the merged wall as a collision"),
        BlockedTile.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestFalse(TEXT("Tile installation rejects the merged wall"), System->ExecuteCommand(TileCommand).bSucceeded);

    // A 100 cm tile has only one center sample at 100 cm sample spacing. Its center
    // reaches the 30 cm wide pillar, while all four vertices reach the flat floor.
    // Checking either sample lattice independently misses this discontinuity.
    Surface->SetWorldRotation(FRotator::ZeroRotator);
    FFFZSFacilitySpec SmallSpec = TileCommand.FacilitySpec;
    SmallSpec.FootprintSizeCells = FIntPoint(100, 100);
    FFFZSGridPlacement SmallPlacement;
    SmallPlacement.OriginCell = FIntPoint(1150, 1150);
    const FFFZSCommand SmallTile = UFFZSGridLibrary::MakeInstallCommand(SmallSpec, TileCommand.ZoneId, SmallPlacement);
    FHitResult CenterHit;
    if (!TestTrue(TEXT("The small tile's sole center sample hits the merged pillar"), World->LineTraceSingleByChannel(
        CenterHit, FVector(1200, 1200, 1000), FVector(1200, 1200, -1000), ECC_Visibility))) return false;
    TestTrue(TEXT("The sole center reaches the pillar's 60 cm top"), FMath::IsNearlyEqual(CenterHit.ImpactPoint.Z, 60.0, 0.1));
    for (const double X : {1150.0, 1250.0})
    {
        for (const double Y : {1150.0, 1250.0})
        {
            FHitResult CornerHit;
            if (!TestTrue(TEXT("Each small tile vertex reaches physical ground"), World->LineTraceSingleByChannel(
                CornerHit, FVector(X, Y, 1000), FVector(X, Y, -1000), ECC_Visibility))) return false;
            TestTrue(TEXT("Every vertex hits the same compound component as the center"), CornerHit.GetComponent() == CenterHit.GetComponent());
            TestTrue(TEXT("Every vertex reaches the flat 0 cm floor"), FMath::IsNearlyZero(CornerHit.ImpactPoint.Z, 0.1));
        }
    }
    const FFFZSPlacementValidation SmallBlocked = System->ValidatePlacement(SmallTile);
    TestFalse(TEXT("A sole center on a merged pillar cannot become a small tile foundation"), SmallBlocked.bIsValid);
    TestTrue(TEXT("The center-to-vertex discontinuity is a collision"),
        SmallBlocked.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestFalse(TEXT("The small tile regression does not depend on a slope limit"),
        SmallBlocked.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
    TestFalse(TEXT("The small tile regression does not depend on a height-variance limit"),
        SmallBlocked.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
    TestFalse(TEXT("Installation also rejects the sole-center pillar"), System->ExecuteCommand(SmallTile).bSucceeded);
    Body->AggGeom.BoxElems.RemoveAt(1);
    Body->InvalidatePhysicsData();
    Body->CreatePhysicsMeshes();
    Surface->RecreatePhysicsState();
    TestTrue(TEXT("The same small tile is valid once the merged pillar is removed"), System->ValidatePlacement(SmallTile).bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadSampledPropTest,
    "FF_ZoneSystem.Road.TerrainSupportRejectsSampledProps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadSampledPropTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Sampled prop instance mesh"), Cube)) return false;
    TArray<UStaticMesh*> MeshesToFinish{Cube};
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    for (const bool bInstanced : {false, true})
    {
        const FString Label = bInstanced ? TEXT("Instanced sampled prop: ") : TEXT("Separate sampled prop: ");
        UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
        UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
        if (!TestNotNull(Label + TEXT("world"), System)) return false;
        AActor* Terrain = World->SpawnActor<AActor>();
        if (!TestNotNull(Label + TEXT("floor actor"), Terrain)) return false;
        UInstancedStaticMeshComponent* Instances = nullptr;
        if (bInstanced)
        {
            Instances = NewObject<UInstancedStaticMeshComponent>(Terrain);
            Terrain->SetRootComponent(Instances);
            Terrain->AddInstanceComponent(Instances);
            Instances->SetStaticMesh(Cube);
            Instances->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Instances->SetCollisionObjectType(ECC_WorldStatic);
            Instances->SetCollisionResponseToAllChannels(ECR_Ignore);
            Instances->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
            Instances->SetCanEverAffectNavigation(false);
            Instances->RegisterComponent();
            Instances->AddInstance(FTransform(FRotator::ZeroRotator,
                FVector(1200, 1200, -10), FVector(40, 40, 0.2)), true);
        }
        else
        {
            AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(2000, 2000, 10));
        }
        const FFFZSCommand Command = MakeTerrainPaving(EFFZSFacilityCategory::Road);
        TestTrue(Label + TEXT("flat floor alone supports paving"), System->ValidatePlacement(Command).bIsValid);
        if (Instances)
        {
            Instances->AddInstance(FTransform(FRotator::ZeroRotator,
                FVector(1050, 1050, 9), FVector(0.5, 0.5, 0.18)), true);
        }
        else
        {
            AActor* Prop = World->SpawnActor<AActor>();
            if (!TestNotNull(Label + TEXT("prop actor"), Prop)) return false;
            AddTerrainBox(*Prop, FVector(1050, 1050, 9), FVector(25, 25, 9));
        }

        // One of sixteen ground samples lands on this short tree-base-sized obstacle.
        // Its 18 cm top is inside the height-variance limit, but it is still an obstruction.
        FHitResult Hit;
        const FCollisionQueryParams GroundProbe(SCENE_QUERY_STAT(FFZSSampledPropTest), false);
        TestTrue(Label + TEXT("a ground sample physically hits the short prop"), World->LineTraceSingleByChannel(
            Hit, FVector(1050, 1050, 100), FVector(1050, 1050, -100), ECC_Visibility, GroundProbe));
        TestTrue(Label + TEXT("the sampled prop is below the 20 cm variance limit"),
            FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 18.0, 0.01));
        const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Command);
        TestFalse(Label + TEXT("the sampled prop cannot be paved over"), Blocked.bIsValid);
        TestTrue(Label + TEXT("the sampled prop remains a world collision"),
            Blocked.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        TestFalse(Label + TEXT("the fixture does not rely on excess slope"),
            Blocked.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
        TestFalse(Label + TEXT("the fixture does not rely on excess height variance"),
            Blocked.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
        TestFalse(Label + TEXT("installation also rejects the sampled prop"), System->ExecuteCommand(Command).bSucceeded);
        const FFFZSCommand TileCommand = MakeTerrainPaving(EFFZSFacilityCategory::Tile);
        const FFFZSPlacementValidation BlockedTile = System->ValidatePlacement(TileCommand);
        TestFalse(Label + TEXT("tile leveling cannot pave over a sampled prop"), BlockedTile.bIsValid);
        TestTrue(Label + TEXT("tile retains the sampled prop collision"),
            BlockedTile.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        FFFZSCommand BuildingCommand = MakeTerrainPaving(EFFZSFacilityCategory::Performance);
        BuildingCommand.FacilitySpec.PlacementObstacleHeightCm = 250.0f;
        TestTrue(Label + TEXT("an ordinary stage also retains a partially sampled prop collision"),
            System->ValidatePlacement(BuildingCommand).Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSOrdinaryTerrainSupportTest,
    "FF_ZoneSystem.Placement.OrdinaryFacilityTerrainAndVisibleObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrdinaryTerrainSupportTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    for (const EFFZSFacilityCategory Category : {EFFZSFacilityCategory::Performance, EFFZSFacilityCategory::Lighting})
    {
        UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
        UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
        if (!TestNotNull(TEXT("Ordinary facility terrain world"), System)) return false;
        AActor* Terrain = World->SpawnActor<AActor>();
        if (!TestNotNull(TEXT("Ordinary facility terrain actor"), Terrain)) return false;
        UBoxComponent* Floor = AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(2000, 2000, 10));
        Floor->SetWorldRotation(FRotator(2, 0, 0));
        FFFZSCommand Command = MakeTerrainPaving(Category);
        Command.FacilitySpec.PlacementObstacleHeightCm = 250.0f;
        UBoxComponent* Sensor = AddTerrainBox(*Terrain, FVector(1200, 1200, 100), FVector(100, 100, 20));
        Sensor->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
        const FFFZSPlacementValidation Gentle = System->ValidatePlacement(Command);
        TestTrue(TEXT("An ordinary facility accepts a verified gentle floor"), Gentle.bIsValid);
        TestFalse(TEXT("Overlap-only sensors do not become blockers when the same query hits physical ground"),
            Gentle.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        TestTrue(TEXT("Supporting terrain produces no misleading obstruction outline"), Gentle.Obstructions.IsEmpty());

        Floor->SetWorldRotation(FRotator(8, 0, 0));
        TestTrue(TEXT("Ordinary facility support still obeys slope limits"),
            System->ValidatePlacement(Command).Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
        Floor->SetWorldRotation(FRotator(4, 0, 0));
        TestTrue(TEXT("Ordinary facility support still obeys height variance limits"),
            System->ValidatePlacement(Command).Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
        Floor->SetWorldRotation(FRotator(2, 0, 0));
        {
            TGuardValue<bool> NoGroundProof(Settings.Settings->bEnableGroundChecks, false);
            TestTrue(TEXT("Unverified ground does not get an obstacle exemption"),
                System->ValidatePlacement(Command).Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        }

        // Both are between ground sample centers; one shares the supporting actor.
        UBoxComponent* SameOwnerProp = AddTerrainBox(*Terrain, FVector(1200, 1200, 30), FVector(15, 15, 30));
        AActor* OtherActor = World->SpawnActor<AActor>();
        if (!TestNotNull(TEXT("Separate facility obstruction"), OtherActor)) return false;
        OtherActor->SetActorLabel(TEXT("TerrainRegressionVisibleBarrier"));
        UBoxComponent* SeparateProp = AddTerrainBox(*OtherActor, FVector(1300, 1200, 30), FVector(15, 15, 30));
        const FFFZSPlacementValidation Blocked = System->ValidatePlacement(Command);
        TestFalse(TEXT("Real props still block an ordinary facility"), Blocked.bIsValid);
        TestEqual(TEXT("Both actual blockers get bounded diagnostics"), Blocked.Obstructions.Num(), 2);
        TestTrue(TEXT("World diagnostic identifies the blocking actor"),
            Blocked.Obstructions.ContainsByPredicate([](const FFFZSPlacementObstruction& Obstruction)
                { return Obstruction.DisplayName.ToString().Contains(TEXT("TerrainRegressionVisibleBarrier")); }));
        TestFalse(TEXT("Execution cannot install through the obstacles"), System->ExecuteCommand(Command).bSucceeded);
        SameOwnerProp->DestroyComponent();
        SeparateProp->DestroyComponent();
        Command.CommandId = FGuid::NewGuid();
        TestTrue(TEXT("The same facility installs after removing the actual obstacles"), System->ExecuteCommand(Command).bSucceeded);
        const FFFZSCommand Move = TerrainEditCommand(EFFZSCommandType::Move,
            Command.FacilityInstanceId, FIntPoint(1500, 1000));
        TestTrue(TEXT("Moving the installed facility across the same slope succeeds"), System->ExecuteCommand(Move).bSucceeded);
        TestTrue(TEXT("Revalidating the installed facility preserves its terrain support"),
            System->ValidatePlacement(Move).bIsValid);
        TestEqual(TEXT("Zone revalidation does not mark the accepted facility for relocation"),
            System->RevalidateFacilitiesInZone(Command.ZoneId), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceTerrainSupportTest,
    "FF_ZoneSystem.Placement.AudienceTerrainUsesIndependentSupport",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSAudienceTerrainSupportTest::RunTest(const FString& Parameters)
{
    FScopedTerrainPlacementSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*this, *World) : nullptr;
    if (!TestNotNull(TEXT("Audience terrain world"), System)) return false;
    AActor* Terrain = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Audience terrain actor"), Terrain)) return false;
    AddTerrainBox(*Terrain, FVector(1200, 1200, -10), FVector(201, 201, 10));
    AddTerrainBox(*Terrain, FVector(1500, 1150, -10), FVector(100, 50, 10));
    UBoxComponent* AudienceFloor = AddTerrainBox(*Terrain, FVector(1200, 1950, -10), FVector(500, 501, 10));
    AudienceFloor->SetWorldRotation(FRotator(2, 0, 0));
    FFFZSCommand Command = MakeTerrainPaving(EFFZSFacilityCategory::Performance);
    Command.FacilitySpec.PlacementObstacleHeightCm = 250.0f;
    FFFZSAudienceLayout& Audience = Command.FacilitySpec.Audience;
    Audience.bEnabled = true;
    Audience.Rows = 2;
    Audience.Columns = 2;
    Audience.FirstSlotLocalCm = FVector(-60, 600, 0);
    Audience.CenterAisleWidthCm = 0;
    Audience.PerimeterMarginCm = 120;
    if (!TestTrue(TEXT("Audience fixture arranges valid portals"), Audience.TryCenterAndArrangePortals())) return false;
    const FFFZSPlacementValidation Gentle = System->ValidatePlacement(Command);
    TestFalse(TEXT("The fixture reserves a separate audience region"), Gentle.ClearanceOrientedRects.IsEmpty());
    TestTrue(TEXT("A separate gently sloping audience floor is accepted"), Gentle.bIsValid);
    AudienceFloor->SetWorldRotation(FRotator(4.6, 0, 0));
    const FFFZSPlacementValidation HeightOnly = System->ValidatePlacement(Command);
    TestFalse(TEXT("Audience height variance still rejects terrain within the slope limit"), HeightOnly.bIsValid);
    TestTrue(TEXT("Audience ground height failure retains its region authority"),
        HeightOnly.Reasons.Contains(EFFZSPlacementFailure::InsufficientClearance));
    TestFalse(TEXT("An audience height failure does not blame the stage footprint"),
        HeightOnly.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestTrue(TEXT("Audience height failure identifies terrain and reports measured versus allowed centimeters"),
        HeightOnly.Obstructions.ContainsByPredicate([](const FFFZSPlacementObstruction& Obstruction)
        {
            return Obstruction.Kind == EFFZSPlacementObstructionKind::GroundSupport &&
                Obstruction.DisplayName.ToString().Contains(TEXT("높이 차")) &&
                Obstruction.Detail.ToString().Contains(TEXT("21.7cm")) &&
                Obstruction.Detail.ToString().Contains(TEXT("20cm"));
        }));
    TestTrue(TEXT("Authority summary preserves the ground failure instead of claiming an object overlap"),
        HeightOnly.Summary.ToString().Contains(TEXT("높이 차")));
    AudienceFloor->SetWorldRotation(FRotator(8, 0, 0));
    const FFFZSPlacementValidation Steep = System->ValidatePlacement(Command);
    TestTrue(TEXT("A distant steep floor cannot inherit the flat stage's support proof"),
        Steep.Reasons.Contains(EFFZSPlacementFailure::InsufficientClearance));
    TestTrue(TEXT("Steep audience terrain is explicitly identified as a ground failure"),
        Steep.Obstructions.ContainsByPredicate([](const FFFZSPlacementObstruction& Obstruction)
            { return Obstruction.Kind == EFFZSPlacementObstructionKind::GroundSupport &&
                Obstruction.DisplayName.ToString().Contains(TEXT("경사")); }));
    AudienceFloor->SetWorldRotation(FRotator(2, 0, 0));
    AddTerrainBox(*Terrain, FVector(1200, 1800, 30), FVector(15, 15, 30));
    const FFFZSPlacementValidation Obstructed = System->ValidatePlacement(Command);
    TestTrue(TEXT("A real obstacle in the audience remains blocked"),
        Obstructed.Reasons.Contains(EFFZSPlacementFailure::InsufficientClearance));
    TestFalse(TEXT("An audience-only obstacle does not blame the stage footprint"),
        Obstructed.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    return true;
}

#endif
