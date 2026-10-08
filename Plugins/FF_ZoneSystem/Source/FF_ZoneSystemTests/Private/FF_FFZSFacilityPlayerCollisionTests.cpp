// FF_FFZSFacilityPlayerCollisionTests.cpp
// 역할: 시설이 지정된 플레이어를 막되 다른 Pawn과 내비게이션 계약을 유지하는지 검증한다.
// Existing mesh collision must block an opted-in player capsule without changing other Pawns or navigation.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "FF_FFZSCollision.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    class FScopedPlayerCollisionTestSettings
    {
    public:
        FScopedPlayerCollisionTestSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bNavigation(Settings->bEnableNavigationProjectionChecks)
            , bProductionPolicies(Settings->bRequireProductionPolicyDomains)
            , MaxEnumeratedCells(Settings->MaxEnumeratedCellsPerFacility)
        {
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bEnableNavigationProjectionChecks = false;
            Settings->bRequireProductionPolicyDomains = false;
            Settings->MaxEnumeratedCellsPerFacility = 1'000'000;
        }

        ~FScopedPlayerCollisionTestSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bEnableNavigationProjectionChecks = bNavigation;
            Settings->bRequireProductionPolicyDomains = bProductionPolicies;
            Settings->MaxEnumeratedCellsPerFacility = MaxEnumeratedCells;
        }

    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround;
        bool bCollision;
        bool bNavigation;
        bool bProductionPolicies;
        int32 MaxEnumeratedCells;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityPlayerCollisionLifecycleTest,
    "FF_ZoneSystem.Integration.Collision.ExistingMeshPlayerOnlyLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityPlayerCollisionLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedPlayerCollisionTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Test world exists"), World)
        || !TestNotNull(TEXT("Existing engine mesh is available"), Cube)
        || !TestNotNull(TEXT("World subsystem exists"), Subsystem)) return false;

    TestEqual(TEXT("Dedicated object channel uses the configured name"),
        UCollisionProfile::Get()->ReturnChannelNameFromContainerIndex(
            static_cast<int32>(FFZSCollision::FacilityPlayerObjectChannel)), FName(TEXT("FacilityPlayerCollision")));

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>(
        AFFZSGameplayZone::StaticClass(), FTransform::Identity, SpawnParameters);
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    if (!TestNotNull(TEXT("Test zone exists"), Zone)) return false;
    Zone->ApplyAcceptedZonePreset(ZoneId);
    if (!TestTrue(TEXT("Test zone registers"), Subsystem->RegisterZone(Zone))) return false;

    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec(EFFZSTestFacilityVariant::FoodTruck);
    Spec.VisualMesh = Cube;
    Spec.VisualSizingMode = EFFZSVisualSizingMode::SourceMeshSize;
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement);
    if (!TestTrue(TEXT("Facility installation succeeds"), Subsystem->ExecuteCommand(Install).bSucceeded)) return false;

    AFFZSFacilityActor* Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed facility exists"), Facility)) return false;
    UStaticMeshComponent* Mesh = Facility->VisualMesh;
    TestTrue(TEXT("Collision uses the supplied mesh asset directly"), Mesh->GetStaticMesh() == Cube);
    TestEqual(TEXT("Installed mesh participates in queries only"), Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("Installed mesh blocks Pawn queries"), Mesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
    TestFalse(TEXT("Mesh does not simulate rigid-body physics"), Mesh->IsSimulatingPhysics());
    TestFalse(TEXT("Mesh does not generate overlap events"), Mesh->GetGenerateOverlapEvents());
    TestFalse(TEXT("Mesh leaves navigation to the existing modifier"), Mesh->CanEverAffectNavigation());
    TestEqual(TEXT("Mesh ignores ground traces"), Mesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
    TestEqual(TEXT("Mesh ignores camera probes"), Mesh->GetCollisionResponseToChannel(ECC_Camera), ECR_Ignore);
    TestEqual(TEXT("Mesh ignores facility selection traces"),
        Mesh->GetCollisionResponseToChannel(FFZSCollision::FacilitySelectionTraceChannel), ECR_Ignore);
    TestEqual(TEXT("Selection proxy still handles selection"),
        Facility->SelectionProxy->GetCollisionResponseToChannel(FFZSCollision::FacilitySelectionTraceChannel), ECR_Block);
    TestEqual(TEXT("Selection padding never blocks the player"),
        Facility->SelectionProxy->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);

    AActor* Probe = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Capsule probe actor exists"), Probe)) return false;
    UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Probe);
    Probe->SetRootComponent(Capsule);
    Probe->AddInstanceComponent(Capsule);
    Capsule->InitCapsuleSize(20.f, 40.f);
    Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
    Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Capsule->SetGenerateOverlapEvents(false);
    Capsule->SetCanEverAffectNavigation(false);
    Capsule->RegisterComponent();

    TestEqual(TEXT("An ordinary Pawn ignores the new facility object channel by default"),
        Capsule->GetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel), ECR_Ignore);
    const auto SweepAcrossMesh = [&]()
    {
        const FVector Center = Mesh->Bounds.Origin;
        const float Distance = Mesh->Bounds.BoxExtent.X + 150.f;
        Probe->SetActorLocation(Center - FVector(Distance, 0.f, 0.f), false);
        FHitResult Hit;
        Capsule->MoveComponent(FVector(Distance * 2.f, 0.f, 0.f), FQuat::Identity, true, &Hit);
        return Hit;
    };
    TestFalse(TEXT("Default Pawn capsule passes through the facility unchanged"), SweepAcrossMesh().bBlockingHit);
    Capsule->SetCollisionResponseToChannel(FFZSCollision::FacilityPlayerObjectChannel, ECR_Block);
    const FHitResult PlayerHit = SweepAcrossMesh();
    TestTrue(TEXT("Opted-in player capsule is stopped by existing mesh collision"), PlayerHit.bBlockingHit);
    TestTrue(TEXT("The blocking hit comes from the visual mesh, not a new box"), PlayerHit.GetComponent() == Mesh);

    AFFZSPlacementPreviewActor* Preview = World->SpawnActor<AFFZSPlacementPreviewActor>();
    if (!TestNotNull(TEXT("Preview actor exists"), Preview)) return false;
    Preview->PreviewMesh->SetStaticMesh(Cube);
    TestEqual(TEXT("The same mesh remains nonblocking in a placement preview"),
        Preview->PreviewMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    Preview->Destroy();

    Subsystem->UnregisterFacility(Facility);
    TestEqual(TEXT("Unregister disables player mesh collision"), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestFalse(TEXT("Player sweep passes after unregister"), SweepAcrossMesh().bBlockingHit);
    if (!TestTrue(TEXT("Facility can register again"), Subsystem->RegisterFacility(Facility))) return false;
    TestTrue(TEXT("Player sweep blocks again after registration"), SweepAcrossMesh().bBlockingHit);
    Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
    if (!TestTrue(TEXT("Repeated registration remains successful"), Subsystem->RegisterFacility(Facility))) return false;
    TestTrue(TEXT("Repeated registration repairs an overridden player collision response"), SweepAcrossMesh().bBlockingHit);

    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = ZoneId;
    Move.Placement.OriginCell = FIntPoint(3000, 1000);
    if (!TestTrue(TEXT("Facility movement succeeds"), Subsystem->ExecuteCommand(Move).bSucceeded)) return false;
    TestTrue(TEXT("Player collision follows the moved mesh"), SweepAcrossMesh().bBlockingHit);

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    AFFZSFacilityActor* PreviousFacility = Facility;
    UStaticMeshComponent* PreviousMesh = Mesh;
    TArray<FText> RestoreErrors;
    if (!TestTrue(TEXT("Snapshot restoration succeeds"), Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors)))
    {
        for (const FText& Error : RestoreErrors) AddError(Error.ToString());
        return false;
    }
    Facility = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Snapshot publishes a restored facility"), Facility)) return false;
    Mesh = Facility->VisualMesh;
    TestTrue(TEXT("Snapshot replaces the old facility actor"), Facility != PreviousFacility);
    TestEqual(TEXT("Retired snapshot mesh has no residual player collision"),
        PreviousMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    TestEqual(TEXT("Restored mesh enables player collision during snapshot publication"),
        Mesh->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    const FHitResult RestoredHit = SweepAcrossMesh();
    TestTrue(TEXT("Player capsule is blocked after snapshot restoration"), RestoredHit.bBlockingHit);
    TestTrue(TEXT("Restored collision comes from the newly published mesh"), RestoredHit.GetComponent() == Mesh);

    FFFZSCommand Remove;
    Remove.CommandId = FGuid::NewGuid();
    Remove.TransactionId = FGuid::NewGuid();
    Remove.Type = EFFZSCommandType::Remove;
    Remove.FacilityInstanceId = Install.FacilityInstanceId;
    if (!TestTrue(TEXT("Facility removal succeeds"), Subsystem->ExecuteCommand(Remove).bSucceeded)) return false;
    TestFalse(TEXT("Removed facility leaves no blocking collision"), SweepAcrossMesh().bBlockingHit);
    Probe->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSTilePlanarSupportLifecycleTest,
    "FF_ZoneSystem.Integration.Collision.TilePlanarSupportLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSTilePlanarSupportLifecycleTest::RunTest(const FString& Parameters)
{
    FScopedPlayerCollisionTestSettings ScopedSettings;
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UFFZSWorldSubsystem* Subsystem = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Test world exists"), World)
        || !TestNotNull(TEXT("World subsystem exists"), Subsystem)) return false;

    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Test zone exists"), Zone)) return false;
    const FName ZoneId(TEXT("GZ_DadaPocha"));
    Zone->ApplyAcceptedZonePreset(ZoneId);
    Zone->bAllowAllFacilityCategories = true;
    if (!TestTrue(TEXT("Test zone registers"), Subsystem->RegisterZone(Zone))) return false;

    FFFZSFacilitySpec Spec;
    Spec.FacilityDefinitionId = TEXT("Tile_PlanarSupportRegression");
    Spec.DisplayName = FText::FromString(TEXT("Planar tile"));
    Spec.Category = EFFZSFacilityCategory::Tile;
    Spec.FootprintSizeCells = FIntPoint(400, 200);
    Spec.ClearanceCells = 0;
    Spec.PlacementObstacleHeightCm = 12.0f;
    Spec.QueueRules.MaxWaitingAgents = 0;
    Spec.QueueRules.ServiceCapacity = 0;
    FFFZSGridPlacement Placement;
    Placement.OriginCell = FIntPoint(1000, 1000);
    const FFFZSCommand Install = UFFZSGridLibrary::MakeInstallCommand(Spec, ZoneId, Placement);
    if (!TestTrue(TEXT("A tile without a visual mesh installs"), Subsystem->ExecuteCommand(Install).bSucceeded)) return false;

    AFFZSFacilityActor* Tile = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Installed tile exists"), Tile)) return false;
    UBoxComponent* Surface = Tile->TileSupportSurface;
    if (!TestNotNull(TEXT("Tile owns a dedicated support surface"), Surface)) return false;
    TestNull(TEXT("Support does not require a loaded visual mesh"), Tile->VisualMesh->GetStaticMesh());
    TestEqual(TEXT("Published support participates in queries"), Surface->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
    TestEqual(TEXT("Support ignores selection traces"),
        Surface->GetCollisionResponseToChannel(FFZSCollision::FacilitySelectionTraceChannel), ECR_Ignore);
    TestEqual(TEXT("Support uses the player-only object channel"),
        Surface->GetCollisionObjectType(), FFZSCollision::FacilityPlayerObjectChannel);
    TestEqual(TEXT("Support blocks player capsule queries"), Surface->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
    TestFalse(TEXT("Support does not affect navigation"), Surface->CanEverAffectNavigation());
    TestFalse(TEXT("Support creates no overlap events"), Surface->GetGenerateOverlapEvents());

    const auto TraceSupport = [&](const FVector2D LocalPoint)
    {
        const FVector Center = Tile->GetActorTransform().TransformPosition(FVector(LocalPoint, 0.0));
        FHitResult Hit;
        World->LineTraceSingleByChannel(Hit, Center + FVector(0.0, 0.0, 1000.0),
            Center - FVector(0.0, 0.0, 1000.0), ECC_Visibility);
        return Hit;
    };
    const auto CheckPlanarTop = [&]()
    {
        for (const FVector2D Point : { FVector2D::ZeroVector, FVector2D(-195.0, -95.0), FVector2D(195.0, 95.0) })
        {
            const FHitResult Hit = TraceSupport(Point);
            TestTrue(TEXT("Ground query reaches the dedicated tile support"), Hit.GetComponent() == Surface);
            TestTrue(TEXT("All points share the authoritative flat tile top"),
                FMath::IsNearlyEqual(Hit.ImpactPoint.Z, Tile->GetActorLocation().Z + Spec.PlacementObstacleHeightCm, 0.01));
        }
        TestFalse(TEXT("Support ends at the exact footprint edge"), TraceSupport(FVector2D(205.0, 0.0)).bBlockingHit);
    };
    CheckPlanarTop();

    // A visual can be larger and offset while its semantic support remains exactly the saved tile.
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Engine cube exists"), Cube)) return false;
    Tile->VisualMesh->SetStaticMesh(Cube);
    Tile->VisualMesh->SetRelativeLocation(FVector(0.0, 0.0, 150.0));
    Tile->VisualMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Tile->VisualMesh->SetCollisionResponseToAllChannels(ECR_Block);
    Surface->SetBoxExtent(FVector(20.0));
    Surface->SetRelativeScale3D(FVector(2.0));
    Surface->SetRelativeRotation(FRotator(20.0, 0.0, 0.0));
    if (!TestTrue(TEXT("Repeated registration repairs authored component overrides"), Subsystem->RegisterFacility(Tile))) return false;
    TestEqual(TEXT("Tile visual cannot override planar ground"),
        Tile->VisualMesh->GetCollisionResponseToChannel(ECC_Visibility), ECR_Ignore);
    TestEqual(TEXT("Tile visual cannot override planar player support"),
        Tile->VisualMesh->GetCollisionResponseToChannel(ECC_Pawn), ECR_Ignore);
    CheckPlanarTop();

    Subsystem->UnregisterFacility(Tile);
    TestFalse(TEXT("Unpublished tile has no residual support"), TraceSupport(FVector2D::ZeroVector).bBlockingHit);
    if (!TestTrue(TEXT("Tile can register again"), Subsystem->RegisterFacility(Tile))) return false;
    CheckPlanarTop();

    FFFZSCommand Move;
    Move.CommandId = FGuid::NewGuid();
    Move.TransactionId = FGuid::NewGuid();
    Move.Type = EFFZSCommandType::Move;
    Move.FacilityInstanceId = Install.FacilityInstanceId;
    Move.ZoneId = ZoneId;
    Move.Placement.OriginCell = FIntPoint(3000, 1000);
    Move.Placement.YawCentidegrees = 3700;
    if (!TestTrue(TEXT("Tile movement succeeds"), Subsystem->ExecuteCommand(Move).bSucceeded)) return false;
    CheckPlanarTop();

    const FFFZSWorldSnapshot Snapshot = Subsystem->CaptureWorldSnapshot();
    UBoxComponent* RetiredSurface = Surface;
    TArray<FText> RestoreErrors;
    if (!TestTrue(TEXT("Tile snapshot restores"), Subsystem->RestoreWorldSnapshot(Snapshot, RestoreErrors))) return false;
    Tile = Subsystem->FindFacility(Install.FacilityInstanceId);
    if (!TestNotNull(TEXT("Restored tile exists"), Tile)) return false;
    Surface = Tile->TileSupportSurface;
    TestEqual(TEXT("Retired snapshot surface has no collision"), RetiredSurface->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    CheckPlanarTop();

    FFFZSCommand Remove;
    Remove.CommandId = FGuid::NewGuid();
    Remove.TransactionId = FGuid::NewGuid();
    Remove.Type = EFFZSCommandType::Remove;
    Remove.FacilityInstanceId = Install.FacilityInstanceId;
    if (!TestTrue(TEXT("Tile removal succeeds"), Subsystem->ExecuteCommand(Remove).bSucceeded)) return false;
    TestFalse(TEXT("Removed tile leaves no ground support"), TraceSupport(FVector2D::ZeroVector).bBlockingHit);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
