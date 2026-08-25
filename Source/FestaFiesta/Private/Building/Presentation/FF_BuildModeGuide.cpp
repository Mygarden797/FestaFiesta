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
	const float CurrentYawDegrees,
	const float SnapDegrees)
{
	const float SafeYawDegrees = FMath::IsFinite(CurrentYawDegrees) ? CurrentYawDegrees : 0.0f;
	const float SafeSnapDegrees = FMath::IsFinite(SnapDegrees) && SnapDegrees > 0.0f
		? SnapDegrees
		: 45.0f;
	const bool bChanged = bPlacementPreviewActive != bInPlacementPreviewActive ||
		!FMath::IsNearlyEqual(CachedYawDegrees, SafeYawDegrees, 0.005f) ||
		!FMath::IsNearlyEqual(CachedSnapDegrees, SafeSnapDegrees, 0.005f);

	bPlacementPreviewActive = bInPlacementPreviewActive;
	CachedYawDegrees = SafeYawDegrees;
	CachedSnapDegrees = SafeSnapDegrees;
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
	SetNamedText(
		TEXT("TXT_GuideRow_2"),
		FText::Format(LOCTEXT("SnapRotation", "[Q] [E]  {0}° 스냅 회전"), SnapText));
	SetNamedText(
		TEXT("TXT_GuideRow_3"),
		FText::Format(
			LOCTEXT("FreeRotation", "[Shift]+[Q/E]  자유 회전 · 현재 {0}°"),
			YawText));
	SetNamedText(
		TEXT("TXT_GuideRow_4"),
		LOCTEXT("CommitOrCancel", "[좌클릭/Enter]  확정 · [우클릭]  취소"));
}

#undef LOCTEXT_NAMESPACE
