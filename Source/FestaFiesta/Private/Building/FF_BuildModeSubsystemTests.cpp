// FF_BuildModeSubsystemTests.cpp

// B 상태 전이, Pause 복귀 입력 소유권과 Q/E 스냅·자유 회전 입력, 임의 yaw 중심 보정을 검증한다.
// 흐름: 상태별 입력 정책 비교 -> 회전 edge/hold 배타성 -> 스냅 각도와 footprint 중심 계산 비교.
// 불변식: 런타임과 같은 FFBuildModeModel 함수를 호출하며 임의 월드 상태를 만들지 않는다.
// 실패/성능: 순수 규칙만 검사해 상태 머신 회귀를 빠르게 찾고 실패 assertion을 입력별로 분리한다.

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildModeSubsystem.h"
#include "FF_FFZSPlacementPreviewActor.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_BuildModeStateTransitionTest,
	"FestaFiesta.Building.BuildMode.StateTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFF_BuildModeStateTransitionTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildModeModel;

	// 첫 B는 배치 UI를 즉시 열고, 두 번째 B는 설치 시설을 다루는 건축 모드로 전환한다.
	TestEqual(TEXT("첫 B는 배치 카탈로그를 즉시 연다"),
		ResolveBuildKeyState(EFF_BuildModeState::Closed),
		EFF_BuildModeState::CatalogOpen);
	TestEqual(TEXT("두 번째 B는 카탈로그에서 건축 모드로 진입한다"),
		ResolveBuildKeyState(ResolveBuildKeyState(EFF_BuildModeState::Closed)),
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
	TestFalse(TEXT("일반 플레이에는 사용자용 격자가 없다"),
		ShouldShowPlacementGrid(EFF_BuildModeState::Closed));
	TestFalse(TEXT("시설 선택 전 기본 건축 상태에는 사용자용 격자가 없다"),
		ShouldShowPlacementGrid(EFF_BuildModeState::BuildMode));
	TestFalse(TEXT("카탈로그가 월드를 가리는 동안 사용자용 격자가 없다"),
		ShouldShowPlacementGrid(EFF_BuildModeState::CatalogOpen));
	TestTrue(TEXT("신규 시설 배치 중에는 로컬 격자를 표시한다"),
		ShouldShowPlacementGrid(EFF_BuildModeState::Placing));
	TestTrue(TEXT("기존 시설 이동 중에는 로컬 격자를 표시한다"),
		ShouldShowPlacementGrid(EFF_BuildModeState::Moving));
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
	// Pause Hub 복귀는 상태를 바꾸지 않고 현재 상태가 소유하던 입력 표면만 정확히 되찾는다.
	TestEqual(TEXT("Closed에는 복원할 건축 입력 소유권이 없다"),
		ResolvePauseResumeInputMode(EFF_BuildModeState::Closed), EPauseResumeInputMode::None);
	TestEqual(TEXT("CatalogOpen은 카탈로그 GameAndUI 포커스를 복원한다"),
		ResolvePauseResumeInputMode(EFF_BuildModeState::CatalogOpen), EPauseResumeInputMode::Catalog);
	TestEqual(TEXT("BuildMode은 월드 건축 입력을 복원한다"),
		ResolvePauseResumeInputMode(EFF_BuildModeState::BuildMode), EPauseResumeInputMode::World);
	TestEqual(TEXT("Placing은 프리뷰를 유지한 월드 건축 입력을 복원한다"),
		ResolvePauseResumeInputMode(EFF_BuildModeState::Placing), EPauseResumeInputMode::World);
	TestEqual(TEXT("Moving은 이동 세션을 유지한 월드 건축 입력을 복원한다"),
		ResolvePauseResumeInputMode(EFF_BuildModeState::Moving), EPauseResumeInputMode::World);
	TestFalse(TEXT("일반 Closed 상태는 건축 Pause lease를 만들지 않는다"),
		CanSuspendInputForPauseMenu(EFF_BuildModeState::Closed, false, false));
	TestTrue(TEXT("Pause 중 단계 전환으로 Closed가 되어도 재오픈은 pending resume를 취소한다"),
		CanSuspendInputForPauseMenu(EFF_BuildModeState::Closed, true, false));
	TestFalse(TEXT("건축 종료 정리 중에는 새 Pause lease를 만들지 않는다"),
		CanSuspendInputForPauseMenu(EFF_BuildModeState::BuildMode, false, true));
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
	TestTrue(TEXT("오늘 편성이 없는 시설은 철거 정책을 통과한다"),
		CanRemoveFacilityWithTodaysProgram(false));
	TestFalse(TEXT("오늘 프로그램이 편성된 시설은 철거 정책에서 막힌다"),
		CanRemoveFacilityWithTodaysProgram(true));
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
	TestEqual(TEXT("초보자용 기본 위치 스냅은 이해하기 쉬운 1m다"),
		UFF_BuildModeSubsystem::DefaultPlacementPositionSnapCm, 100);
	TestEqual(TEXT("정밀 위치 스냅 fallback은 1/4m다"),
		UFF_BuildModeSubsystem::DefaultPrecisionPlacementPositionSnapCm, 25);
	TestEqual(TEXT("자유 배치는 권위 좌표의 최소 1cm까지 허용한다"),
		UFF_BuildModeSubsystem::DefaultFreePlacementPositionSnapCm, 1);
	TestEqual(TEXT("기본 입력에는 1m 위치 맞춤을 사용한다"),
		ResolvePlacementPositionSnapCm(false, false, 100, 25, 1), 100);
	TestEqual(TEXT("Ctrl은 25cm 정밀 맞춤을 사용한다"),
		ResolvePlacementPositionSnapCm(true, false, 100, 25, 1), 25);
	TestEqual(TEXT("Alt는 1cm 자유 배치를 사용한다"),
		ResolvePlacementPositionSnapCm(false, true, 100, 25, 1), 1);
	TestEqual(TEXT("Ctrl과 Alt를 함께 누르면 더 자유로운 Alt가 우선한다"),
		ResolvePlacementPositionSnapCm(true, true, 100, 25, 1), 1);
	TestEqual(TEXT("1cm 자유 배치에서도 측정용 1m 격자는 유지한다"),
		ResolveVisualGridStepCm(1, 100), 100);
	TestEqual(TEXT("25cm 정밀 배치는 보조 격자와 실제 맞춤이 일치한다"),
		ResolveVisualGridStepCm(25, 100), 25);

	using namespace FFZSPlacementGridModel;
	TestEqual(TEXT("음수 좌표의 격자 하한 정렬은 0 쪽으로 잘못 당기지 않는다"),
		AlignDownToGrid(-10.0, 100), -100.0);
	TestEqual(TEXT("음수 좌표의 격자 상한 정렬은 다음 기준선 0이다"),
		AlignUpToGrid(-10.0, 100), 0.0);
	const float PreviousGridHalfExtentCm = ResolveGridHalfExtentCm(
		600, 300.0f, 1800.0f, 100);
	const float ExpandedGridHalfExtentCm = ResolveGridHalfExtentCm(
		600, 600.0f, 3600.0f, 100);
	TestEqual(TEXT("600cm footprint와 확대된 600cm 여백은 반경 900cm다"),
		ExpandedGridHalfExtentCm, 900.0f);
	TestEqual(TEXT("시설 바깥으로 보이는 격자 여백은 기존 300cm에서 정확히 두 배다"),
		ExpandedGridHalfExtentCm - 300.0f,
		(PreviousGridHalfExtentCm - 300.0f) * 2.0f);
	TestEqual(TEXT("소프트 최대 반경은 4000cm 대형 시설 자체를 자르지 않는다"),
		ResolveGridHalfExtentCm(4000, 300.0f, 1800.0f, 100), 2000.0f);
	TestTrue(TEXT("음수 방향 5m 기준선도 굵은 선으로 판정한다"),
		IsMajorGridCoordinate(-500.0, 500));
	TestEqual(TEXT("패치 중심은 완전한 격자 농도를 유지한다"),
		ResolveEdgeOpacity(0.0, 0.0, 1000.0), 1.0f);
	TestEqual(TEXT("패치 가장자리는 지형을 가리지 않도록 25퍼센트로 흐려진다"),
		ResolveEdgeOpacity(1000.0, 0.0, 1000.0), 0.25f);

	return true;
}

#endif
