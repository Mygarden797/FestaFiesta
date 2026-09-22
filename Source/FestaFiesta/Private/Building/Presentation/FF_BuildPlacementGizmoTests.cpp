// FF_BuildPlacementGizmoTests.cpp
// 역할: 시설 조작 손잡이의 동작과 저장된 화면 연결을 자동 검증한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/Presentation/FF_BuildPlacementGizmo.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementGizmoMathTest,
	"FestaFiesta.Building.PlacementGizmo.PlaneAndGesture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementGizmoMathTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildGizmoMath;
	FVector Point;
	TestTrue(TEXT("Pointer ray resolves a sloping ground plane"), IntersectGroundPlane(
		FVector(20, 40, 100), FVector(0, 0, -1), FVector::ZeroVector,
		FVector(-0.25, 0, 1).GetSafeNormal(), Point));
	TestTrue(TEXT("Ground slope affects height without changing pointer XY"), Point.Equals(FVector(20, 40, 5), 0.001));
	TestFalse(TEXT("Parallel rays cannot produce unbounded translation"), IntersectGroundPlane(
		FVector(0, 0, 100), FVector(1, 0, 0), FVector::ZeroVector, FVector::UpVector, Point));
	TestFalse(TEXT("Grazing rays cannot produce unstable translation"), IntersectGroundPlane(
		FVector(0, 0, 100), FVector(1, 0, -0.001), FVector::ZeroVector, FVector::UpVector, Point));
	TestFalse(TEXT("An intersection behind the camera cannot start a gesture"), IntersectGroundPlane(
		FVector(0, 0, 100), FVector(0, 0, 1), FVector::ZeroVector, FVector::UpVector, Point));
	TestFalse(TEXT("A zero direction is rejected"), IntersectGroundPlane(
		FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector, Point));

	FGesture Move;
	TestTrue(TEXT("X gesture pins its initial point and plane"), Move.Begin(EFF_BuildGizmoHandle::MoveX,
		FVector(10, 20, 8), FVector::ZeroVector, FVector::UpVector));
	TestFalse(TEXT("A second press cannot replace a running gesture"), Move.Begin(EFF_BuildGizmoHandle::MoveY,
		FVector::ZeroVector, FVector::ZeroVector, FVector::UpVector));
	FFF_BuildGizmoDelta Delta = Move.Update(FVector(36, 500, 99));
	TestTrue(TEXT("X movement returns only world X with no height modification"),
		Delta.bChanged && Delta.WorldTranslation.Equals(FVector(26, 0, 0)));
	Delta = Move.Update(FVector(35, 500, 99));
	TestTrue(TEXT("Output is total displacement from the original point, not the previous sample"),
		Delta.WorldTranslation.Equals(FVector(25, 0, 0)));
	TestFalse(TEXT("Stationary input does not request another validation"), Move.Update(FVector(35, 500, 99)).bChanged);
	Delta = Move.End(false);
	TestTrue(TEXT("Release completes preview manipulation without marking it cancelled"),
		Delta.bConsumed && Delta.bEnded && !Delta.bCancelled && !Move.IsActive());
	TestTrue(TEXT("Release preserves the last complete total delta"), Delta.WorldTranslation.Equals(FVector(25, 0, 0)));
	TestFalse(TEXT("Repeated release cannot consume the next interaction"), Move.End(false).bConsumed);
	TestTrue(TEXT("A new Y gesture can begin after release"), Move.Begin(EFF_BuildGizmoHandle::MoveY,
		FVector(10, 20, 8), FVector::ZeroVector, FVector::UpVector));
	TestTrue(TEXT("Y movement does not leak world X or height"),
		Move.Update(FVector(50, -5, -50)).WorldTranslation.Equals(FVector(0, -25, 0)));
	Delta = Move.End(true);
	TestTrue(TEXT("Cancellation asks the owner to restore its preview snapshot"),
		Delta.bCancelled && Delta.bEnded && !Move.IsActive());

	FGesture Rotate;
	const FVector Pivot(250, -90, 4);
	const auto AtAngle = [&](double Degrees)
	{
		const double Angle = FMath::DegreesToRadians(Degrees);
		return Pivot + FVector(100 * FMath::Cos(Angle), 100 * FMath::Sin(Angle), 0);
	};
	TestFalse(TEXT("Rotation cannot start at its singular center"), Rotate.Begin(EFF_BuildGizmoHandle::RotateYaw,
		Pivot, Pivot, FVector::UpVector));
	TestTrue(TEXT("Rotation starts around the supplied facility center"), Rotate.Begin(EFF_BuildGizmoHandle::RotateYaw,
		AtAngle(350), Pivot, FVector::UpVector));
	Delta = Rotate.Update(AtAngle(10));
	TestTrue(TEXT("Crossing zero degrees uses the short positive angular change"), FMath::IsNearlyEqual(Delta.YawDegrees, 20.0f, 0.001f));
	for (const double Angle : {100.0, 190.0, 280.0, 10.0}) Delta = Rotate.Update(AtAngle(Angle));
	TestTrue(TEXT("Yaw remains continuous across a complete revolution"), FMath::IsNearlyEqual(Delta.YawDegrees, 380.0f, 0.001f));
	TestTrue(TEXT("Rotation never reports translation"), Delta.WorldTranslation.IsNearlyZero());
	TestFalse(TEXT("Passing through the center preserves the last safe angle"), Rotate.Update(Pivot).bChanged);
	TestTrue(TEXT("A cancelled rotation releases its handle"), Rotate.End(true).bCancelled && !Rotate.IsActive());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementGizmoHitTest,
	"FestaFiesta.Building.PlacementGizmo.ProjectedHitTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementGizmoHitTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildGizmoMath;
	const FVector Center(100, -80, 17);
	const FVector Normal = FVector(-0.15, 0.22, 1).GetSafeNormal();
	TArray<FSegment> Geometry;
	BuildGeometry(Center, Normal, 120, Geometry);
	TestEqual(TEXT("Small arrows and ring have a fixed, bounded drawing budget"), Geometry.Num(), 56);
	for (const FSegment& Segment : Geometry)
	{
		TestTrue(TEXT("Both ends lie on the supplied ground plane"),
			FMath::Abs(FVector::DotProduct(Segment.Start - Center, Normal)) < 0.001 &&
			FMath::Abs(FVector::DotProduct(Segment.End - Center, Normal)) < 0.001);
		TestTrue(TEXT("Handle vertices remain within the compact footprint"),
			FVector::Distance(Segment.Start, Center) <= 125.0 && FVector::Distance(Segment.End, Center) <= 125.0);
	}
	for (const FIntPoint Viewport : {FIntPoint(1280, 720), FIntPoint(1920, 1080)})
	{
		// Orthographic projection of the exact render segments: pixel tolerance is independent
		// of viewport size and DPI. Perspective projection is supplied by the real controller.
		const FVector2D ScreenCenter(Viewport.X * 0.5, Viewport.Y * 0.5);
		const auto Project = [&](const FVector& World)
		{
			return ScreenCenter + FVector2D((World.X - Center.X) * 0.6, -(World.Y - Center.Y) * 0.6);
		};
		TArray<FProjectedSegment> Projected;
		for (const FSegment& Segment : Geometry) Projected.Add({Project(Segment.Start), Project(Segment.End), Segment.Handle});
		TestEqual(TEXT("Visible X arrow has an eleven-pixel hit target"),
			HitTest(Projected, Projected[0].End + FVector2D(10, 0)), EFF_BuildGizmoHandle::MoveX);
		TestEqual(TEXT("The end of the Y arrow is selectable"),
			HitTest(Projected, Projected[3].End), EFF_BuildGizmoHandle::MoveY);
		const FProjectedSegment& Ring = Projected[6 + 30];
		TestEqual(TEXT("An unobstructed ring arc selects yaw"),
			HitTest(Projected, (Ring.Start + Ring.End) * 0.5), EFF_BuildGizmoHandle::RotateYaw);
		TestEqual(TEXT("Empty space does not become a viewport-wide capture target"),
			HitTest(Projected, ScreenCenter + FVector2D(200, 200)), EFF_BuildGizmoHandle::None);
		TestEqual(TEXT("Pointer outside the hit slop does not grab the X axis"),
			HitTest(Projected, Projected[0].End + FVector2D(12, 0)), EFF_BuildGizmoHandle::None);
	}
	TestTrue(TEXT("Collapsed projected segments retain a bounded circular endpoint target"),
		FMath::IsNearlyEqual(DistanceToSegmentSquared(FVector2D(3, 4), FVector2D::ZeroVector, FVector2D::ZeroVector), 25.0));
	BuildGeometry(Center, FVector::RightVector, 120, Geometry);
	TestTrue(TEXT("Unsupported vertical surfaces emit neither visual nor clickable handles"), Geometry.IsEmpty());
	return true;
}

#if WITH_EDITOR
#include "Components/LineBatchComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "SceneManagement.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildPlacementGizmoLifecycleTest,
	"FestaFiesta.Building.PlacementGizmo.Cleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildPlacementGizmoLifecycleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::EditorPreview))
	{
		Fixture.ForwardErrorMessages(this);
		return false;
	}
	UWorld* World = Fixture.GetTestWorld();
	AFF_BuildPlacementGizmo* Gizmo = World->SpawnActor<AFF_BuildPlacementGizmo>();
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Transient gizmo"), Gizmo) || !TestNotNull(TEXT("Pointer owner"), Controller)) return false;
	ULineBatchComponent* Lines = Gizmo->FindComponentByClass<ULineBatchComponent>();
	if (!TestNotNull(TEXT("Game renderer owns the lines"), Lines)) return false;
	TestFalse(TEXT("Gizmo never participates in placement collision"), Gizmo->GetActorEnableCollision());
	TestFalse(TEXT("Gizmo has no replicated state"), Gizmo->GetIsReplicated());
	TestFalse(TEXT("Drawing cannot modify navigation"), Lines->CanEverAffectNavigation());
	TestEqual(TEXT("Line geometry has no collision"), Lines->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Gizmo starts hidden until the explicit adjustment mode opens"), Gizmo->IsGizmoVisible());
	const auto SeedGesture = [&]()
	{
		Gizmo->SetPreview(Controller, FTransform::Identity, FIntPoint(200, 300), FVector::ZeroVector, FVector::UpVector);
		Gizmo->SetGizmoVisible(true);
		Gizmo->bPendingCancellation = false;
		Gizmo->Gesture.Begin(EFF_BuildGizmoHandle::MoveX, FVector(20, 0, 0), FVector::ZeroVector, FVector::UpVector);
		Lines->DrawLine(FVector::ZeroVector, FVector(100, 0, 0), FLinearColor::White,
			SDPG_Foreground, 2.0f, 0.0f);
	};
	SeedGesture();
	Gizmo->SetGizmoVisible(false);
	TestFalse(TEXT("Hiding the mode cancels its gesture"), Gizmo->IsInteracting());
	TestTrue(TEXT("Hiding flushes every persistent line"), Lines->BatchedLines.IsEmpty());
	FFF_BuildGizmoDelta Delta = Gizmo->HandlePointer({});
	TestTrue(TEXT("An asynchronous hide delivers one cancellation for snapshot restoration"), Delta.bCancelled && Delta.bEnded);
	TestFalse(TEXT("Cancellation cannot leak into a second pointer frame"), Gizmo->HandlePointer({}).bCancelled);
	SeedGesture();
	FFF_BuildGizmoPointerInput Input;
	Input.bPaused = true;
	Delta = Gizmo->HandlePointer(Input);
	TestTrue(TEXT("Pause releases input ownership and requests preview restoration"), Delta.bCancelled && !Gizmo->IsInteracting());
	TestTrue(TEXT("Pause removes persistent handle geometry"), Lines->BatchedLines.IsEmpty());
	SeedGesture();
	Input = {};
	Input.bCancel = true;
	Delta = Gizmo->HandlePointer(Input);
	TestTrue(TEXT("Escape/right-click cancellation ends the gesture"), Delta.bCancelled && !Gizmo->IsInteracting());
	TestTrue(TEXT("Explicit cancellation flushes the visual"), Lines->BatchedLines.IsEmpty());
	SeedGesture();
	Input = {};
	Input.bBlockedByUi = true;
	Delta = Gizmo->HandlePointer(Input);
	TestTrue(TEXT("UI takes ownership without a drag continuing behind its controls"), Delta.bCancelled && !Gizmo->IsInteracting());
	SeedGesture();
	Gizmo->SetActorHiddenInGame(true);
	TestTrue(TEXT("Engine-level hiding also tears down gesture and lines"), !Gizmo->IsInteracting() && Lines->BatchedLines.IsEmpty());
	SeedGesture();
	Gizmo->SetPreview(nullptr, FTransform::Identity, FIntPoint(200, 300), FVector::ZeroVector, FVector::UpVector);
	TestTrue(TEXT("Losing the controller cannot leave an orphaned gesture"), !Gizmo->IsInteracting() && Lines->BatchedLines.IsEmpty());
	SeedGesture();
	Gizmo->Destroy();
	TestTrue(TEXT("Destroying the transient actor releases any gesture"), !Gizmo->IsInteracting());
	return true;
}
#endif
#endif
