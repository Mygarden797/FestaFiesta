// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/FF_OverviewCameraPawn.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "FF_FFZSWorldSubsystem.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

// 실제 Pawn Tick과 Zone 폴리곤을 함께 검사한다. 리그 수학만 검사하거나
// bConstrainFocusToZones를 테스트에서 꺼 버리면 떨어진 Zone 사이에서 멈추는 회귀를 놓친다.
namespace FFOverviewCameraZoneTests
{
	constexpr double ZoneHalfSize = 1000.0;
	constexpr double SecondZoneX = 6000.0;
	constexpr double FocusZ = 100.0;
	constexpr double PositionTolerance = 0.1;

	struct FZoneTestScene
	{
		UWorld* World = nullptr;
		AFF_OverviewCameraPawn* Camera = nullptr;
		AFFZSGameplayZone* FirstZone = nullptr;
		AFFZSGameplayZone* SecondZone = nullptr;

		~FZoneTestScene()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
		}

		AFFZSGameplayZone* AddZone(FAutomationTestBase& Test, UFFZSWorldSubsystem& Zones,
			const FName ZoneId, const double CenterX)
		{
			AFFZSGameplayZone* Zone = World->SpawnActor<AFFZSGameplayZone>();
			if (!Test.TestNotNull(TEXT("Fixture zone actor exists"), Zone)
				|| !Test.TestTrue(*FString::Printf(TEXT("Fixture accepts project preset %s"), *ZoneId.ToString()),
					Zone->ApplyAcceptedZonePreset(ZoneId))
				|| !Test.TestNotNull(TEXT("Fixture zone has a boundary spline"), Zone->BoundarySpline.Get()))
			{
				return nullptr;
			}
			Zone->SetActorLocation(FVector(CenterX, 0.0, 0.0));
			Zone->BoundarySpline->ClearSplinePoints(false);
			for (const FVector Corner : { FVector(-ZoneHalfSize, -ZoneHalfSize, 0.0),
				FVector(ZoneHalfSize, -ZoneHalfSize, 0.0), FVector(ZoneHalfSize, ZoneHalfSize, 0.0),
				FVector(-ZoneHalfSize, ZoneHalfSize, 0.0) })
			{
				const int32 Index = Zone->BoundarySpline->GetNumberOfSplinePoints();
				Zone->BoundarySpline->AddSplinePoint(Corner, ESplineCoordinateSpace::Local, false);
				Zone->BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
			}
			Zone->BoundarySpline->SetClosedLoop(true, true);
			TArray<FText> Errors;
			if (!Zone->ValidateZoneData(Errors))
			{
				for (const FText& Error : Errors)
				{
					Test.AddError(FString::Printf(TEXT("Camera zone fixture %s: %s"),
						*ZoneId.ToString(), *Error.ToString()));
				}
				return nullptr;
			}
			return Test.TestTrue(TEXT("Real zone subsystem registers the fixture"), Zones.RegisterZone(Zone))
				? Zone : nullptr;
		}

		bool Create(FAutomationTestBase& Test, const bool bFarZoom, const float DeltaSeconds,
			const bool bStrictContainment = false)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(true)
				.CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false)
				.SetTransactional(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr,
				true, ERHIFeatureLevel::Num, &Values);
			if (!Test.TestNotNull(TEXT("Camera zone test world exists"), World)) return false;
			UFFZSWorldSubsystem* Zones = World->GetSubsystem<UFFZSWorldSubsystem>();
			if (!Test.TestNotNull(TEXT("Real zone subsystem exists"), Zones)) return false;
			Zones->SetChangePhase(EFFZSChangePhase::InitialSetup);
			FirstZone = AddZone(Test, *Zones, FFZSTest::ProjectZoneId(), 0.0);
			SecondZone = AddZone(Test, *Zones, FFZSTest::SecondaryProjectZoneId(), SecondZoneX);
			if (!FirstZone || !SecondZone) return false;
			const FVector GapCenter(SecondZoneX * 0.5, 0.0, FocusZ);
			if (!Test.TestTrue(TEXT("A real unzoned gap separates the two fixture zones"),
				!FirstZone->ContainsWorldPoint(GapCenter) && !SecondZone->ContainsWorldPoint(GapCenter))) return false;

			Camera = World->SpawnActor<AFF_OverviewCameraPawn>();
			if (!Test.TestNotNull(TEXT("Real overview camera pawn exists"), Camera)) return false;
			// 지면 추종은 별도 Motion 테스트가 검사한다. 수평 이동의 기본 제한/보간/속도는 유지한다.
			Camera->CameraSettings.bFollowGroundHeight = false;
			Test.TestTrue(TEXT("Default camera still derives its outer bounds from zones"), Camera->CameraSettings.bUseZoneBounds);
			Test.TestFalse(TEXT("Strict polygon containment must be opt-in by default"), Camera->CameraSettings.bConstrainFocusToZones);
			Test.TestEqual(TEXT("Default outer bounds retain their authored viewing margin"), Camera->CameraSettings.ZoneBoundsPadding, 3000.f);
			if (bStrictContainment)
			{
				Camera->CameraSettings.bConstrainFocusToZones = true;
			}
			const float ArmLength = bFarZoom ? Camera->CameraSettings.MaxArmLength : Camera->CameraSettings.MinArmLength;
			Camera->InitializeOverview(FVector(0.0, 0.0, FocusZ), ArmLength);
			Camera->Tick(DeltaSeconds);
			const FBox& Bounds = Camera->CameraSettings.FocusBounds;
			return Test.TestTrue(TEXT("Pawn derives the combined padded bounds from both actual zones"),
				Bounds.IsValid && Bounds.Min.X == -4000.0 && Bounds.Max.X == 10000.0
				&& Bounds.Min.Y == -4000.0 && Bounds.Max.Y == 4000.0)
				&& Test.TestTrue(TEXT("First placement preserves an already valid initial focus"),
					Camera->GetCameraState().FocusLocation == FVector(0.0, 0.0, FocusZ));
		}
	};

	bool IsExactlyEqual(const FFF_OverviewCameraState& A, const FFF_OverviewCameraState& B)
	{
		return A.FocusLocation == B.FocusLocation && A.ArmLength == B.ArmLength
			&& A.Yaw == B.Yaw && A.Pitch == B.Pitch;
	}

	bool ReleaseAndRemainIdle(AFF_OverviewCameraPawn& Camera, const float DeltaSeconds)
	{
		const FFF_OverviewCameraState ReleasedState = Camera.GetCameraState();
		const FVector ReleasedActorLocation = Camera.GetActorLocation();
		for (int32 Frame = 0; Frame < FMath::CeilToInt(2.f / DeltaSeconds); ++Frame)
		{
			Camera.Tick(DeltaSeconds);
			if (Camera.GetMotionState() != EFF_OverviewCameraMotionState::Idle
				|| !IsExactlyEqual(Camera.GetCameraState(), ReleasedState)
				|| Camera.GetActorLocation() != ReleasedActorLocation)
			{
				return false;
			}
		}
		return true;
	}

	void ApplyInput(AFF_OverviewCameraPawn& Camera, const FVector2D& Input,
		const bool bEdgeScroll, const float DeltaSeconds)
	{
		if (bEdgeScroll) Camera.AddEdgeScrollInput(Input);
		else Camera.AddPanInput(Input);
		Camera.Tick(DeltaSeconds);
	}

	bool IsWithinOuterBounds(const AFF_OverviewCameraPawn& Camera)
	{
		const FVector Focus = Camera.GetCameraState().FocusLocation;
		const FBox& Bounds = Camera.CameraSettings.FocusBounds;
		return Bounds.IsValid && Focus.X >= Bounds.Min.X - PositionTolerance && Focus.X <= Bounds.Max.X + PositionTolerance
			&& Focus.Y >= Bounds.Min.Y - PositionTolerance && Focus.Y <= Bounds.Max.Y + PositionTolerance;
	}

	bool RunCrossing(FAutomationTestBase& Test, const bool bEdgeScroll)
	{
		for (const int32 FrameRate : { 30, 60, 144 })
		{
			for (const bool bFarZoom : { false, true })
			{
				const FString Context = FString::Printf(TEXT("%s %d FPS %s arm"),
					bEdgeScroll ? TEXT("Edge") : TEXT("Keyboard"), FrameRate, bFarZoom ? TEXT("far") : TEXT("near"));
				Test.AddInfo(Context);
				const float DeltaSeconds = 1.f / FrameRate;
				FZoneTestScene Scene;
				if (!Scene.Create(Test, bFarZoom, DeltaSeconds)) return false;
				AFF_OverviewCameraPawn& Camera = *Scene.Camera;
				Test.TestTrue(*FString::Printf(TEXT("%s: initial no-input view stays fixed"), *Context), ReleaseAndRemainIdle(Camera, DeltaSeconds));
				for (const bool bForward : { true, false })
				{
					const double DestinationX = bForward ? SecondZoneX : 0.0;
					const FVector2D Input(0.0, bForward ? 1.0 : -1.0);
					bool bCrossedGap = false;
					bool bStayedWithinBounds = true;
					bool bReachedDestination = false;
					for (int32 Frame = 0; Frame < FrameRate * 20; ++Frame)
					{
						ApplyInput(Camera, Input, bEdgeScroll, DeltaSeconds);
						const FVector Focus = Camera.GetCameraState().FocusLocation;
						bCrossedGap |= Focus.X > ZoneHalfSize + PositionTolerance && Focus.X < SecondZoneX - ZoneHalfSize - PositionTolerance
							&& !Scene.FirstZone->ContainsWorldPoint(Focus) && !Scene.SecondZone->ContainsWorldPoint(Focus);
						bStayedWithinBounds &= IsWithinOuterBounds(Camera);
						bReachedDestination = bForward ? Focus.X >= DestinationX : Focus.X <= DestinationX;
						if (bReachedDestination) break;
					}
					const FString Leg = FString::Printf(TEXT("%s %s"), *Context, bForward ? TEXT("A to B") : TEXT("B to A"));
					Test.TestTrue(*FString::Printf(TEXT("%s: repeated input actually traverses the unzoned gap"), *Leg), bCrossedGap);
					Test.TestTrue(*FString::Printf(TEXT("%s: arrives inside the other zone within bounded time"), *Leg),
						bReachedDestination && (bForward ? Scene.SecondZone : Scene.FirstZone)->ContainsWorldPoint(Camera.GetCameraState().FocusLocation));
					Test.TestTrue(*FString::Printf(TEXT("%s: all intermediate positions retain outer bounds"), *Leg), bStayedWithinBounds);
					Test.TestTrue(*FString::Printf(TEXT("%s: release immediately freezes the completed view"), *Leg), ReleaseAndRemainIdle(Camera, DeltaSeconds));
				}
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraZoneKeyboardTest,
	"FestaFiesta.Camera.Zones.KeyboardCrossesDisconnectedZones",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraZoneKeyboardTest::RunTest(const FString& Parameters)
{
	return FFOverviewCameraZoneTests::RunCrossing(*this, false);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraZoneEdgeScrollTest,
	"FestaFiesta.Camera.Zones.EdgeScrollCrossesDisconnectedZones",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraZoneEdgeScrollTest::RunTest(const FString& Parameters)
{
	return FFOverviewCameraZoneTests::RunCrossing(*this, true);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraZoneOuterBoundsTest,
	"FestaFiesta.Camera.Zones.CombinedOuterBoundsAndIdle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraZoneOuterBoundsTest::RunTest(const FString& Parameters)
{
	using namespace FFOverviewCameraZoneTests;
	for (const int32 FrameRate : { 30, 60, 144 })
	{
		for (const bool bFarZoom : { false, true })
		{
			for (const bool bEdgeScroll : { false, true })
			{
				const FString Context = FString::Printf(TEXT("%d FPS %s arm %s"), FrameRate,
					bFarZoom ? TEXT("far") : TEXT("near"), bEdgeScroll ? TEXT("edge") : TEXT("keyboard"));
				AddInfo(Context);
				const float DeltaSeconds = 1.f / FrameRate;
				FZoneTestScene Scene;
				if (!Scene.Create(*this, bFarZoom, DeltaSeconds)) return false;
				AFF_OverviewCameraPawn& Camera = *Scene.Camera;
				for (const FVector2D Input : { FVector2D(0.0, 1.0), FVector2D(0.0, -1.0), FVector2D(1.0, 0.0), FVector2D(-1.0, 0.0) })
				{
					const bool bXAxis = Input.Y != 0.0;
					const bool bPositive = bXAxis ? Input.Y > 0.0 : Input.X > 0.0;
					const FBox& Bounds = Camera.CameraSettings.FocusBounds;
					const double Boundary = bXAxis ? (bPositive ? Bounds.Max.X : Bounds.Min.X) : (bPositive ? Bounds.Max.Y : Bounds.Min.Y);
					bool bStayedWithinBounds = true;
					bool bReachedBoundary = false;
					for (int32 Frame = 0; Frame < FrameRate * 30; ++Frame)
					{
						ApplyInput(Camera, Input, bEdgeScroll, DeltaSeconds);
						bStayedWithinBounds &= IsWithinOuterBounds(Camera);
						const FVector Focus = Camera.GetCameraState().FocusLocation;
						bReachedBoundary = FMath::IsNearlyEqual(bXAxis ? Focus.X : Focus.Y, Boundary, PositionTolerance);
						if (bReachedBoundary) break;
					}
					// 경계에 도착해도 계속 누른다. 한 번 경계에 닿는 것과 바깥으로 못 나가는 것은 다르다.
					for (int32 Frame = 0; Frame < FrameRate; ++Frame)
					{
						ApplyInput(Camera, Input, bEdgeScroll, DeltaSeconds);
						bStayedWithinBounds &= IsWithinOuterBounds(Camera);
					}
					const FString Leg = FString::Printf(TEXT("%s %s%s"), *Context, bPositive ? TEXT("+") : TEXT("-"), bXAxis ? TEXT("X") : TEXT("Y"));
					TestTrue(*FString::Printf(TEXT("%s: reaches the combined padded boundary"), *Leg), bReachedBoundary);
					TestTrue(*FString::Printf(TEXT("%s: sustained input cannot escape the outer bounds"), *Leg), bStayedWithinBounds);
					TestTrue(*FString::Printf(TEXT("%s: releasing at the boundary returns to an exact idle"), *Leg), ReleaseAndRemainIdle(Camera, DeltaSeconds));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraZoneStrictOptInTest,
	"FestaFiesta.Camera.Zones.StrictZoneContainmentIsOptIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_OverviewCameraZoneStrictOptInTest::RunTest(const FString& Parameters)
{
	using namespace FFOverviewCameraZoneTests;
	for (const int32 FrameRate : { 30, 60, 144 })
	{
		for (const bool bFarZoom : { false, true })
		{
			for (const bool bEdgeScroll : { false, true })
			{
				const FString Context = FString::Printf(TEXT("Strict %d FPS %s arm %s"), FrameRate,
					bFarZoom ? TEXT("far") : TEXT("near"), bEdgeScroll ? TEXT("edge") : TEXT("keyboard"));
				AddInfo(Context);
				const float DeltaSeconds = 1.f / FrameRate;
				FZoneTestScene Scene;
				if (!Scene.Create(*this, bFarZoom, DeltaSeconds, true)) return false;
				AFF_OverviewCameraPawn& Camera = *Scene.Camera;
				for (const bool bForward : { true, false })
				{
					// 역방향 strict 검사는 두 번째 Zone 안에서 시작한다. 이동 검사는 위의 왕복 테스트가 담당한다.
					if (!bForward)
					{
						Camera.InitializeOverview(FVector(SecondZoneX, 0.0, FocusZ), Camera.GetCameraState().ArmLength);
						Camera.Tick(DeltaSeconds);
					}
					const AFFZSGameplayZone& StartingZone = *(bForward ? Scene.FirstZone : Scene.SecondZone);
					const double StartingX = Camera.GetCameraState().FocusLocation.X;
					bool bStayedInStartingZone = true;
					for (int32 Frame = 0; Frame < FrameRate * 8; ++Frame)
					{
						ApplyInput(Camera, FVector2D(0.0, bForward ? 1.0 : -1.0), bEdgeScroll, DeltaSeconds);
						bStayedInStartingZone &= StartingZone.ContainsWorldPoint(Camera.GetCameraState().FocusLocation);
					}
					const FString Leg = FString::Printf(TEXT("%s %s"), *Context, bForward ? TEXT("forward") : TEXT("reverse"));
					TestTrue(*FString::Printf(TEXT("%s: opt-in containment keeps every frame inside its starting polygon"), *Leg), bStayedInStartingZone);
					TestTrue(*FString::Printf(TEXT("%s: strict mode still permits movement within the polygon"), *Leg),
						FMath::Abs(Camera.GetCameraState().FocusLocation.X - StartingX) > ZoneHalfSize * 0.5);
					TestTrue(*FString::Printf(TEXT("%s: release remains idle after polygon blocking"), *Leg), ReleaseAndRemainIdle(Camera, DeltaSeconds));
				}
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
