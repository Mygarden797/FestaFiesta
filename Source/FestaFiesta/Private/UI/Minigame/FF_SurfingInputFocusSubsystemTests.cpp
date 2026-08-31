// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/Minigame/FF_SurfingInputFocusPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_SurfingInputFocusPolicyTest,
	"FestaFiesta.UI.Minigame.Surfing.InputFocusPolicy",
	EAutomationTestFlags_ApplicationContextMask
		| EAutomationTestFlags::ProductFilter)

bool FFF_SurfingInputFocusPolicyTest::RunTest(const FString& Parameters)
{
	using FFSurfingInputFocusPolicy::ShouldRepairFocus;

	TestTrue(
		TEXT("활성 서핑 플레이에서 포커스가 없으면 복구한다"),
		ShouldRepairFocus(true, true, true, false, false));
	TestFalse(
		TEXT("이미 화면 또는 자식이 포커스를 가지면 건드리지 않는다"),
		ShouldRepairFocus(true, true, true, false, true));
	TestFalse(
		TEXT("결과 화면에서는 플레이 포커스를 복구하지 않는다"),
		ShouldRepairFocus(true, false, true, false, false));
	TestFalse(
		TEXT("일시정지 중에는 다른 모달의 포커스를 빼앗지 않는다"),
		ShouldRepairFocus(true, true, true, true, false));
	TestFalse(
		TEXT("게임 창이 비활성이면 내부 포커스를 강제하지 않는다"),
		ShouldRepairFocus(true, true, false, false, false));
	TestFalse(
		TEXT("비활성 화면은 포커스를 요청하지 않는다"),
		ShouldRepairFocus(false, true, true, false, false));

	return true;
}

#endif

