// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/FF_OverviewCameraPawn.h"
#include "CollisionQueryParams.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

// 실제 Pawn과 충돌 바닥을 함께 검사한다. 리그의 수학 테스트만으로는 입력 종료 후
// Pawn Tick이 다시 지면을 추종하거나 오래된 목표를 되살리는 회귀를 발견할 수 없다.
namespace FFOverviewCameraMotionTests
{
	struct FTestScene
	{
		UWorld* World = nullptr;
		AFF_OverviewCameraPawn* Camera = nullptr;
		AActor* Floor = nullptr;

		explicit FTestScene(const bool bFollowGround = true)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Values);
			if (!World)
			{
				return;
			}

			Floor = World->SpawnActor<AActor>();
			if (!Floor)
			{
				return;
			}
			UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
			Floor->SetRootComponent(Box);
			Floor->AddInstanceComponent(Box);
			Box->SetBoxExtent(FVector(50000.0, 50000.0, 10.0));
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Ignore);
			Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			Box->SetCanEverAffectNavigation(false);
			Box->RegisterComponent();
			SetFloorHeight(0.0);

			Camera = World->SpawnActor<AFF_OverviewCameraPawn>();
			if (!Camera)
			{
				return;
			}
			FFF_OverviewCameraSettings& Settings = Camera->CameraSettings;
			Settings.bUseZoneBounds = false;
			Settings.bConstrainFocusToZones = false;
			Settings.bFollowGroundHeight = bFollowGround;
			Settings.GroundSampleCount = 0;
			Settings.GroundFocusOffset = 100.f;
			Settings.GroundFollowSpeed = 6.f;
			Settings.GroundMaxClimbSpeed = 700.f;
			Settings.MinArmLength = 800.f;
			Settings.MaxArmLength = 16000.f;
			Settings.MinCameraHeightAboveFocus = 0.f;
			Settings.InterpSpeed = 12.f;
			Settings.ZoomInterpSpeed = 22.f;
			Settings.ZoomToCursorStrength = 1.f;
			Camera->InitializeOverview(FVector(0.0, 0.0, 100.0), 8000.f);
			Camera->Tick(1.f / 60.f);
		}

		~FTestScene()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		void SetFloorHeight(const double Height) const
		{
			Floor->SetActorLocation(FVector(0.0, 0.0, Height - 10.0));
		}

		bool HasFloorAt(const double Height) const
		{
			FHitResult Hit;
			return World->LineTraceSingleByChannel(Hit, FVector(0.0, 0.0, 10000.0),
				FVector(0.0, 0.0, -10000.0), ECC_Visibility, FCollisionQueryParams())
				&& FMath::IsNearlyEqual(Hit.ImpactPoint.Z, Height, 0.01);
		}

		AActor* SpawnFollowTarget() const
		{
			AActor* Target = World->SpawnActor<AActor>();
			if (Target)
			{
				USceneComponent* Root = NewObject<USceneComponent>(Target);
				Target->SetRootComponent(Root);
				Target->AddInstanceComponent(Root);
				Root->RegisterComponent();
			}
			return Target;
		}
	};

	bool Settle(AFF_OverviewCameraPawn& Camera, const float DeltaSeconds = 1.f / 60.f)
	{
		// 유한 시간 안에 완료해야 한다. 지면을 계속 쫓으면 이 제한에 걸린다.
		const int32 MaxFrames = FMath::CeilToInt(8.f / DeltaSeconds);
		for (int32 Frame = 0; Frame < MaxFrames; ++Frame)
		{
			Camera.Tick(DeltaSeconds);
			if (Camera.GetMotionState() == EFF_OverviewCameraMotionState::Idle)
			{
				return true;
			}
		}
		return false;
	}

	bool IsExactlyEqual(const FFF_OverviewCameraState& A, const FFF_OverviewCameraState& B)
	{
		return A.FocusLocation == B.FocusLocation && A.ArmLength == B.ArmLength
			&& A.Yaw == B.Yaw && A.Pitch == B.Pitch;
	}

	bool RemainsFrozen(AFF_OverviewCameraPawn& Camera, const int32 Frames = 120,
		const float DeltaSeconds = 1.f / 60.f)
	{
		const FFF_OverviewCameraState Before = Camera.GetCameraState();
		const FVector ActorLocation = Camera.GetActorLocation();
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Camera.Tick(DeltaSeconds);
			if (Camera.GetMotionState() != EFF_OverviewCameraMotionState::Idle
				|| !IsExactlyEqual(Camera.GetCameraState(), Before)
				|| Camera.GetActorLocation() != ActorLocation)
			{
				return false;
			}
		}
		return true;
	}
}

using namespace FFOverviewCameraMotionTests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraIdleGroundTest,
	"FestaFiesta.Camera.Motion.IdleIgnoresGroundChanges",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraIdleGroundTest::RunTest(const FString& Parameters)
{
	FTestScene Scene;
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	TestTrue(TEXT("The query-only floor is visible to ground traces"), Scene.HasFloorAt(0.0));
	TestTrue(TEXT("Initialization finishes in idle"), Settle(*Scene.Camera));
	const FFF_OverviewCameraState Before = Scene.Camera->GetCameraState();
	Scene.SetFloorHeight(600.0);
	TestTrue(TEXT("The physical floor moved; this is not a missing-collision test"), Scene.HasFloorAt(600.0));
	TestTrue(TEXT("Idle preserves every camera value exactly after ground changes"), RemainsFrozen(*Scene.Camera));
	Scene.Camera->AddPanInput(FVector2D(0.0, 1.0));
	Scene.Camera->Tick(1.f / 60.f);
	TestTrue(TEXT("Manual input resumes ground correction"),
		Scene.Camera->GetCameraState().FocusLocation.Z > Before.FocusLocation.Z);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraDirectReleaseTest,
	"FestaFiesta.Camera.Motion.DirectInputReleaseDiscardsTargets",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraDirectReleaseTest::RunTest(const FString& Parameters)
{
	FTestScene Scene(false);
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	AFF_OverviewCameraPawn& Camera = *Scene.Camera;
	for (int32 Frame = 0; Frame < 15; ++Frame)
	{
		Camera.AddPanInput(FVector2D(0.0, 1.0));
		Camera.Tick(1.f / 60.f);
	}
	const FFF_OverviewCameraState ReleasedPan = Camera.GetCameraState();
	TestTrue(TEXT("Held pan moves forward"), ReleasedPan.FocusLocation.X > 0.0);
	TestTrue(TEXT("Held input has an explicit state"),
		Camera.GetMotionState() == EFF_OverviewCameraMotionState::DirectInput);
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Pan release stops at the displayed position"), IsExactlyEqual(Camera.GetCameraState(), ReleasedPan));
	TestTrue(TEXT("Released pan has no idle drift"), RemainsFrozen(Camera));
	Camera.AddPanInput(FVector2D(0.0, -1.0));
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Opposite input does not resume the old forward target"),
		Camera.GetCameraState().FocusLocation.X < ReleasedPan.FocusLocation.X);
	Camera.Tick(1.f / 60.f);
	for (int32 Frame = 0; Frame < 15; ++Frame)
	{
		Camera.AddRotateInput(1.f);
		Camera.Tick(1.f / 60.f);
	}
	const FFF_OverviewCameraState ReleasedRotation = Camera.GetCameraState();
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Rotation release stops exactly"), IsExactlyEqual(Camera.GetCameraState(), ReleasedRotation));
	Camera.AddRotateInput(-1.f);
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Reverse rotation starts from the stopped view"),
		Camera.GetCameraState().Yaw < ReleasedRotation.Yaw);
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Rotation also remains frozen after release"), RemainsFrozen(Camera));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraZoomCompletionTest,
	"FestaFiesta.Camera.Motion.ZoomPitchAndAnchorFinishAtAllFrameRates",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraZoomCompletionTest::RunTest(const FString& Parameters)
{
	for (const int32 FrameRate : { 30, 60, 144 })
	{
		FTestScene Scene(false);
		if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
		AFF_OverviewCameraPawn& Camera = *Scene.Camera;
		const float DeltaSeconds = 1.f / FrameRate;
		const FFF_OverviewCameraState Before = Camera.GetCameraState();
		const FVector Anchor(1800.0, 900.0, Before.FocusLocation.Z);
		Camera.AddZoomInputAtAnchor(3.f, Anchor);
		TestTrue(*FString::Printf(TEXT("%d FPS zoom completes in finite time"), FrameRate), Settle(Camera, DeltaSeconds));
		const FFF_OverviewCameraState After = Camera.GetCameraState();
		const float ExpectedArm = Before.ArmLength / FMath::Pow(Camera.CameraSettings.ZoomStepMultiplier, 3.f);
		TestEqual(TEXT("All wheel steps reach their intended zoom"), After.ArmLength, ExpectedArm, 0.01f);
		TestTrue(TEXT("Zooming in retains the lower viewing angle"), After.Pitch > Before.Pitch);
		TestTrue(TEXT("Cursor zoom moves focus toward the chosen point"),
			FVector::DistSquared(After.FocusLocation, Anchor) < FVector::DistSquared(Before.FocusLocation, Anchor));
		const FVector ExpectedFocus = FMath::Lerp(Anchor, Before.FocusLocation, ExpectedArm / Before.ArmLength);
		TestTrue(TEXT("Cursor strength one preserves the existing anchor destination"), After.FocusLocation.Equals(ExpectedFocus, 0.01));
		TestTrue(TEXT("Completed zoom freezes position, pitch, yaw and arm exactly"),
			RemainsFrozen(Camera, FrameRate * 2, DeltaSeconds));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraTransitionGroundTest,
	"FestaFiesta.Camera.Motion.TransitionCachesGroundUntilNextCommand",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraTransitionGroundTest::RunTest(const FString& Parameters)
{
	FTestScene Scene;
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	AFF_OverviewCameraPawn& Camera = *Scene.Camera;
	Scene.SetFloorHeight(300.0);
	TestTrue(TEXT("New command sees a raised physical floor"), Scene.HasFloorAt(300.0));
	Camera.AddZoomInput(2.f);
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("The command starts a transition"),
		Camera.GetMotionState() == EFF_OverviewCameraMotionState::Transition);
	Scene.SetFloorHeight(900.0);
	TestTrue(TEXT("Floor changes again while the transition is active"), Scene.HasFloorAt(900.0));
	TestTrue(TEXT("The transition completes without chasing the changed collider"), Settle(Camera));
	TestEqual(TEXT("Only the floor sampled for the command is applied"), Camera.GetCameraState().FocusLocation.Z, 400.0, 0.11);
	TestTrue(TEXT("The completed view remains fixed above its cached floor"), RemainsFrozen(Camera));
	Camera.AddZoomInput(-1.f);
	TestTrue(TEXT("The next command completes normally"), Settle(Camera));
	TestEqual(TEXT("A new command refreshes ground height"), Camera.GetCameraState().FocusLocation.Z, 1000.0, 0.11);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraStopTest,
	"FestaFiesta.Camera.Motion.StopClearsTransitionAndQueuedInput",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraStopTest::RunTest(const FString& Parameters)
{
	FTestScene Scene(false);
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	AFF_OverviewCameraPawn& Camera = *Scene.Camera;
	Camera.AddZoomInputAtAnchor(5.f, FVector(1500.0, 1000.0, 100.0));
	Camera.Tick(1.f / 60.f);
	const FFF_OverviewCameraState Stopped = Camera.GetCameraState();
	Camera.AddPanInput(FVector2D(1.0, 1.0));
	Camera.AddRotateInput(1.f);
	Camera.StopCameraMotion();
	TestTrue(TEXT("Stopping does not snap forward to the unfinished zoom destination"), IsExactlyEqual(Camera.GetCameraState(), Stopped));
	TestTrue(TEXT("Stopping clears queued input as well as interpolation"), RemainsFrozen(Camera));
	Camera.AddZoomInput(-1.f);
	TestTrue(TEXT("A new command can start after a stop"), Settle(Camera));
	TestEqual(TEXT("New zoom starts from the stopped arm length"), Camera.GetCameraState().ArmLength,
		Stopped.ArmLength * Camera.CameraSettings.ZoomStepMultiplier, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraDiscreteCommandsTest,
	"FestaFiesta.Camera.Motion.FocusSnapResetAndZoomDuringPan",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraDiscreteCommandsTest::RunTest(const FString& Parameters)
{
	FTestScene Scene(false);
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	AFF_OverviewCameraPawn& Camera = *Scene.Camera;
	const FFF_OverviewCameraState Initial = Camera.GetCameraState();
	const FVector Focus(2500.0, -800.0, Initial.FocusLocation.Z);
	Camera.FocusOnLocation(Focus);
	TestTrue(TEXT("A focus command completes"), Settle(Camera));
	TestTrue(TEXT("Focus reaches the requested position"), Camera.GetCameraState().FocusLocation.Equals(Focus, 0.01));
	TestTrue(TEXT("Focus completion freezes"), RemainsFrozen(Camera));
	Camera.SnapRotate(1);
	TestTrue(TEXT("Snap rotation completes"), Settle(Camera));
	TestEqual(TEXT("Snap reaches its requested angle"), Camera.GetCameraState().Yaw, Camera.CameraSettings.YawSnapDegrees, 0.01f);
	TestTrue(TEXT("Snap completion freezes"), RemainsFrozen(Camera));
	const float BeforeZoom = Camera.GetCameraState().ArmLength;
	Camera.AddPanInput(FVector2D(0.0, 1.0));
	Camera.AddZoomInput(2.f);
	Camera.Tick(1.f / 60.f);
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		Camera.AddPanInput(FVector2D(0.0, 1.0));
		Camera.Tick(1.f / 60.f);
	}
	const FVector ReleasedFocusDuringZoom = Camera.GetCameraState().FocusLocation;
	TestTrue(TEXT("A wheel command survives release of simultaneously held pan"), Settle(Camera));
	TestTrue(TEXT("Pan release freezes focus while the independent zoom finishes"),
		Camera.GetCameraState().FocusLocation == ReleasedFocusDuringZoom);
	TestEqual(TEXT("Combined pan does not truncate the requested zoom"), Camera.GetCameraState().ArmLength,
		BeforeZoom / FMath::Pow(Camera.CameraSettings.ZoomStepMultiplier, 2.f), 0.01f);
	Camera.ResetView();
	TestTrue(TEXT("Reset completes"), Settle(Camera));
	TestTrue(TEXT("Reset restores the complete initial view"), IsExactlyEqual(Camera.GetCameraState(), Initial));
	TestTrue(TEXT("Reset does not leave residual movement"), RemainsFrozen(Camera));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraFollowTest,
	"FestaFiesta.Camera.Motion.FollowRequiresOptInAndManualInputExits",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraFollowTest::RunTest(const FString& Parameters)
{
	FTestScene Scene(false);
	if (!TestNotNull(TEXT("Camera is created"), Scene.Camera)) return false;
	AFF_OverviewCameraPawn& Camera = *Scene.Camera;
	AActor* Target = Scene.SpawnFollowTarget();
	if (!TestNotNull(TEXT("Follow target is created"), Target)) return false;
	Target->SetActorLocation(FVector(1500.0, 0.0, 100.0));
	TestTrue(TEXT("Moving an unselected actor cannot move the idle camera"), RemainsFrozen(Camera));
	Camera.SetFollowTarget(Target);
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Explicit follow selects its own motion state"),
		Camera.GetMotionState() == EFF_OverviewCameraMotionState::Following);
	const double FirstX = Camera.GetCameraState().FocusLocation.X;
	Target->SetActorLocation(FVector(3000.0, 0.0, 100.0));
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("An explicitly followed actor moves the camera without manual input"),
		Camera.GetCameraState().FocusLocation.X > FirstX);
	Camera.AddRotateInput(1.f);
	Camera.Tick(1.f / 60.f);
	TestNull(TEXT("Manual rotation releases follow"), Camera.GetFollowTarget());
	Camera.Tick(1.f / 60.f);
	TestTrue(TEXT("Released manual input returns to a frozen view"), RemainsFrozen(Camera));
	Camera.SetFollowTarget(Target);
	Camera.Tick(1.f / 60.f);
	Camera.AddZoomInput(1.f);
	TestNull(TEXT("Even center zoom releases follow"), Camera.GetFollowTarget());
	TestTrue(TEXT("Zoom completes independently of the former target"), Settle(Camera));
	Camera.AddZoomInput(1000.f);
	TestTrue(TEXT("The minimum zoom is reached"), Settle(Camera));
	Camera.SetFollowTarget(Target);
	Camera.Tick(1.f / 60.f);
	Camera.AddZoomInput(1.f);
	TestNull(TEXT("A wheel gesture at the zoom limit still releases follow"), Camera.GetFollowTarget());
	Camera.SetFollowTarget(Target);
	Camera.Tick(1.f / 60.f);
	Camera.StopCameraMotion();
	TestNull(TEXT("Explicit stop releases follow"), Camera.GetFollowTarget());
	Target->SetActorLocation(FVector(-3000.0, 0.0, 100.0));
	TestTrue(TEXT("Stopped follow cannot resume because the actor moved"), RemainsFrozen(Camera));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
