// FF_BuildTopButtonStyle.cpp

#include "Building/UI/FF_BuildTopButtonStyle.h"

#include "Brushes/SlateNoResource.h"

UFF_BuildTopButtonStyle::UFF_BuildTopButtonStyle()
{
	bSingleMaterial = false;
	NormalBase = FSlateNoResource();
	NormalHovered = FSlateNoResource();
	NormalPressed = FSlateNoResource();
	SelectedBase = FSlateNoResource();
	SelectedHovered = FSlateNoResource();
	SelectedPressed = FSlateNoResource();
	Disabled = FSlateNoResource();

	// 외곽 기본 브러시가 사라진 뒤에도 WBP의 112x68 Root_Size가 클릭 영역을 보장한다.
	ButtonPadding = FMargin(0.0f);
	CustomPadding = FMargin(0.0f);
	MinWidth = 0;
	MinHeight = 0;
	MaxWidth = 0;
	MaxHeight = 0;
}
