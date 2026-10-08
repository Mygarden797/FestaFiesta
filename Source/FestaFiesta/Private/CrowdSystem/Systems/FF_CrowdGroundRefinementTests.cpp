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
        Report->SetStringField(TEXT("scope"), TEXT("Read-only queries against production PIE ground caches and independent WorldStatic physics; no map or settings changes."));
        Report->SetNumberField(TEXT("configured_global_spacing_cm"), Crowd->HeightFieldSpacingUU);
        Report->SetNumberField(TEXT("cliff_step_cm"), Crowd->HeightFieldCliffStepUU);
        Report->SetNumberField(TEXT("refinement_count"), Crowd->GetGroundRefinementCount());
        Report->SetNumberField(TEXT("refinement_sample_count"), Crowd->GetGroundRefinementSampleCount());
        Report->SetNumberField(TEXT("refinement_sample_payload_bytes"), Crowd->GetGroundRefinementSampleCount() * (sizeof(double) + sizeof(uint8)));
        Report->SetNumberField(TEXT("refinement_build_ms"), Crowd->GetGroundRefinementBuildSeconds() * 1000.0);
        Report->SetNumberField(TEXT("surface_error_limit_cm"), 5.0);
        Test->TestEqual(TEXT("Production keeps its 500 cm global sampling setting"), Crowd->HeightFieldSpacingUU, 500.f);
        Test->TestEqual(TEXT("Both explicit walkways have cached ground refinements"), Crowd->GetGroundRefinementCount(), 2);

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

        TSet<FString> ExpectedLabels {TEXT("FF_CrowdWalkway_DeckDescent"), TEXT("FF_CrowdWalkway_ParkingGate3")};
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
            Test->TestTrue(TEXT("Each tagged walkway has its expected unique production label"), ExpectedLabels.Remove(Label) == 1);
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
        Test->TestEqual(TEXT("Exactly the two authored production walkways are tagged"), TaggedActors, 2);
        Test->TestTrue(TEXT("Both named production walkways were audited"), ExpectedLabels.IsEmpty());
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
