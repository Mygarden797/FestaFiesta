#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_MapGridActor.h"
#include "FF_MapGridComponent.h"
#include "FF_MapGridModel.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapGridZoomStabilityTest,
    "FF_MapOverview.Grid.ZoomSpacingAndStableWindow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapGridZoomStabilityTest::RunTest(const FString& Parameters)
{
    using namespace FFMapGridModel;
    TestEqual(TEXT("Readable close view keeps metre cells"), ResolveZoomStep(100, 30.0), 100);
    TestEqual(TEXT("Distant view chooses a readable lattice multiple"), ResolveZoomStep(100, 1.0), 1600);
    TestEqual(TEXT("Small zoom movements stay inside the hysteresis band"), ResolveZoomStep(100, 9.5, 100), 100);
    TestEqual(TEXT("Unavailable projection preserves the last spacing"), ResolveZoomStep(100, 0.0, 800), 800);
    int32 PreviousStep = 400;
    for (const double Pixels : { 4.0, 3.2, 2.8, 2.2, 1.4, 0.7, 1.6, 3.0, 5.2, 10.4, 15.0 })
    {
        const int32 ChosenStep = ResolveZoomStep(100, Pixels, PreviousStep);
        const double ChosenPixels = Pixels * ChosenStep / 100.0;
        TestTrue(TEXT("A coarse LOD follows actual projected readability, including hysteresis"),
            ChosenStep == 100 || (ChosenPixels >= 9.0 && ChosenPixels < 22.0));
        PreviousStep = ChosenStep;
    }
    TestEqual(TEXT("Zoom can legitimately retain the same centimetre interval across distinct projected scales"),
        ResolveZoomStep(100, 2.8, ResolveZoomStep(100, 4.0, 400)), 400);
    const FBox2D A = MakeLocalWindow(FVector2D(1000.0), 100);
    const FBox2D B = MakeLocalWindow(FVector2D(1010.0), 100);
    TestTrue(TEXT("Small camera movement does not change terrain or spatial cache coordinates"),
        A.Min.Equals(B.Min) && A.Max.Equals(B.Max));
    TestTrue(TEXT("Local edge fading stays readable"), EdgeOpacity(FVector2D(100.0), FVector2D::ZeroVector,
        FVector2D(100.0), false) >= 0.55f);
    TestEqual(TEXT("Whole map has no camera-window fade"), EdgeOpacity(FVector2D(100.0), FVector2D::ZeroVector,
        FVector2D(100.0), true), 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapGridAuthorityCompactionTest,
    "FF_MapOverview.Grid.ConservativeCompactionAndWorldCoordinates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapGridAuthorityCompactionTest::RunTest(const FString& Parameters)
{
    FFFZSBuildableGridMask Source, Compact;
    Source.Initialize(FVector2D(-200.0), FVector2D(200.0), 100);
    for (int32 Y = 0; Y < 4; ++Y)
        for (int32 X = 0; X < 4; ++X) Source.SetBuildable(X, Y, X != 1 || Y != 1);
    Source.FacilityCategory = EFFZSFacilityCategory::Tile;
    Source.bIncludeGroundSurfaceSupport = false;
    FFFZSBuildGridObstacle& Obstacle = Source.DeferredWorldObstacles.AddDefaulted_GetRef();
    Obstacle.FirstCell = FIntPoint(1, 1);
    Obstacle.EndCell = FIntPoint(3, 3);
    const FTransform ZoneTransform(FRotator(0, 90, 0), FVector(1000, 2000, 30));
    FTransform CompactTransform;
    TestTrue(TEXT("Compact an authority result into the actor's assigned share"),
        FFMapGridModel::CompactAuthorityMask(Source, ZoneTransform, 4, 200.0, Compact, CompactTransform));
    TestEqual(TEXT("Retained cell count respects the smaller share"), Compact.GetCellCount(), 4);
    TestEqual(TEXT("One blocked source cell removes its entire coarse cell"), Compact.GetBuildableCellCount(), 3);
    TestFalse(TEXT("Coarse overview never makes an authority obstacle appear free"), Compact.IsBuildable(0, 0));
    TestTrue(TEXT("Normalized coordinates still reach the exact rotated world lattice"),
        CompactTransform.TransformPosition(FVector(100, 100, 0)).Equals(
            ZoneTransform.TransformPosition(FVector::ZeroVector), 0.001));
    TestTrue(TEXT("Terrain category and support exclusions are preserved"),
        Compact.FacilityCategory == EFFZSFacilityCategory::Tile && !Compact.bIncludeGroundSurfaceSupport);
    TestEqual(TEXT("Obstacle ranges conservatively remap into the compact mask"),
        Compact.DeferredWorldObstacles[0].EndCell, FIntPoint(2, 2));
    TestEqual(TEXT("Presentation leaves the authority mask untouched"), Source.GetBuildableCellCount(), 15);
    Source.Initialize(FVector2D(-500000.0), FVector2D(500000.0), 25);
    TestTrue(TEXT("Huge authority windows compact to a global allocation share"),
        FFMapGridModel::CompactAuthorityMask(Source, FTransform::Identity, 128, 25, Compact, CompactTransform));
    TestTrue(TEXT("A huge world cannot exceed the per-region share"), Compact.GetCellCount() <= 128);
    const FTransform OffsetRegion(FRotator(0, 90, 0), FVector(1037, -2063, 0));
    const FVector WorldOriginLocal = OffsetRegion.InverseTransformPosition(FVector::ZeroVector);
    Source.Initialize(FVector2D(-1000.0), FVector2D(1000.0), 100,
        FVector2D(WorldOriginLocal.X, WorldOriginLocal.Y));
    TestTrue(TEXT("Unified overview compacts around the world lattice"),
        FFMapGridModel::CompactAuthorityMask(Source, OffsetRegion, 16, 400,
            Compact, CompactTransform, true));
    const double CoarseStep = CompactTransform.GetScale3D().X * Compact.GetStepCm();
    const FVector WorldCorner = CompactTransform.TransformPosition(FVector::ZeroVector);
    TestTrue(TEXT("Coarse grid does not shift when the coordinate region has an offset or rotated origin"),
        FMath::IsNearlyEqual(WorldCorner.X, FMath::GridSnap(WorldCorner.X, CoarseStep), 0.001) &&
        FMath::IsNearlyEqual(WorldCorner.Y, FMath::GridSnap(WorldCorner.Y, CoarseStep), 0.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapGridWorldOverviewTest,
    "FF_MapOverview.Grid.WorldModesBudgetAndAuthorityIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMapGridWorldOverviewTest::RunTest(const FString& Parameters)
{
    UFFZSDeveloperSettings* Settings = GetMutableDefault<UFFZSDeveloperSettings>();
    TGuardValue<bool> GroundGuard(Settings->bEnableGroundChecks, false);
    TGuardValue<bool> CollisionGuard(Settings->bEnableWorldCollisionChecks, false);
    const UWorld::InitializationValues IVS = UWorld::InitializationValues().AllowAudioPlayback(false)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &IVS);
    if (!TestNotNull(TEXT("Create an isolated test world"), World)) return false;
    UFFZSWorldSubsystem* System = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Existing authority is available in the world"), System))
    {
        World->DestroyWorld(false);
        return false;
    }
    AFFZSGameplayZone* Near = World->SpawnActor<AFFZSGameplayZone>();
    AFFZSGameplayZone* Far = World->SpawnActor<AFFZSGameplayZone>();
    Near->ZoneId = TEXT("GZ_Beach01");
    Far->ZoneId = TEXT("GZ_Beach02");
    Far->SetActorLocation(FVector(100000, 0, 0));
    TestTrue(TEXT("Register first existing authority region"), System->RegisterZone(Near));
    TestTrue(TEXT("Register a distant disjoint authority region"), System->RegisterZone(Far));
    AFFMapGridActor* Grid = World->SpawnActor<AFFMapGridActor>();
    const int64 Revision = System->GetWorldRevision();
    Grid->RefreshGrid(*System, FVector(5000, 5000, 0), FVector::UpVector, 100, false);
    TArray<UFFMapGridComponent*> Components;
    Grid->GetComponents(Components);
    TestEqual(TEXT("Nearby mode omits a region one kilometre away"), Components.Num(), 1);
    Grid->RefreshGrid(*System, FVector(5000, 5000, 0), FVector::UpVector, 100, true);
    Grid->RefreshGrid(*System, FVector(5000, 5000, 0), FVector::UpVector, 100, true);
    Grid->GetComponents(Components);
    TestEqual(TEXT("Whole mode includes both disjoint regions"), Components.Num(), 2);
    bool bHasFarGeometry = false;
    for (const UFFMapGridComponent* Component : Components)
        for (const FBatchedLine& Line : Component->BatchedLines) bHasFarGeometry |= Line.Start.X >= 100000.0;
    TestTrue(TEXT("Whole map has actual far-region grid geometry"), bHasFarGeometry);
    TestTrue(TEXT("The actor caps aggregate retained cells"), Grid->GetRetainedCellCount() <= FFMapGridModel::MaximumCells);
    TestTrue(TEXT("The actor caps aggregate rendered strokes"), Grid->GetRenderedLineCount() <= FFMapGridModel::MaximumRenderedLines);
    TestTrue(TEXT("All regions share one terrain trace allowance"), Grid->GetLastTraceBudget() <= 512);
    TestTrue(TEXT("Only one authority mask may rebuild in an update"), Grid->GetLastAuthorityRebuildCount() <= 1);
    TestEqual(TEXT("Display mode switches never issue authority mutations"), System->GetWorldRevision(), Revision);
    uint64 Presentation = 0;
    uint64 SurfaceUpdates = 0;
    for (const UFFMapGridComponent* Component : Components)
    {
        Presentation += Component->GetPresentationRevision();
        SurfaceUpdates += Component->GetSurfaceUpdateCount();
    }
    Grid->RefreshGrid(*System, FVector(5010, 5010, 0), FVector::UpVector, 100, true);
    uint64 StablePresentation = 0;
    uint64 StableSurfaceUpdates = 0;
    for (const UFFMapGridComponent* Component : Components)
    {
        StablePresentation += Component->GetPresentationRevision();
        StableSurfaceUpdates += Component->GetSurfaceUpdateCount();
    }
    TestEqual(TEXT("Small movement retains render proxies"), StablePresentation, Presentation);
    TestEqual(TEXT("Settled timer ticks do not traverse terrain vertices again"), StableSurfaceUpdates, SurfaceUpdates);
    Grid->HideGrid();
    TestEqual(TEXT("Hidden mode releases line geometry"), Grid->GetRenderedLineCount(), 0);
    TestFalse(TEXT("Hidden mode has no scheduled terrain work"), Grid->HasPendingSamples());
    World->DestroyWorld(false);
    return true;
}

#endif
