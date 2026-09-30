// 준비 패널의 공통 제작 틀. 저장 후 스타일·여백·글꼴은 각 WBP Designer가 소유한다.
#pragma once

#include "CoreMinimal.h"
#include "UI/Core/FF_UIWbpBuilderCommon.h"

class UBorder;
class UButton;
class UHorizontalBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;

namespace FFPrepPanelSkyBuilder
{
	FLinearColor Color(uint8 R, uint8 G, uint8 B);
	extern const FLinearColor Paper, Ink, Muted, SkyLine, SkyWash, Green;
	FSlateBrush Brush(FLinearColor Fill, float Radius = 0.f, float Width = 0.f,
		FLinearColor Stroke = FLinearColor::Transparent);
	void Row(UVerticalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false);
	void Column(UHorizontalBox& Parent, UWidget& Child, FMargin Padding = FMargin(0.f), bool bFill = false);
	UTextBlock* Text(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, const FText& Preview,
		int32 FontSize = 18, FLinearColor Tint = Ink, bool bVariable = false);
	USizeBox* Rule(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, bool bVertical = false);
	UButton* Button(UWidgetBlueprint& BP, UWidgetTree& Tree, const TCHAR* Name, UWidget& Content, bool bPrimary = false);
	// 글꼴 기준선 대신 정사각 아이콘을 사용해 X의 시각적 중심을 맞춘다.
	USizeBox* CloseIcon(UWidgetBlueprint& BP, UWidgetTree& Tree);
	struct FPanelFrame
	{
		UVerticalBox* Body;
		UVerticalBox* Footer;
	};
	// 제목/닫기는 고정, 본문만 스크롤, 실행 영역은 항상 하단에 남긴다.
	FPanelFrame MakeFrame(UWidgetBlueprint& BP, UWidgetTree& Tree, const FText& Title, const FString& IconPackage);
}
