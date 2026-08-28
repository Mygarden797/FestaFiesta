// FF_BuildTopButtonStyle.cpp

// 상단 액션 버튼의 모든 상태 브러시를 NoDraw로 초기화하고 공통 크기·색 기본값을 설정한다.
// 흐름: 상태 브러시 초기화 -> 여백 제거 -> 최소/최대 크기를 WBP 레이아웃에 위임.
// 불변식: Normal·Selected·Disabled 상태에서 CommonButton 기본 배경 면이 다시 나타나지 않는다.
// 실패/수명: WBP Root_Size가 클릭 영역을 보장하며 스타일은 외부 브러시 자원을 소유하지 않는다.

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
