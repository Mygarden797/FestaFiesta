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
		"[TAB] 시점 전환\n"
		"설치된 시설을 클릭하면 시설정보를 확인합니다. 누른 채 드래그할 때만 시설이 이동합니다.\n"
		"[마우스 놓기] 이동 확정 · [우클릭] 이동 취소 후 정보 유지\n"
		"[Ctrl+Z] 실행 취소 · [Ctrl+Y] 다시 실행\n"
        "시설 선택 후 [7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열\n"
        "건축 설정 → 대기열 설정: 정원·자리 간격·통로 폭 · 전체 기본값 적용");
}

FText UFF_BuildModeGuide::MakePlacementHelpText(const bool bInMoving, const bool bInDragPlacementEnabled,
	const int32 DragPlacementGapCm)
{
	if (!bInMoving && bInDragPlacementEnabled)
	{
		return FText::Format(LOCTEXT("DragPlacementGapHelp",
			"첫 설치 전부터 실제 시설의 모양과 재질을 가진 반투명 미리보기가 마우스를 따라갑니다.\n"
			"누른 채 바닥을 드래그하면 설치 예정 시설들이 일정 간격으로 표시됩니다. 마우스를 놓으면 설치 가능한 곳만 한 번에 설치합니다.\n"
			"클릭만 하면 한 개를 설치하며, 설치한 뒤에도 다음 미리보기가 커서를 따라갑니다.\n"
			"첫 시설의 위치와 각도를 기준으로 시설 크기 + 여유 {0}cm 간격을 맞추며, 빠르게 움직여도 중간 칸을 채웁니다.\n"
			"건축 설정의 시설 사이 간격에서 0~1000cm를 지정합니다. 0cm는 시설을 붙여 놓습니다.\n"
			"[Ctrl] 정밀 맞춤 · [Alt] 자유 배치는 첫 시설의 시작 위치에 적용됩니다.\n"
			"[Q/E] 회전 · [Shift+Q/E] 부드럽게 회전하면 드래그 미리보기를 취소합니다. 다시 클릭해 시작하세요.\n"
			"[Enter/배치 확정] 한 개 설치 · [G] 격자 표시/숨기기\n"
			"직접 조정한 위치에서 [좌클릭/Enter]로 설치하면 다음 미리보기가 커서로 돌아옵니다. [T] 커서 배치 전환\n"
			"[TAB] 시점 전환\n"
			"[우클릭] 건축모드 UI 다시 표시 (시설 목록으로 돌아가기)\n"
			"[Ctrl+Z] 드래그 한 번으로 설치한 시설 함께 취소 · [Ctrl+Y] 함께 다시 실행\n"
			"[7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열\n"
            "건축 설정 → 대기열 설정에서 정원·자리 간격·통로 폭을 조절합니다.\n"
			"초록은 배치 가능, 빨강은 배치 불가입니다. 놓을 때 비용·건축 한도를 다시 확인하고, 설치할 수 없는 곳은 건너뜁니다."),
			FText::AsNumber(FMath::Clamp(DragPlacementGapCm, 0, 1000)));
	}
	if (bInMoving)
	{
		return LOCTEXT("HeldMoveHelp",
			"시설정보를 보면서 시설을 누른 채 드래그하세요. 클릭만 하면 정보만 표시합니다.\n"
			"[마우스 놓기] 이동 확정 · [우클릭] 이동 취소 후 정보 유지\n"
			"UI 위에서 놓거나 배치할 수 없는 위치에서 놓으면 원래 위치로 돌아갑니다.\n"
			"[Ctrl] 정밀 위치 맞춤 · [Alt] 자유 배치 · [G] 격자 표시/숨기기\n"
			"[WASD] 카메라 이동 · [TAB] 시점 전환\n"
			"시설·대기열·관객석 전체가 건축 경계 안에 들어가야 합니다.\n"
			"[Ctrl+Z] 실행 취소 · [Ctrl+Y] 다시 실행");
	}
	const FText ConfirmText = LOCTEXT("PlacingHelpAction", "반투명 미리보기가 마우스를 따라갑니다. [좌클릭/Enter] 해당 위치에 설치 · 다음 미리보기로 계속 배치\n[우클릭] 건축모드 UI 다시 표시 (시설 목록으로 돌아가기)");
	return FText::Format(
		LOCTEXT("PlacementHelp",
			"격자는 후보 바닥이며, 시설 전체의 배치 가능 여부를 뜻하지 않습니다.\n"
			"시설·대기열·보라색 관객석 전체가 흰색 건축 경계 안에 들어가야 합니다. 구역 사이 빈틈도 피하세요.\n"
			"붉은 사선은 문제가 있는 부분입니다. 배치 안내에서 정확한 원인을 확인하세요.\n"
			"[G] 격자 표시/숨기기 · 먼 곳은 커서를 옮겨 확인\n"
			"[TAB] 시점 전환\n"
			"[Q/E] 일정 각도로 회전 · [Shift+Q/E] 누르는 동안 부드럽게 회전\n"
			"[Ctrl] 누르는 동안 정밀 위치 맞춤 · [Alt] 누르는 동안 자유 배치\n"
			"직접 조정한 위치에서 [좌클릭/Enter]로 설치하면 다음 미리보기가 커서로 돌아옵니다. [T] 커서 배치 전환\n"
			"{0}\n"
			"[7] 기본 · [8] 좌측 · [9] 우측 · [0] U자 대기열\n"
            "건축 설정 → 대기열 설정에서 정원·자리 간격·통로 폭을 조절합니다.\n"
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

void UFF_BuildModeGuide::SetDragPlacementGapCm(const int32 GapCm)
{
	const int32 SafeGap = FMath::Clamp(GapCm, 0, 1000);
	if (CachedDragPlacementGapCm != SafeGap)
	{
		CachedDragPlacementGapCm = SafeGap;
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
	SetNamedText(TEXT("TXT_GuideRow_0"), LOCTEXT("CameraAndPause", "카메라 이동 · 일시정지 · [TAB] 시점 전환"));
	SetNamedText(TEXT("TXT_GuideRow_1"), bPlacementGridVisible
		? (bPlacementPreviewActive
			? LOCTEXT("PlacementGridLegend", "격자: 후보 바닥 · 흰선: 건축 경계 · 붉은 사선: 문제 부분")
			: LOCTEXT("BuildGridLegend", "격자: 후보 바닥 · 흰선: 건축 경계"))
		: LOCTEXT("HiddenGridLegend", "격자를 숨겼어요 · [G] 다시 표시"));

	if (!bPlacementPreviewActive)
	{
		SetNamedText(TEXT("TXT_GuideRow_2"), LOCTEXT("CameraRotate", "[Q] [E]  카메라 회전"));
		SetNamedText(TEXT("TXT_GuideRow_3"), LOCTEXT("SelectFacility", "[좌클릭] 시설정보 · 누른 채 드래그 이동"));
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
				? LOCTEXT("DragRotationWithModifier", "[Q/E] {0}° · [Shift+Q/E] 부드럽게 · 회전 시 미리보기 취소")
				: LOCTEXT("RotationWithModifier", "[Q/E] {0}° 회전 · [Shift+Q/E] 부드럽게 · 현재 {1}°"),
			SnapText,
			YawText));
	SetNamedText(
		TEXT("TXT_GuideRow_3"),
		FText::Format(
			bDragPlacement
				? LOCTEXT("DragPositionGap", "첫 위치 {0}cm · [Ctrl/Alt] 시작점 · 시설 사이 {2}cm")
				: LOCTEXT("PositionSnapAndGrid", "현재 {0}cm 맞춤 · [Ctrl] 정밀 · [Alt] 자유 · [G] 격자 {1}"),
			PositionSnapText,
			bPlacementGridVisible
				? LOCTEXT("GridOn", "켜짐")
				: LOCTEXT("GridOff", "꺼짐"),
			FText::AsNumber(CachedDragPlacementGapCm)));
	SetNamedText(
		TEXT("TXT_GuideRow_4"),
		bMoving
			? LOCTEXT("MoveOrCancel", "[좌클릭 드래그] 이동 · [마우스 놓기] 확정\n[우클릭] 이동 취소 · 시설정보 유지")
			: (bDragPlacement
				? LOCTEXT("DragInstallStrokeOrCatalog", "[드래그] 간격 맞춤 미리보기 · [놓기] 가능한 곳만 설치\n[Ctrl+Z] 드래그 전체 취소 · [우클릭] 시설 목록")
				: LOCTEXT("RepeatInstallOrCatalog", "[좌클릭/Enter] 한 개씩 설치 · 직접 조정 중 [Enter] 확정\n[T] 커서 배치 · [우클릭] 건축모드 UI 다시 표시")));
}

#undef LOCTEXT_NAMESPACE
