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

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
