// FF_BuildModeSubsystemTests.cpp

// B/Escape 상태 전이와 Q/E 스냅·자유 회전 입력, 임의 yaw 중심 보정의 순수 정책을 검증한다.
// 흐름: 상태별 입력 정책 비교 -> 회전 edge/hold 배타성 -> 스냅 각도와 footprint 중심 계산 비교.
// 불변식: 런타임과 같은 FFBuildModeModel 함수를 호출하며 임의 월드 상태를 만들지 않는다.
// 실패/성능: 순수 규칙만 검사해 상태 머신 회귀를 빠르게 찾고 실패 assertion을 입력별로 분리한다.

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

	// B 키는 Closed에서 세션을 열고 활성 세션에서는 프리뷰를 정리한 뒤 카탈로그를 토글한다.
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

	// 표시 정책은 전이 함수와 같은 상태 정의를 공유해야 UI가 닫힌 뒤에도 남지 않는다.
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
	// 동기 WorldChanged 이벤트가 재진입해 상태를 바꿔도 이전 Move 성공 처리가 새 세션을 덮지 않아야 한다.
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
	// Escape와 단계 정책은 활성 세션만 닫고 이미 Closed인 상태에는 부작용을 만들지 않는다.
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
	TestTrue(TEXT("라이브 전환으로 재배치가 닫히면 활성 건축 세션을 자동 종료한다"),
		ShouldExitWhenReconfigureCloses(EFF_BuildModeState::BuildMode, false));
	TestFalse(TEXT("아침 점검처럼 재배치 가능 단계에서는 건축 세션을 유지한다"),
		ShouldExitWhenReconfigureCloses(EFF_BuildModeState::BuildMode, true));
	TestFalse(TEXT("이미 닫힌 건축 세션은 단계 전환에서 중복 종료하지 않는다"),
		ShouldExitWhenReconfigureCloses(EFF_BuildModeState::Closed, false));
	TestTrue(TEXT("Festival Stage와 Zone Phase가 모두 허용할 때만 건축 세션에 진입한다"),
		CanEnterBuildSession(true, true));
	TestFalse(TEXT("Live Stage에서는 Zone Phase가 늦게 바뀌어도 건축 진입을 막는다"),
		CanEnterBuildSession(false, true));
	TestFalse(TEXT("Zone Phase가 닫히면 재배치 가능 Stage에서도 건축 진입을 막는다"),
		CanEnterBuildSession(true, false));

	// 월드 시설 작업은 카탈로그·프리뷰 입력과 경쟁하지 않는 BuildMode에서만 허용한다.
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildModeRotationInputModelTest,
	"FestaFiesta.Building.BuildMode.RotationInputModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildModeRotationInputModelTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildModeModel;

	// 한 Q/E edge가 스냅과 자유 회전을 동시에 시작하면 첫 프레임에 각도가 두 번 변한다.
	TestTrue(TEXT("일반 Q/E edge는 스냅을 한 번 적용한다"),
		ShouldApplySnappedRotation(true, false));
	TestFalse(TEXT("일반 Q/E edge는 자유 회전 gesture를 시작하지 않는다"),
		ShouldStartFreeRotationGesture(true, false));
	TestFalse(TEXT("Shift+Q/E edge는 스냅을 적용하지 않는다"),
		ShouldApplySnappedRotation(true, true));
	TestTrue(TEXT("Shift+Q/E edge는 자유 회전 gesture만 시작한다"),
		ShouldStartFreeRotationGesture(true, true));
	TestFalse(TEXT("홀드 중 반복 프레임은 새 스냅 edge가 아니다"),
		ShouldApplySnappedRotation(false, false));

	TestEqual(TEXT("Shift+Q 홀드는 왼쪽 자유 회전이다"),
		ResolveFreeRotationDirection(true, true, false, false, true), -1.0f);
	TestEqual(TEXT("Shift+E 홀드는 오른쪽 자유 회전이다"),
		ResolveFreeRotationDirection(false, false, true, true, true), 1.0f);
	TestEqual(TEXT("Q/E 자유 회전을 함께 홀드하면 상쇄된다"),
		ResolveFreeRotationDirection(true, true, true, true, true), 0.0f);
	TestEqual(TEXT("Shift를 놓으면 latched gesture도 회전하지 않는다"),
		ResolveFreeRotationDirection(true, true, false, false, false), 0.0f);

	TestEqual(TEXT("0도에서 오른쪽 45도 스냅은 45도다"),
		ResolveNextSnappedYawDegrees(0.0f, 45.0f, 1), 45.0f);
	TestEqual(TEXT("이미 45도면 오른쪽 스냅은 다음 90도로 전진한다"),
		ResolveNextSnappedYawDegrees(45.0f, 45.0f, 1), 90.0f);
	TestEqual(TEXT("자유 회전 46도에서 오른쪽 스냅은 91도가 아니라 90도다"),
		ResolveNextSnappedYawDegrees(46.0f, 45.0f, 1), 90.0f);
	TestEqual(TEXT("자유 회전 -46도에서 왼쪽 스냅은 -90도다"),
		ResolveNextSnappedYawDegrees(-46.0f, 45.0f, -1), -90.0f);

	const FIntPoint FootprintSize(4, 2);
	TestTrue(TEXT("0도 footprint 중심은 기존 Size/2와 같다"),
		ResolveRotatedFootprintCenterOffset(FootprintSize, 0.0f).Equals(
			FVector2D(2.0, 1.0), 0.001));
	TestTrue(TEXT("90도 footprint 중심은 첫 셀 중심을 기준으로 회전한다"),
		ResolveRotatedFootprintCenterOffset(FootprintSize, 90.0f).Equals(
			FVector2D(0.0, 2.0), 0.001));
	TestTrue(TEXT("45도 footprint 중심도 axis-aligned 외접 Rect가 아닌 실제 yaw를 따른다"),
		ResolveRotatedFootprintCenterOffset(FootprintSize, 45.0f).Equals(
			FVector2D(1.20710678, 1.91421356), 0.001));

	TestEqual(TEXT("회전 스냅 fallback은 45도다"),
		UFF_BuildModeSubsystem::DefaultPlacementRotationSnapDegrees, 45.0f);
	TestEqual(TEXT("자유 회전 fallback은 초당 90도다"),
		UFF_BuildModeSubsystem::DefaultFreePlacementRotationSpeedDegreesPerSecond, 90.0f);
	TestEqual(TEXT("위치 스냅 fallback은 권위 격자와 같은 1cm다"),
		UFF_BuildModeSubsystem::DefaultPlacementPositionSnapCm, 1);

	return true;
}

#endif
