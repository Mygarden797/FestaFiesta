// FF_FFZSRoadTests.cpp
// 역할: 길과 타일의 점유, 겹침, 시설 설치 가능 여부와 저장·복원 계약을 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGrid.h"
#include "FF_FFZSBuildGridSurface.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSSaveGameTestObject.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/BodySetup.h"
#include "StaticMeshCompiler.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    struct FScopedRoadSettings
    {
        UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
        TGuardValue<bool> Ground{Settings->bEnableGroundChecks, false};
        TGuardValue<bool> Collision{Settings->bEnableWorldCollisionChecks, true};
        TGuardValue<bool> Navigation{Settings->bEnableNavigationProjectionChecks, false};
        TGuardValue<bool> Policies{Settings->bRequireProductionPolicyDomains, false};
        TGuardValue<int32> Budget{Settings->MaxEnumeratedCellsPerFacility, 4'000'000};
    };

    FFFZSFacilitySpec MakeRoadSpec(UStaticMesh* Mesh = nullptr)
    {
        FFFZSFacilitySpec Spec;
        Spec.FacilityDefinitionId = TEXT("Road_Surface_Test");
        Spec.DisplayName = FText::FromString(TEXT("Test paving"));
        Spec.Category = EFFZSFacilityCategory::Road;
        Spec.FootprintSizeCells = FIntPoint(400, 400);
        Spec.ClearanceCells = 0;
        Spec.PlacementObstacleHeightCm = 8.0f;
        Spec.QueueRules.MaxWaitingAgents = 0;
        Spec.QueueRules.ServiceCapacity = 0;
        Spec.VisualMesh = Mesh;
        Spec.VisualSizingMode = EFFZSVisualSizingMode::MatchPlacementBounds;
        return Spec;
    }

    FFFZSFacilitySpec MakeTileSpec(UStaticMesh* Mesh = nullptr)
    {
        FFFZSFacilitySpec Spec = MakeRoadSpec(Mesh);
        Spec.FacilityDefinitionId = TEXT("Tile_Surface_Test");
        Spec.Category = EFFZSFacilityCategory::Tile;
        // A stale legacy flag must not override the Tile category.
        Spec.bAllowFacilityPlacementOnTop = false;
        return Spec;
    }

    FFFZSCommand InstallAt(const FFFZSFacilitySpec& Spec, const FIntPoint Origin)
    {
        FFFZSGridPlacement Placement;
        Placement.OriginCell = Origin;
        return UFFZSGridLibrary::MakeInstallCommand(Spec, TEXT("GZ_DadaPocha"), Placement);
    }

    FFFZSCommand MoveTo(const FGuid Id, const FIntPoint Origin)
    {
        FFFZSCommand Command;
        Command.CommandId = FGuid::NewGuid();
        Command.TransactionId = FGuid::NewGuid();
        Command.Type = EFFZSCommandType::Move;
        Command.FacilityInstanceId = Id;
        Command.ZoneId = TEXT("GZ_DadaPocha");
        Command.Placement.OriginCell = Origin;
        return Command;
    }

    UFFZSWorldSubsystem* PrepareRoadWorld(UWorld& World)
    {
        UFFZSWorldSubsystem* System = World.GetSubsystem<UFFZSWorldSubsystem>();
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!System || !Zone) return nullptr;
        Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha"));
        Zone->bAllowAllFacilityCategories = true;
        return System->RegisterZone(Zone) ? System : nullptr;
    }

    AActor* SpawnRoadTestBox(UWorld& World, const FVector& Location, const FVector& Extent)
    {
        AActor* Actor = World.SpawnActor<AActor>();
        if (!Actor) return nullptr;
        UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box);
        Actor->AddInstanceComponent(Box);
        Box->SetBoxExtent(Extent);
        Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Ignore);
        Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Box->SetCanEverAffectNavigation(false);
        Box->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSPassiveFootprintTest, "FF_ZoneSystem.Road.PassiveFootprintPlacementAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPassiveFootprintTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Passive placement world"), System)) return false;
    int32 Row = 0;
    for (const EFFZSFacilityCategory Category : {
        EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile, EFFZSFacilityCategory::Lighting })
    {
        FFFZSFacilitySpec Legacy = MakeRoadSpec();
        Legacy.Category = Category;
        Legacy.PlacementObstacleHeightCm = 100.f;
        Legacy.ClearanceCells = 100;
        Legacy.QueueRules.MaxWaitingAgents = 3;
        Legacy.QueueRules.ServiceCapacity = 1;
        Legacy.QueueReservedRects.Emplace(FIntPoint(0, 400), FIntPoint(400, 800));
        Legacy.QueueReservedCells.Add(FIntPoint(400, 0));
        Legacy.QueueSplinePointsCm = { FVector(0, 300, 0), FVector(0, 600, 0) };
        Legacy.Anchors.AddDefaulted();
        Legacy.QueueLayout = EFFZSQueueLayout::Right;
        const FIntPoint Origin(1000, 1000 + Row++ * 2000);
        const FFFZSCommand First = InstallAt(Legacy, Origin);
        if (!TestTrue(TEXT("Legacy passive geometry is normalized on install"),
            System->ExecuteCommand(First).bSucceeded)) return false;
        AFFZSFacilityActor* Actor = System->FindFacility(First.FacilityInstanceId);
        if (!TestNotNull(TEXT("Installed passive facility"), Actor)) return false;
        TestEqual(TEXT("Base spec has no clearance"), Actor->GetBaseFacilitySpec().ClearanceCells, 0);
        TestTrue(TEXT("Base spec has no reserved queue"), Actor->GetBaseFacilitySpec().QueueReservedRects.IsEmpty());
        TestEqual(TEXT("No waiting capacity"), Actor->GetFacilitySpec().QueueRules.MaxWaitingAgents, 0);

        const FFFZSCommand Neighbor = InstallAt(Legacy, Origin + FIntPoint(400, 0));
        TestTrue(TEXT("A shared footprint edge is placeable without a gap"),
            System->ExecuteCommand(Neighbor).bSucceeded);
        TestFalse(TEXT("A one-centimeter footprint overlap still fails"),
            System->ValidatePlacement(InstallAt(Legacy, Origin + FIntPoint(399, 0))).bIsValid);
        TestTrue(TEXT("Former queue area is available"),
            System->ValidatePlacement(InstallAt(Legacy, Origin + FIntPoint(0, 400))).bIsValid);

        FFFZSWorldSnapshot Snapshot = System->CaptureWorldSnapshot();
        for (FFFZSFacilityInstanceSnapshot& Saved : Snapshot.Facilities)
        {
            if (Saved.FacilityInstanceId == First.FacilityInstanceId) Saved.FacilitySpec = Legacy;
        }
        TArray<FText> Errors;
        if (!TestTrue(TEXT("Old saved passive reservations are removed before restoring occupancy"),
            System->RestoreWorldSnapshot(Snapshot, Errors))) return false;
        Actor = System->FindFacility(First.FacilityInstanceId);
        if (!TestNotNull(TEXT("Restored passive facility"), Actor)) return false;
        TestEqual(TEXT("Restored base spec has no clearance"), Actor->GetBaseFacilitySpec().ClearanceCells, 0);
        TestTrue(TEXT("Restored base spec has no queue path"), Actor->GetBaseFacilitySpec().QueueSplinePointsCm.IsEmpty());
        TestEqual(TEXT("Passive queue remains closed"), Actor->CaptureInstanceSnapshot().Queue.State, EFFZSQueueState::Closed);
        TestTrue(TEXT("Restore leaves the caller's original snapshot unchanged"), Snapshot.Facilities.ContainsByPredicate(
            [&First](const FFFZSFacilityInstanceSnapshot& Saved)
            { return Saved.FacilityInstanceId == First.FacilityInstanceId && Saved.FacilitySpec.ClearanceCells == 100; }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadSpecTest, "FF_ZoneSystem.Road.SpecAndSaveCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadSpecTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Existing Lighting ordinal stays serialized as 7"), static_cast<uint8>(EFFZSFacilityCategory::Lighting), uint8(7));
    TestEqual(TEXT("Road is appended after existing categories"), static_cast<uint8>(EFFZSFacilityCategory::Road), uint8(8));
    TestEqual(TEXT("Tile is appended without changing existing categories"), static_cast<uint8>(EFFZSFacilityCategory::Tile), uint8(9));
    TArray<FText> Errors;
    for (const EFFZSFacilityCategory Category : { EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile })
    {
        FFFZSFacilitySpec Surface = MakeRoadSpec();
        Surface.Category = Category;
        TestTrue(TEXT("Both categories are ground surfaces"), Surface.IsGroundSurface());
        for (const bool bLegacyAllowsPlacement : { false, true })
        {
            Surface.bAllowFacilityPlacementOnTop = bLegacyAllowsPlacement;
            TestEqual(TEXT("Only Tile permits facilities regardless of the legacy flag"),
                Surface.AllowsFacilityPlacementOnTop(), Category == EFFZSFacilityCategory::Tile);
        }
        TestTrue(TEXT("An 8 cm ground surface needs no queue or service anchors"), Surface.IsValid(Errors));
        Surface.QueueRules.ServiceCapacity = 1;
        TestFalse(TEXT("Ground surface service data fails authoring validation"), Surface.IsValid(Errors));
        Surface.QueueRules.ServiceCapacity = 0;
        Surface.PlacementObstacleHeightCm = 0.0f;
        TestFalse(TEXT("A zero-thickness ground surface is invalid"), Surface.IsValid(Errors));
    }
    FFFZSFacilitySpec Facility = FFZSTest::MakeValidFacilitySpec();
    TestFalse(TEXT("An ordinary facility is not a ground surface"), Facility.IsGroundSurface());
    TestFalse(TEXT("An ordinary facility does not support another facility"), Facility.AllowsFacilityPlacementOnTop());
    Facility.PlacementObstacleHeightCm = 8.0f;
    TestFalse(TEXT("Other facilities retain their 10 cm minimum"), Facility.IsValid(Errors));
    FFFZSCommand RoadReceipt = InstallAt(MakeRoadSpec(), FIntPoint::ZeroValue);
    const FString LegacyRoadFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(RoadReceipt);
    RoadReceipt.FacilitySpec.bAllowFacilityPlacementOnTop = false;
    TestNotEqual(TEXT("Road keeps its legacy receipt payload for saved command replay"),
        UFFZSGridLibrary::BuildCommandFingerprint(RoadReceipt), LegacyRoadFingerprint);
    FFFZSCommand TileReceipt = InstallAt(MakeTileSpec(), FIntPoint::ZeroValue);
    const FString TileFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(TileReceipt);
    TileReceipt.FacilitySpec.bAllowFacilityPlacementOnTop = true;
    TestEqual(TEXT("New Tile command semantics ignore the legacy permission flag"),
        UFFZSGridLibrary::BuildCommandFingerprint(TileReceipt), TileFingerprint);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadLayerLifecycleTest, "FF_ZoneSystem.Road.OccupancyMoveUpgradeAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadLayerLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Tile test world registers"), System)) return false;
    const FFFZSFacilitySpec TileSpec = MakeTileSpec();
    const FFFZSCommand Tile = InstallAt(TileSpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("First tile installs"), System->ExecuteCommand(Tile).bSucceeded)) return false;
    TestFalse(TEXT("A second tile cannot stack on the first"), System->ValidatePlacement(
        InstallAt(TileSpec, FIntPoint(1000, 1000))).bIsValid);
    TestTrue(TEXT("Tiles may meet at a shared edge"), System->ExecuteCommand(
        InstallAt(TileSpec, FIntPoint(1400, 1000))).bSucceeded);
    TestTrue(TEXT("A tile can occupy the future queue and clearance area"), System->ExecuteCommand(
        InstallAt(TileSpec, FIntPoint(1000, 1500))).bSucceeded);
    FFFZSFacilitySpec RoadSpec = MakeRoadSpec();
    RoadSpec.bAllowFacilityPlacementOnTop = true;
    const FFFZSCommand Road = InstallAt(RoadSpec, FIntPoint(6000, 1000));
    TestTrue(TEXT("Road installs with a contradictory legacy permission flag"), System->ExecuteCommand(Road).bSucceeded);
    TestFalse(TEXT("Road cannot overlap Tile"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(1000, 1000))).bIsValid);
    TestFalse(TEXT("Tile cannot overlap Road"), System->ValidatePlacement(
        InstallAt(TileSpec, FIntPoint(6000, 1000))).bIsValid);
    TestFalse(TEXT("Road cannot overlap another Road"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(6000, 1000))).bIsValid);
    const FFFZSCommand BlockedFacility = InstallAt(FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck), FIntPoint(6000, 1000));
    TestFalse(TEXT("Road rejects facility placement even with legacy permission enabled"), System->ValidatePlacement(BlockedFacility).bIsValid);
    FFFZSBuildableGridMask DisabledMask;
    System->BuildPlacementGridMask(BlockedFacility, FVector2D(6000, 1000), FVector2D(6400, 1400), 100, 0.0, DisabledMask);
    TestEqual(TEXT("Road is blocked in the facility grid preview"), DisabledMask.GetBuildableCellCount(), 0);

    FFFZSFacilitySpec FacilitySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    FacilitySpec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Tier;
    Tier.SizeMultiplier = 1.1f;
    FacilitySpec.UpgradeTiers.Add(Tier);
    const FFFZSCommand Facility = InstallAt(FacilitySpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Facility footprint, clearance and queue may overlap tiles"),
        System->ExecuteCommand(Facility).bSucceeded)) return false;
    TestEqual(TEXT("Foundation reaches tile height before any visual mesh loads"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestFalse(TEXT("Facilities still block facilities on the same tile"), System->ValidatePlacement(
        InstallAt(FacilitySpec, FIntPoint(1000, 1000))).bIsValid);
    TestTrue(TEXT("Size upgrade validation ignores paving reservations"),
        System->ValidateUpgradePlacement(Facility.FacilityInstanceId, 1.1f).bIsValid);
    TestFalse(TEXT("Tile move cannot strand an installed facility"), System->ExecuteCommand(
        MoveTo(Tile.FacilityInstanceId, FIntPoint(2000, 1000))).bSucceeded);
    FFFZSCommand RemoveTile = MoveTo(Tile.FacilityInstanceId, FIntPoint(1000, 1000));
    RemoveTile.Type = EFFZSCommandType::Remove;
    TestFalse(TEXT("Tile removal cannot strand an installed facility"), System->ExecuteCommand(RemoveTile).bSucceeded);
    TestFalse(TEXT("Road cannot be inserted under an existing queue"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(1500, 1500))).bIsValid);
    TestTrue(TEXT("Tile may be added beneath only the existing queue"), System->ExecuteCommand(
        InstallAt(TileSpec, FIntPoint(1500, 1500))).bSucceeded);

    FFFZSBuildableGridMask Mask;
    System->BuildPlacementGridMask(InstallAt(TileSpec, FIntPoint(1000, 1000)),
        FVector2D(1000.0), FVector2D(1400.0), 100, 0.0, Mask);
    TestEqual(TEXT("Tile preview masks existing tiles"), Mask.GetBuildableCellCount(), 0);
    FFFZSCommand Moving = MoveTo(Facility.FacilityInstanceId, FIntPoint(1000, 1000));
    System->BuildPlacementGridMask(Moving, FVector2D(1000.0), FVector2D(1400.0), 100, 0.0, Mask);
    TestEqual(TEXT("Moving facility preview releases itself and retains tile buildability"), Mask.GetBuildableCellCount(), 16);

    FFFZSCommand OpenTile = MoveTo(Tile.FacilityInstanceId, FIntPoint(1000, 1000));
    OpenTile.Type = EFFZSCommandType::Open;
    TestTrue(TEXT("Tile participates in the active festival lifecycle"), System->ExecuteCommand(OpenTile).bSucceeded);
    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot = System->CaptureWorldSnapshot();
    FFFZSWorldSnapshot InvalidLayeredSnapshot = Save->Snapshot;
    for (FFFZSFacilityInstanceSnapshot& Saved : InvalidLayeredSnapshot.Facilities)
    {
        if (Saved.FacilityInstanceId == Tile.FacilityInstanceId) Saved.FacilitySpec.Category = EFFZSFacilityCategory::Road;
    }
    TArray<FText> InvalidErrors;
    TestFalse(TEXT("Restore rejects a Road underneath a saved facility"),
        System->RestoreWorldSnapshot(InvalidLayeredSnapshot, InvalidErrors));
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Layered world serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Saved world deserializes"), Loaded)) return false;
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Overlapping tile/facility world restores atomically"), System->RestoreWorldSnapshot(Loaded->Snapshot, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Saved foundation elevation survives restore"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestEqual(TEXT("Active tile with an empty closed queue survives restore"),
        System->FindFacility(Tile.FacilityInstanceId)->GetFacilityState(), EFFZSFacilityState::Active);
    TestTrue(TEXT("Legacy Road receipt data survives serialization and restore"),
        System->FindFacility(Road.FacilityInstanceId)->GetFacilitySpec().bAllowFacilityPlacementOnTop);
    TestFalse(TEXT("Contradictory legacy Tile flag survives serialization and restore"),
        System->FindFacility(Tile.FacilityInstanceId)->GetFacilitySpec().bAllowFacilityPlacementOnTop);
    TestTrue(TEXT("Restored Tile permission derives from category"),
        System->FindFacility(Tile.FacilityInstanceId)->GetFacilitySpec().AllowsFacilityPlacementOnTop());
    TestFalse(TEXT("Restored Road still blocks upper facilities"), System->ValidatePlacement(BlockedFacility).bIsValid);
    TestTrue(TEXT("Road install receipt survives restore"), System->ExecuteCommand(Road).bWasDuplicateReplay);
    TestTrue(TEXT("Tile install receipt survives restore"), System->ExecuteCommand(Tile).bWasDuplicateReplay);
    TestEqual(TEXT("Revalidation preserves a valid layered world"), System->RevalidateFacilitiesInZone(TEXT("GZ_DadaPocha")), 0);
    FFFZSCommand CloseTile = MoveTo(Tile.FacilityInstanceId, FIntPoint(1000, 1000));
    CloseTile.Type = EFFZSCommandType::Close;
    TestTrue(TEXT("Restored tile closes"), System->ExecuteCommand(CloseTile).bSucceeded);
    TestTrue(TEXT("Facility moves off the tile"), System->ExecuteCommand(
        MoveTo(Facility.FacilityInstanceId, FIntPoint(4000, 4000))).bSucceeded);
    TestEqual(TEXT("Moving onto bare ground removes tile height"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 0.0);
    TestFalse(TEXT("Tile cannot be inserted beneath an existing structure on bare ground"), System->ValidatePlacement(
        InstallAt(TileSpec, FIntPoint(4100, 4000))).bIsValid);
    TestFalse(TEXT("Road cannot be inserted beneath an existing structure on bare ground"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(4100, 4000))).bIsValid);
    TestTrue(TEXT("Facility can move back onto the tile"), System->ExecuteCommand(Moving).bSucceeded);
    TestEqual(TEXT("Moving back restores tile height"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestTrue(TEXT("Facility moves clear before altering its tile support"), System->ExecuteCommand(
        MoveTo(Facility.FacilityInstanceId, FIntPoint(4000, 4000))).bSucceeded);
    TestTrue(TEXT("Tile can move once the supported facility is relocated"), System->ExecuteCommand(
        MoveTo(Tile.FacilityInstanceId, FIntPoint(2000, 1000))).bSucceeded);
    TestFalse(TEXT("Moved tile retains ground-layer occupancy"), System->ValidatePlacement(
        InstallAt(TileSpec, FIntPoint(2000, 1000))).bIsValid);
    TestTrue(TEXT("Upper-layer facility retains its own occupancy after relocation"), System->ValidatePlacement(
        InstallAt(FacilitySpec, FIntPoint(4000, 4000))).Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    TestFalse(TEXT("Facility movement onto Road remains prohibited"), System->ExecuteCommand(
        MoveTo(Facility.FacilityInstanceId, FIntPoint(6000, 1000))).bSucceeded);
    TestTrue(TEXT("Unoccupied Road can move"), System->ExecuteCommand(
        MoveTo(Road.FacilityInstanceId, FIntPoint(6000, 2500))).bSucceeded);
    TestFalse(TEXT("Moved Road retains its blocking category"), System->ValidatePlacement(
        InstallAt(FacilitySpec, FIntPoint(6000, 2500))).bIsValid);
    FFFZSCommand RemoveRoad = MoveTo(Road.FacilityInstanceId, FIntPoint(6000, 2500));
    RemoveRoad.Type = EFFZSCommandType::Remove;
    TestTrue(TEXT("Unoccupied Road can be removed"), System->ExecuteCommand(RemoveRoad).bSucceeded);
    TestTrue(TEXT("Road removal releases the ground layer"), System->ValidatePlacement(
        InstallAt(TileSpec, FIntPoint(6000, 2500))).bIsValid);
    RemoveTile.CommandId = FGuid::NewGuid();
    RemoveTile.TransactionId = FGuid::NewGuid();
    TestTrue(TEXT("Tile can be removed once its supported facility has moved"), System->ExecuteCommand(RemoveTile).bSucceeded);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSLegacyRoadMigrationTest, "FF_ZoneSystem.Road.LegacyRoadSnapshotMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSLegacyRoadMigrationTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Migration world"), System)) return false;
    FFFZSFacilitySpec BuiltInSpec = MakeTileSpec();
    BuiltInSpec.FacilityDefinitionId = TEXT("Tile_Surface_Sand");
    BuiltInSpec.FootprintSizeCells = FIntPoint(440, 360);
    BuiltInSpec.PlacementObstacleHeightCm = 12.0f;
    BuiltInSpec.InstallCostKey = TEXT("Cost.Road.Surface.Sand");
    BuiltInSpec.RevenueRuleKey = TEXT("Saved.Custom.Revenue");
    BuiltInSpec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Tier;
    Tier.TargetLevel = 2;
    Tier.SizeMultiplier = 1.25f;
    Tier.ServiceDurationMultiplier = 0.8f;
    Tier.RevenueMultiplier = 1.3f;
    BuiltInSpec.UpgradeTiers.Add(Tier);
    const FFFZSCommand BuiltIn = InstallAt(BuiltInSpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Built-in paving fixture installs"), System->ExecuteCommand(BuiltIn).bSucceeded)) return false;
    FFFZSCommand Upgrade = MoveTo(BuiltIn.FacilityInstanceId, FIntPoint(1000, 1000));
    Upgrade.Type = EFFZSCommandType::Upgrade;
    Upgrade.TargetUpgradeLevel = Tier.TargetLevel;
    Upgrade.UpgradeSizeMultiplier = Tier.SizeMultiplier;
    Upgrade.UpgradeServiceDurationMultiplier = Tier.ServiceDurationMultiplier;
    Upgrade.UpgradeRevenueMultiplier = Tier.RevenueMultiplier;
    if (!TestTrue(TEXT("Paving fixture receives its paid upgrade"), System->ExecuteCommand(Upgrade).bSucceeded)) return false;

    FFFZSFacilitySpec UpperSpec = MakeRoadSpec();
    UpperSpec.FacilityDefinitionId = TEXT("Lighting_MigrationProbe");
    UpperSpec.Category = EFFZSFacilityCategory::Lighting;
    UpperSpec.FootprintSizeCells = FIntPoint(200, 200);
    UpperSpec.PlacementObstacleHeightCm = 100.0f;
    const FFFZSCommand Upper = InstallAt(UpperSpec, FIntPoint(1100, 1100));
    if (!TestTrue(TEXT("Facility installs on the saved paving"), System->ExecuteCommand(Upper).bSucceeded)) return false;

    FFFZSFacilitySpec CustomSpec = MakeTileSpec();
    CustomSpec.FacilityDefinitionId = TEXT("Custom_Paving_KeepIdentity");
    const FFFZSCommand Custom = InstallAt(CustomSpec, FIntPoint(3000, 1000));
    if (!TestTrue(TEXT("Custom paving fixture installs"), System->ExecuteCommand(Custom).bSucceeded)) return false;
    FFFZSFacilitySpec BlockingSpec = MakeRoadSpec();
    BlockingSpec.bAllowFacilityPlacementOnTop = false;
    const FFFZSCommand BlockingRoad = InstallAt(BlockingSpec, FIntPoint(6000, 1000));
    if (!TestTrue(TEXT("Nonpermissive legacy road fixture installs"), System->ExecuteCommand(BlockingRoad).bSucceeded)) return false;

    FFFZSWorldSnapshot Legacy = System->CaptureWorldSnapshot();
    const FFFZSFacilityInstanceSnapshot* OriginalBuiltIn = Legacy.Facilities.FindByPredicate(
        [&](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == BuiltIn.FacilityInstanceId; });
    if (!TestNotNull(TEXT("Saved upgraded paving"), OriginalBuiltIn)) return false;
    const FFFZSFacilityInstanceSnapshot ExpectedBuiltIn = *OriginalBuiltIn;
    const FFFZSFacilityInstanceSnapshot* OriginalUpper = Legacy.Facilities.FindByPredicate(
        [&](const FFFZSFacilityInstanceSnapshot& Saved) { return Saved.FacilityInstanceId == Upper.FacilityInstanceId; });
    if (!TestNotNull(TEXT("Saved facility on upgraded paving"), OriginalUpper)) return false;
    const FTransform ExpectedUpperTransform = OriginalUpper->WorldTransform;
    FFFZSCommand LegacyInstall = BuiltIn;
    LegacyInstall.FacilitySpec.Category = EFFZSFacilityCategory::Road;
    LegacyInstall.FacilitySpec.FacilityDefinitionId = TEXT("Road_Surface_Sand");
    LegacyInstall.FacilitySpec.bAllowFacilityPlacementOnTop = true;
    Legacy.SchemaVersion = 7;
    for (FFFZSFacilityInstanceSnapshot& Saved : Legacy.Facilities)
    {
        if (Saved.FacilityInstanceId == BuiltIn.FacilityInstanceId || Saved.FacilityInstanceId == Custom.FacilityInstanceId)
        {
            Saved.FacilitySpec.Category = EFFZSFacilityCategory::Road;
            Saved.FacilitySpec.bAllowFacilityPlacementOnTop = true;
            if (Saved.FacilityInstanceId == BuiltIn.FacilityInstanceId)
            {
                Saved.FacilitySpec.FacilityDefinitionId = TEXT("Road_Surface_Sand");
            }
        }
    }
    const FString HistoricalFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(LegacyInstall);
    for (FFFZSCommandReceiptSnapshot& Receipt : Legacy.CommandReceipts)
    {
        if (Receipt.CommandId == BuiltIn.CommandId) Receipt.PayloadFingerprint = HistoricalFingerprint;
    }

    TArray<FText> Errors;
    if (!TestTrue(TEXT("Schema 7 Road with a facility above migrates atomically"), System->RestoreWorldSnapshot(Legacy, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    const AFFZSFacilityActor* MigratedBuiltIn = System->FindFacility(BuiltIn.FacilityInstanceId);
    if (!TestNotNull(TEXT("Migrated built-in actor"), MigratedBuiltIn)) return false;
    const FFFZSFacilityInstanceSnapshot Migrated = MigratedBuiltIn->CaptureInstanceSnapshot();
    TestEqual(TEXT("Permissive Road becomes Tile"), Migrated.FacilitySpec.Category, EFFZSFacilityCategory::Tile);
    TestEqual(TEXT("Known built-in ID resolves to the matching Tile definition"),
        Migrated.FacilitySpec.FacilityDefinitionId, FName(TEXT("Tile_Surface_Sand")));
    TestEqual(TEXT("Custom definition identity remains intact"),
        System->FindFacility(Custom.FacilityInstanceId)->GetFacilitySpec().FacilityDefinitionId, CustomSpec.FacilityDefinitionId);
    TestEqual(TEXT("Custom permissive Road also becomes Tile"),
        System->FindFacility(Custom.FacilityInstanceId)->GetFacilitySpec().Category, EFFZSFacilityCategory::Tile);
    TestEqual(TEXT("False-flag Road stays Road"),
        System->FindFacility(BlockingRoad.FacilityInstanceId)->GetFacilitySpec().Category, EFFZSFacilityCategory::Road);
    TestFalse(TEXT("Legacy nonpermissive Road still rejects facilities"), System->ValidatePlacement(
        InstallAt(UpperSpec, FIntPoint(6100, 1100))).bIsValid);
    TestEqual(TEXT("Authored base footprint survives migration"), Migrated.FacilitySpec.FootprintSizeCells, BuiltInSpec.FootprintSizeCells);
    TestEqual(TEXT("Authored slab height survives migration"), Migrated.FacilitySpec.PlacementObstacleHeightCm, BuiltInSpec.PlacementObstacleHeightCm);
    TestEqual(TEXT("Economy cost key survives migration"), Migrated.FacilitySpec.InstallCostKey, BuiltInSpec.InstallCostKey);
    TestEqual(TEXT("Economy revenue key survives migration"), Migrated.FacilitySpec.RevenueRuleKey, BuiltInSpec.RevenueRuleKey);
    TestEqual(TEXT("Paid upgrade level survives migration"), Migrated.UpgradeLevel, ExpectedBuiltIn.UpgradeLevel);
    TestEqual(TEXT("Paid size effect survives migration"), Migrated.UpgradeSizeMultiplier, ExpectedBuiltIn.UpgradeSizeMultiplier);
    TestEqual(TEXT("Paid duration effect survives migration"), Migrated.UpgradeServiceDurationMultiplier, ExpectedBuiltIn.UpgradeServiceDurationMultiplier);
    TestEqual(TEXT("Paid revenue effect survives migration"), Migrated.UpgradeRevenueMultiplier, ExpectedBuiltIn.UpgradeRevenueMultiplier);
    TestEqual(TEXT("Authored tiers survive schema 7 migration"), Migrated.FacilitySpec.UpgradeTiers.Num(), BuiltInSpec.UpgradeTiers.Num());
    TestTrue(TEXT("Existing upgraded transform survives migration"), Migrated.WorldTransform.Equals(ExpectedBuiltIn.WorldTransform));
    const AFFZSFacilityActor* MigratedUpper = System->FindFacility(Upper.FacilityInstanceId);
    if (!TestNotNull(TEXT("Migrated facility on paving"), MigratedUpper)) return false;
    TestTrue(TEXT("Facility preserves its complete saved transform on upgraded paving"),
        MigratedUpper->GetActorTransform().Equals(ExpectedUpperTransform));
    TestTrue(TEXT("Historical Road install command replays after identity migration"), System->ExecuteCommand(LegacyInstall).bWasDuplicateReplay);

    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot = System->CaptureWorldSnapshot();
    TestEqual(TEXT("Migrated save writes schema 8"), Save->Snapshot.SchemaVersion, 8);
    const FFFZSCommandReceiptSnapshot* SavedReceipt = Save->Snapshot.CommandReceipts.FindByPredicate(
        [&](const FFFZSCommandReceiptSnapshot& Receipt) { return Receipt.CommandId == BuiltIn.CommandId; });
    if (!TestNotNull(TEXT("Historical receipt still exists"), SavedReceipt)) return false;
    TestEqual(TEXT("Historical receipt bytes remain unchanged"), SavedReceipt->PayloadFingerprint, HistoricalFingerprint);
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Migrated save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Migrated save deserializes"), Loaded)) return false;
    TestTrue(TEXT("Schema 8 migrated world restores without reinterpreting categories"), System->RestoreWorldSnapshot(Loaded->Snapshot, Errors));
    TestTrue(TEXT("Historical receipt replays after schema 8 save roundtrip"), System->ExecuteCommand(LegacyInstall).bWasDuplicateReplay);
    FFFZSWorldSnapshot InvalidCurrent = Loaded->Snapshot;
    for (FFFZSFacilityInstanceSnapshot& Saved : InvalidCurrent.Facilities)
    {
        if (Saved.FacilityInstanceId == BuiltIn.FacilityInstanceId)
        {
            Saved.FacilitySpec.Category = EFFZSFacilityCategory::Road;
            Saved.FacilitySpec.bAllowFacilityPlacementOnTop = true;
        }
    }
    TestFalse(TEXT("Schema 8 Road overlap rejects even if its legacy flag is true"), System->RestoreWorldSnapshot(InvalidCurrent, Errors));
    TestEqual(TEXT("Failed schema 8 restore preserves the previous Tile world"),
        System->FindFacility(BuiltIn.FacilityInstanceId)->GetFacilitySpec().Category, EFFZSFacilityCategory::Tile);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadPhysicalSurfaceTest, "FF_ZoneSystem.Road.PhysicalSurfaceGridAndObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadPhysicalSurfaceTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    Settings.Settings->bEnableGroundChecks = true;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Collision mesh"), Cube)) return false;
    TArray<UStaticMesh*> MeshesToFinish{Cube};
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    for (const EFFZSFacilityCategory Category : { EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile })
    {
        const bool bIsTile = Category == EFFZSFacilityCategory::Tile;
        const FString Label = bIsTile ? TEXT("Tile: ") : TEXT("Road: ");
        UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
        UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
        if (!TestNotNull(Label + TEXT("world"), System)) return false;
        if (!TestNotNull(Label + TEXT("physical terrain"), SpawnRoadTestBox(*World,
            FVector(5000, 5000, -10), FVector(6000, 6000, 10)))) return false;
        const FFFZSCommand Surface = InstallAt(bIsTile ? MakeTileSpec(Cube) : MakeRoadSpec(Cube), FIntPoint(1000, 1000));
        if (!TestTrue(Label + TEXT("physical surface installs"), System->ExecuteCommand(Surface).bSucceeded)) return false;
        AFFZSFacilityActor* SurfaceActor = System->FindFacility(Surface.FacilityInstanceId);
        TestEqual(Label + TEXT("participates in support traces"),
            SurfaceActor->VisualMesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
        TestEqual(Label + TEXT("remains walkable by the player capsule"),
            SurfaceActor->VisualMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
        TArray<UBoxComponent*> Boxes;
        SurfaceActor->GetComponents(Boxes);
        for (const UBoxComponent* Box : Boxes)
        {
            TestFalse(Label + TEXT("creates no navigation hole or costly queue lane"), Box->CanEverAffectNavigation());
        }
        FHitResult Hit;
        TestTrue(Label + TEXT("ground ray hits the surface"), World->LineTraceSingleByChannel(Hit,
            FVector(1200, 1200, 100), FVector(1200, 1200, -100), ECC_Visibility));
        TestTrue(Label + TEXT("collision top is 8 cm"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 8.0, 0.01));
        const FFFZSPlacementValidation SurfaceMove = System->ValidatePlacement(
            MoveTo(Surface.FacilityInstanceId, FIntPoint(1000, 1000)));
        TestTrue(Label + TEXT("can be validated at its own footprint"), SurfaceMove.bIsValid);
        TestTrue(Label + TEXT("movement traces underlying ground without accumulated elevation"),
            FMath::IsNearlyZero(SurfaceMove.PlacementTransform.GetLocation().Z, 0.01));

        const FFFZSCommand Facility = InstallAt(FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck), FIntPoint(1000, 1000));
        FFFZSBuildableGridMask Mask;
        System->BuildPlacementGridMask(Facility, FVector2D(1100.0), FVector2D(1300.0), 100, 0.0, Mask);
        FFFZSBuildGridSurfaceCache GridSurface;
        GridSurface.Update(*World, FTransform::Identity, 0.0, Mask, 512);
        TestEqual(Label + TEXT("facility preview follows category permission"), Mask.GetBuildableCellCount(), bIsTile ? 4 : 0);
        if (bIsTile)
        {
            TestTrue(Label + TEXT("grid follows the support surface"),
                FMath::IsNearlyEqual(GridSurface.GetWorldPoint(FVector2D(1200.0)).Z, 8.0, 0.01));
        }

        AActor* Obstacle = SpawnRoadTestBox(*World, FVector(1100, 1100, 60), FVector(30, 30, 60));
        if (!TestNotNull(Label + TEXT("scenery obstacle on paving"), Obstacle)) return false;
        TestFalse(Label + TEXT("does not bypass scenery obstruction validation"), System->ValidatePlacement(Facility).bIsValid);
        Obstacle->Destroy();
        const FFFZSPlacementValidation Validation = System->ValidatePlacement(Facility);
        TestEqual(Label + TEXT("only Tile supports installation on mixed paving and terrain"), Validation.bIsValid, bIsTile);
        if (bIsTile)
        {
            TestTrue(Label + TEXT("mixed ground samples never sink the foundation below paving"),
                FMath::IsNearlyEqual(Validation.PlacementTransform.GetLocation().Z, 8.0, 0.01));
        }
        else
        {
            TestTrue(Label + TEXT("blocked placement identifies occupied ground"),
                Validation.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadSpatialGroundTest, "FF_ZoneSystem.Road.SpatialGroundSampling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadSpatialGroundTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    Settings.Settings->bEnableGroundChecks = true;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Ground sampling collision mesh"), Cube)) return false;
    TArray<UStaticMesh*> MeshesToFinish{Cube};
    FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Ground sampling world"), System) ||
        !TestNotNull(TEXT("Terrain under both zones"), SpawnRoadTestBox(*World,
            FVector(10000, 5000, -10), FVector(20000, 6000, 10)))) return false;

    // A shared-edge neighbor can have decorative collision extending into the next road.
    FFFZSFacilitySpec NeighborSpec = MakeRoadSpec(Cube);
    NeighborSpec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    NeighborSpec.VisualOffsetCm = FVector(240, 50, 50);
    const FFFZSCommand Neighbor = InstallAt(NeighborSpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Offset shared-edge road installs"), System->ExecuteCommand(Neighbor).bSucceeded)) return false;
    FHitResult Hit;
    const FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(FFZSSpatialGroundTest), false);
    TestTrue(TEXT("A real simple-collision ray reaches the shared-edge overhang"), World->LineTraceSingleByChannel(
        Hit, FVector(1450, 1250, 300), FVector(1450, 1250, -100), ECC_Visibility, SupportQuery));
    TestTrue(TEXT("The shared-edge fixture physically hits the neighboring road"),
        Hit.GetActor() == System->FindFacility(Neighbor.FacilityInstanceId));
    const FFFZSCommand NextRoad = InstallAt(MakeRoadSpec(Cube), FIntPoint(1400, 1000));
    const auto CheckTerrainHeight = [&](const TCHAR* Label)
    {
        const FFFZSPlacementValidation Result = System->ValidatePlacement(NextRoad);
        TestTrue(FString(Label) + TEXT(" remains a valid placement"), Result.bIsValid);
        TestTrue(FString(Label) + TEXT(" samples terrain below paving"),
            FMath::IsNearlyZero(Result.PlacementTransform.GetLocation().Z, 0.01));
    };
    CheckTerrainHeight(TEXT("Touching spatial candidate"));

    AFFZSGameplayZone* RemoteZone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Second zone"), RemoteZone) ||
        !TestTrue(TEXT("Second accepted zone"), RemoteZone->ApplyAcceptedZonePreset(TEXT("GZ_SunsetBeachClub")))) return false;
    RemoteZone->SetActorLocation(FVector(20000, 0, 0));
    RemoteZone->bAllowAllFacilityCategories = true;
    if (!TestTrue(TEXT("Second zone registers"), System->RegisterZone(RemoteZone))) return false;

    // The index intentionally stores authoritative footprints, not arbitrary visual offsets.
    // This other-zone tile's collision reaches the candidate while its footprint is far away.
    FFFZSFacilitySpec RemoteSpec = MakeTileSpec(Cube);
    RemoteSpec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    RemoteSpec.VisualOffsetCm = FVector(-19650, 50, 150);
    FFFZSCommand RemoteTile = InstallAt(RemoteSpec, FIntPoint(1000, 1000));
    RemoteTile.ZoneId = RemoteZone->ZoneId;
    if (!TestTrue(TEXT("Other-zone offset tile installs"), System->ExecuteCommand(RemoteTile).bSucceeded)) return false;
    TestTrue(TEXT("A real simple-collision ray hits the out-of-index tile"), World->LineTraceSingleByChannel(
        Hit, FVector(1550, 1250, 300), FVector(1550, 1250, -100), ECC_Visibility, SupportQuery));
    TestTrue(TEXT("Ray fixture actually reaches the remote registered tile"),
        Hit.GetActor() == System->FindFacility(RemoteTile.FacilityInstanceId));
    CheckTerrainHeight(TEXT("Out-of-footprint collision from another zone"));

    const FFFZSPlacementValidation Moving = System->ValidatePlacement(
        MoveTo(Neighbor.FacilityInstanceId, FIntPoint(1000, 1000)));
    TestTrue(TEXT("Moving a road still ignores itself"), Moving.bIsValid &&
        FMath::IsNearlyZero(Moving.PlacementTransform.GetLocation().Z, 0.01));
    if (!TestNotNull(TEXT("Ordinary scenery obstruction"), SpawnRoadTestBox(*World,
        FVector(1650, 1250, 60), FVector(20, 20, 60)))) return false;
    TestFalse(TEXT("Skipping paving never skips ordinary scenery"), System->ValidatePlacement(NextRoad).bIsValid);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSPlacedFacilityCountTest, "FF_ZoneSystem.World.PlacedFacilityCount",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPlacedFacilityCountTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Facility count world"), System)) return false;
    TestEqual(TEXT("Empty registry has no placed facilities"), System->GetPlacedFacilityCount(), 0);
    const FFFZSCommand Road = InstallAt(MakeRoadSpec(), FIntPoint(1000, 1000));
    const FFFZSCommand Tile = InstallAt(MakeTileSpec(), FIntPoint(3000, 1000));
    if (!TestTrue(TEXT("Counted road installs"), System->ExecuteCommand(Road).bSucceeded) ||
        !TestTrue(TEXT("Counted tile installs"), System->ExecuteCommand(Tile).bSucceeded)) return false;
    TestEqual(TEXT("Unopened road and tile both count"), System->GetPlacedFacilityCount(), 2);
    TestTrue(TEXT("Install replay remains idempotent"), System->ExecuteCommand(Road).bWasDuplicateReplay);
    TestEqual(TEXT("Receipt replay cannot duplicate the count"), System->GetPlacedFacilityCount(), 2);
    FFFZSCommand Remove = MoveTo(Road.FacilityInstanceId, FIntPoint(1000, 1000));
    Remove.Type = EFFZSCommandType::Remove;
    if (!TestTrue(TEXT("Counted road is removed"), System->ExecuteCommand(Remove).bSucceeded)) return false;
    TestEqual(TEXT("Removed facilities are excluded"), System->GetPlacedFacilityCount(), 1);
    AFFZSFacilityActor* Remaining = System->FindFacility(Tile.FacilityInstanceId);
    if (!TestNotNull(TEXT("Remaining registered tile"), Remaining)) return false;
    Remaining->Destroy();
    TestEqual(TEXT("Destroyed weak references are excluded"), System->GetPlacedFacilityCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadProductionCollisionTest, "FF_ZoneSystem.Road.ProductionRoadAndTileMeshes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadProductionCollisionTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    Settings.Settings->bEnableGroundChecks = true;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Production road world"), System)) return false;
    if (!TestNotNull(TEXT("Physical terrain"), SpawnRoadTestBox(*World,
        FVector(5000, 5000, -10), FVector(6000, 6000, 10)))) return false;
    const TArray<FString> RoadNames = { TEXT("Sand"), TEXT("Asphalt"), TEXT("Herringbone"), TEXT("Concrete"),
        TEXT("BlueWhiteStripedWalkway"), TEXT("ShellPatternWalkway"), TEXT("LuminousEdgeNightPath") };
    const TArray<FString> TileNames = { TEXT("Wood"), TEXT("Stone"), TEXT("GrassBlockPicnicTile"),
        TEXT("OceanMosaicPlazaTile"), TEXT("ShipDeckTile") };
    for (const EFFZSFacilityCategory Category : { EFFZSFacilityCategory::Road, EFFZSFacilityCategory::Tile })
    {
        const FString SurfaceType = Category == EFFZSFacilityCategory::Tile ? TEXT("Tile") : TEXT("Road");
        const TArray<FString>& Names = Category == EFFZSFacilityCategory::Tile ? TileNames : RoadNames;
        for (int32 Index = 0; Index < Names.Num(); ++Index)
        {
            const FString Path = TEXT("/Game/FestaFiesta/Building/FF_Data_Building/") + SurfaceType + TEXT("/PDA_FFZS_") + SurfaceType + TEXT("_Surface_") + Names[Index];
            UFFZSFacilityDefinition* Definition = LoadObject<UFFZSFacilityDefinition>(nullptr, *Path);
            if (!TestNotNull(Names[Index] + TEXT(" production definition"), Definition)) return false;
            UStaticMesh* Mesh = Definition->Spec.VisualMesh.LoadSynchronous();
            if (!TestNotNull(Names[Index] + TEXT(" imported mesh"), Mesh)) return false;
            AddInfo(FString::Printf(TEXT("%s static mesh compilation pending before collision fixture: %s"),
                *Names[Index], Mesh->IsCompiling() ? TEXT("true") : TEXT("false")));
            // Loading a cold editor UObject does not finish its async mesh compilation. UE deliberately
            // skips UStaticMeshComponent physics creation while IsCompiling(), so wait on this asset's
            // actual readiness before testing its imported collision (never on a fixed time delay).
            TArray<UStaticMesh*> MeshesToFinish{Mesh};
            FStaticMeshCompilingManager::Get().FinishCompilation(MeshesToFinish);
            if (!TestFalse(Names[Index] + TEXT(" collision fixture mesh finished compilation"), Mesh->IsCompiling())) return false;
            TestEqual(Names[Index] + TEXT(" category: ") + SurfaceType, Definition->Spec.Category, Category);
            TArray<FText> SpecErrors;
            if (!TestTrue(Names[Index] + TEXT(" authored specification is valid"), Definition->Spec.IsValid(SpecErrors)))
            {
                for (const FText& Error : SpecErrors) AddError(Error.ToString());
                return false;
            }
            const FIntPoint Origin(1000 + Index * 1200, Category == EFFZSFacilityCategory::Tile ? 3000 : 1000);
            const FFFZSCommand Surface = InstallAt(Definition->Spec, Origin);
            if (!TestTrue(Names[Index] + TEXT(" production ground surface installs"), System->ExecuteCommand(Surface).bSucceeded)) return false;
            const AFFZSFacilityActor* Actor = System->FindFacility(Surface.FacilityInstanceId);
            if (!TestNotNull(Names[Index] + TEXT(" installed actor"), Actor)) return false;
            TestEqual(Names[Index] + TEXT(" installation preserves authored dimensions"),
                Actor->GetFacilitySpec().FootprintSizeCells, Definition->Spec.FootprintSizeCells);
            TestEqual(Names[Index] + TEXT(" installation preserves upper-facility permission"),
                Actor->GetFacilitySpec().AllowsFacilityPlacementOnTop(), Category == EFFZSFacilityCategory::Tile);
            TestTrue(Names[Index] + TEXT(" imported mesh physics state is ready"), Actor->VisualMesh->IsPhysicsStateCreated());
            // UWorld's omitted DefaultQueryParam requests complex render triangles. Match the runtime
            // ground/player support query instead: the authored UCX slab bridges decorative grooves.
            const FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(FFZSRoadProductionSupport), false);
            const FIntPoint Footprint = Definition->Spec.FootprintSizeCells;
            const double ExpectedSurfaceZ = Actor->GetActorLocation().Z + Definition->Spec.PlacementObstacleHeightCm;
            const UBodySetup* BodySetup = Mesh->GetBodySetup();
            if (!TestNotNull(Names[Index] + TEXT(" authored collision body"), BodySetup)) return false;
            const double ExpectedCollisionZ = BodySetup->AggGeom.CalcAABB(Actor->VisualMesh->GetComponentTransform()).Max.Z;
            // Expansion70 floors deliberately place their planar inlays up to 1.1 mm
            // above the foundation collider (_floors). Check the collider precisely,
            // and separately ensure that this authored visual inset stays sub-1.5 mm.
            TestTrue(Names[Index] + TEXT(" foundation collision remains just below the visual surface"),
                ExpectedCollisionZ <= ExpectedSurfaceZ + 0.05 && ExpectedCollisionZ >= ExpectedSurfaceZ - 0.15);
            // Keep nine interior support samples when a designer changes the PDA dimensions.
            // Fixed 50/200/350 cm samples would trace outside a smaller, correctly scaled road.
            for (const double XFraction : { 0.125, 0.5, 0.875 })
            {
                for (const double YFraction : { 0.125, 0.5, 0.875 })
                {
                    const double X = Footprint.X * XFraction;
                    const double Y = Footprint.Y * YFraction;
                    const FVector SampleWorld = Actor->GetActorTransform().TransformPosition(
                        FVector(X - Footprint.X * 0.5, Y - Footprint.Y * 0.5, 0.0));
                    FHitResult Hit;
                    World->LineTraceSingleByChannel(Hit, FVector(SampleWorld.X, SampleWorld.Y, ExpectedSurfaceZ + 100.0),
                        FVector(SampleWorld.X, SampleWorld.Y, Actor->GetActorLocation().Z - 100.0), ECC_Visibility, SupportQuery);
                    TestTrue(FString::Printf(TEXT("%s authored collision at %.2f,%.2f supports tile: hit=%s Z=%.6f expected=%s Z=%.6f"),
                        *Names[Index], X, Y, *GetNameSafe(Hit.GetActor()), Hit.ImpactPoint.Z, *Actor->GetName(), ExpectedCollisionZ),
                        Hit.GetActor() == Actor && FMath::IsNearlyEqual(Hit.ImpactPoint.Z, ExpectedCollisionZ, 0.05));
                }
            }
            // A compact non-service facility fits fully on the authored tile. A fixed 6x3 m
            // food truck would test neighboring roads and mixed terrain instead of this PDA.
            FFFZSFacilitySpec UpperSpec;
            UpperSpec.FacilityDefinitionId = TEXT("Lighting_GroundSurfaceSupportProbe");
            UpperSpec.DisplayName = FText::FromString(TEXT("Ground surface support probe"));
            UpperSpec.Category = EFFZSFacilityCategory::Lighting;
            UpperSpec.FootprintSizeCells = FIntPoint(FMath::Max(1, Footprint.X / 2), FMath::Max(1, Footprint.Y / 2));
            UpperSpec.ClearanceCells = 0;
            UpperSpec.PlacementObstacleHeightCm = 100.0f;
            UpperSpec.QueueRules.MaxWaitingAgents = 0;
            UpperSpec.QueueRules.ServiceCapacity = 0;
            TestTrue(Names[Index] + TEXT(" upper-facility probe is valid on bare terrain"), System->ValidatePlacement(
                InstallAt(UpperSpec, Origin + FIntPoint(0, Footprint.Y + 100))).bIsValid);
            const FIntPoint UpperOrigin = Origin + FIntPoint(
                (Footprint.X - UpperSpec.FootprintSizeCells.X) / 2, (Footprint.Y - UpperSpec.FootprintSizeCells.Y) / 2);
            const FFFZSPlacementValidation Facility = System->ValidatePlacement(InstallAt(UpperSpec, UpperOrigin));
            TestEqual(Names[Index] + TEXT(" upper-facility acceptance follows its category"),
                Facility.bIsValid, Category == EFFZSFacilityCategory::Tile);
            if (Category == EFFZSFacilityCategory::Tile)
            {
                TestTrue(Names[Index] + TEXT(" supports the upper foundation at its authored surface height"),
                    FMath::IsNearlyEqual(Facility.PlacementTransform.GetLocation().Z, ExpectedSurfaceZ, 0.05));
            }
            else
            {
                TestTrue(Names[Index] + TEXT(" disallowed upper placement fails because its footprint is occupied"),
                    Facility.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
            }
            // Exercise the same authority commands used by a drag with the real imported
            // meshes and terrain checks enabled, including shared-edge support traces.
            for (int32 Neighbor = 1; Neighbor <= 2; ++Neighbor)
            {
                TestTrue(FString::Printf(TEXT("%s continuous neighbor %d installs with real collision"), *Names[Index], Neighbor),
                    System->ExecuteCommand(InstallAt(Definition->Spec,
                        Origin + FIntPoint(Neighbor * Footprint.X, 0))).bSucceeded);
            }
        }
    }
    return true;
}

#endif
