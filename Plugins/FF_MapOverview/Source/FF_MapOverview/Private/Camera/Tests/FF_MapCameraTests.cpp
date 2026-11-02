#if WITH_DEV_AUTOMATION_TESTS

#include "FF_MapCameraLibrary.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace
{
    bool AllCornersInsideSafeFrame(const FBox& Bounds, const FTransform& Camera, const double Fov,
        const double Aspect, const FBox2D& SafeRect)
    {
        const double TanX = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5));
        const double TanY = TanX / Aspect;
        for (int32 Index = 0; Index < 8; ++Index)
        {
            const FVector Corner((Index & 1) ? Bounds.Max.X : Bounds.Min.X,
                (Index & 2) ? Bounds.Max.Y : Bounds.Min.Y,
                (Index & 4) ? Bounds.Max.Z : Bounds.Min.Z);
            // Independently project into camera coordinates instead of reproducing the fit solver.
            const FVector View = Camera.InverseTransformPosition(Corner);
            if (View.X < 100.0) return false;
            const FVector2D Ndc(View.Y / (View.X * TanX), View.Z / (View.X * TanY));
            if (Ndc.X < SafeRect.Min.X - 1.e-8 || Ndc.X > SafeRect.Max.X + 1.e-8
                || Ndc.Y < SafeRect.Min.Y - 1.e-8 || Ndc.Y > SafeRect.Max.Y + 1.e-8) return false;
        }
        return true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapWholeCameraPerspectiveFitTest,
    "FF_MapOverview.Camera.PerspectiveFit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapWholeCameraPerspectiveFitTest::RunTest(const FString& Parameters)
{
    const FBox Maps[] = {
        FBox(FVector(-5000.0, -5000.0, -1000.0), FVector(5000.0, 5000.0, 2000.0)),
        FBox(FVector(0.0, 0.0, 0.0), FVector(120000.0, 15000.0, 4000.0)),
        FBox(FVector(-10000.0, -80000.0, -2000.0), FVector(10000.0, 80000.0, 16000.0)),
        FBox(FVector(1.e8, -1.e8, 3000.0), FVector(1.e8 + 72000.0, -1.e8 + 120000.0, 23000.0))
    };
    const double Aspects[] = { 16.0 / 9.0, 4.0 / 3.0, 32.0 / 9.0, 9.0 / 16.0 };
    const FRotator Rotations[] = { FRotator(-62.0, 0.0, 0.0), FRotator(-45.0, 135.0, 0.0),
        FRotator(-90.0, 90.0, 0.0), FRotator(-55.0, -38.0, 12.0) };
    const FBox2D SafeRect(FVector2D(-0.94, -0.55), FVector2D(0.62, 0.92));
    for (const FBox& Bounds : Maps)
    {
        for (const double Aspect : Aspects)
        {
            for (const FRotator& Rotation : Rotations)
            {
                FTransform Camera;
                if (!TestTrue(TEXT("Perspective fit produces a camera"),
                    FFMapCamera::ComputeWholeMapView(Bounds, Rotation, 60.0, Aspect, SafeRect, Camera))) return false;
                TestTrue(TEXT("All near and far corners fit within HUD-safe frame"),
                    AllCornersInsideSafeFrame(Bounds, Camera, 60.0, Aspect, SafeRect));
            }
        }
    }
    FTransform Camera;
    TestFalse(TEXT("Empty map rejected"), FFMapCamera::ComputeWholeMapView(FBox(ForceInit),
        Rotations[0], 60.0, 1.0, SafeRect, Camera));
    TestFalse(TEXT("Zero aspect rejected"), FFMapCamera::ComputeWholeMapView(Maps[0],
        Rotations[0], 60.0, 0.0, SafeRect, Camera));
    TestFalse(TEXT("Invalid FOV rejected"), FFMapCamera::ComputeWholeMapView(Maps[0],
        Rotations[0], 180.0, 1.0, SafeRect, Camera));
    TestFalse(TEXT("Collapsed safe frame rejected"), FFMapCamera::ComputeWholeMapView(Maps[0],
        Rotations[0], 60.0, 1.0, FBox2D(FVector2D(0.0), FVector2D(0.0)), Camera));
    TestFalse(TEXT("Safe frame outside viewport rejected"), FFMapCamera::ComputeWholeMapView(Maps[0],
        Rotations[0], 60.0, 1.0, FBox2D(FVector2D(-2.0), FVector2D(1.0)), Camera));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapCameraAuthoritativeBoundsTest,
    "FF_MapOverview.Camera.AuthoritativeBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapCameraAuthoritativeBoundsTest::RunTest(const FString& Parameters)
{
    FBox Bounds(ForceInit);
    TestFalse(TEXT("No world has no map bounds"), FFMapCamera::TryGetMapBounds(nullptr, Bounds));
    const UWorld::InitializationValues Init = UWorld::InitializationValues()
        .AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
        .CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false,
        ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Isolated transient world"), World)) return false;
    struct FWorldCleanup
    {
        UWorld* World;
        ~FWorldCleanup() { World->DestroyWorld(false); }
    } Cleanup { World };
    UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
    if (!TestNotNull(TEXT("Placement authority"), Zones)) return false;
    TestFalse(TEXT("An empty registry does not invent map bounds"), FFMapCamera::TryGetMapBounds(World, Bounds));

    AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
    if (!TestNotNull(TEXT("Zone"), Zone)
        || !TestTrue(TEXT("Existing accepted project identity"), Zone->ApplyAcceptedZonePreset(FFZSTest::ProjectZoneId()))) return false;
    Zone->SetActorLocationAndRotation(FVector(10000.0, 20000.0, 300.0), FRotator(0.0, 90.0, 0.0));
    Zone->VerticalExtentCm = 1000.0f;
    Zone->BoundarySpline->ClearSplinePoints(false);
    const FVector Points[] = { FVector(0.0, 0.0, 0.0), FVector(4000.0, 0.0, 0.0),
        FVector(4000.0, 2000.0, 0.0), FVector(0.0, 2000.0, 0.0) };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Points); ++Index)
    {
        Zone->BoundarySpline->AddSplinePoint(Points[Index], ESplineCoordinateSpace::Local, false);
        Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
    }
    Zone->BoundarySpline->SetClosedLoop(true, true);
    if (!TestTrue(TEXT("Registered authority zone"), Zones->RegisterZone(Zone))) return false;
    TestTrue(TEXT("Zone bounds available without unified build actor"), FFMapCamera::TryGetMapBounds(World, Bounds));
    TestTrue(TEXT("Translated/rotated zone world minimum"), Bounds.Min.Equals(FVector(8000.0, 20000.0, -200.0), 0.01));
    TestTrue(TEXT("Translated/rotated zone world maximum"), Bounds.Max.Equals(FVector(10000.0, 24000.0, 800.0), 0.01));

    AFFZSBuildArea* BuildArea = World->SpawnActor<AFFZSBuildArea>();
    if (!TestNotNull(TEXT("Unified build area"), BuildArea)
        || !TestTrue(TEXT("Unified build area registered"), Zones->RegisterBuildArea(BuildArea))) return false;
    TestTrue(TEXT("Unified authority bounds available"), FFMapCamera::TryGetMapBounds(World, Bounds));
    TestTrue(TEXT("Camera uses exact unified authority extent"), Bounds.Equals(BuildArea->GetBuildBounds(), 0.01));
    Zones->UnregisterZone(Zone);
    TestFalse(TEXT("Unloaded zone cannot leave stale camera/map bounds"), FFMapCamera::TryGetMapBounds(World, Bounds));
    return true;
}

#endif
