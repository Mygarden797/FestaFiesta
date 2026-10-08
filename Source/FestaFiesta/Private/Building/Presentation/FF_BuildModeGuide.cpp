// FF_BuildModeGuide.cpp

// 저장된 건축 조작 안내 WBP의 named text를 현재 배치 상태와 동기화한다.
// 흐름: 서브시스템 상태 전달 -> 값 캐시 -> Construct 완료 시 안전한 이름/타입 조회 -> 문구 적용.
// 불변식: WBP 구조와 입력 hit-test 정책을 바꾸지 않으며 텍스트가 없거나 타입이 다르면 해당 행만 생략한다.
// 실패/성능: 같은 상태·각도는 다시 포맷하지 않아 자유 회전 중에도 불필요한 Slate 갱신을 만들지 않는다.

#include "Building/Presentation/FF_BuildModeGuide.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildModeGuide"

FText UFF_BuildModeGuide::MakeBuildOverviewHelpText()
{
	return LOCTEXT("BuildOverviewHelp",
		"격자는 시설을 놓아 볼 후보 바닥, 흰색 선은 건축 경계입니다.\n"
		"시설·대기열·관객석 전체의 배치 가능 여부는 배치 안내에서 확인하세요.\n"
		"구역 밖, 다른 시설이 쓰는 공간, 보호된 길에는 격자가 나타나지 않습니다.\n"
		"먼 곳은 커서나 카메라를 옮겨 확인하세요.\n"
		"[G] 격자 표시/숨기기 · [WASD] 카메라 이동 · [Q/E] 카메라 회전\n"
		"설치된 시설을 선택해 이동하거나 철거할 수 있습니다.\n"
		"[Ctrl+Z] 실행 취소 · [Ctrl+Y] 다시 실행\n"
		"시설 선택 후 [7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열");
}

FText UFF_BuildModeGuide::MakePlacementHelpText(const bool bInMoving, const bool bInDragPlacementEnabled)
{
	if (!bInMoving && bInDragPlacementEnabled)
	{
		return LOCTEXT("DragPlacementHelp",
			"[좌클릭] 누른 채 바닥을 드래그하면 시설이 이어서 설치됩니다. 버튼을 놓으면 즉시 멈춥니다.\n"
			"첫 시설의 위치와 각도를 기준으로 시설 크기만큼 간격을 맞추며, 빠르게 움직여도 중간 칸을 채웁니다.\n"
			"[Ctrl] 정밀 맞춤 · [Alt] 자유 배치는 첫 시설의 시작 위치에 적용됩니다.\n"
			"[Q/E] 회전 · [Shift+Q/E] 부드럽게 회전하면 드래그가 끝납니다. 다시 클릭해 시작하세요.\n"
			"[Enter/배치 확정] 한 개 설치 · [G] 격자 표시/숨기기\n"
			"[우클릭] 건축모드 UI 다시 표시 (시설 목록으로 돌아가기)\n"
			"[Ctrl+Z] 실행 취소 · [Ctrl+Y] 다시 실행\n"
			"[7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열\n"
			"초록은 배치 가능, 빨강은 배치 불가입니다. 설치할 때마다 비용과 건축 한도를 확인합니다.");
	}
	const FText ConfirmText = bInMoving
		? LOCTEXT("MovingHelpAction", "[좌클릭/Enter] 이동 확정 · [우클릭] 이동 취소")
		: LOCTEXT("PlacingHelpAction", "[좌클릭/Enter] 설치 · 설치 후 같은 시설을 계속 배치\n[우클릭] 건축모드 UI 다시 표시 (시설 목록으로 돌아가기)");
	return FText::Format(
		LOCTEXT("PlacementHelp",
			"격자는 후보 바닥이며, 시설 전체의 배치 가능 여부를 뜻하지 않습니다.\n"
			"시설·대기열·보라색 관객석 전체가 흰색 건축 경계 안에 들어가야 합니다. 구역 사이 빈틈도 피하세요.\n"
			"붉은 사선은 문제가 있는 부분입니다. 배치 안내에서 정확한 원인을 확인하세요.\n"
			"[G] 격자 표시/숨기기 · 먼 곳은 커서를 옮겨 확인\n"
			"[Q/E] 일정 각도로 회전 · [Shift+Q/E] 누르는 동안 부드럽게 회전\n"
			"[Ctrl] 누르는 동안 정밀 위치 맞춤 · [Alt] 누르는 동안 자유 배치\n"
			"{0}\n"
			"[7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열\n"
			"[Ctrl+Z] 실행 취소 · [Ctrl+Y] 다시 실행\n"
			"공간 외에 설치 비용·건축 한도도 충족해야 합니다."),
		ConfirmText);
}

void UFF_BuildModeGuide::SetDragPlacementEnabled(const bool bInDragPlacementEnabled)
{
	if (bDragPlacementEnabled != bInDragPlacementEnabled)
	{
		bDragPlacementEnabled = bInDragPlacementEnabled;
		ApplyGuideText();
	}
}

void UFF_BuildModeGuide::SetPlacementGuideState(
	const bool bInPlacementPreviewActive,
	const bool bInMoving,
	const float CurrentYawDegrees,
	const float RotationSnapDegrees,
	const int32 PositionSnapCm,
	const bool bInPlacementGridVisible)
{
	const float SafeYawDegrees = FMath::IsFinite(CurrentYawDegrees) ? CurrentYawDegrees : 0.0f;
	const float SafeSnapDegrees = FMath::IsFinite(RotationSnapDegrees) && RotationSnapDegrees > 0.0f
		? RotationSnapDegrees
		: 45.0f;
	const int32 SafePositionSnapCm = PositionSnapCm >= 1 && PositionSnapCm <= 1000
		? PositionSnapCm
		: 100;
	const bool bChanged = bPlacementPreviewActive != bInPlacementPreviewActive ||
		bMoving != bInMoving ||
		bPlacementGridVisible != bInPlacementGridVisible ||
		!FMath::IsNearlyEqual(CachedYawDegrees, SafeYawDegrees, 0.005f) ||
		!FMath::IsNearlyEqual(CachedSnapDegrees, SafeSnapDegrees, 0.005f) ||
		CachedPositionSnapCm != SafePositionSnapCm;

	bPlacementPreviewActive = bInPlacementPreviewActive;
	bMoving = bInMoving;
	bPlacementGridVisible = bInPlacementGridVisible;
	CachedYawDegrees = SafeYawDegrees;
	CachedSnapDegrees = SafeSnapDegrees;
	CachedPositionSnapCm = SafePositionSnapCm;
	if (bChanged)
	{
		ApplyGuideText();
	}
}

void UFF_BuildModeGuide::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyGuideText();
}

void UFF_BuildModeGuide::ApplyGuideText()
{
	const auto SetNamedText = [this](const FName WidgetName, const FText& Text)
	{
		if (UTextBlock* TextBlock = Cast<UTextBlock>(GetWidgetFromName(WidgetName)))
		{
			TextBlock->SetText(Text);
		}
	};
	SetNamedText(TEXT("TXT_GuideRow_0"), LOCTEXT("CameraAndPause", "카메라 이동 · 일시정지"));
	SetNamedText(TEXT("TXT_GuideRow_1"), bPlacementGridVisible
		? (bPlacementPreviewActive
			? LOCTEXT("PlacementGridLegend", "격자: 후보 바닥 · 흰선: 건축 경계 · 붉은 사선: 문제 부분")
			: LOCTEXT("BuildGridLegend", "격자: 후보 바닥 · 흰선: 건축 경계"))
		: LOCTEXT("HiddenGridLegend", "격자를 숨겼어요 · [G] 다시 표시"));

	if (!bPlacementPreviewActive)
	{
		SetNamedText(TEXT("TXT_GuideRow_2"), LOCTEXT("CameraRotate", "[Q] [E]  카메라 회전"));
		SetNamedText(TEXT("TXT_GuideRow_3"), LOCTEXT("SelectFacility", "[좌클릭]  철거/이동 대상 선택"));
		SetNamedText(TEXT("TXT_GuideRow_4"), LOCTEXT("OverviewGridAndCancel", "[G] 격자 표시/숨기기 · [우클릭] 선택 취소"));
		return;
	}

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = 0;
	NumberOptions.MaximumFractionalDigits = 2;
	const FText SnapText = FText::AsNumber(CachedSnapDegrees, &NumberOptions);
	const FText YawText = FText::AsNumber(CachedYawDegrees, &NumberOptions);
	const FText PositionSnapText = FText::AsNumber(CachedPositionSnapCm, &NumberOptions);
	const bool bDragPlacement = bDragPlacementEnabled && !bMoving;
	SetNamedText(
		TEXT("TXT_GuideRow_2"),
		FText::Format(
			bDragPlacement
				? LOCTEXT("DragRotationWithModifier", "[Q/E] {0}° · [Shift+Q/E] 부드럽게 · 회전 시 드래그 종료")
				: LOCTEXT("RotationWithModifier", "[Q/E] {0}° 회전 · [Shift+Q/E] 부드럽게 · 현재 {1}°"),
			SnapText,
			YawText));
	SetNamedText(
		TEXT("TXT_GuideRow_3"),
		FText::Format(
			bDragPlacement
				? LOCTEXT("DragPositionSnap", "첫 위치 {0}cm · [Ctrl/Alt] 시작점 조정 · 이후 시설 크기 간격")
				: LOCTEXT("PositionSnapAndGrid", "현재 {0}cm 맞춤 · [Ctrl] 정밀 · [Alt] 자유 · [G] 격자 {1}"),
			PositionSnapText,
			bPlacementGridVisible
				? LOCTEXT("GridOn", "켜짐")
				: LOCTEXT("GridOff", "꺼짐")));
	SetNamedText(
		TEXT("TXT_GuideRow_4"),
		bMoving
			? LOCTEXT("MoveOrCancel", "[좌클릭/Enter]  이동 · [우클릭]  이동 취소")
			: (bDragPlacement
				? LOCTEXT("DragInstallOrCatalog", "[좌클릭 누른 채 이동] 연속 설치 · 놓으면 정지\n[우클릭] 건축모드 UI 다시 표시")
				: LOCTEXT("RepeatInstallOrCatalog", "[좌클릭/Enter] 한 개씩 설치 · [우클릭] 건축모드 UI 다시 표시")));
}

#undef LOCTEXT_NAMESPACE
