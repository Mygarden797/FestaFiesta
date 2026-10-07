// Verify terrain support is not reported as a paving obstruction, while real obstacles remain blocked.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
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
    };

    UFFZSWorldSubsystem* PrepareTerrainWorld(UWorld& World)
    {
        UFFZSWorldSubsystem* System = World.GetSubsystem<UFFZSWorldSubsystem>();
        AFFZSGameplayZone* Zone = World.SpawnActor<AFFZSGameplayZone>();
        if (!System || !Zone) return nullptr;
        Zone->ApplyAcceptedZonePreset(TEXT("GZ_DadaPocha"));
        Zone->bAllowAllFacilityCategories = true;
        return System->RegisterZone(Zone) ? System : nullptr;
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
        FFFZSGridPlacement Placement;
        Placement.OriginCell = FIntPoint(1000, 1000);
        return UFFZSGridLibrary::MakeInstallCommand(Spec, TEXT("GZ_DadaPocha"), Placement);
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
        UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*World) : nullptr;
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

        // Keep physical limits authoritative even when the contacted component is support.
        Floor->SetWorldRotation(FRotator(8, 0, 0));
        const FFFZSPlacementValidation Steep = System->ValidatePlacement(Command);
        TestFalse(Label + TEXT("steep ground is rejected"), Steep.bIsValid);
        TestTrue(Label + TEXT("steep ground reports excess slope"),
            Steep.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope));
        Floor->SetWorldRotation(FRotator(4, 0, 0));
        const FFFZSPlacementValidation Uneven = System->ValidatePlacement(Command);
        TestFalse(Label + TEXT("excess height variance is rejected"), Uneven.bIsValid);
        TestTrue(Label + TEXT("height variance remains enforced below the slope limit"),
            Uneven.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded));
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
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*World) : nullptr;
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
    UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*World) : nullptr;
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
    TestTrue(TEXT("A merged-mesh floor alone supports paving"), System->ValidatePlacement(Command).bIsValid);
    {
        TGuardValue<bool> GroundDisabled(Settings.Settings->bEnableGroundChecks, false);
        const FFFZSPlacementValidation Unverified = System->ValidatePlacement(Command);
        TestFalse(TEXT("Disabling ground sampling grants no support exemption"), Unverified.bIsValid);
        TestTrue(TEXT("Unverified intersecting geometry retains ordinary collision validation"),
            Unverified.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision));
    }

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
        UFFZSWorldSubsystem* System = World ? PrepareTerrainWorld(*World) : nullptr;
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
    }
    return true;
}

#endif
