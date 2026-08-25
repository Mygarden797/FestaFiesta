// FF_BuildModeGuide.cpp

// 저장된 건축 조작 안내 WBP의 named text를 현재 배치 상태와 동기화한다.
// 흐름: 서브시스템 상태 전달 -> 값 캐시 -> Construct 완료 시 안전한 이름/타입 조회 -> 문구 적용.
// 불변식: WBP 구조와 입력 hit-test 정책을 바꾸지 않으며 텍스트가 없거나 타입이 다르면 해당 행만 생략한다.
// 실패/성능: 같은 상태·각도는 다시 포맷하지 않아 자유 회전 중에도 불필요한 Slate 갱신을 만들지 않는다.

#include "Building/Presentation/FF_BuildModeGuide.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FFBuildModeGuide"

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

	if (!bPlacementPreviewActive)
	{
		SetNamedText(TEXT("TXT_GuideRow_2"), LOCTEXT("CameraRotate", "[Q] [E]  카메라 회전"));
		SetNamedText(TEXT("TXT_GuideRow_3"), LOCTEXT("SelectFacility", "[좌클릭]  철거/이동 대상 선택"));
		SetNamedText(TEXT("TXT_GuideRow_4"), LOCTEXT("CancelSelection", "[우클릭]  선택 취소"));
		return;
	}

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = 0;
	NumberOptions.MaximumFractionalDigits = 2;
	const FText SnapText = FText::AsNumber(CachedSnapDegrees, &NumberOptions);
	const FText YawText = FText::AsNumber(CachedYawDegrees, &NumberOptions);
	const FText PositionSnapText = FText::AsNumber(CachedPositionSnapCm, &NumberOptions);
	SetNamedText(
		TEXT("TXT_GuideRow_2"),
		FText::Format(
			LOCTEXT("Rotation", "[Q/E]  {0}° 회전 · [Shift] 부드럽게 · 현재 {1}°"),
			SnapText,
			YawText));
	SetNamedText(
		TEXT("TXT_GuideRow_3"),
		FText::Format(
			LOCTEXT("PositionSnapAndGrid", "현재 {0}cm 맞춤 · [Ctrl] 정밀 · [Alt] 자유 · [G] 격자 {1}"),
			PositionSnapText,
			bPlacementGridVisible
				? LOCTEXT("GridOn", "켜짐")
				: LOCTEXT("GridOff", "꺼짐")));
	SetNamedText(
		TEXT("TXT_GuideRow_4"),
		bMoving
			? LOCTEXT("MoveOrCancel", "[좌클릭/Enter]  이동 · [우클릭]  이동 취소")
			: LOCTEXT("InstallOrCatalog", "[좌클릭/Enter]  설치 · [우클릭]  목록으로"));
}

#undef LOCTEXT_NAMESPACE
