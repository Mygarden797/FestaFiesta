#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AssetCompilingManager.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CrowdSystem/FF_BaseCrowd.h"
#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FF_FFZSFacilityActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Serialization/JsonSerializer.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
const FName RefinementTag(TEXT("FF_CrowdGroundRefinement"));

AActor* AddGroundBox(UWorld& World, const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
{
    AActor* Actor = World.SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
    Actor->SetRootComponent(Box);
    Actor->AddInstanceComponent(Box);
    Box->SetBoxExtent(Extent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Box->SetCanEverAffectNavigation(false);
    Box->RegisterComponent();
    Actor->SetActorLocationAndRotation(Center, Rotation);
    return Actor;
}

AStaticMeshActor* AddSurface(UWorld& World, UStaticMesh& Mesh, const FTransform& Transform, bool bTagged)
{
    AStaticMeshActor* Actor = World.SpawnActor<AStaticMeshActor>();
    UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(&Mesh);
    Component->SetCollisionProfileName(TEXT("BlockAll"));
    Actor->SetActorTransform(Transform);
    Component->SetMobility(EComponentMobility::Static);
    Component->RecreatePhysicsState();
    if (bTagged) Actor->Tags.Add(RefinementTag);
    return Actor;
}

void Bake(UFF_CrowdSimulationSubsystem& Crowd)
{
    Crowd.ConfigureSimAreaWithBounds(FVector::ZeroVector,
        FBox::BuildAABB(FVector::ZeroVector, FVector(2500, 2500, 2000)));
}

bool PhysicalFloor(UWorld& World, const FVector& Point, FHitResult& Hit)
{
    return World.LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, 2000),
        FVector(Point.X, Point.Y, -2000), ECC_WorldStatic,
        FCollisionQueryParams(SCENE_QUERY_STAT(CrowdRefinementTest), false));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGroundRefinementSurfaceTest,
    "FestaFiesta.Crowd.Ground.RefinementMatchesPhysicalOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGroundRefinementSurfaceTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Fixture world"), World) || !TestNotNull(TEXT("Real cube mesh"), Cube)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    AddGroundBox(*World, FVector(0, 0, -10), FVector(5000, 5000, 10));
    const FVector Center(137, 173, 70);
    const FRotator Rotation(-15, 27, 0);
    const FTransform RampTransform(Rotation, Center, FVector(5.2, 4, .3));
    AStaticMeshActor* Ramp = AddSurface(*World, *Cube, RampTransform, true);
    // An existing landing sits above part of the new slab. Using only the slab's
    // analytic plane would put NPC feet inside this landing.
    const FVector LandingXY = Center + FRotator(0, 27, 0).RotateVector(FVector(-240, 0, 0));
    AActor* Landing = AddGroundBox(*World, FVector(LandingXY.X, LandingXY.Y, 140),
        FVector(110, 220, 10), FRotator(0, 27, 0));

    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->bHeightFieldRequireNavMesh = false; // Isolate height sampling, not navigation, in this physics fixture.
    Crowd->HeightFieldSpacingUU = 500.f;
    Bake(*Crowd);
    TestEqual(TEXT("Only the tagged walkway is refined"), Crowd->GetGroundRefinementCount(), 1);
    TestEqual(TEXT("Global grid remains coarse"), Crowd->GetHeightField().GetNumX(), 11);
    FFF_CrowdHeightField::FCellProbe CoreProbe, ApronProbe;
    TestTrue(TEXT("The walkway core can be probed"), Crowd->GetHeightFieldAt(FVector2D(Center.X, Center.Y)).ProbeCell(FVector2D(Center.X, Center.Y), CoreProbe));
    TestEqual(TEXT("Immediate approaches use 5 cm samples"), CoreProbe.CornerXYCm[1].X - CoreProbe.CornerXYCm[0].X, 5.0);
    const FVector2D ApronPoint(Center.X + 700.0, Center.Y);
    TestTrue(TEXT("The surrounding apron can be probed"), Crowd->GetHeightFieldAt(ApronPoint).ProbeCell(ApronPoint, ApronProbe));
    TestEqual(TEXT("The broad apron retains 25 cm samples"), ApronProbe.CornerXYCm[1].X - ApronProbe.CornerXYCm[0].X, 25.0);
    bool bSawCoarseError = false;
    bool bCheckedHigherLanding = false;
    for (const double Along : {-42.0, -12.0, 0.0, 15.0, 32.0})
    {
        for (const double Across : {-20.0, 0.0, 20.0})
        {
            const FVector Point = RampTransform.TransformPosition(FVector(Along, Across, 50));
            FHitResult Hit;
            if (!TestTrue(TEXT("Real physics finds the composite floor"), PhysicalFloor(*World, Point, Hit))) continue;
            double FineZ = 0, CoarseZ = 0, DifferentInputZ = 0;
            TestTrue(TEXT("Refined floor exists"), Crowd->TryGetGroundZ(Point, FineZ));
            Crowd->GetHeightField().TryGetHeightCm(FVector2D(Point.X, Point.Y), CoarseZ);
            Crowd->TryGetGroundZ(FVector(Point.X, Point.Y, -10000), DifferentInputZ);
            TestTrue(TEXT("Cached height agrees with independently traced visible floor"), FMath::Abs(FineZ - Hit.ImpactPoint.Z) <= 5.0);
            TestEqual(TEXT("Vertical input cannot change an XY ground query"), DifferentInputZ, FineZ);
            TestTrue(TEXT("Refined walkable floor remains traversable"), Crowd->IsGroundWalkable(Point));
            bSawCoarseError |= FMath::Abs(CoarseZ - Hit.ImpactPoint.Z) > 10.0;
            if (Hit.GetActor() == Landing)
            {
                bCheckedHigherLanding = true;
                TestTrue(TEXT("Overlapping landing wins over the lower ramp plane"), FineZ > Point.Z + 5.0);
            }
        }
    }
    TestTrue(TEXT("Fixture reproduces a material error in the original coarse grid"), bSawCoarseError);
    TestTrue(TEXT("Fixture actually checks the higher overlapping landing"), bCheckedHigherLanding);
    const FVector2D Outside(2200, 2200);
    TestTrue(TEXT("Outside the apron uses the unchanged global field"),
        &Crowd->GetHeightFieldAt(Outside) == &Crowd->GetHeightField());

    // A query uses cached physics, not a new per-NPC line trace.
    const FVector Probe = RampTransform.TransformPosition(FVector(15, 0, 50));
    double Before = 0, After = 0;
    Crowd->TryGetGroundZ(Probe, Before);
    Ramp->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Crowd->TryGetGroundZ(Probe, After);
    TestEqual(TEXT("Physics changes do not trigger per-query traces"), After, Before);
    Ramp->Tags.Reset();
    Bake(*Crowd);
    TestEqual(TEXT("Rebuild removes stale local patches when the tag is removed"), Crowd->GetGroundRefinementCount(), 0);
    Crowd->TryGetGroundZ(Probe, After);
    double RebuiltCoarse = 0;
    Crowd->GetHeightField().TryGetHeightCm(FVector2D(Probe.X, Probe.Y), RebuiltCoarse);
    TestEqual(TEXT("After removing the tag queries return to the rebuilt global field"), After, RebuiltCoarse);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGroundRefinementScopeTest,
    "FestaFiesta.Crowd.Ground.RefinementScopeAndWalkability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGroundRefinementScopeTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (!TestNotNull(TEXT("Fixture world"), World) || !TestNotNull(TEXT("Cube"), Cube) || !TestNotNull(TEXT("Sphere"), Sphere)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    AddGroundBox(*World, FVector(0, 0, -10), FVector(5000, 5000, 10));
    AStaticMeshActor* Untagged = AddSurface(*World, *Cube,
        FTransform(FRotator::ZeroRotator, FVector(-1000, 0, 90), FVector(2, 2, .2)), false);
    AddSurface(*World, *Sphere, FTransform(FRotator::ZeroRotator, FVector(1000, 0, 100)), true);
    AddSurface(*World, *Cube, FTransform(FRotator(70, 0, 0), FVector(0, 1000, 100)), true);
    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->bHeightFieldRequireNavMesh = false;
    AddExpectedError(TEXT("Crowd ground refinement rejected unsupported tagged surface"), EAutomationExpectedErrorFlags::Contains, 2);
    Bake(*Crowd);
    TestEqual(TEXT("Untagged or unsupported meshes cannot create a patch"), Crowd->GetGroundRefinementCount(), 0);

    // Remove invalid authoring from the fixture, then exercise local ceiling
    // decisions where the coarse cell still contains lower terrain corners.
    for (TActorIterator<AStaticMeshActor> It(World); It; ++It) It->Tags.Reset();
    Untagged->Tags.Add(RefinementTag);
    Crowd->MaxWalkableHeightAboveOriginUU = 50.f;
    Bake(*Crowd);
    TestEqual(TEXT("A valid explicitly tagged cube creates one patch"), Crowd->GetGroundRefinementCount(), 1);
    const FVector HighPoint(-1000, 0, 100);
    TestTrue(TEXT("Fixture exposes the coarse cell's permissive ceiling decision"),
        Crowd->GetHeightField().IsWalkable(FVector2D(HighPoint.X, HighPoint.Y)));
    TestFalse(TEXT("Movement checks the actual refined high surface against the ceiling"), Crowd->IsGroundWalkable(HighPoint));
    TestTrue(TEXT("Flat terrain outside remains walkable"), Crowd->IsGroundWalkable(FVector(2000, 2000, 0)));
    return true;
}

// A finite grid cannot reconstruct an arbitrary vertical edge exactly. Exercise
// several edge phases without tuning lattice offsets: the fine core confines
// errors to one 5 cm cell instead of spreading a curb across a 25 cm apron cell.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGroundRefinementCurbTest,
    "FestaFiesta.Crowd.Ground.RefinementCoreBoundsCurbSpatialError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGroundRefinementCurbTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Fixture world"), World) || !TestNotNull(TEXT("Real cube mesh"), Cube)) return false;
    FAssetCompilingManager::Get().FinishAllCompilation();
    AddGroundBox(*World, FVector(0, 0, 90), FVector(5000, 5000, 10));
    AActor* Curb = AddGroundBox(*World, FVector(-500, 0, 105), FVector(500, 200, 5));
    AddSurface(*World, *Cube, FTransform(FRotator::ZeroRotator, FVector(0, 0, 80), FVector(2, 2, .1)), true);
    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->bHeightFieldRequireNavMesh = false;
    bool bExercisedFineDiscontinuity = false;
    for (const double Phase : {0.75, 2.25, 4.25})
    {
        Curb->SetActorLocation(FVector(-500 + Phase, 0, 105));
        Bake(*Crowd);
        FFF_CrowdHeightField Reference25;
        Reference25.Build(FVector2D::ZeroVector, 250.0, 25.0,
            [World](const FVector2D& XY, double& Z)
            {
                FHitResult Hit;
                if (!PhysicalFloor(*World, FVector(XY.X, XY.Y, 0), Hit)) return false;
                Z = Hit.ImpactPoint.Z;
                return true;
            });
        int32 FineErrors = 0, ApronErrors = 0;
        for (const double Y : {-17.0, 17.0})
        {
            for (int32 X = -40; X <= 40; ++X)
            {
                const FVector Point(X, Y, 0);
                FHitResult Hit;
                double Fine = 0, Apron = 0;
                if (!TestTrue(TEXT("Independent physics sees the real curb"), PhysicalFloor(*World, Point, Hit))) continue;
                TestTrue(TEXT("Core has a cached floor"), Crowd->TryGetGroundZ(Point, Fine));
                TestTrue(TEXT("The reference 25 cm cell has a floor"), Reference25.TryGetHeightCm(FVector2D(X, Y), Apron));
                if (FMath::Abs(Fine - Hit.ImpactPoint.Z) > 5.0)
                {
                    ++FineErrors;
                    bExercisedFineDiscontinuity = true;
                    TestTrue(TEXT("Fine-grid curb error is confined to a 5 cm physical edge band"), FMath::Abs(X - Phase) <= 5.0);
                }
                ApronErrors += FMath::Abs(Apron - Hit.ImpactPoint.Z) > 5.0;
            }
        }
        TestTrue(TEXT("Each independent curb phase reduces the spatial extent of grounding error"), FineErrors < ApronErrors);
    }
    TestTrue(TEXT("The fixture exposes, rather than hides, finite-grid edge uncertainty"), bExercisedFineDiscontinuity);
    TestTrue(TEXT("Small tagged surface keeps both caches bounded"), Crowd->GetGroundRefinementSampleCount() < 20000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGateGroundRefinementTest,
    "FestaFiesta.Crowd.Ground.GateRefinementSlopeAndLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGateGroundRefinementTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Independent gate-ground fixture world"), World)) return false;
    AddGroundBox(*World, FVector(0, 0, -1010), FVector(5000, 5000, 10));
    AActor* Slope = AddGroundBox(*World, FVector::ZeroVector, FVector(2500, 2500, 20), FRotator(8, 0, 0));
    Slope->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->bHeightFieldRequireNavMesh = false;
    Crowd->HeightFieldSpacingUU = 500.f;
    Crowd->HeightFieldCliffStepUU = 50.f;
    Bake(*Crowd);
    TestEqual(TEXT("No entrance patch is baked before gates are gathered"), Crowd->GetGateGroundRefinementCount(), 0);
    const int32 GlobalSamples = Crowd->GetHeightField().GetStats().SampleCount;
    FFF_CrowdGate Gate;
    Gate.GateId = TEXT("GroundFixture");
    Gate.GateLocation = FVector(137, 173, 0);
    Gate.GateRadiusUU = 250.f;
    Gate.GateWeight = 1.f;
    Gate.bAllowSpawn = true;
    Gate.bAllowExit = true;
    // Navigation is covered by the production test. This test deliberately has
    // no NavData so only static-physics caching and lifecycle are exercised.
    AddExpectedMessage(TEXT("NavMesh 검증 생략"), ELogVerbosity::Warning,
        EAutomationExpectedMessageFlags::Contains, 3);
    Crowd->GatherCrowdGates({Gate});
    TestEqual(TEXT("A validated entrance receives one ground patch"), Crowd->GetGateGroundRefinementCount(), 1);
    TestEqual(TEXT("Gate patches do not masquerade as tagged walkway patches"), Crowd->GetGroundRefinementCount(), 0);
    TestEqual(TEXT("Gathering gates preserves the coarse global sample count"), Crowd->GetHeightField().GetStats().SampleCount, GlobalSamples);
    TestTrue(TEXT("The local entrance has a bounded sample payload"),
        Crowd->GetGateGroundRefinementSampleCount() > 0 && Crowd->GetGateGroundRefinementSampleCount() < 100000);
    bool bReproducedCoarseSlopeError = false;
    for (const double X : {-313.0, -123.0, 137.0, 327.0, 567.0})
    {
        for (const double Y : {23.0, 173.0, 323.0})
        {
            const FVector Point(X, Y, 0);
            FHitResult Hit;
            if (!TestTrue(TEXT("Real tilted static ground exists"), PhysicalFloor(*World, Point, Hit))) continue;
            double Fine = 0, Coarse = 0;
            TestTrue(TEXT("The entrance cache provides a floor"), Crowd->TryGetGroundZ(Point, Fine));
            TestTrue(TEXT("The original coarse field provides a comparison"), Crowd->GetHeightField().TryGetHeightCm(FVector2D(X, Y), Coarse));
            bReproducedCoarseSlopeError |= FMath::Abs(Coarse - Hit.ImpactPoint.Z) > 10.0;
            TestTrue(TEXT("A gentle slope no longer snaps feet to a distant coarse corner"), FMath::Abs(Fine - Hit.ImpactPoint.Z) <= 5.0);
        }
    }
    TestTrue(TEXT("The fixture exposes the coarse grid's false cliff classification"), bReproducedCoarseSlopeError);
    const FVector2D Core(137, 173), Apron(1037, 173), Outside(2237, 173);
    FFF_CrowdHeightField::FCellProbe Probe;
    TestTrue(TEXT("Entrance core is queryable"), Crowd->GetHeightFieldAt(Core).ProbeCell(Core, Probe));
    TestEqual(TEXT("Entrance core uses a fixed world-aligned 5 cm lattice"), Probe.CornerXYCm[1].X - Probe.CornerXYCm[0].X, 5.0);
    TestTrue(TEXT("Entrance apron is queryable"), Crowd->GetHeightFieldAt(Apron).ProbeCell(Apron, Probe));
    TestEqual(TEXT("The entrance apron retains 25 cm samples"), Probe.CornerXYCm[1].X - Probe.CornerXYCm[0].X, 25.0);
    TestTrue(TEXT("Beyond the entrance apron uses the unchanged global field"), &Crowd->GetHeightFieldAt(Outside) == &Crowd->GetHeightField());
    double Before = 0, Cached = 0, Rebuilt = 0;
    Crowd->TryGetGroundZ(FVector(Core, 0), Before);
    Slope->SetActorLocation(FVector(0, 0, 100));
    Crowd->TryGetGroundZ(FVector(Core, 0), Cached);
    TestEqual(TEXT("A ground query performs no new physics trace"), Cached, Before);
    Bake(*Crowd);
    Crowd->TryGetGroundZ(FVector(Core, 0), Rebuilt);
    TestEqual(TEXT("Explicit height-field rebuild refreshes existing entrance patches"), Rebuilt - Before, 100.0, .01);
    Crowd->GatherCrowdGates({});
    TestEqual(TEXT("Removing gates releases their cached patches"), Crowd->GetGateGroundRefinementCount(), 0);
    TestEqual(TEXT("Removing gates releases the entire sample payload"), Crowd->GetGateGroundRefinementSampleCount(), 0);
    TestTrue(TEXT("Removed gate location returns to coarse ground"), &Crowd->GetHeightFieldAt(Core) == &Crowd->GetHeightField());
    Gate.GateRadiusUU = 10000.f;
    AddExpectedMessage(TEXT("Gate ground refinement exceeds bounded entrance budget"), ELogVerbosity::Warning);
    Crowd->GatherCrowdGates({Gate});
    TestEqual(TEXT("An oversized entrance cannot allocate an unbounded fine grid"), Crowd->GetGateGroundRefinementCount(), 0);
    TestEqual(TEXT("Budget rejection leaves the global field intact"), Crowd->GetHeightField().GetStats().SampleCount, GlobalSamples);
    Crowd->GatherCrowdGates({});
    // Exercise the actual emergency fallback path without creating visitors.
    AddExpectedMessage(TEXT("방문객 분포가 비었다"), ELogVerbosity::Error);
    Crowd->BeginArrivalSchedule(0, FVector(Core, 0), 250.f);
    TestTrue(TEXT("An empty authored list activates the real emergency fallback"), Crowd->IsUsingFallbackGate());
    TestEqual(TEXT("The emergency fallback remains available for spawning"), Crowd->GetSpawnGateCount(), 1);
    Bake(*Crowd);
    TestEqual(TEXT("A full rebuild does not promote the synthetic fallback to authored precision"), Crowd->GetGateGroundRefinementCount(), 0);
    TestTrue(TEXT("Emergency fallback continues to use global ground"), &Crowd->GetHeightFieldAt(Core) == &Crowd->GetHeightField());
    Gate.GateRadiusUU = 250.f;
    Crowd->GatherCrowdGates({Gate});
    TestEqual(TEXT("Regathering a real authored gate restores its precision patch"), Crowd->GetGateGroundRefinementCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGateGroundEdgeTest,
    "FestaFiesta.Crowd.Ground.GateRefinementRotatedCurbEdges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGateGroundEdgeTest::RunTest(const FString& Parameters)
{
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!TestNotNull(TEXT("Independent rotated-curb fixture world"), World)) return false;
    AddGroundBox(*World, FVector(0, 0, 90), FVector(5000, 5000, 10));
    const FRotator Rotation(0, 31, 0);
    const FVector Along = Rotation.RotateVector(FVector(1, 0, 0));
    const FVector Across = Rotation.RotateVector(FVector(0, 1, 0));
    AActor* Curb = AddGroundBox(*World, Along * -500.0 + FVector(0, 0, 106), FVector(500, 200, 6), Rotation);
    Curb->GetRootComponent()->SetMobility(EComponentMobility::Movable);
    UFF_CrowdSimulationSubsystem* Crowd = NewObject<UFF_CrowdSimulationSubsystem>(World);
    Crowd->bHeightFieldRequireNavMesh = false;
    Bake(*Crowd);
    FFF_CrowdGate Gate;
    Gate.GateId = TEXT("RotatedCurb");
    Gate.GateLocation = FVector::ZeroVector;
    Gate.GateRadiusUU = 250.f;
    Gate.GateWeight = 1.f;
    Gate.bAllowSpawn = true;
    Gate.bAllowExit = true;
    AddExpectedMessage(TEXT("NavMesh 검증 생략"), ELogVerbosity::Warning);
    Crowd->GatherCrowdGates({Gate});
    bool bSawResolvedBoundary = false;
    for (const double Phase : {.37, 1.61, 3.23})
    {
        Curb->SetActorLocation(Along * (-500.0 + Phase) + FVector(0, 0, 106));
        Bake(*Crowd);
        TestTrue(TEXT("Physical height discontinuities create bounded edge subcells"),
            Crowd->GetGateGroundEdgeRefinementCount() > 0 && Crowd->GetGateGroundEdgeRefinementCount() <= 4096);
        TestTrue(TEXT("Edge subcells are included in the total sample budget"), Crowd->GetGateGroundRefinementSampleCount() <= 1600000);
        FFF_CrowdHeightField Reference5;
        Reference5.Build(FVector2D::ZeroVector, 250.0, 5.0,
            [World](const FVector2D& XY, double& Z)
            {
                FHitResult Hit;
                if (!PhysicalFloor(*World, FVector(XY, 0), Hit)) return false;
                Z = Hit.ImpactPoint.Z;
                return true;
            });
        int32 FineErrors = 0, ReferenceErrors = 0;
        for (const double Lateral : {-17.0, 17.0})
        {
            for (int32 Step = -40; Step <= 40; ++Step)
            {
                const double DistanceFromEdge = Step * .25;
                const FVector Point = Along * (Phase + DistanceFromEdge) + Across * Lateral;
                FHitResult Hit;
                double Fine = 0, Reference = 0;
                if (!TestTrue(TEXT("Independent physics finds the rotated curb or its surrounding floor"), PhysicalFloor(*World, Point, Hit))) continue;
                TestTrue(TEXT("The adaptive cache contains a real floor sample"), Crowd->TryGetGroundZ(Point, Fine));
                FFF_CrowdHeightField::FCellProbe Boundary;
                Crowd->GetHeightFieldAt(FVector2D(Point.X, Point.Y)).ProbeCell(FVector2D(Point.X, Point.Y), Boundary);
                bSawResolvedBoundary |= Boundary.bHasDiscontinuity && Boundary.bBoundaryResolved;
                TestTrue(TEXT("The original 5 cm grid contains a comparison sample"), Reference5.TryGetHeightCm(FVector2D(Point.X, Point.Y), Reference));
                if (FMath::Abs(Fine - Hit.ImpactPoint.Z) > 5.0)
                {
                    ++FineErrors;
                    // Two classes cannot form a fictitious ramp. Only the
                    // fixed 0.01 cm boundary-location tolerance remains; this
                    // does not relax the production route's 5 cm requirement.
                    TestTrue(TEXT("Remaining edge uncertainty is within the sampled boundary tolerance"), FMath::Abs(DistanceFromEdge) <= .01);
                }
                ReferenceErrors += FMath::Abs(Reference - Hit.ImpactPoint.Z) > 5.0;
            }
        }
        TestTrue(TEXT("Each independent rotated curb phase reduces the extent of false ramp heights"), FineErrors < ReferenceErrors);
    }
    TestTrue(TEXT("The regression actually queries reconstructed boundaries"), bSawResolvedBoundary);
    Curb->SetActorEnableCollision(false);
    Bake(*Crowd);
    TestEqual(TEXT("A rebuild removes obsolete edge cells when the physical step disappears"), Crowd->GetGateGroundEdgeRefinementCount(), 0);
    Crowd->GatherCrowdGates({});
    TestEqual(TEXT("Removing gates releases both their parent and edge samples"), Crowd->GetGateGroundRefinementSampleCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGroundBoundaryRepresentationTest,
    "FestaFiesta.Crowd.Ground.DiscontinuityRepresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGroundBoundaryRepresentationTest::RunTest(const FString& Parameters)
{
    FFF_CrowdHeightField Field;
    int32 SamplerCalls = 0;
    const auto Diagonal = [&SamplerCalls](const FVector2D& XY, double& Z)
    {
        ++SamplerCalls;
        Z = XY.X + XY.Y > .45 ? 112.0 : 100.0;
        return true;
    };
    Field.Build(FVector2D(.5), .5, 1.0, Diagonal);
    const int32 Attempts = Field.RefineDiscontinuities(5.0, .01, 100, Diagonal);
    TestEqual(TEXT("Every boundary sampler attempt is counted"), SamplerCalls, 4 + Attempts);
    TestEqual(TEXT("A two-class diagonal resolves one boundary"), Field.GetStats().BoundaryCellCount, 1);
    TestEqual(TEXT("A supported diagonal remains known"), Field.GetStats().UnknownBoundaryCellCount, 0);
    const int32 CallsAfterBake = SamplerCalls;
    for (int32 X = 0; X < 11; ++X)
        for (int32 Y = 0; Y < 11; ++Y)
        {
            const FVector2D XY(.03 + X * .09, .02 + Y * .09);
            if (FMath::Abs(XY.X + XY.Y - .45) / FMath::Sqrt(2.0) <= .01) continue;
            double Z = 0.0;
            TestTrue(TEXT("Resolved class has a cached height"), Field.TryGetHeightCm(XY, Z));
            TestEqual(TEXT("No intermediate ramp height crosses the physical step"), Z, XY.X + XY.Y > .45 ? 112.0 : 100.0, .001);
        }
    TestEqual(TEXT("Boundary queries never call the sampler"), SamplerCalls, CallsAfterBake);
    double Z = 0.0;
    TestTrue(TEXT("Refined fields retain clamped outside queries"), Field.TryGetHeightCm(FVector2D(-100, 1), Z));
    TestEqual(TEXT("Outside query classifies its clamped position"), Z, 112.0);
    FFF_CrowdHeightField::FCellProbe Probe;
    Field.ProbeCell(FVector2D(-100, 1), Probe);
    TestTrue(TEXT("Diagnostic boundary reports the same clamped high class"), Probe.bBoundaryResolved && Probe.bQueryOnHighSide);
    Field.SetWalkableCeilingCm(105.0);
    TestFalse(TEXT("A high class above the ceiling cannot borrow low-class support"), Field.TryGetHeightCm(FVector2D(.8), Z));
    TestFalse(TEXT("The same excluded high class is not walkable"), Field.IsWalkable(FVector2D(.8)));
    TestTrue(TEXT("The low class retains valid support below the ceiling"), Field.TryGetHeightCm(FVector2D(.05), Z));
    Field.ClearWalkableCeiling();
    Field.SetBlockUnknownGround(true);
    Field.RefineDiscontinuities(5.0, .01, 1, Diagonal);
    TestEqual(TEXT("Exhausted refinement uses exactly its allowed attempts"), Field.GetStats().BoundaryProbeCount, 1);
    TestEqual(TEXT("Exhausted candidate remains explicitly unresolved"), Field.GetStats().UnknownBoundaryCellCount, 1);
    TestFalse(TEXT("Budget exhaustion never falls back to scalar blending"), Field.TryGetHeightCm(FVector2D(.8), Z));
    TestFalse(TEXT("Configured unknown-ground blocking applies to an unresolved boundary"), Field.IsWalkable(FVector2D(.8)));
    const auto Checkerboard = [](const FVector2D& XY, double& Height)
    {
        Height = ((XY.X < .5) == (XY.Y < .5)) ? 112.0 : 100.0;
        return true;
    };
    Field.Build(FVector2D(.5), .5, 1.0, Checkerboard);
    Field.RefineDiscontinuities(5.0, .01, 100, Checkerboard);
    TestEqual(TEXT("An ambiguous checkerboard is not guessed as one line"), Field.GetStats().UnknownBoundaryCellCount, 1);
    TestFalse(TEXT("Checkerboard produces no invented support"), Field.TryGetHeightCm(FVector2D(.2), Z));
    const auto Missing = [](const FVector2D& XY, double& Height)
    {
        Height = 100.0;
        return XY.X < .5 || XY.Y < .5;
    };
    Field.Build(FVector2D(.5), .5, 1.0, Missing);
    Field.RefineDiscontinuities(5.0, .01, 100, Missing);
    TestFalse(TEXT("Mixed missing geometry does not fill the hole from other corners"), Field.TryGetHeightCm(FVector2D(.8), Z));
    const auto ThirdFloor = [](const FVector2D& XY, double& Height)
    {
        Height = FMath::Abs(XY.X - .5) < .1 && FMath::Abs(XY.Y - .5) < .1
            ? 106.0 : (XY.X + XY.Y > .45 ? 112.0 : 100.0);
        return true;
    };
    Field.Build(FVector2D(.5), .5, 1.0, ThirdFloor);
    Field.RefineDiscontinuities(5.0, .01, 100, ThirdFloor);
    TestFalse(TEXT("Interior validation rejects an observed third floor"), Field.TryGetHeightCm(FVector2D(.5), Z));
    Field.Build(FVector2D(.5), .5, 1.0, [](const FVector2D&, double& Height) { Height = 100.0; return true; });
    TestEqual(TEXT("Rebuild clears all obsolete boundary work counters"), Field.GetStats().BoundaryProbeCount, 0);
    Field.ProbeCell(FVector2D(.5), Probe);
    TestFalse(TEXT("Rebuild clears boundary classification"), Probe.bHasDiscontinuity);
    TestTrue(TEXT("Rebuilt continuous ground is available again"), Field.TryGetHeightCm(FVector2D(.5), Z));
    Field.MarkCellUnknown(FVector2D(.5));
    Field.MarkCellUnknown(FVector2D(.5));
    TestEqual(TEXT("Repeated budget rejection stores only one compact marker"), Field.GetStats().UnknownBoundaryCellCount, 1);
    TestFalse(TEXT("A rejected parent cell cannot fabricate support without an edge grid"), Field.TryGetHeightCm(FVector2D(.5), Z));
    TestFalse(TEXT("A rejected parent cell obeys unknown-ground blocking"), Field.IsWalkable(FVector2D(.5)));
    Field.ProbeCell(FVector2D(.5), Probe);
    TestTrue(TEXT("Parent rejection is visible in diagnostic evidence"), Probe.bHasDiscontinuity && !Probe.bBoundaryResolved);
    Field.Build(FVector2D(.5), .5, 1.0, [](const FVector2D&, double& Height) { Height = 100.0; return true; });
    TestTrue(TEXT("A later rebuild clears compact rejection markers"), Field.TryGetHeightCm(FVector2D(.5), Z));
    return true;
}

namespace
{
TArray<TSharedPtr<FJsonValue>> GroundVectorJson(const FVector& Value)
{
    return {MakeShared<FJsonValueNumber>(Value.X), MakeShared<FJsonValueNumber>(Value.Y), MakeShared<FJsonValueNumber>(Value.Z)};
}

TSharedRef<FJsonObject> GroundCellJson(const FFF_CrowdHeightField& Field, const FVector2D& Point)
{
    const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
    FFF_CrowdHeightField::FCellProbe Probe;
    const bool bValid = Field.ProbeCell(Point, Probe);
    Result->SetBoolField(TEXT("valid"), bValid);
    if (bValid)
    {
        Result->SetBoolField(TEXT("has_discontinuity"), Probe.bHasDiscontinuity);
        if (Probe.bHasDiscontinuity)
        {
            Result->SetBoolField(TEXT("boundary_resolved"), Probe.bBoundaryResolved);
            Result->SetBoolField(TEXT("query_on_high_side"), Probe.bQueryOnHighSide);
            Result->SetNumberField(TEXT("high_corner_mask"), Probe.HighCornerMask);
            Result->SetNumberField(TEXT("boundary_tolerance_cm"), Probe.BoundaryToleranceCm);
            Result->SetArrayField(TEXT("boundary_start"), GroundVectorJson(FVector(Probe.BoundaryStartCm, 0)));
            Result->SetArrayField(TEXT("boundary_end"), GroundVectorJson(FVector(Probe.BoundaryEndCm, 0)));
        }
        Result->SetNumberField(TEXT("spacing_cm"), FVector2D::Distance(Probe.CornerXYCm[0], Probe.CornerXYCm[1]));
        TArray<TSharedPtr<FJsonValue>> Corners;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const TSharedRef<FJsonObject> Corner = MakeShared<FJsonObject>();
            Corner->SetBoolField(TEXT("valid"), Probe.bCornerValid[Index]);
            Corner->SetArrayField(TEXT("point"), GroundVectorJson(FVector(Probe.CornerXYCm[Index], Probe.CornerHeightCm[Index])));
            Corners.Add(MakeShared<FJsonValueObject>(Corner));
        }
        Result->SetArrayField(TEXT("corners"), Corners);
    }
    return Result;
}

class FFF_CheckProductionGroundRefinements final : public IAutomationLatentCommand
{
public:
    explicit FFF_CheckProductionGroundRefinements(FAutomationTestBase* InTest) : Test(InTest) {}

    virtual bool Update() override
    {
        if (StartedAt < 0.0) StartedAt = FPlatformTime::Seconds();
        UWorld* World = nullptr;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
            if (Context.WorldType == EWorldType::PIE && Context.World()) { World = Context.World(); break; }
        UFF_CrowdSimulationSubsystem* Crowd = World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
        UNavigationSystemV1* Navigation = World ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
        const ANavigationData* Data = Navigation ? Navigation->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        if (!World || !World->HasBegunPlay() || !Crowd || !Crowd->IsSimulationInitialized()
            || !Crowd->GetHeightField().IsValid() || !Data || UNavigationSystemV1::IsNavigationBeingBuiltOrLocked(World))
        {
            if (FPlatformTime::Seconds() - StartedAt < 60.0) return false;
            Test->AddError(TEXT("Production crowd ground field/navigation did not become ready within 60 seconds."));
            return true;
        }

        const TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
        Report->SetStringField(TEXT("world"), World->GetPathName());
        Report->SetStringField(TEXT("scope"), TEXT("Read-only audit of all four current entrance approaches and any authored local refinement surfaces against production PIE ground caches and independent WorldStatic physics; no map or settings changes."));
        Report->SetNumberField(TEXT("configured_global_spacing_cm"), Crowd->HeightFieldSpacingUU);
        Report->SetNumberField(TEXT("cliff_step_cm"), Crowd->HeightFieldCliffStepUU);
        Report->SetNumberField(TEXT("refinement_count"), Crowd->GetGroundRefinementCount());
        Report->SetNumberField(TEXT("refinement_sample_count"), Crowd->GetGroundRefinementSampleCount());
        Report->SetNumberField(TEXT("refinement_sample_payload_bytes"), Crowd->GetGroundRefinementSampleCount() * (sizeof(double) + sizeof(uint8)));
        Report->SetNumberField(TEXT("refinement_build_ms"), Crowd->GetGroundRefinementBuildSeconds() * 1000.0);
        Report->SetNumberField(TEXT("gate_refinement_count"), Crowd->GetGateGroundRefinementCount());
        Report->SetNumberField(TEXT("gate_refinement_sample_count"), Crowd->GetGateGroundRefinementSampleCount());
        Report->SetStringField(TEXT("gate_refinement_sample_count_definition"), TEXT("Grid vertices plus every boundary sampler attempt, including rejected cells."));
        Report->SetNumberField(TEXT("gate_refinement_cache_allocated_bytes"), static_cast<double>(Crowd->GetGateGroundRefinementAllocatedBytes()));
        Report->SetNumberField(TEXT("gate_refinement_edge_cell_count"), Crowd->GetGateGroundEdgeRefinementCount());
        Report->SetNumberField(TEXT("gate_refinement_build_ms"), Crowd->GetGateGroundRefinementBuildSeconds() * 1000.0);
        Test->TestEqual(TEXT("All four production entrances have bounded ground refinements"), Crowd->GetGateGroundRefinementCount(), 4);
        Report->SetNumberField(TEXT("surface_error_limit_cm"), 5.0);
        Test->TestEqual(TEXT("Production keeps its 500 cm global sampling setting"), Crowd->HeightFieldSpacingUU, 500.f);

        // Match RebuildHeightField's channel, ignored actors, trace span and
        // navigation rejection. This traces anew only in the automation audit.
        FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ProductionCrowdGroundAudit), false);
        if (const UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>())
            for (const FFFZSFacilityReadModel& Facility : Zones->BuildReadModel().Facilities)
                if (const AFFZSFacilityActor* Actor = Zones->FindFacility(Facility.FacilityInstanceId)) TraceParams.AddIgnoredActor(Actor);
        for (TActorIterator<AFF_BaseCrowd> It(World); It; ++It) TraceParams.AddIgnoredActor(*It);
        const double TopZ = FMath::Max(Crowd->SimOrigin.Z, Crowd->GetSimAreaBounds().Max.Z);
        const double StartZ = TopZ + FMath::Max(static_cast<double>(Crowd->HeightFieldTraceAboveUU), 0.0);
        const double EndZ = FMath::Min(Crowd->SimOrigin.Z, TopZ) - 50000.0;
        const auto TraceFloor = [&](const FVector& Point, bool bComplex, FHitResult& Hit, FVector& Projected) -> bool
        {
            FCollisionQueryParams LayerParams = TraceParams;
            LayerParams.bTraceComplex = bComplex;
            for (int32 Layer = 0; Layer <= 4; ++Layer)
            {
                if (!World->LineTraceSingleByChannel(Hit, FVector(Point.X, Point.Y, StartZ),
                    FVector(Point.X, Point.Y, EndZ), ECC_WorldStatic, LayerParams)) return false;
                FNavLocation OnNav;
                if (Navigation->ProjectPointToNavigation(Hit.ImpactPoint, OnNav, FVector(100, 100, 60), Data))
                {
                    Projected = OnNav.Location;
                    return true;
                }
                if (!Hit.GetComponent()) return false;
                LayerParams.AddIgnoredComponent(Hit.GetComponent());
            }
            return false;
        };

        // The October layout replaces the legacy deck/parking repair cubes.
        // Audit the actual four entrance approaches independently of refinement
        // tags so an empty tag collection can never turn this into a vacuous pass.
        TArray<TSharedPtr<FJsonValue>> GateApproaches;
        constexpr double ApproachLengthCm = 500.0;
        const TCHAR* const ExpectedGateNames[] = {TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4")};
        FNavLocation CenterNav;
        const bool bCenterProjects = Test->TestTrue(TEXT("Production simulation center projects to navigation"),
            Navigation->ProjectPointToNavigation(Crowd->SimOrigin, CenterNav, FVector(300, 300, 2000), Data));
        for (const TCHAR* GateName : ExpectedGateNames)
        {
            const FString Label = FString::Printf(TEXT("Gate %s approach"), GateName);
            const FName GateId(GateName);
            const FFF_CrowdGate* Gate = Crowd->GetValidGates().FindByPredicate(
                [GateId](const FFF_CrowdGate& Candidate) { return Candidate.GateId == GateId; });
            const TSharedRef<FJsonObject> Approach = MakeShared<FJsonObject>();
            Approach->SetStringField(TEXT("gate"), GateName);
            GateApproaches.Add(MakeShared<FJsonValueObject>(Approach));
            if (!Test->TestTrue(Label + TEXT(": the authored entrance survives runtime validation"),
                Gate && Gate->bAllowSpawn) || !bCenterProjects) continue;

            FPathFindingQuery Query(nullptr, *Data, Gate->GateLocation, CenterNav.Location);
            Query.SetAllowPartialPaths(false);
            const FPathFindingResult Forward = Navigation->FindPathSync(Query);
            FPathFindingQuery ReverseQuery(nullptr, *Data, CenterNav.Location, Gate->GateLocation);
            ReverseQuery.SetAllowPartialPaths(false);
            const FPathFindingResult Reverse = Navigation->FindPathSync(ReverseQuery);
            const auto IsComplete = [](const FPathFindingResult& Result)
            {
                return Result.IsSuccessful() && !Result.IsPartial() && Result.Path.IsValid()
                    && Result.Path->GetPathPoints().Num() >= 2;
            };
            Approach->SetBoolField(TEXT("forward_complete"), IsComplete(Forward));
            Approach->SetBoolField(TEXT("reverse_complete"), IsComplete(Reverse));
            Test->TestTrue(Label + TEXT(": return route to the entrance is complete"), IsComplete(Reverse));
            if (!Test->TestTrue(Label + TEXT(": route into the festival is complete"), IsComplete(Forward))) continue;

            TArray<TSharedPtr<FJsonValue>> Samples;
            int32 MissingSamples = 0, BlockedSamples = 0;
            double MaxError = 0, MaxExcessJump = 0, AuditedDistance = 0;
            const TArray<FNavPathPoint>& PathPoints = Forward.Path->GetPathPoints();
            for (const double Side : {-50.0, 0.0, 50.0})
            {
                double Distance = 0, PreviousGround = 0, PreviousPhysical = 0;
                bool bHasPrevious = false;
                for (int32 Segment = 1; Segment < PathPoints.Num() && Distance < ApproachLengthCm; ++Segment)
                {
                    const FVector Start = PathPoints[Segment - 1].Location;
                    const FVector End = PathPoints[Segment].Location;
                    const double SegmentLength = FVector::Dist2D(Start, End);
                    if (SegmentLength <= UE_SMALL_NUMBER) continue;
                    const double Length = FMath::Min(SegmentLength, ApproachLengthCm - Distance);
                    const FVector Along = (End - Start).GetSafeNormal2D();
                    const FVector Across(-Along.Y, Along.X, 0);
                    // A turn rotates the offset lane. Its two corner samples
                    // are not adjacent 10 cm steps, so do not compare them as
                    // a vertical jump. Both retain the independent 5 cm floor gate.
                    bHasPrevious = false;
                    const int32 Steps = FMath::Max(1, FMath::CeilToInt(Length / 10.0));
                    for (int32 Index = 0; Index <= Steps; ++Index)
                    {
                        const double AlongDistance = Length * static_cast<double>(Index) / Steps;
                        const FVector Point = FMath::Lerp(Start, End, AlongDistance / SegmentLength) + Across * Side;
                        FHitResult Hit, ComplexHit;
                        FVector Projected, ComplexProjected;
                        double GroundZ = 0;
                        const bool bFloor = TraceFloor(Point, false, Hit, Projected);
                        const bool bComplex = TraceFloor(Point, true, ComplexHit, ComplexProjected);
                        const bool bGround = Crowd->TryGetGroundZ(Point, GroundZ);
                        const bool bWalkable = Crowd->IsGroundWalkable(Point);
                        const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
                        Sample->SetNumberField(TEXT("path_segment"), Segment);
                        Sample->SetNumberField(TEXT("distance_from_gate_cm"), Distance + AlongDistance);
                        Sample->SetNumberField(TEXT("side_cm"), Side);
                        Sample->SetArrayField(TEXT("requested"), GroundVectorJson(Point));
                        Sample->SetBoolField(TEXT("physical_floor_found"), bFloor);
                        Sample->SetBoolField(TEXT("ground_query_succeeded"), bGround);
                        Sample->SetBoolField(TEXT("walkable"), bWalkable);
                        Sample->SetObjectField(TEXT("selected_cell"),
                            GroundCellJson(Crowd->GetHeightFieldAt(FVector2D(Point.X, Point.Y)), FVector2D(Point.X, Point.Y)));
                        if (bGround) Sample->SetNumberField(TEXT("ground_z"), GroundZ);
                        if (bFloor)
                        {
                            Sample->SetArrayField(TEXT("physical_floor"), GroundVectorJson(Hit.ImpactPoint));
                            Sample->SetStringField(TEXT("physical_actor"), Hit.GetActor() ? Hit.GetActor()->GetActorLabel() : FString());
                        }
                        if (bComplex) Sample->SetArrayField(TEXT("complex_floor"), GroundVectorJson(ComplexHit.ImpactPoint));
                        if (bGround && bFloor)
                        {
                            const double Error = FMath::Abs(GroundZ - Hit.ImpactPoint.Z);
                            Sample->SetNumberField(TEXT("absolute_error_cm"), Error);
                            MaxError = FMath::Max(MaxError, Error);
                            if (bHasPrevious) MaxExcessJump = FMath::Max(MaxExcessJump,
                                FMath::Abs(GroundZ - PreviousGround) - FMath::Abs(Hit.ImpactPoint.Z - PreviousPhysical));
                            PreviousGround = GroundZ;
                            PreviousPhysical = Hit.ImpactPoint.Z;
                            bHasPrevious = true;
                        }
                        else { ++MissingSamples; bHasPrevious = false; }
                        BlockedSamples += !bWalkable;
                        Samples.Add(MakeShared<FJsonValueObject>(Sample));
                    }
                    Distance += Length;
                }
                AuditedDistance = Distance;
                Test->TestTrue(Label + TEXT(": every lane covers the full 500 cm entrance approach"),
                    FMath::IsNearlyEqual(Distance, ApproachLengthCm, .01));
            }
            Approach->SetNumberField(TEXT("audited_distance_cm"), AuditedDistance);
            Approach->SetNumberField(TEXT("missing_samples"), MissingSamples);
            Approach->SetNumberField(TEXT("blocked_samples"), BlockedSamples);
            Approach->SetNumberField(TEXT("max_ground_error_cm"), MaxError);
            Approach->SetNumberField(TEXT("max_excess_height_jump_cm"), MaxExcessJump);
            Approach->SetArrayField(TEXT("samples"), Samples);
            Test->TestTrue(Label + TEXT(": all three lanes contain dense physical samples"), Samples.Num() >= 153);
            Test->TestEqual(Label + TEXT(": every physical floor and cached ground query exists"), MissingSamples, 0);
            Test->TestEqual(Label + TEXT(": audited entrance lanes are walkable"), BlockedSamples, 0);
            Test->TestTrue(Label + TEXT(": feet stay within 5 cm of actual physics"), MaxError <= 5.0);
            Test->TestTrue(Label + TEXT(": no extra vertical jump above 5 cm"), MaxExcessJump <= 5.0);
            Test->AddInfo(FString::Printf(TEXT("%s: samples=%d maxError=%.3f cm excessJump=%.3f cm"),
                *Label, Samples.Num(), MaxError, MaxExcessJump));
        }
        Report->SetArrayField(TEXT("gate_approaches"), GateApproaches);

        TArray<TSharedPtr<FJsonValue>> Walkways;
        int32 TaggedActors = 0;
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (!It->ActorHasTag(RefinementTag)) continue;
            ++TaggedActors;
            const FString Label = It->GetActorLabel();
            const TSharedRef<FJsonObject> Walkway = MakeShared<FJsonObject>();
            Walkway->SetStringField(TEXT("label"), Label);
            Walkway->SetStringField(TEXT("actor"), It->GetPathName());
            Walkways.Add(MakeShared<FJsonValueObject>(Walkway));
            const AStaticMeshActor* Actor = Cast<AStaticMeshActor>(*It);
            const UStaticMeshComponent* Component = Actor ? Actor->GetStaticMeshComponent() : nullptr;
            if (!Test->TestTrue(TEXT("Production refinement is a static cube with query collision"),
                Component && Component->GetStaticMesh() && Component->Mobility == EComponentMobility::Static
                && Component->GetStaticMesh()->GetPathName() == TEXT("/Engine/BasicShapes/Cube.Cube")
                && Component->IsQueryCollisionEnabled())) continue;
            const FTransform Transform = Component->GetComponentTransform();
            const FVector TopCenter = Transform.TransformPosition(FVector(0, 0, 50));
            const FVector Along = Transform.TransformVector(FVector(1, 0, 0)).GetSafeNormal2D();
            const FVector Across = Transform.TransformVector(FVector(0, 1, 0)).GetSafeNormal2D();
            const double HalfLength = Transform.TransformVector(FVector(50, 0, 0)).Size2D();
            const double Width = Transform.TransformVector(FVector(0, 100, 0)).Size2D();
            Walkway->SetNumberField(TEXT("horizontal_length_cm"), HalfLength * 2.0);
            Walkway->SetNumberField(TEXT("width_cm"), Width);
            Walkway->SetArrayField(TEXT("top_center"), GroundVectorJson(TopCenter));
            TArray<TSharedPtr<FJsonValue>> Samples;
            int32 MissingSamples = 0, NonLocalSamples = 0, BlockedSamples = 0;
            double MaxFineError = 0, MaxCoarseError = 0, MaxSimpleComplexDifference = 0;
            double MaxExcessHeightJump = 0;
            bool bExpectedLocalSpacing = true;
            for (const double Side : {-0.3, 0.0, 0.3})
            {
                bool bHasPrevious = false;
                double PreviousFine = 0, PreviousPhysical = 0;
                const int32 Steps = FMath::CeilToInt((HalfLength * 2.0 + 200.0) / 10.0);
                for (int32 Index = 0; Index <= Steps; ++Index)
                {
                    const double Offset = FMath::Lerp(-HalfLength - 100.0, HalfLength + 100.0, static_cast<double>(Index) / Steps);
                    const FVector Point = TopCenter + Along * Offset + Across * (Width * Side);
                    const FVector2D XY(Point.X, Point.Y);
                    const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
                    Sample->SetNumberField(TEXT("along_cm"), Offset);
                    Sample->SetNumberField(TEXT("side_cm"), Width * Side);
                    Sample->SetArrayField(TEXT("requested"), GroundVectorJson(Point));
                    const FFF_CrowdHeightField& SelectedField = Crowd->GetHeightFieldAt(XY);
                    const bool bLocal = &SelectedField != &Crowd->GetHeightField();
                    Sample->SetBoolField(TEXT("local_field"), bLocal);
                    NonLocalSamples += !bLocal;
                    Sample->SetObjectField(TEXT("selected_cell"), GroundCellJson(SelectedField, XY));
                    Sample->SetObjectField(TEXT("global_cell"), GroundCellJson(Crowd->GetHeightField(), XY));
                    FFF_CrowdHeightField::FCellProbe LocalProbe;
                    bExpectedLocalSpacing &= SelectedField.ProbeCell(XY, LocalProbe)
                        && FMath::IsNearlyEqual(FVector2D::Distance(LocalProbe.CornerXYCm[0], LocalProbe.CornerXYCm[1]), 5.0, .01);
                    FHitResult SimpleHit, ComplexHit;
                    FVector SimpleNav, ComplexNav;
                    double FineZ = 0, CoarseZ = 0;
                    const bool bSimple = TraceFloor(Point, false, SimpleHit, SimpleNav);
                    const bool bComplex = TraceFloor(Point, true, ComplexHit, ComplexNav);
                    const bool bFine = Crowd->TryGetGroundZ(Point, FineZ);
                    const bool bCoarse = Crowd->GetHeightField().TryGetHeightCm(XY, CoarseZ);
                    const bool bWalkable = Crowd->IsGroundWalkable(Point);
                    Sample->SetBoolField(TEXT("walkable"), bWalkable);
                    Sample->SetBoolField(TEXT("physical_floor_found"), bSimple);
                    Sample->SetBoolField(TEXT("ground_query_succeeded"), bFine);
                    BlockedSamples += !bWalkable;
                    if (bFine) Sample->SetNumberField(TEXT("ground_z"), FineZ);
                    if (bCoarse) Sample->SetNumberField(TEXT("global_z"), CoarseZ);
                    if (bSimple)
                    {
                        Sample->SetArrayField(TEXT("physical_floor"), GroundVectorJson(SimpleHit.ImpactPoint));
                        Sample->SetArrayField(TEXT("normal"), GroundVectorJson(SimpleHit.ImpactNormal));
                        Sample->SetArrayField(TEXT("nav_projection"), GroundVectorJson(SimpleNav));
                        Sample->SetStringField(TEXT("physical_actor"), SimpleHit.GetActor() ? SimpleHit.GetActor()->GetActorLabel() : FString());
                        Sample->SetStringField(TEXT("physical_component"), GetPathNameSafe(SimpleHit.GetComponent()));
                    }
                    if (bComplex)
                    {
                        Sample->SetArrayField(TEXT("complex_floor"), GroundVectorJson(ComplexHit.ImpactPoint));
                        if (bSimple) MaxSimpleComplexDifference = FMath::Max(MaxSimpleComplexDifference,
                            FMath::Abs(SimpleHit.ImpactPoint.Z - ComplexHit.ImpactPoint.Z));
                    }
                    if (bFine && bSimple)
                    {
                        const double Error = FMath::Abs(FineZ - SimpleHit.ImpactPoint.Z);
                        Sample->SetNumberField(TEXT("absolute_error_cm"), Error);
                        MaxFineError = FMath::Max(MaxFineError, Error);
                        if (bCoarse) MaxCoarseError = FMath::Max(MaxCoarseError, FMath::Abs(CoarseZ - SimpleHit.ImpactPoint.Z));
                        if (bHasPrevious) MaxExcessHeightJump = FMath::Max(MaxExcessHeightJump,
                            FMath::Abs(FineZ - PreviousFine) - FMath::Abs(SimpleHit.ImpactPoint.Z - PreviousPhysical));
                        PreviousFine = FineZ;
                        PreviousPhysical = SimpleHit.ImpactPoint.Z;
                        bHasPrevious = true;
                    }
                    else { ++MissingSamples; bHasPrevious = false; }
                    Samples.Add(MakeShared<FJsonValueObject>(Sample));
                }
            }
            Walkway->SetArrayField(TEXT("samples"), Samples);
            Walkway->SetNumberField(TEXT("missing_samples"), MissingSamples);
            Walkway->SetNumberField(TEXT("nonlocal_samples"), NonLocalSamples);
            Walkway->SetNumberField(TEXT("blocked_samples"), BlockedSamples);
            Walkway->SetNumberField(TEXT("max_fine_error_cm"), MaxFineError);
            Walkway->SetNumberField(TEXT("max_coarse_error_cm"), MaxCoarseError);
            Walkway->SetNumberField(TEXT("max_simple_complex_difference_cm"), MaxSimpleComplexDifference);
            Walkway->SetNumberField(TEXT("max_excess_height_jump_cm"), MaxExcessHeightJump);
            // Record the transition back to the unchanged global field, rather
            // than inferring that a 500 cm apron eliminates every boundary seam.
            TArray<TSharedPtr<FJsonValue>> BoundarySamples;
            double MaxBoundaryError = 0;
            bool bSaw25CmApron = false, bSawGlobalField = false;
            for (const double Direction : {-1.0, 1.0})
            {
                for (double Distance = HalfLength + 100.0; Distance <= HalfLength + 2000.0; Distance += 25.0)
                {
                    const FVector Candidate = TopCenter + Along * (Distance * Direction);
                    if (&Crowd->GetHeightFieldAt(FVector2D(Candidate.X, Candidate.Y)) != &Crowd->GetHeightField()) continue;
                    for (const double Delta : {-35.0, -25.0, -15.0, -5.0, 5.0, 15.0})
                    {
                        const FVector Point = TopCenter + Along * ((Distance + Delta) * Direction);
                        const FVector2D XY(Point.X, Point.Y);
                        const TSharedRef<FJsonObject> Boundary = MakeShared<FJsonObject>();
                        Boundary->SetArrayField(TEXT("point"), GroundVectorJson(Point));
                        Boundary->SetBoolField(TEXT("local_field"), &Crowd->GetHeightFieldAt(XY) != &Crowd->GetHeightField());
                        Boundary->SetObjectField(TEXT("selected_cell"), GroundCellJson(Crowd->GetHeightFieldAt(XY), XY));
                        FFF_CrowdHeightField::FCellProbe BoundaryProbe;
                        bSaw25CmApron |= Crowd->GetHeightFieldAt(XY).ProbeCell(XY, BoundaryProbe)
                            && FMath::IsNearlyEqual(FVector2D::Distance(BoundaryProbe.CornerXYCm[0], BoundaryProbe.CornerXYCm[1]), 25.0, .01);
                        bSawGlobalField |= &Crowd->GetHeightFieldAt(XY) == &Crowd->GetHeightField();
                        FHitResult Hit;
                        FVector Projected;
                        double GroundZ = 0;
                        const bool bFloor = TraceFloor(Point, false, Hit, Projected);
                        const bool bGround = Crowd->TryGetGroundZ(Point, GroundZ);
                        Boundary->SetBoolField(TEXT("physical_floor_found"), bFloor);
                        Boundary->SetBoolField(TEXT("ground_query_succeeded"), bGround);
                        if (bFloor) Boundary->SetArrayField(TEXT("physical_floor"), GroundVectorJson(Hit.ImpactPoint));
                        if (bGround) Boundary->SetNumberField(TEXT("ground_z"), GroundZ);
                        if (bFloor && bGround)
                        {
                            const double Error = FMath::Abs(GroundZ - Hit.ImpactPoint.Z);
                            Boundary->SetNumberField(TEXT("absolute_error_cm"), Error);
                            MaxBoundaryError = FMath::Max(MaxBoundaryError, Error);
                        }
                        BoundarySamples.Add(MakeShared<FJsonValueObject>(Boundary));
                    }
                    break;
                }
            }
            Walkway->SetArrayField(TEXT("apron_boundary_samples"), BoundarySamples);
            Walkway->SetNumberField(TEXT("max_apron_boundary_error_cm"), MaxBoundaryError);
            Walkway->SetStringField(TEXT("apron_boundary_verdict"), TEXT("Diagnostic evidence for review; the 5 cm route gate covers the walkway and its 100 cm approaches."));

            TArray<TSharedPtr<FJsonValue>> Routes;
            for (const double Side : {-0.3, 0.0, 0.3})
            {
                const FVector Upper = TopCenter - Along * (HalfLength + 100.0) + Across * (Width * Side);
                const FVector Lower = TopCenter + Along * (HalfLength + 100.0) + Across * (Width * Side);
                FHitResult UpperHit, LowerHit;
                FVector UpperNav, LowerNav;
                const bool bEndpoints = TraceFloor(Upper, false, UpperHit, UpperNav) && TraceFloor(Lower, false, LowerHit, LowerNav);
                for (const bool bReverse : {false, true})
                {
                    const TSharedRef<FJsonObject> Route = MakeShared<FJsonObject>();
                    Route->SetNumberField(TEXT("side_cm"), Width * Side);
                    Route->SetBoolField(TEXT("uphill"), bReverse);
                    bool bComplete = false;
                    if (bEndpoints)
                    {
                        FPathFindingQuery Query(nullptr, *Data, bReverse ? LowerNav : UpperNav, bReverse ? UpperNav : LowerNav);
                        Query.SetAllowPartialPaths(false);
                        const FPathFindingResult Result = Navigation->FindPathSync(Query);
                        bComplete = Result.IsSuccessful() && !Result.IsPartial() && Result.Path.IsValid()
                            && Result.Path->GetPathPoints().Num() >= 2;
                        TArray<TSharedPtr<FJsonValue>> Points;
                        if (Result.Path.IsValid())
                            for (const FNavPathPoint& Point : Result.Path->GetPathPoints())
                                Points.Add(MakeShared<FJsonValueArray>(GroundVectorJson(Point.Location)));
                        Route->SetArrayField(TEXT("path_points"), Points);
                    }
                    Route->SetBoolField(TEXT("complete"), bComplete);
                    Test->TestTrue(Label + TEXT(": approach endpoints have a complete directed navigation route"), bComplete);
                    Routes.Add(MakeShared<FJsonValueObject>(Route));
                }
            }
            Walkway->SetArrayField(TEXT("controlled_navigation_routes"), Routes);
            Test->TestEqual(Label + TEXT(": every audited floor/query exists"), MissingSamples, 0);
            Test->TestEqual(Label + TEXT(": walkway and 100 cm approaches use the local field"), NonLocalSamples, 0);
            Test->TestTrue(Label + TEXT(": walkway and approaches use 5 cm core cells"), bExpectedLocalSpacing);
            Test->TestTrue(Label + TEXT(": broad apron retains 25 cm cells"), bSaw25CmApron);
            Test->TestTrue(Label + TEXT(": beyond apron returns to the global field"), bSawGlobalField);
            Test->TestEqual(Label + TEXT(": audited route is walkable"), BlockedSamples, 0);
            Test->TestTrue(Label + TEXT(": feet stay within 5 cm of actual physics"), MaxFineError <= 5.0);
            Test->TestTrue(Label + TEXT(": no extra vertical jump above 5 cm"), MaxExcessHeightJump <= 5.0);
            Test->AddInfo(FString::Printf(TEXT("%s: samples=%d fineError=%.3f cm coarseError=%.3f cm excessJump=%.3f cm apronBoundaryError=%.3f cm"),
                *Label, Samples.Num(), MaxFineError, MaxCoarseError, MaxExcessHeightJump, MaxBoundaryError));
        }
        Report->SetArrayField(TEXT("walkways"), Walkways);
        Report->SetNumberField(TEXT("tagged_actor_count"), TaggedActors);
        Test->TestEqual(TEXT("Every authored tagged surface has a cached refinement"),
            Crowd->GetGroundRefinementCount(), TaggedActors);
        const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/CrowdGroundRefinement"));
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Path = FPaths::Combine(Directory,
            TEXT("production-ground-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".json"));
        FString Json;
        Test->TestTrue(TEXT("Production ground evidence JSON is written"),
            FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json))
            && FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
        Test->AddInfo(TEXT("Production ground evidence: ") + FPaths::ConvertRelativePathToFull(Path));
        return true;
    }

private:
    FAutomationTestBase* Test;
    double StartedAt = -1.0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_CrowdGroundRefinementProductionTest,
    "FestaFiesta.Crowd.Ground.ProductionWalkwayFloor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_CrowdGroundRefinementProductionTest::RunTest(const FString& Parameters)
{
    UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(TEXT("/Game/FestaFiesta/Maps/Test_Main"));
    if (!TestNotNull(TEXT("Production map loads"), World)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FFF_CheckProductionGroundRefinements(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif
