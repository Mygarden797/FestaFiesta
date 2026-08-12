// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/FF_OverviewCameraRig.h"

// 카메라 버그는 "왜 확대하면 화면이 홱 날아가지", "왜 돌린 뒤에 위로 밀면 옆으로 가지"처럼
// 수치 문제가 대부분이다. 그건 화면을 보면서 잡는 것보다 값을 찍어보는 게 빠르다.
//
// LWC 주의: UE5 에서 FVector 와 FVector2D 의 성분은 double 이다.
// 그래서 .X / .Y / .Z 를 TestEqual 로 비교할 때 기대값과 허용오차에 f 를 붙이면
// float 과 double 오버로드가 모두 후보가 되어 C2666 으로 컴파일이 막힌다.
//   안 됨: TestEqual(TEXT("..."), Location.X, 5000.f, 0.01f);
//   됨   : TestEqual(TEXT("..."), Location.X, 5000.0, 0.01);
// 반대로 ArmLength / Yaw / Pitch 는 float 이라 f 를 붙여야 한다.

namespace FFF_OverviewCameraRigTestHelpers
{
	FFF_OverviewCameraSettings MakeSettings()
	{
		FFF_OverviewCameraSettings Settings;
		Settings.MinArmLength = 800.f;
		Settings.MaxArmLength = 16000.f;
		Settings.ZoomStepMultiplier = 1.15f;
		Settings.PitchAtMinZoom = -35.f;
		Settings.PitchAtMaxZoom = -72.f;
		Settings.PanSpeedAtMaxZoom = 6000.f;
		Settings.PanSpeedMinScale = 0.18f;
		Settings.YawSnapDegrees = 45.f;
		Settings.YawSpeedDegrees = 120.f;
		Settings.InterpSpeed = 0.f;   // 테스트에서는 보간 없이 즉시 반영
		return Settings;
	}

	FFF_OverviewCameraRig MakeRig(float StartArmLength = 8000.f)
	{
		FFF_OverviewCameraRig Rig;
		Rig.Initialize(MakeSettings(), FVector::ZeroVector, StartArmLength);
		return Rig;
	}
}

using namespace FFF_OverviewCameraRigTestHelpers;

// 1. 줌은 곱셈이라 어느 거리에서도 체감이 같아야 하고, 왕복하면 제자리로 와야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_ZoomIsMultiplicativeAndClamped, "FestaFiesta.Camera.Rig.ZoomIsMultiplicativeAndClamped", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_ZoomIsMultiplicativeAndClamped::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig(8000.f);

	Rig.Zoom(1.f);
	Rig.Zoom(-1.f);
	TestEqual(TEXT("당겼다 물리면 제자리로 온다"), Rig.GetTargetState().ArmLength, 8000.f, 0.1f);

	for (int32 i = 0; i < 60; ++i)
	{
		Rig.Zoom(1.f);
	}
	TestEqual(TEXT("아무리 당겨도 최소 거리 아래로 안 간다"), Rig.GetTargetState().ArmLength, 800.f, 0.01f);

	for (int32 i = 0; i < 120; ++i)
	{
		Rig.Zoom(-1.f);
	}
	TestEqual(TEXT("아무리 물려도 최대 거리 위로 안 간다"), Rig.GetTargetState().ArmLength, 16000.f, 0.01f);
	return true;
}

// 2. 피치는 줌에서 파생된다. 플레이어가 각도를 따로 만지면 구도가 망가진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_PitchFollowsZoom, "FestaFiesta.Camera.Rig.PitchFollowsZoom", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_PitchFollowsZoom::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig();

	TestEqual(TEXT("최근접에서는 얕게 눕는다"), Rig.ResolvePitchForArmLength(800.f), -35.f, 0.01f);
	TestEqual(TEXT("최원거리에서는 거의 수직으로 내려다본다"), Rig.ResolvePitchForArmLength(16000.f), -72.f, 0.01f);

	// 멀어질수록 단조롭게 가팔라져야 한다. 중간에 뒤집히면 줌하다 화면이 출렁인다.
	float Previous = Rig.ResolvePitchForArmLength(800.f);
	bool bMonotonic = true;
	for (float Arm = 1300.f; Arm <= 16000.f; Arm += 500.f)
	{
		const float Current = Rig.ResolvePitchForArmLength(Arm);
		bMonotonic &= (Current <= Previous + KINDA_SMALL_NUMBER);
		Previous = Current;
	}
	TestTrue(TEXT("줌아웃할수록 피치가 단조롭게 가팔라진다"), bMonotonic);
	return true;
}

// 3. 확대할수록 팬이 느려져야 한다. 안 그러면 줌인 상태에서 화면이 홱 날아간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_PanSpeedScalesWithZoom, "FestaFiesta.Camera.Rig.PanSpeedScalesWithZoom", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_PanSpeedScalesWithZoom::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Far = MakeRig(16000.f);
	FFF_OverviewCameraRig Near = MakeRig(800.f);

	TestEqual(TEXT("최원거리에서는 설정한 최대 속도"), Far.GetCurrentPanSpeed(), 6000.f, 0.1f);
	TestEqual(TEXT("최근접에서는 최소 비율까지 줄어든다"), Near.GetCurrentPanSpeed(), 6000.f * 0.18f, 0.1f);
	TestTrue(TEXT("확대 상태가 더 느리다"), Near.GetCurrentPanSpeed() < Far.GetCurrentPanSpeed());
	return true;
}

// 4. 회전한 뒤에도 "위로 밀면 보이는 쪽으로" 가야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_PanIsCameraRelative, "FestaFiesta.Camera.Rig.PanIsCameraRelative", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_PanIsCameraRelative::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig();

	Rig.Pan(FVector2D(0.f, 1.f), 1.f);
	const FVector AfterForward = Rig.GetTargetState().FocusLocation;
	TestTrue(TEXT("Yaw 0에서 위로 밀면 +X 로 간다"), AfterForward.X > 100.f && FMath::Abs(AfterForward.Y) < 1.f);

	// 90도 돌린 뒤 같은 입력을 주면 이번엔 +Y 로 가야 한다.
	FFF_OverviewCameraRig Rotated = MakeRig();
	Rotated.SnapRotate(1);
	Rotated.SnapRotate(1);   // 45 x 2 = 90도
	TestEqual(TEXT("두 번 스냅하면 90도"), Rotated.GetTargetState().Yaw, 90.f, 0.01f);

	Rotated.Pan(FVector2D(0.f, 1.f), 1.f);
	const FVector AfterRotated = Rotated.GetTargetState().FocusLocation;
	TestTrue(TEXT("90도 돌린 뒤 위로 밀면 +Y 로 간다"), AfterRotated.Y > 100.f && FMath::Abs(AfterRotated.X) < 1.f);
	return true;
}

// 5. 스냅 회전이 어중간한 각도에서 제자리로 되돌아가면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_SnapRotateAlwaysAdvances, "FestaFiesta.Camera.Rig.SnapRotateAlwaysAdvances", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_SnapRotateAlwaysAdvances::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig();

	Rig.SnapRotate(1);
	TestEqual(TEXT("0에서 오른쪽 탭이면 45"), Rig.GetTargetState().Yaw, 45.f, 0.01f);

	// 45도에서 살짝 더 돌아간 상태. 반올림만 쓰면 여기서 오른쪽 탭이 45로 되돌아간다.
	Rig.Rotate(1.f, 1.f / 120.f);   // 1도 추가
	Rig.SnapRotate(1);
	TestEqual(TEXT("46도에서 오른쪽 탭은 90으로 전진한다"), Rig.GetTargetState().Yaw, 90.f, 0.01f);

	// 여덟 번 돌면 한 바퀴.
	FFF_OverviewCameraRig FullTurn = MakeRig();
	for (int32 i = 0; i < 8; ++i)
	{
		FullTurn.SnapRotate(1);
	}
	TestEqual(TEXT("여덟 번 스냅하면 제자리"), FMath::Abs(FullTurn.GetTargetState().Yaw), 0.f, 0.01f);
	return true;
}

// 6. 초점이 축제장 밖으로 나가면 빈 지형만 보인다. 경계로 잘라야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_ClampsFocusToBounds, "FestaFiesta.Camera.Rig.ClampsFocusToBounds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_ClampsFocusToBounds::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraSettings Settings = MakeSettings();
	Settings.FocusBounds = FBox(FVector(-5000.f, -5000.f, 0.f), FVector(5000.f, 5000.f, 0.f));

	FFF_OverviewCameraRig Rig;
	Rig.Initialize(Settings, FVector::ZeroVector, 8000.f);

	Rig.FocusOn(FVector(99999.f, 99999.f, 0.f));
	const FVector Clamped = Rig.GetTargetState().FocusLocation;
	TestEqual(TEXT("X 가 경계로 잘린다"), Clamped.X, 5000.0, 0.01);
	TestEqual(TEXT("Y 가 경계로 잘린다"), Clamped.Y, 5000.0, 0.01);

	// 오래 밀어도 경계를 못 넘는다.
	for (int32 i = 0; i < 100; ++i)
	{
		Rig.Pan(FVector2D(1.f, 1.f), 1.f);
	}
	TestTrue(TEXT("계속 밀어도 경계 안에 남는다"),
		Rig.GetTargetState().FocusLocation.X <= 5000.f + 0.01f);

	// 경계가 설정되지 않았으면 자르지 않는다.
	FFF_OverviewCameraRig Unbounded = MakeRig();
	Unbounded.FocusOn(FVector(99999.f, 0.f, 0.f));
	TestEqual(TEXT("경계 미설정이면 그대로 간다"), Unbounded.GetTargetState().FocusLocation.X, 99999.0, 0.1);
	return true;
}

// 7. 설정이 뒤집혀 있어도 0으로 나누거나 줌이 반대로 돌면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_SurvivesInvalidSettings, "FestaFiesta.Camera.Rig.SurvivesInvalidSettings", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_SurvivesInvalidSettings::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraSettings Broken;
	Broken.MinArmLength = 5000.f;
	Broken.MaxArmLength = 1000.f;    // 뒤집힘
	Broken.ZoomStepMultiplier = 0.5f; // 1 미만이라 줌이 반대로 돈다

	FFF_OverviewCameraRig Rig;
	Rig.Initialize(Broken, FVector::ZeroVector, 3000.f);

	TestTrue(TEXT("보정된 설정이 유효하다"), Rig.GetSettings().IsValid());
	TestTrue(TEXT("최대가 최소보다 크다"), Rig.GetSettings().MaxArmLength > Rig.GetSettings().MinArmLength);

	Rig.Zoom(1.f);
	TestTrue(TEXT("줌 후에도 거리가 유한하다"), FMath::IsFinite(Rig.GetTargetState().ArmLength));
	TestTrue(TEXT("정규화 줌이 0~1 범위다"),
		Rig.GetNormalizedZoom() >= 0.f && Rig.GetNormalizedZoom() <= 1.f);
	return true;
}

// 8. 보간은 최단 경로로 돌아야 한다. 179도에서 -179도로 갈 때 한 바퀴 돌면 안 된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_YawInterpTakesShortestPath, "FestaFiesta.Camera.Rig.YawInterpTakesShortestPath", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_YawInterpTakesShortestPath::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraSettings Settings = MakeSettings();
	Settings.InterpSpeed = 10.f;

	FFF_OverviewCameraRig Rig;
	Rig.Initialize(Settings, FVector::ZeroVector, 8000.f);

	// 표시각을 179도 근처로 올려둔 뒤 반대편으로 목표를 준다.
	Rig.Rotate(1.f, 179.f / Settings.YawSpeedDegrees);
	Rig.SnapToTarget();
	Rig.Rotate(1.f, 4.f / Settings.YawSpeedDegrees);   // 목표만 -177 쪽으로 넘어감

	// 한 스텝 보간했을 때 각도가 0 쪽으로 크게 되돌아가면 한 바퀴 돈 것이다.
	Rig.AdvanceInterpolation(1.f / 60.f);
	const float DisplayYaw = Rig.GetDisplayState().Yaw;
	TestTrue(TEXT("보간 중에 반대편으로 한 바퀴 돌지 않는다"), FMath::Abs(DisplayYaw) > 170.f);
	return true;
}

// 9. 커서 기준 줌은 앵커를 화면상 같은 자리에 붙들어야 한다.
//    화면상 오프셋은 (앵커 - 초점) / 팔길이 에 비례하므로 이 비율이 보존되는지 본다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_ZoomTowardsKeepsAnchorOnScreen, "FestaFiesta.Camera.Rig.ZoomTowardsKeepsAnchorOnScreen", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_ZoomTowardsKeepsAnchorOnScreen::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig(8000.f);
	const FVector Anchor(3000.f, 0.f, 0.f);

	const float BeforeRatio =
		(Anchor.X - Rig.GetTargetState().FocusLocation.X) / Rig.GetTargetState().ArmLength;

	for (int32 i = 0; i < 5; ++i)
	{
		Rig.ZoomTowards(1.f, Anchor);
		const float AfterRatio =
			(Anchor.X - Rig.GetTargetState().FocusLocation.X) / Rig.GetTargetState().ArmLength;
		TestEqual(*FString::Printf(TEXT("%d회 줌 후에도 화면상 위치가 같다"), i + 1),
			AfterRatio, BeforeRatio, 1e-4f);
	}

	// 초점이 앵커 쪽으로 다가가긴 해야 한다. 아예 안 움직이면 커서 줌이 안 걸린 것이다.
	TestTrue(TEXT("초점이 앵커 쪽으로 이동한다"), Rig.GetTargetState().FocusLocation.X > 100.f);
	return true;
}

// 10. 줌이 클램프에 걸렸으면 초점도 안 움직여야 한다.
//     안 그러면 최대 줌에서 휠을 계속 굴릴 때 화면이 커서 쪽으로 계속 기어간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_ZoomTowardsStopsAtClamp, "FestaFiesta.Camera.Rig.ZoomTowardsStopsAtClamp", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_ZoomTowardsStopsAtClamp::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraRig Rig = MakeRig(800.f);   // 이미 최대 줌
	const FVector Anchor(5000.f, 0.f, 0.f);
	const FVector FocusBefore = Rig.GetTargetState().FocusLocation;

	for (int32 i = 0; i < 20; ++i)
	{
		Rig.ZoomTowards(1.f, Anchor);
	}

	TestEqual(TEXT("팔 길이가 최소에 머문다"), Rig.GetTargetState().ArmLength, 800.f, 0.01f);
	TestTrue(TEXT("초점이 커서 쪽으로 기어가지 않는다"),
		Rig.GetTargetState().FocusLocation.Equals(FocusBefore, 0.01f));
	return true;
}

// 11. 커서 광선을 수평면에 떨어뜨리는 계산.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_ProjectsRayToPlane, "FestaFiesta.Camera.Rig.ProjectsRayToPlane", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_ProjectsRayToPlane::RunTest(const FString& Parameters)
{
	FVector Hit = FVector::ZeroVector;

	TestTrue(TEXT("수직 하향 광선은 바로 아래에 떨어진다"),
		FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(
			FVector(100.f, 200.f, 5000.f), FVector(0.f, 0.f, -1.f), 0.f, Hit));
	TestTrue(TEXT("교점이 정확하다"), Hit.Equals(FVector(100.f, 200.f, 0.f), 0.01f));

	TestTrue(TEXT("45도 하향도 계산된다"),
		FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(
			FVector(0.f, 0.f, 1000.f), FVector(1.f, 0.f, -1.f), 0.f, Hit));
	TestTrue(TEXT("45도 교점이 정확하다"), Hit.Equals(FVector(1000.f, 0.f, 0.f), 0.01f));

	// 평행하거나 위를 보는 광선은 실패해야 한다. 여기서 true 를 주면 앵커가 무한대로 날아간다.
	TestFalse(TEXT("수평 광선은 실패한다"),
		FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(
			FVector(0.f, 0.f, 1000.f), FVector(1.f, 0.f, 0.f), 0.f, Hit));
	TestFalse(TEXT("위를 보는 광선은 실패한다"),
		FFF_OverviewCameraRig::ProjectRayToHorizontalPlane(
			FVector(0.f, 0.f, 1000.f), FVector(0.f, 0.f, 1.f), 0.f, Hit));
	return true;
}

// 12. 화면 가장자리 스크롤 입력.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_EdgeScrollInput, "FestaFiesta.Camera.Rig.EdgeScrollInput", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_EdgeScrollInput::RunTest(const FString& Parameters)
{
	const FVector2D Viewport(1920.f, 1080.f);
	constexpr float Margin = 24.f;

	using FRig = FFF_OverviewCameraRig;

	TestTrue(TEXT("화면 중앙에서는 스크롤하지 않는다"),
		FRig::ComputeEdgeScrollInput(FVector2D(960.f, 540.f), Viewport, Margin).IsNearlyZero());

	TestEqual(TEXT("좌측 끝은 -X"),
		FRig::ComputeEdgeScrollInput(FVector2D(0.f, 540.f), Viewport, Margin).X, -1.0, 0.01);
	TestEqual(TEXT("우측 끝은 +X"),
		FRig::ComputeEdgeScrollInput(FVector2D(1920.f, 540.f), Viewport, Margin).X, 1.0, 0.01);

	// 화면 좌표는 아래로 갈수록 Y 가 커진다. 팬 입력은 화면 위쪽이 양수라 뒤집혀야 한다.
	TestEqual(TEXT("상단은 +Y"),
		FRig::ComputeEdgeScrollInput(FVector2D(960.f, 0.f), Viewport, Margin).Y, 1.0, 0.01);
	TestEqual(TEXT("하단은 -Y"),
		FRig::ComputeEdgeScrollInput(FVector2D(960.f, 1080.f), Viewport, Margin).Y, -1.0, 0.01);

	// 경계에서 갑자기 최대 속도가 되면 튀어 보인다. 가까울수록 세져야 한다.
	const float Near = FMath::Abs(FRig::ComputeEdgeScrollInput(FVector2D(6.f, 540.f), Viewport, Margin).X);
	const float Far = FMath::Abs(FRig::ComputeEdgeScrollInput(FVector2D(18.f, 540.f), Viewport, Margin).X);
	TestTrue(TEXT("가장자리에 가까울수록 세진다"), Near > Far);
	TestTrue(TEXT("경계 밖 바로 안쪽은 0 이다"),
		FMath::IsNearlyZero(FRig::ComputeEdgeScrollInput(FVector2D(24.f, 540.f), Viewport, Margin).X, 0.01f));

	// 다른 창을 쓰는 중에 화면이 흘러가면 안 된다.
	TestTrue(TEXT("커서가 창 밖이면 0"),
		FRig::ComputeEdgeScrollInput(FVector2D(-50.f, 540.f), Viewport, Margin).IsNearlyZero());
	TestTrue(TEXT("뷰포트 크기가 0이면 0"),
		FRig::ComputeEdgeScrollInput(FVector2D(10.f, 10.f), FVector2D::ZeroVector, Margin).IsNearlyZero());

	const FVector2D Corner = FRig::ComputeEdgeScrollInput(FVector2D(0.f, 0.f), Viewport, Margin);
	TestTrue(TEXT("좌상단 모서리는 대각선으로 움직인다"), Corner.X < -0.99f && Corner.Y > 0.99f);
	return true;
}

// 13. 각도 변화 구간과 줌 가능 범위 분리.
//     큰 맵이라 줌아웃 여유를 넉넉히 주면 실사용 거리대의 각도가 전부 눕는 문제를 막는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_OverviewCameraRig_PitchSaturatesBeforeMaxZoom, "FestaFiesta.Camera.Rig.PitchSaturatesBeforeMaxZoom", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_OverviewCameraRig_PitchSaturatesBeforeMaxZoom::RunTest(const FString& Parameters)
{
	FFF_OverviewCameraSettings Settings = MakeSettings();
	Settings.PitchSaturationArmLength = 4000.f;   // 줌 상한은 16000 그대로

	FFF_OverviewCameraRig Rig;
	Rig.Initialize(Settings, FVector::ZeroVector, 4000.f);

	TestEqual(TEXT("최근접은 시작 각도 그대로"), Rig.ResolvePitchForArmLength(800.f), -35.f, 0.01f);
	TestEqual(TEXT("포화 거리에서 최종 각도에 도달한다"), Rig.ResolvePitchForArmLength(4000.f), -72.f, 0.01f);

	// 여기서부터는 더 물러나도 각도가 안 변하고 높이만 올라간다.
	TestEqual(TEXT("포화 이후 각도 고정 (6000)"), Rig.ResolvePitchForArmLength(6000.f), -72.f, 0.01f);
	TestEqual(TEXT("포화 이후 각도 고정 (16000)"), Rig.ResolvePitchForArmLength(16000.f), -72.f, 0.01f);

	// 포화 전 구간은 여전히 단조롭게 가팔라져야 한다.
	float Previous = Rig.ResolvePitchForArmLength(800.f);
	bool bMonotonic = true;
	for (float Arm = 900.f; Arm <= 4000.f; Arm += 100.f)
	{
		const float Current = Rig.ResolvePitchForArmLength(Arm);
		bMonotonic &= (Current <= Previous + KINDA_SMALL_NUMBER);
		Previous = Current;
	}
	TestTrue(TEXT("포화 전 구간은 단조 감소"), bMonotonic);

	// 미설정이면 기존처럼 MaxArmLength 를 기준으로 쓴다.
	FFF_OverviewCameraRig Legacy = MakeRig();
	TestTrue(TEXT("미설정이면 중간 거리에서 아직 안 눕는다"),
		Legacy.ResolvePitchForArmLength(4000.f) > -50.f);

	// 포화 거리를 최소보다 작게 넣어도 0 으로 나누거나 각도가 뒤집히면 안 된다.
	FFF_OverviewCameraSettings Broken = MakeSettings();
	Broken.PitchSaturationArmLength = 500.f;
	FFF_OverviewCameraRig Guarded;
	Guarded.Initialize(Broken, FVector::ZeroVector, 2000.f);
	TestTrue(TEXT("잘못된 포화 거리에서도 각도가 범위 안에 있다"),
		Guarded.ResolvePitchForArmLength(2000.f) <= -35.f
		&& Guarded.ResolvePitchForArmLength(2000.f) >= -72.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
