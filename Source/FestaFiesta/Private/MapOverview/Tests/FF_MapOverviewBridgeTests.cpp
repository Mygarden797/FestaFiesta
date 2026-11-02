#if WITH_DEV_AUTOMATION_TESTS

#include "MapOverview/FF_MapOverviewGameBridgeSubsystem.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/FF_FestivalPlayerController.h"
#include "Camera/FF_OverviewCameraPawn.h"
#include "Character/FF_FestivalCharacter.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Misc/AutomationTest.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
    // Uses real possession, controller modes and native camera interpolation. No live world,
    // subsystem registration, project setting or authored asset is changed by this fixture.
    struct FMapBridgeTestScene
    {
        UWorld* World = nullptr;
        AFF_FestivalPlayerController* Controller = nullptr;
        AFF_OverviewCameraPawn* Camera = nullptr;
        AFF_FestivalCharacter* Visitor = nullptr;
        ACameraActor* MapView = nullptr;
        TStrongObjectPtr<UFF_MapOverviewGameBridgeSubsystem> Bridge;

        ~FMapBridgeTestScene()
        {
            Bridge.Reset();
            if (World) World->DestroyWorld(false);
        }

        void SettleCamera() const
        {
            for (int32 Frame = 0; Frame < 180; ++Frame) Camera->Tick(1.0f / 60.0f);
        }

        bool Create(FAutomationTestBase& Test)
        {
            const UWorld::InitializationValues Init = UWorld::InitializationValues()
                .AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
                .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
                .SetTransactional(false);
            World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
                ERHIFeatureLevel::Num, &Init);
            if (!Test.TestNotNull(TEXT("Isolated initialized game world"), World)) return false;
            UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
            AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
            if (!Test.TestNotNull(TEXT("Real placement authority"), Zones)
                || !Test.TestNotNull(TEXT("Camera zone fixture"), Zone)
                || !Test.TestTrue(TEXT("Fixture uses accepted project zone identity"),
                    Zone->ApplyAcceptedZonePreset(FFZSTest::ProjectZoneId()))) return false;
            Zone->BoundarySpline->ClearSplinePoints(false);
            const FVector Corners[] = { FVector(-20000.0, -20000.0, 0.0),
                FVector(20000.0, -20000.0, 0.0), FVector(20000.0, 20000.0, 0.0),
                FVector(-20000.0, 20000.0, 0.0) };
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Corners); ++Index)
            {
                Zone->BoundarySpline->AddSplinePoint(Corners[Index], ESplineCoordinateSpace::Local, false);
                Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
            }
            Zone->BoundarySpline->SetClosedLoop(true, true);
            if (!Test.TestTrue(TEXT("Fixture region registers without changing defaults"), Zones->RegisterZone(Zone))) return false;

            FActorSpawnParameters Spawn;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Controller = World->SpawnActor<AFF_FestivalPlayerController>(Spawn);
            Camera = World->SpawnActor<AFF_OverviewCameraPawn>(Spawn);
            Visitor = World->SpawnActor<AFF_FestivalCharacter>(Spawn);
            MapView = World->SpawnActor<ACameraActor>(Spawn);
            Bridge.Reset(NewObject<UFF_MapOverviewGameBridgeSubsystem>());
            if (!Test.TestNotNull(TEXT("Real festival controller"), Controller)
                || !Test.TestNotNull(TEXT("Real overview camera"), Camera)
                || !Test.TestNotNull(TEXT("Real controllable festival visitor"), Visitor)
                || !Test.TestNotNull(TEXT("Temporary map view target"), MapView)
                || !Test.TestNotNull(TEXT("Standalone typed bridge without registration"), Bridge.Get())) return false;
            // CreateWorld initializes the world, but SpawnActor does not route actor initialization
            // until actors-for-play is enabled. This public lifecycle call creates the controller's
            // real PlayerCameraManager through PostInitializeComponents, so view targets are real.
            World->InitializeActorsForPlay(FURL());
            if (!Test.TestTrue(TEXT("Controller completed actor initialization"), Controller->IsActorInitialized())
                || !Test.TestNotNull(TEXT("Native player camera manager initialized"), Controller->PlayerCameraManager.Get())) return false;
            Camera->CameraSettings.bFollowGroundHeight = false;
            Camera->InitializeOverview(FVector(0.0, 0.0, 100.0), 8000.0f);
            Camera->Tick(1.0f / 60.0f);
            Controller->Possess(Camera);
            Controller->SwitchToOverview(0.0f);
            Controller->SetVisitorCharacter(Visitor);
            Test.AddInfo(FString::Printf(TEXT("Bridge fixture: pawn=%s, target=%s, camera=%s, mode=%d, camera manager=%s"),
                *GetNameSafe(Controller->GetPawn()), *GetNameSafe(Controller->GetViewTarget()), *GetNameSafe(Camera),
                static_cast<int32>(Controller->GetViewMode()), *GetNameSafe(Controller->PlayerCameraManager)));
            return Test.TestTrue(TEXT("Fixture begins with real overview possession"),
                Controller->GetPawn() == Camera && Controller->GetViewTarget() == Camera
                && Controller->GetViewMode() == EFF_CameraViewMode::Overview);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapBridgeOverviewRoundTripTest,
    "FF_MapOverview.Bridge.OverviewRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapBridgeOverviewRoundTripTest::RunTest(const FString& Parameters)
{
    FMapBridgeTestScene Scene;
    if (!Scene.Create(*this)) return false;
    const FFF_OverviewCameraState Initial = Scene.Camera->GetCameraState();
    FVector PlayerMapLocation;
    TestFalse(TEXT("Missing controller cannot supply player map location"), Scene.Bridge->TryGetPlayerMapLocation(nullptr, PlayerMapLocation));
    TestTrue(TEXT("Construction overview supplies controllable player focus"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Construction minimap centers on native overview focus"), PlayerMapLocation.Equals(Initial.FocusLocation, 0.01));
    Scene.MapView->SetActorLocation(FVector(90000.0, 60000.0, 30000.0));
    if (!TestTrue(TEXT("Enter whole-map presentation"), Scene.Bridge->EnterMapView(Scene.Controller, Scene.MapView))) return false;
    TestEqual(TEXT("Whole map owns the view"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.MapView));
    TestEqual(TEXT("Native controller records actor view and blocks overview motion"),
        Scene.Controller->GetViewMode(), EFF_CameraViewMode::Actor);
    TestTrue(TEXT("Overview pawn remains possessed"), Scene.Controller->GetPawn() == Scene.Camera);
    TestTrue(TEXT("Whole-map view still supplies original user location"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Whole-map camera cannot move local minimap to its distant viewpoint"), PlayerMapLocation.Equals(Initial.FocusLocation, 0.01));
    Scene.SettleCamera();
    TestTrue(TEXT("Hidden native camera retains focus"), Scene.Camera->GetCameraState().FocusLocation.Equals(Initial.FocusLocation, 0.01));
    TestEqual(TEXT("Hidden native camera retains zoom"), Scene.Camera->GetCameraState().ArmLength, Initial.ArmLength);
    if (!TestTrue(TEXT("Restore original overview"), Scene.Bridge->ExitMapView(Scene.Controller, Scene.Camera))) return false;
    TestEqual(TEXT("Restore sets overview mode"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Overview);
    TestEqual(TEXT("Restore sets original camera target"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Camera));

    TestTrue(TEXT("Map interaction can focus another location"), Scene.Bridge->FocusMapPoint(Scene.Controller, FVector(2500.0, 1000.0, 100.0)));
    Scene.SettleCamera();
    TestTrue(TEXT("Map interaction updates the user-centered location"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Local minimap follows new controllable overview focus"), PlayerMapLocation.Equals(FVector(2500.0, 1000.0, 100.0), 0.1));
    Scene.Camera->ResetView();
    Scene.SettleCamera();
    TestTrue(TEXT("Whole-map presentation preserves native Home/reset initial focus"),
        Scene.Camera->GetCameraState().FocusLocation.Equals(Initial.FocusLocation, 0.1));
    TestTrue(TEXT("Whole-map presentation preserves native Home/reset initial zoom"),
        FMath::IsNearlyEqual(Scene.Camera->GetCameraState().ArmLength, Initial.ArmLength, 0.1f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapBridgeCharacterRoundTripTest,
    "FF_MapOverview.Bridge.CharacterRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapBridgeCharacterRoundTripTest::RunTest(const FString& Parameters)
{
    FMapBridgeTestScene Scene;
    if (!Scene.Create(*this)
        || !TestTrue(TEXT("Enter real character control"), Scene.Controller->PossessCharacter(Scene.Visitor, 0.0f))) return false;
    const FVector VisitorLocation(-1700.0, 2300.0, 180.0);
    Scene.Visitor->SetActorLocation(VisitorLocation);
    FVector PlayerMapLocation;
    TestTrue(TEXT("Character mode supplies player map position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Character minimap follows actual pawn position"), PlayerMapLocation.Equals(VisitorLocation, 0.01));
    Scene.Visitor->SetShiftLock(true);
    TestFalse(TEXT("Character shift lock hides cursor"), Scene.Controller->bShowMouseCursor);
    if (!TestTrue(TEXT("Enter map from character view"), Scene.Bridge->EnterMapView(Scene.Controller, Scene.MapView))) return false;
    TestTrue(TEXT("Map suspends character possession, not just its camera"),
        Scene.Controller->GetPawn() == Scene.Camera);
    TestTrue(TEXT("Hidden visitor no longer receives controller input"), Scene.Visitor->GetController() == nullptr);
    TestEqual(TEXT("Map view uses actor mode"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Actor);
    TestTrue(TEXT("Map interaction has a visible cursor"), Scene.Controller->bShowMouseCursor);
    TestTrue(TEXT("Whole-map view preserves character map position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Whole-map view keeps local minimap centered on original character"), PlayerMapLocation.Equals(VisitorLocation, 0.01));
    const FVector MovedVisitor = VisitorLocation + FVector(900.0, -500.0, 0.0);
    Scene.Visitor->SetActorLocation(MovedVisitor);
    TestTrue(TEXT("Moving original character updates whole-view local map position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Local map follows character movement during whole-map view"), PlayerMapLocation.Equals(MovedVisitor, 0.01));
    if (!TestTrue(TEXT("Restore character mode"), Scene.Bridge->ExitMapView(Scene.Controller, Scene.Visitor))) return false;
    TestEqual(TEXT("Character mode restored"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Character);
    TestTrue(TEXT("Character possession restored"), Scene.Controller->GetPawn() == Scene.Visitor);
    TestEqual(TEXT("Character target restored"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Visitor));
    TestTrue(TEXT("Visitor has its original controller again"), Scene.Visitor->GetController() == Scene.Controller);
    TestFalse(TEXT("Character shift-lock cursor state restored"), Scene.Controller->bShowMouseCursor);
    TestTrue(TEXT("Restored character continues supplying player map position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Restored character minimap retains actual moved position"), PlayerMapLocation.Equals(MovedVisitor, 0.01));

    TestFalse(TEXT("Invalid map target rejected"), Scene.Bridge->EnterMapView(Scene.Controller, nullptr));
    TestEqual(TEXT("Rejected transition leaves character mode intact"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Character);
    TestTrue(TEXT("Rejected transition leaves character possession intact"), Scene.Controller->GetPawn() == Scene.Visitor);
    TestEqual(TEXT("Rejected transition leaves visible character intact"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Visitor));

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AFF_FestivalCharacter* OtherPlayer = Scene.World->SpawnActor<AFF_FestivalCharacter>(Spawn);
    if (!TestNotNull(TEXT("A different controllable character"), OtherPlayer)) return false;
    const FVector OtherLocation(4500.0, 1100.0, 220.0);
    OtherPlayer->SetActorLocation(OtherLocation);
    if (!TestTrue(TEXT("Possess another character without changing stored visitor"), Scene.Controller->PossessCharacter(OtherPlayer, 0.0f))) return false;
    TestTrue(TEXT("Different possessed character supplies map position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Minimap follows actual controlled pawn instead of default visitor"), PlayerMapLocation.Equals(OtherLocation, 0.01));
    if (!TestTrue(TEXT("Enter whole-map view from different character"), Scene.Bridge->EnterMapView(Scene.Controller, Scene.MapView))) return false;
    TestTrue(TEXT("Whole-map view supplies previous actual controlled pawn"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Whole-map local view preserves different character position"), PlayerMapLocation.Equals(OtherLocation, 0.01));

    ACameraActor* ExternalView = Scene.World->SpawnActor<ACameraActor>(Spawn);
    if (!TestNotNull(TEXT("External camera takeover target"), ExternalView)
        || !TestTrue(TEXT("Another actor takes over whole-map view"), Scene.Controller->SwitchToActorView(ExternalView, 0.0f))) return false;
    TestTrue(TEXT("External takeover still supplies controllable player position"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("A stale map session cannot keep following the previous character"),
        PlayerMapLocation.Equals(Scene.Camera->GetCameraState().FocusLocation, 0.01));
    Scene.Controller->SwitchToOverview(0.0f);
    const FVector ResumedFocus(1200.0, 2700.0, 100.0);
    Scene.Camera->FocusOnLocation(ResumedFocus);
    Scene.SettleCamera();
    TestTrue(TEXT("Native overview return supplies resumed player focus"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Native view switching clears stale character centering behavior"), PlayerMapLocation.Equals(ResumedFocus, 0.1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapBridgeLostTargetFallbackTest,
    "FF_MapOverview.Bridge.LostTargetFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapBridgeLostTargetFallbackTest::RunTest(const FString& Parameters)
{
    FMapBridgeTestScene Scene;
    if (!Scene.Create(*this)) return false;
    ACameraActor* Previous = Scene.World->SpawnActor<ACameraActor>();
    if (!TestNotNull(TEXT("Original actor view target"), Previous)
        || !TestTrue(TEXT("Enter original actor view"), Scene.Controller->SwitchToActorView(Previous, 0.0f))
        || !TestTrue(TEXT("Enter map over original actor"), Scene.Bridge->EnterMapView(Scene.Controller, Scene.MapView))) return false;
    TWeakObjectPtr<AActor> PreviousWeak = Previous;
    Previous->Destroy();
    TestFalse(TEXT("Original view target really despawned"), PreviousWeak.IsValid());
    // The camera helper substitutes the still-possessed overview pawn when its weak target is lost.
    TestTrue(TEXT("Lost actor restores helper's overview fallback"), Scene.Bridge->ExitMapView(Scene.Controller, Scene.Camera));
    TestEqual(TEXT("Lost actor fallback must not leave Actor mode on overview pawn"),
        Scene.Controller->GetViewMode(), EFF_CameraViewMode::Overview);
    TestEqual(TEXT("Lost actor fallback restores overview target"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Camera));

    if (!TestTrue(TEXT("Enter character before second fallback"), Scene.Controller->PossessCharacter(Scene.Visitor, 0.0f))
        || !TestTrue(TEXT("Enter map over character"), Scene.Bridge->EnterMapView(Scene.Controller, Scene.MapView))) return false;
    Scene.Visitor->Destroy();
    TestTrue(TEXT("Lost character restores null-target fallback"), Scene.Bridge->ExitMapView(Scene.Controller, nullptr));
    TestEqual(TEXT("Lost character fallback is controllable overview mode"),
        Scene.Controller->GetViewMode(), EFF_CameraViewMode::Overview);
    TestTrue(TEXT("Lost character fallback keeps overview possession"), Scene.Controller->GetPawn() == Scene.Camera);
    TestEqual(TEXT("Lost character fallback restores overview view"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Camera));
    FVector PlayerMapLocation;
    TestTrue(TEXT("Lost character fallback still supplies user-centered map location"), Scene.Bridge->TryGetPlayerMapLocation(Scene.Controller, PlayerMapLocation));
    TestTrue(TEXT("Lost character map center falls back to controllable overview focus"),
        PlayerMapLocation.Equals(Scene.Camera->GetCameraState().FocusLocation, 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFMapBridgeClickSwitchesToOverviewTest,
    "FF_MapOverview.Bridge.ClickSwitchesToOverview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFMapBridgeClickSwitchesToOverviewTest::RunTest(const FString& Parameters)
{
    FMapBridgeTestScene Scene;
    if (!Scene.Create(*this)) return false;
    const FVector CharacterClick(2400.0, 1200.0, 100.0);
    if (!TestTrue(TEXT("Character view before map click"), Scene.Controller->PossessCharacter(Scene.Visitor, 0.0f))
        || !TestTrue(TEXT("Map click accepted from character view"), Scene.Bridge->FocusMapPoint(Scene.Controller, CharacterClick))) return false;
    Scene.SettleCamera();
    TestEqual(TEXT("Map click makes overview camera visible"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Camera));
    TestTrue(TEXT("Map click restores overview input owner"), Scene.Controller->GetPawn() == Scene.Camera);
    TestEqual(TEXT("Map click switches native mode to overview"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Overview);
    TestTrue(TEXT("Visible native camera actually reaches clicked point"), Scene.Camera->GetCameraState().FocusLocation.Equals(CharacterClick, 0.1));

    const FVector ActorClick(-3100.0, 700.0, 100.0);
    if (!TestTrue(TEXT("Actor view before second map click"), Scene.Controller->SwitchToActorView(Scene.MapView, 0.0f))
        || !TestTrue(TEXT("Map click accepted from actor view"), Scene.Bridge->FocusMapPoint(Scene.Controller, ActorClick))) return false;
    Scene.SettleCamera();
    TestEqual(TEXT("Actor-view map click switches to overview"), Scene.Controller->GetViewMode(), EFF_CameraViewMode::Overview);
    TestEqual(TEXT("Actor-view map click displays the focused camera"), Scene.Controller->GetViewTarget(), static_cast<AActor*>(Scene.Camera));
    TestTrue(TEXT("Actor-view map click reaches requested point"), Scene.Camera->GetCameraState().FocusLocation.Equals(ActorClick, 0.1));
    return true;
}

#endif
