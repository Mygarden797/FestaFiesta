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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadSpecTest, "FF_ZoneSystem.Road.SpecAndSaveCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadSpecTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Existing Lighting ordinal stays serialized as 7"), static_cast<uint8>(EFFZSFacilityCategory::Lighting), uint8(7));
    TestEqual(TEXT("Road is appended after existing categories"), static_cast<uint8>(EFFZSFacilityCategory::Road), uint8(8));
    TArray<FText> Errors;
    FFFZSFacilitySpec Road = MakeRoadSpec();
    TestTrue(TEXT("An 8 cm road needs no queue or service anchors"), Road.IsValid(Errors));
    Road.QueueRules.ServiceCapacity = 1;
    TestFalse(TEXT("Road service data fails authoring validation"), Road.IsValid(Errors));
    Road = MakeRoadSpec();
    Road.PlacementObstacleHeightCm = 0.0f;
    TestFalse(TEXT("A zero-thickness road is invalid"), Road.IsValid(Errors));
    FFFZSFacilitySpec Facility = FFZSTest::MakeValidFacilitySpec();
    Facility.PlacementObstacleHeightCm = 8.0f;
    TestFalse(TEXT("Other facilities retain their 10 cm minimum"), Facility.IsValid(Errors));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadLayerLifecycleTest, "FF_ZoneSystem.Road.OccupancyMoveUpgradeAndRestore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadLayerLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    if (!TestNotNull(TEXT("Road test world registers"), System)) return false;
    const FFFZSFacilitySpec RoadSpec = MakeRoadSpec();
    const FFFZSCommand Road = InstallAt(RoadSpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("First road installs"), System->ExecuteCommand(Road).bSucceeded)) return false;
    TestFalse(TEXT("A second road cannot stack on the first"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(1000, 1000))).bIsValid);
    TestTrue(TEXT("Roads may meet at a shared edge"), System->ExecuteCommand(
        InstallAt(RoadSpec, FIntPoint(1400, 1000))).bSucceeded);
    TestTrue(TEXT("A road can occupy the future queue and clearance area"), System->ExecuteCommand(
        InstallAt(RoadSpec, FIntPoint(1000, 1500))).bSucceeded);
    FFFZSFacilitySpec BlockingRoadSpec = RoadSpec;
    BlockingRoadSpec.bAllowFacilityPlacementOnTop = false;
    const FFFZSCommand BlockingRoad = InstallAt(BlockingRoadSpec, FIntPoint(6000, 1000));
    TestTrue(TEXT("A road may disable facilities through its saved spec"), System->ExecuteCommand(BlockingRoad).bSucceeded);
    const FFFZSCommand BlockedFacility = InstallAt(FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck), FIntPoint(6000, 1000));
    TestFalse(TEXT("Disabled road rejects facility placement"), System->ValidatePlacement(BlockedFacility).bIsValid);
    FFFZSBuildableGridMask DisabledMask;
    System->BuildPlacementGridMask(BlockedFacility, FVector2D(6000, 1000), FVector2D(6400, 1400), 100, 0.0, DisabledMask);
    TestEqual(TEXT("Disabled road is blocked in facility grid preview"), DisabledMask.GetBuildableCellCount(), 0);

    FFFZSFacilitySpec FacilitySpec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    FacilitySpec.bUsesFacilityLevels = true;
    FFFZSFacilityUpgradeTier Tier;
    Tier.SizeMultiplier = 1.1f;
    FacilitySpec.UpgradeTiers.Add(Tier);
    const FFFZSCommand Facility = InstallAt(FacilitySpec, FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Facility footprint, clearance and queue may overlap roads"),
        System->ExecuteCommand(Facility).bSucceeded)) return false;
    TestEqual(TEXT("Foundation reaches road height before any visual mesh loads"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestFalse(TEXT("Facilities still block facilities on the same road"), System->ValidatePlacement(
        InstallAt(FacilitySpec, FIntPoint(1000, 1000))).bIsValid);
    TestTrue(TEXT("Size upgrade validation ignores paving reservations"),
        System->ValidateUpgradePlacement(Facility.FacilityInstanceId, 1.1f).bIsValid);
    TestFalse(TEXT("Road move cannot strand an installed facility"), System->ExecuteCommand(
        MoveTo(Road.FacilityInstanceId, FIntPoint(2000, 1000))).bSucceeded);
    FFFZSCommand RemoveRoad = MoveTo(Road.FacilityInstanceId, FIntPoint(1000, 1000));
    RemoveRoad.Type = EFFZSCommandType::Remove;
    TestFalse(TEXT("Road removal cannot strand an installed facility"), System->ExecuteCommand(RemoveRoad).bSucceeded);
    TestTrue(TEXT("Road may be added beneath only the existing queue"), System->ExecuteCommand(
        InstallAt(RoadSpec, FIntPoint(1500, 1500))).bSucceeded);

    FFFZSBuildableGridMask Mask;
    System->BuildPlacementGridMask(InstallAt(RoadSpec, FIntPoint(1000, 1000)),
        FVector2D(1000.0), FVector2D(1400.0), 100, 0.0, Mask);
    TestEqual(TEXT("Road preview masks existing roads"), Mask.GetBuildableCellCount(), 0);
    FFFZSCommand Moving = MoveTo(Facility.FacilityInstanceId, FIntPoint(1000, 1000));
    System->BuildPlacementGridMask(Moving, FVector2D(1000.0), FVector2D(1400.0), 100, 0.0, Mask);
    TestEqual(TEXT("Moving facility preview releases itself and retains road buildability"), Mask.GetBuildableCellCount(), 16);

    FFFZSCommand OpenRoad = MoveTo(Road.FacilityInstanceId, FIntPoint(1000, 1000));
    OpenRoad.Type = EFFZSCommandType::Open;
    TestTrue(TEXT("Road participates in the active festival lifecycle"), System->ExecuteCommand(OpenRoad).bSucceeded);
    UFFZSSaveGameTestObject* Save = NewObject<UFFZSSaveGameTestObject>();
    Save->Snapshot = System->CaptureWorldSnapshot();
    FFFZSWorldSnapshot InvalidLayeredSnapshot = Save->Snapshot;
    for (FFFZSFacilityInstanceSnapshot& Saved : InvalidLayeredSnapshot.Facilities)
    {
        if (Saved.FacilityInstanceId == Road.FacilityInstanceId) Saved.FacilitySpec.bAllowFacilityPlacementOnTop = false;
    }
    TArray<FText> InvalidErrors;
    TestFalse(TEXT("Restore rejects a disallowed road underneath a saved facility"),
        System->RestoreWorldSnapshot(InvalidLayeredSnapshot, InvalidErrors));
    TArray<uint8> Bytes;
    if (!TestTrue(TEXT("Layered world serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes))) return false;
    UFFZSSaveGameTestObject* Loaded = Cast<UFFZSSaveGameTestObject>(UGameplayStatics::LoadGameFromMemory(Bytes));
    if (!TestNotNull(TEXT("Saved world deserializes"), Loaded)) return false;
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Overlapping road/facility world restores atomically"), System->RestoreWorldSnapshot(Loaded->Snapshot, Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    TestEqual(TEXT("Saved foundation elevation survives restore"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestEqual(TEXT("Active road with an empty closed queue survives restore"),
        System->FindFacility(Road.FacilityInstanceId)->GetFacilityState(), EFFZSFacilityState::Active);
    TestFalse(TEXT("Road placement permission survives serialization and restore"),
        System->FindFacility(BlockingRoad.FacilityInstanceId)->GetFacilitySpec().bAllowFacilityPlacementOnTop);
    TestEqual(TEXT("Revalidation preserves a valid layered world"), System->RevalidateFacilitiesInZone(TEXT("GZ_DadaPocha")), 0);
    FFFZSCommand CloseRoad = MoveTo(Road.FacilityInstanceId, FIntPoint(1000, 1000));
    CloseRoad.Type = EFFZSCommandType::Close;
    TestTrue(TEXT("Restored road closes"), System->ExecuteCommand(CloseRoad).bSucceeded);
    TestTrue(TEXT("Facility moves off the road"), System->ExecuteCommand(
        MoveTo(Facility.FacilityInstanceId, FIntPoint(4000, 4000))).bSucceeded);
    TestEqual(TEXT("Moving onto bare ground removes road height"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 0.0);
    TestFalse(TEXT("Road cannot be inserted beneath an existing structure on bare ground"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(4100, 4000))).bIsValid);
    TestTrue(TEXT("Facility can move back onto the road"), System->ExecuteCommand(Moving).bSucceeded);
    TestEqual(TEXT("Moving back restores road height"),
        System->FindFacility(Facility.FacilityInstanceId)->GetActorLocation().Z, 8.0);
    TestTrue(TEXT("Facility moves clear before altering its road support"), System->ExecuteCommand(
        MoveTo(Facility.FacilityInstanceId, FIntPoint(4000, 4000))).bSucceeded);
    TestTrue(TEXT("Road can move once the supported facility is relocated"), System->ExecuteCommand(
        MoveTo(Road.FacilityInstanceId, FIntPoint(2000, 1000))).bSucceeded);
    TestFalse(TEXT("Moved road retains ground-layer occupancy"), System->ValidatePlacement(
        InstallAt(RoadSpec, FIntPoint(2000, 1000))).bIsValid);
    TestTrue(TEXT("Upper-layer facility retains its own occupancy after relocation"), System->ValidatePlacement(
        InstallAt(FacilitySpec, FIntPoint(4000, 4000))).Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadPhysicalSurfaceTest, "FF_ZoneSystem.Road.PhysicalSurfaceGridAndObstacles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRoadPhysicalSurfaceTest::RunTest(const FString& Parameters)
{
    FScopedRoadSettings Settings;
    Settings.Settings->bEnableGroundChecks = true;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* System = World ? PrepareRoadWorld(*World) : nullptr;
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Road world"), System) || !TestNotNull(TEXT("Collision mesh"), Cube)) return false;
    if (!TestNotNull(TEXT("Physical terrain"), SpawnRoadTestBox(*World,
        FVector(5000, 5000, -10), FVector(6000, 6000, 10)))) return false;
    const FFFZSCommand Road = InstallAt(MakeRoadSpec(Cube), FIntPoint(1000, 1000));
    if (!TestTrue(TEXT("Physical road installs"), System->ExecuteCommand(Road).bSucceeded)) return false;
    AFFZSFacilityActor* RoadActor = System->FindFacility(Road.FacilityInstanceId);
    TestEqual(TEXT("Road participates in support traces"),
        RoadActor->VisualMesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Block);
    TestEqual(TEXT("Road remains walkable by the player capsule"),
        RoadActor->VisualMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
    TArray<UBoxComponent*> Boxes;
    RoadActor->GetComponents(Boxes);
    for (const UBoxComponent* Box : Boxes)
    {
        TestFalse(TEXT("Paving creates no navigation hole or costly queue lane"), Box->CanEverAffectNavigation());
    }
    FHitResult Hit;
    TestTrue(TEXT("Ground ray hits the road surface"), World->LineTraceSingleByChannel(Hit,
        FVector(1200, 1200, 100), FVector(1200, 1200, -100), ECC_Visibility));
    TestTrue(TEXT("Road collision top is 8 cm"), FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 8.0, 0.01));
    const FFFZSPlacementValidation RoadMove = System->ValidatePlacement(MoveTo(Road.FacilityInstanceId, FIntPoint(1000, 1000)));
    TestTrue(TEXT("Road can be validated at its own footprint"), RoadMove.bIsValid);
    TestTrue(TEXT("Road movement traces underlying ground, avoiding accumulated elevation"),
        FMath::IsNearlyZero(RoadMove.PlacementTransform.GetLocation().Z, 0.01));

    const FFFZSCommand Facility = InstallAt(FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck), FIntPoint(1000, 1000));
    FFFZSBuildableGridMask Mask;
    System->BuildPlacementGridMask(Facility, FVector2D(1100.0), FVector2D(1300.0), 100, 0.0, Mask);
    FFFZSBuildGridSurfaceCache Surface;
    Surface.Update(*World, FTransform::Identity, 0.0, Mask, 512);
    TestEqual(TEXT("Road cells stay buildable for a facility"), Mask.GetBuildableCellCount(), 4);
    TestTrue(TEXT("Grid follows the road support surface"),
        FMath::IsNearlyEqual(Surface.GetWorldPoint(FVector2D(1200.0)).Z, 8.0, 0.01));

    AActor* Obstacle = SpawnRoadTestBox(*World, FVector(1100, 1100, 60), FVector(30, 30, 60));
    if (!TestNotNull(TEXT("Scenery obstacle on paving"), Obstacle)) return false;
    TestFalse(TEXT("Road does not bypass scenery obstruction validation"), System->ValidatePlacement(Facility).bIsValid);
    Obstacle->Destroy();
    const FFFZSPlacementValidation Validation = System->ValidatePlacement(Facility);
    TestTrue(TEXT("Facility is valid on mixed paving and bare ground"), Validation.bIsValid);
    TestTrue(TEXT("Mixed ground samples never sink the foundation below paving"),
        FMath::IsNearlyEqual(Validation.PlacementTransform.GetLocation().Z, 8.0, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRoadProductionCollisionTest, "FF_ZoneSystem.Road.ProductionSixMeshes",
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
    const TArray<FString> Names = { TEXT("Sand"), TEXT("Asphalt"), TEXT("Wood"), TEXT("Stone"), TEXT("Herringbone"), TEXT("Concrete") };
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        const FString Path = TEXT("/Game/FestaFiesta/Zone/Data/FacilityDefinitions/Road/PDA_FFZS_Road_Surface_") + Names[Index];
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
        TestEqual(Names[Index] + TEXT(" Road category"), Definition->Spec.Category, EFFZSFacilityCategory::Road);
        TestTrue(Names[Index] + TEXT(" enables facilities above by default"), Definition->Spec.bAllowFacilityPlacementOnTop);
        const FIntPoint Origin(1000 + Index * 800, 1000);
        const FFFZSCommand Road = InstallAt(Definition->Spec, Origin);
        if (!TestTrue(Names[Index] + TEXT(" production road installs"), System->ExecuteCommand(Road).bSucceeded)) return false;
        const AFFZSFacilityActor* Actor = System->FindFacility(Road.FacilityInstanceId);
        if (!TestNotNull(Names[Index] + TEXT(" installed actor"), Actor)) return false;
        TestTrue(Names[Index] + TEXT(" imported mesh physics state is ready"), Actor->VisualMesh->IsPhysicsStateCreated());
        // UWorld's omitted DefaultQueryParam requests complex render triangles. Match the runtime
        // ground/player support query instead: the authored UCX slab bridges decorative grooves.
        const FCollisionQueryParams SupportQuery(SCENE_QUERY_STAT(FFZSRoadProductionSupport), false);
        for (const double X : { 50.0, 200.0, 350.0 })
        {
            for (const double Y : { 50.0, 200.0, 350.0 })
            {
                FHitResult Hit;
                World->LineTraceSingleByChannel(Hit, FVector(Origin.X + X, Origin.Y + Y, 100),
                    FVector(Origin.X + X, Origin.Y + Y, -100), ECC_Visibility, SupportQuery);
                TestTrue(FString::Printf(TEXT("%s authored collision at %.0f,%.0f supports tile: hit=%s Z=%.6f expected=%s Z=8"),
                    *Names[Index], X, Y, *GetNameSafe(Hit.GetActor()), Hit.ImpactPoint.Z, *Actor->GetName()),
                    Hit.GetActor() == Actor && FMath::IsNearlyEqual(Hit.ImpactPoint.Z, 8.0, 0.05));
            }
        }
        const FFFZSPlacementValidation Facility = System->ValidatePlacement(InstallAt(
            FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck), Origin));
        TestTrue(Names[Index] + TEXT(" accepts a facility on its surface"), Facility.bIsValid);
        TestTrue(Names[Index] + TEXT(" supports facility foundation at 8 cm"),
            FMath::IsNearlyEqual(Facility.PlacementTransform.GetLocation().Z, 8.0, 0.05));
    }
    return true;
}

#endif
