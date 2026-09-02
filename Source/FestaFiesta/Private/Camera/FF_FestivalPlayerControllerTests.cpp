// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Camera/FF_CameraTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalCameraRotationBuildStatePolicyTest,
	"FestaFiesta.Camera.Input.RotationBuildStatePolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalCameraRotationBuildStatePolicyTest::RunTest(const FString& Parameters)
{
	struct FBuildStateCase
	{
		const TCHAR* Label;
		EFF_BuildModeState State;
		bool bExpectedRotationAllowed;
	};

	const FBuildStateCase Cases[] =
	{
		{ TEXT("Closed"), EFF_BuildModeState::Closed, true },
		{ TEXT("BuildMode"), EFF_BuildModeState::BuildMode, true },
		{ TEXT("CatalogOpen"), EFF_BuildModeState::CatalogOpen, true },
		{ TEXT("Placing"), EFF_BuildModeState::Placing, false },
		{ TEXT("Moving"), EFF_BuildModeState::Moving, false },
	};

	for (const FBuildStateCase& TestCase : Cases)
	{
		const EFF_CameraInputBlock Reason =
			TestCase.State == EFF_BuildModeState::Closed
				? EFF_CameraInputBlock::None
				: EFF_CameraInputBlock::BuildSession;
		const bool bRotationAllowed = FFCameraInputModel::IsRotationAllowed(
			Reason,
			FFBuildModeModel::IsPreviewActive(TestCase.State));

		TestEqual(
			*FString::Printf(TEXT("%s 상태의 카메라 회전 정책"), TestCase.Label),
			bRotationAllowed,
			TestCase.bExpectedRotationAllowed);
	}

	TestTrue(
		TEXT("시설 프리뷰 중에도 팬과 줌은 유지한다"),
		FFCameraInputModel::IsMovementAllowed(EFF_CameraInputBlock::BuildSession));
	TestFalse(
		TEXT("모달 UI에서는 회전을 막는다"),
		FFCameraInputModel::IsRotationAllowed(EFF_CameraInputBlock::ModalUi, false));
	TestFalse(
		TEXT("액터 시점에서는 회전을 막는다"),
		FFCameraInputModel::IsRotationAllowed(EFF_CameraInputBlock::ActorView, false));
	TestFalse(
		TEXT("캐릭터 조종 중에는 회전을 막는다"),
		FFCameraInputModel::IsRotationAllowed(EFF_CameraInputBlock::CharacterControl, false));
	TestTrue(
		TEXT("캐릭터 조종 중에도 시설 피킹은 허용한다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::CharacterControl,
			/*bBuildSessionActive=*/false));
	TestTrue(
		TEXT("평상시 관찰 시점에서도 시설 피킹을 유지한다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::None,
			/*bBuildSessionActive=*/false));
	TestFalse(
		TEXT("캐릭터 조종 중인 건축 세션에서는 피킹을 막는다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::CharacterControl,
			/*bBuildSessionActive=*/true));
	TestFalse(
		TEXT("관찰 시점의 건축 입력도 시설 피킹으로 전달하지 않는다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::BuildSession,
			/*bBuildSessionActive=*/true));
	TestFalse(
		TEXT("모달 UI에서는 시설 피킹을 막는다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::ModalUi,
			/*bBuildSessionActive=*/false));
	TestFalse(
		TEXT("액터 관람 중에는 시설 피킹을 막는다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::ActorView,
			/*bBuildSessionActive=*/false));
	TestFalse(
		TEXT("연출 중에는 시설 피킹을 막는다"),
		FFCameraInputModel::IsPickingAllowed(
			EFF_CameraInputBlock::Scripted,
			/*bBuildSessionActive=*/false));
	TestTrue(
		TEXT("IA_Pick이 유실된 프레임의 좌클릭은 native fallback으로 복구한다"),
		FFCameraInputModel::ShouldUsePrimaryPointerPickFallback(false, true));
	TestFalse(
		TEXT("IA_Pick이 이미 처리한 좌클릭은 같은 프레임에 중복 방송하지 않는다"),
		FFCameraInputModel::ShouldUsePrimaryPointerPickFallback(true, true));
	TestFalse(
		TEXT("좌클릭 edge가 없으면 fallback 선택을 만들지 않는다"),
		FFCameraInputModel::ShouldUsePrimaryPointerPickFallback(false, false));

	return true;
}

// 연출 시점 입력 정책.
//
// 피날레처럼 화면을 보여주는 구간에서는 조작을 전부 막는다. 연출 중에 카메라가
// 움직이면 보여주려던 구도가 무너지고, 관찰 카메라가 화면에 없는 동안 조작을
// 받으면 돌아왔을 때 엉뚱한 데를 보고 있다.
//
// 나중에 회전만 열 수도 있다. 그때 이 테스트가 계약이 바뀐 걸 알려준다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FestivalCameraScriptedViewPolicyTest,
	"FestaFiesta.Camera.Input.ScriptedViewPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FestivalCameraScriptedViewPolicyTest::RunTest(const FString& Parameters)
{
	using namespace FFCameraInputModel;

	TestFalse(TEXT("연출 중에는 팬·줌이 막힌다"),
		IsMovementAllowed(EFF_CameraInputBlock::Scripted));
	TestFalse(TEXT("연출 중에는 회전도 막힌다"),
		IsRotationAllowed(EFF_CameraInputBlock::Scripted, /*bBuildPreviewActive=*/false));

	// 건축 프리뷰 여부와 무관하게 막혀야 한다. 두 조건이 얽히면 판정이 흔들린다.
	TestFalse(TEXT("건축 프리뷰가 켜져 있어도 결과는 같다"),
		IsRotationAllowed(EFF_CameraInputBlock::Scripted, /*bBuildPreviewActive=*/true));

	// 연출 상태를 추가하면서 기존 정책이 흔들리지 않았는지 같이 본다.
	TestTrue(TEXT("평상시에는 팬·줌이 열려 있다"),
		IsMovementAllowed(EFF_CameraInputBlock::None));
	TestTrue(TEXT("건축 세션 중에도 팬·줌은 열려 있다"),
		IsMovementAllowed(EFF_CameraInputBlock::BuildSession));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
