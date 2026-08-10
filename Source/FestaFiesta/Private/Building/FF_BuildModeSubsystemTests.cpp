// FF_BuildModeSubsystemTests.cpp

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildModeStateTransitionTest,
	"FestaFiesta.Building.BuildMode.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildModeStateTransitionTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildModeModel;

	TestEqual(TEXT("첫 B는 카탈로그 없는 건축 모드로 진입한다"),
		ResolveBuildKeyState(EFF_BuildModeState::Closed),
		EFF_BuildModeState::BuildMode);
	TestEqual(TEXT("건축 모드의 B는 카탈로그를 연다"),
		ResolveBuildKeyState(EFF_BuildModeState::BuildMode),
		EFF_BuildModeState::CatalogOpen);
	TestEqual(TEXT("열린 카탈로그의 B는 기본 건축 모드로 돌아간다"),
		ResolveBuildKeyState(EFF_BuildModeState::CatalogOpen),
		EFF_BuildModeState::BuildMode);
	TestEqual(TEXT("배치 중 B는 다른 시설 선택을 위해 카탈로그를 연다"),
		ResolveBuildKeyState(EFF_BuildModeState::Placing),
		EFF_BuildModeState::CatalogOpen);

	TestFalse(TEXT("Closed는 건축 세션이 아니다"),
		IsBuildSessionActive(EFF_BuildModeState::Closed));
	TestTrue(TEXT("BuildMode는 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::BuildMode));
	TestTrue(TEXT("CatalogOpen은 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::CatalogOpen));
	TestTrue(TEXT("Placing은 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::Placing));
	TestFalse(TEXT("Closed의 Esc는 건축 종료를 중복 실행하지 않는다"),
		ShouldExitOnEscape(EFF_BuildModeState::Closed));
	TestTrue(TEXT("BuildMode의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::BuildMode));
	TestTrue(TEXT("CatalogOpen의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::CatalogOpen));
	TestTrue(TEXT("Placing의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::Placing));

	TestTrue(TEXT("설치 시설 철거는 카탈로그 없는 BuildMode에서만 허용한다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::BuildMode));
	TestFalse(TEXT("카탈로그에서는 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::CatalogOpen));
	TestFalse(TEXT("배치 중에는 기존 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::Placing));
	TestFalse(TEXT("Closed에서는 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::Closed));
	TestFalse(TEXT("첫 클릭은 철거하지 않고 확인 대상을 선택한다"),
		ShouldConfirmRemoval(EFF_BuildModeState::BuildMode, false, false));
	TestFalse(TEXT("다른 시설 클릭은 확인 대상을 교체한다"),
		ShouldConfirmRemoval(EFF_BuildModeState::BuildMode, true, false));
	TestTrue(TEXT("선택한 같은 시설의 두 번째 클릭만 철거를 확인한다"),
		ShouldConfirmRemoval(EFF_BuildModeState::BuildMode, true, true));
	TestFalse(TEXT("카탈로그에서는 선택이 남아 있어도 철거를 확인하지 않는다"),
		ShouldConfirmRemoval(EFF_BuildModeState::CatalogOpen, true, true));
	TestTrue(TEXT("UI가 처리하지 않은 raw B 입력은 상태 전환에 사용한다"),
		ShouldProcessRawBuildKey(false, true));
	TestFalse(TEXT("카탈로그 UI가 처리한 B는 같은 프레임에 다시 토글하지 않는다"),
		ShouldProcessRawBuildKey(true, true));

	return true;
}

#endif
