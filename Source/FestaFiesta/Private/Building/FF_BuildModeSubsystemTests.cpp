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
	TestEqual(TEXT("이동 중 B는 이동을 정리하고 카탈로그를 연다"),
		ResolveBuildKeyState(EFF_BuildModeState::Moving),
		EFF_BuildModeState::CatalogOpen);

	TestFalse(TEXT("Closed는 건축 세션이 아니다"),
		IsBuildSessionActive(EFF_BuildModeState::Closed));
	TestTrue(TEXT("BuildMode는 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::BuildMode));
	TestTrue(TEXT("CatalogOpen은 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::CatalogOpen));
	TestTrue(TEXT("Placing은 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::Placing));
	TestTrue(TEXT("Moving은 건축 세션이다"),
		IsBuildSessionActive(EFF_BuildModeState::Moving));
	TestTrue(TEXT("Placing은 프리뷰 활성 상태다"),
		IsPreviewActive(EFF_BuildModeState::Placing));
	TestTrue(TEXT("Moving은 프리뷰 활성 상태다"),
		IsPreviewActive(EFF_BuildModeState::Moving));
	TestFalse(TEXT("BuildMode는 프리뷰 활성 상태가 아니다"),
		IsPreviewActive(EFF_BuildModeState::BuildMode));
	TestEqual(TEXT("이동 취소는 선택 가능한 BuildMode로 복귀한다"),
		ResolvePreviewCancelState(EFF_BuildModeState::Moving),
		EFF_BuildModeState::BuildMode);
	TestEqual(TEXT("신규 배치 취소는 카탈로그로 복귀한다"),
		ResolvePreviewCancelState(EFF_BuildModeState::Placing),
		EFF_BuildModeState::CatalogOpen);
	const FGuid MoveFacilityId = FGuid::NewGuid();
	const FGuid MoveTransactionId = FGuid::NewGuid();
	TestTrue(TEXT("동기 이벤트 뒤에도 같은 이동 세션이면 성공 정리를 수행한다"),
		IsCurrentMoveSession(
			EFF_BuildModeState::Moving,
			MoveFacilityId,
			MoveTransactionId,
			MoveFacilityId,
			MoveTransactionId));
	TestFalse(TEXT("동기 이벤트가 건축 모드를 닫았으면 이전 이동 성공이 상태를 되살리지 않는다"),
		IsCurrentMoveSession(
			EFF_BuildModeState::Closed,
			MoveFacilityId,
			MoveTransactionId,
			MoveFacilityId,
			MoveTransactionId));
	TestFalse(TEXT("새 이동 transaction은 이전 이동 성공 정리 대상이 아니다"),
		IsCurrentMoveSession(
			EFF_BuildModeState::Moving,
			MoveFacilityId,
			FGuid::NewGuid(),
			MoveFacilityId,
			MoveTransactionId));
	TestFalse(TEXT("Closed의 Esc는 건축 종료를 중복 실행하지 않는다"),
		ShouldExitOnEscape(EFF_BuildModeState::Closed));
	TestTrue(TEXT("BuildMode의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::BuildMode));
	TestTrue(TEXT("CatalogOpen의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::CatalogOpen));
	TestTrue(TEXT("Placing의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::Placing));
	TestTrue(TEXT("Moving의 Esc는 전체 건축 모드를 종료한다"),
		ShouldExitOnEscape(EFF_BuildModeState::Moving));

	TestTrue(TEXT("설치 시설 철거는 카탈로그 없는 BuildMode에서만 허용한다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::BuildMode));
	TestFalse(TEXT("카탈로그에서는 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::CatalogOpen));
	TestFalse(TEXT("배치 중에는 기존 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::Placing));
	TestFalse(TEXT("이동 중에는 기존 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::Moving));
	TestFalse(TEXT("Closed에서는 시설을 철거하지 않는다"),
		CanRemoveInstalledFacility(EFF_BuildModeState::Closed));
	TestFalse(TEXT("첫 클릭은 작업 대상을 선택한다"),
		ShouldKeepActionSelection(EFF_BuildModeState::BuildMode, false, false));
	TestFalse(TEXT("다른 시설 클릭은 작업 대상을 교체한다"),
		ShouldKeepActionSelection(EFF_BuildModeState::BuildMode, true, false));
	TestTrue(TEXT("선택한 같은 시설을 다시 클릭해도 작업 선택만 유지한다"),
		ShouldKeepActionSelection(EFF_BuildModeState::BuildMode, true, true));
	TestFalse(TEXT("카탈로그에서는 작업 선택을 유지하지 않는다"),
		ShouldKeepActionSelection(EFF_BuildModeState::CatalogOpen, true, true));
	TestTrue(TEXT("UI가 처리하지 않은 raw B 입력은 상태 전환에 사용한다"),
		ShouldProcessRawBuildKey(false, true));
	TestFalse(TEXT("카탈로그 UI가 처리한 B는 같은 프레임에 다시 토글하지 않는다"),
		ShouldProcessRawBuildKey(true, true));

	return true;
}

#endif
