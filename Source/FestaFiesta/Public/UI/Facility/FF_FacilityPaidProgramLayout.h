// 게임 화면과 에디터 제작 도구가 함께 사용하는 공통 규칙이다.
// FF_FacilityPaidProgramLayout.h
// 역할: 프로그램 구매 창의 승인된 두 배치를 생성기와 검증 코드가 같은 기준으로 사용하게 한다.
// 기본 배치는 시설 정보 창과 같은 폭으로 늘어나며, Clarity V2는 고정 높이의 목록/확인 화면이다.
#pragma once

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"

namespace FFFacilityPaidProgramLayout
{
	// 기본 시설 정보 창과 구매 창은 번갈아 나타나므로 폭과 우측/상단 여백을 공유한다.
	inline constexpr float InspectorWidth = 405.0f;
	inline constexpr float RightMargin = 18.0f;
	inline constexpr float TopMargin = 18.0f;
	inline constexpr float InspectorBottomReserve = 126.0f;

	enum class EVariant : uint8
	{
		InspectorDock,
		ClarityV2
	};

	struct FDock
	{
		FAnchors Anchors;
		FVector2D Alignment;
		FMargin Offsets;
	};

	// 아래쪽 앵커가 늘어나면 Bottom은 하단 여백이고, 고정 앵커이면 창의 높이다.
	inline FDock GetDock(const EVariant Variant)
	{
		if (Variant == EVariant::ClarityV2)
		{
			return {FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f),
				FMargin(-RightMargin, TopMargin, 456.0f, 620.0f)};
		}
		return {FAnchors(1.0f, 0.0f, 1.0f, 1.0f), FVector2D(1.0f, 0.0f),
			FMargin(-RightMargin, TopMargin, InspectorWidth, InspectorBottomReserve)};
	}

	// V2 생성기가 넣는 목록 복귀 버튼으로 저장된 화면의 배치를 식별한다. 창 크기로 추측하지 않는다.
	inline EVariant GetVariant(const UWidgetTree& Tree)
	{
		return Tree.FindWidget(TEXT("Button_BackToPrograms"))
			? EVariant::ClarityV2 : EVariant::InspectorDock;
	}

	// 생성/명시적인 디자인 적용 시에만 호출한다. 런타임에 디자이너 배치를 덮어쓰지 않는다.
	inline void Apply(UCanvasPanelSlot& Slot, const EVariant Variant)
	{
		const FDock Dock = GetDock(Variant);
		Slot.SetAnchors(Dock.Anchors);
		Slot.SetAlignment(Dock.Alignment);
		Slot.SetOffsets(Dock.Offsets);
		Slot.SetAutoSize(false);
	}
}
