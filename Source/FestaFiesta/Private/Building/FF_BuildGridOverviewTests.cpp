// FF_BuildGridOverviewTests.cpp
// 역할: BuildGridOverview 기능의 동작 계약을 자동 검사한다 (검사 경로: FestaFiesta.Building.BuildMode.PlacementGridOverview).

// Integration coverage for the build session's persistent surroundings grid. The
// fixture has a real local player but no render viewport, so the placement-surface
// fallback is required; no mouse, camera, authority commands or world ticks are used.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Components/SplineComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "FF_FFZSBuildableGridActor.h"
#include "FF_FFZSBuildableGridComponent.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Widgets/SOverlay.h"

namespace
{
    class FScopedBuildOverviewSettings
    {
    public:
        FScopedBuildOverviewSettings()
            : Settings(GetMutableDefault<UFFZSDeveloperSettings>())
            , bGround(Settings->bEnableGroundChecks)
            , bCollision(Settings->bEnableWorldCollisionChecks)
            , bRestrictZones(Settings->bRestrictZoneIdsToProjectPresets)
            , Radius(Settings->PlacementGridMaxRadiusCm)
        {
            // Spatial exclusion and real terrain sampling have separate plugin tests.
            // This fixture isolates which persistent presentation owns the grid.
            Settings->bEnableGroundChecks = false;
            Settings->bEnableWorldCollisionChecks = false;
            Settings->bRestrictZoneIdsToProjectPresets = false;
            Settings->PlacementGridMaxRadiusCm = 2000.0f;
        }

        ~FScopedBuildOverviewSettings()
        {
            Settings->bEnableGroundChecks = bGround;
            Settings->bEnableWorldCollisionChecks = bCollision;
            Settings->bRestrictZoneIdsToProjectPresets = bRestrictZones;
            Settings->PlacementGridMaxRadiusCm = Radius;
        }

    private:
        UFFZSDeveloperSettings* Settings;
        bool bGround, bCollision, bRestrictZones;
        float Radius;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildGridOverviewTest,
    "FestaFiesta.Building.BuildMode.PlacementGridOverview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildGridOverviewTest::RunTest(const FString& Parameters)
{
    FScopedBuildOverviewSettings Settings;
    const TSharedRef<SOverlay> Overlay = SNew(SOverlay);
    FTestWorldWrapper Fixture;
    if (!Fixture.CreateTestWorld(EWorldType::Game))
    {
        Fixture.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Fixture.GetTestWorld();
    UGameInstance* Game = World ? World->GetGameInstance() : nullptr;
    UFF_BuildModeSubsystem* Build = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
    UFFZSWorldSubsystem* Zones = World ? World->GetSubsystem<UFFZSWorldSubsystem>() : nullptr;
    if (!TestNotNull(TEXT("Game world"), World) || !TestNotNull(TEXT("Game instance"), Game) ||
        !TestNotNull(TEXT("Production build subsystem"), Build) || !TestNotNull(TEXT("Zone subsystem"), Zones))
    {
        return false;
    }
    FWorldContext* Context = GEngine->GetWorldContextFromWorld(World);
    if (!TestNotNull(TEXT("Test world context"), Context))
    {
        return false;
    }
    UGameViewportClient* Viewport = NewObject<UGameViewportClient>(GEngine);
    Context->GameViewport = Viewport;
    Viewport->Init(*Context, Game, false);
    Viewport->SetViewportOverlayWidget(nullptr, Overlay);
    FString PlayerError;
    ULocalPlayer* Player = Game->CreateLocalPlayer(0, PlayerError, false);
    APlayerController* Controller = World->SpawnActor<APlayerController>();
    if (!TestNotNull(*FString::Printf(TEXT("Real local player: %s"), *PlayerError), Player) ||
        !TestNotNull(TEXT("Local controller"), Controller))
    {
        return false;
    }
    Controller->SetPlayer(Player);
    // The lightweight world wrapper does not run PostInitializeComponents, which
    // normally registers this controller for the production GetFirstPlayerController.
    World->AddController(Controller);
    if (!TestEqual(TEXT("Production lookup finds the real local controller"), Build->GetLocalPlayerController(), Controller))
    {
        return false;
    }
    int32 ViewportWidth = -1;
    int32 ViewportHeight = -1;
    Controller->GetViewportSize(ViewportWidth, ViewportHeight);
    TestEqual(TEXT("Fixture cannot anchor a grid from a render viewport width"), ViewportWidth, 0);
    TestEqual(TEXT("Fixture cannot anchor a grid from a render viewport height"), ViewportHeight, 0);

    const auto AddZone = [&](const FName Id, const FVector Location) -> AFFZSGameplayZone*
    {
        AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
        if (!Zone || !Zone->ApplyAcceptedZonePreset(Id))
        {
            return nullptr;
        }
        Zone->SetActorLocation(Location);
        const FVector Corners[] = {
            FVector(0.0, 0.0, 0.0), FVector(1000.0, 0.0, 0.0),
            FVector(1000.0, 1000.0, 0.0), FVector(0.0, 1000.0, 0.0)
        };
        Zone->BoundarySpline->ClearSplinePoints(false);
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
        {
            Zone->BoundarySpline->AddSplinePoint(Corners[Index], ESplineCoordinateSpace::Local, false);
            Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
        }
        Zone->BoundarySpline->SetClosedLoop(true, true);
        return Zones->RegisterZone(Zone) ? Zone : nullptr;
    };
    AFFZSGameplayZone* FirstZone = AddZone(TEXT("BuildOverviewA"), FVector::ZeroVector);
    AFFZSGameplayZone* AdjacentZone = AddZone(TEXT("BuildOverviewB"), FVector(1500.0, 0.0, 0.0));
    if (!TestNotNull(TEXT("First registered zone"), FirstZone) ||
        !TestNotNull(TEXT("Adjacent registered zone"), AdjacentZone))
    {
        return false;
    }
    const int64 InitialRevision = Zones->GetWorldRevision();

    // Seed the cursor-derived target only. The state transitions, preview creation,
    // surface getter, persistent overview and G toggle all use production methods.
    Build->SpawnPlacementPreview();
    AFFZSPlacementPreviewActor* Preview = Build->PlacementPreview;
    if (!TestNotNull(TEXT("Production placement preview"), Preview))
    {
        return false;
    }
    FVector SurfacePoint;
    FVector SurfaceNormal;
    TestFalse(TEXT("A new preview has no usable grid surface"), Preview->GetPlacementGridSurface(SurfacePoint, SurfaceNormal));
    const FVector TargetSurface(500.0, 500.0, 0.0);
    Preview->SetPlacementGridSurface(TargetSurface, FVector::UpVector);
    TestTrue(TEXT("Preview exposes its sampled ground surface"), Preview->GetPlacementGridSurface(SurfacePoint, SurfaceNormal));
    TestTrue(TEXT("Sampled surface point is preserved"), SurfacePoint.Equals(TargetSurface));
    TestTrue(TEXT("Sampled surface normal is preserved"), SurfaceNormal.Equals(FVector::UpVector));
    Build->CurrentZoneId = FirstZone->ZoneId;
    Build->bHasPlacementTarget = true;
    Build->bBuildSessionActive = true;
    Build->bPlacementGridVisible = true;
    Build->SetState(EFF_BuildModeState::Placing);

    AFFZSBuildableGridActor* Overview = Build->BuildGridOverview;
    if (!TestNotNull(TEXT("Entering placement creates the surrounding overview from the target surface"), Overview))
    {
        return false;
    }
    TArray<UFFZSBuildableGridComponent*> OriginalGrids;
    Overview->GetComponents(OriginalGrids);
    if (!TestEqual(TEXT("The overview covers both nearby zones during placement"), OriginalGrids.Num(), 2))
    {
        return false;
    }
    const auto CheckVisibleOverview = [&](const TCHAR* Stage)
    {
        TestEqual(*FString::Printf(TEXT("%s: overview actor is reused"), Stage), Build->BuildGridOverview.Get(), Overview);
        TestFalse(*FString::Printf(TEXT("%s: overview actor is visible"), Stage), Overview->IsHidden());
        TArray<UFFZSBuildableGridComponent*> CurrentGrids;
        Overview->GetComponents(CurrentGrids);
        TestEqual(*FString::Printf(TEXT("%s: no extra zone components are allocated"), Stage), CurrentGrids.Num(), OriginalGrids.Num());
        for (UFFZSBuildableGridComponent* Grid : OriginalGrids)
        {
            TestTrue(*FString::Printf(TEXT("%s: original zone component is reused"), Stage), CurrentGrids.Contains(Grid));
            TestTrue(*FString::Printf(TEXT("%s: nearby zone retains visible lines"), Stage),
                !Grid->bHiddenInGame && !Grid->BatchedLines.IsEmpty());
        }
        TestFalse(*FString::Printf(TEXT("%s: preview never draws a duplicate local grid"), Stage), Preview->bDrawPlacementGrid);
        TestTrue(*FString::Printf(TEXT("%s: preview local grid has no lines"), Stage), Preview->PlacementGridLines->BatchedLines.IsEmpty());
    };
    const auto CheckHiddenOverview = [&](const TCHAR* Stage)
    {
        TestTrue(*FString::Printf(TEXT("%s: overview actor is hidden"), Stage), Overview->IsHidden());
        for (const UFFZSBuildableGridComponent* Grid : OriginalGrids)
        {
            TestTrue(*FString::Printf(TEXT("%s: persistent zone lines are cleared"), Stage),
                Grid->bHiddenInGame && Grid->BatchedLines.IsEmpty());
        }
    };

    CheckVisibleOverview(TEXT("Entering placement"));
    Build->RefreshBuildGridOverview();
    CheckVisibleOverview(TEXT("Refreshing placement"));
    for (const EFF_BuildModeState PreviewState : { EFF_BuildModeState::Placing, EFF_BuildModeState::Moving })
    {
        Build->SetState(PreviewState);
        CheckVisibleOverview(TEXT("Preview state transition"));
        Build->SetPlacementGridVisible(false);
        CheckHiddenOverview(TEXT("G toggle off"));
        TestFalse(TEXT("G toggle off also keeps the local grid disabled"), Preview->bDrawPlacementGrid);
        Build->SetPlacementGridVisible(true);
        CheckVisibleOverview(TEXT("G toggle on"));
    }

    Build->bHasPlacementTarget = false;
    Build->RefreshBuildGridOverview();
    CheckHiddenOverview(TEXT("Neither a camera nor a valid target is available"));
    Build->bHasPlacementTarget = true;
    Build->RefreshBuildGridOverview();
    CheckVisibleOverview(TEXT("Placement surface becomes available again"));

    Build->SetState(EFF_BuildModeState::Closed);
    TestNull(TEXT("Closing the build session releases the overview reference"), Build->BuildGridOverview.Get());
    TestTrue(TEXT("Closing destroys the transient overview actor"), Overview->IsActorBeingDestroyed());
    Build->RefreshBuildGridOverview();
    TestNull(TEXT("A closed session cannot recreate the surrounding grid"), Build->BuildGridOverview.Get());
    TestEqual(TEXT("Grid state, refreshes and visibility toggles never execute authority commands"),
        Zones->GetWorldRevision(), InitialRevision);
    Build->DestroyPlacementPreview();
    Build->bHasPlacementTarget = false;
    Build->bBuildSessionActive = false;
    return true;
}

#endif
