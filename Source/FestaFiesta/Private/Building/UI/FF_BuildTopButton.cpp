// FF_BuildTopButton.cpp

// 상단 액션 버튼의 투명 스타일과 상태별 콘텐츠 색·선택 표시선을 동기화한다.
// 흐름: 네이티브 기본 스타일 설정 -> 생성 훅 -> 선택/해제 콜백마다 표시선 갱신.
// 불변식: PreConstruct와 런타임은 같은 갱신 함수를 사용해 동일한 선택 상태를 표현한다.
// 실패/성능: 선택 표시 BindWidget이 없으면 갱신만 생략하고 별도 Tick이나 동적 자원을 만들지 않는다.

#include "Building/UI/FF_BuildTopButton.h"

#include "Building/UI/FF_BuildTopButtonStyle.h"
#include "Components/Image.h"

UFF_BuildTopButton::UFF_BuildTopButton()
{
	// 네이티브 기본값으로 지정해 WBP 단독 미리보기와 런타임 모두 기본 회색 SButton을 사용하지 않는다.
	SetStyle(UFF_BuildTopButtonStyle::StaticClass());

	// 공용 버튼의 호버 색은 밝은 Surface 위 사용을 전제로 하므로 투명 상단 버튼에는 밝은 강조색을 쓴다.
	NormalContentColor = FLinearColor(0.91f, 0.96f, 1.0f, 1.0f);
	HighlightedContentColor = FLinearColor(0.08f, 0.95f, 0.88f, 1.0f);
	DisabledContentColor = FLinearColor(0.58f, 0.69f, 0.78f, 1.0f);
}

void UFF_BuildTopButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	RefreshSelectedIndicator();
}

void UFF_BuildTopButton::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshSelectedIndicator();
}

void UFF_BuildTopButton::NativeOnSelected(const bool bBroadcast)
{
	Super::NativeOnSelected(bBroadcast);
	RefreshSelectedIndicator();
}

void UFF_BuildTopButton::NativeOnDeselected(const bool bBroadcast)
{
	Super::NativeOnDeselected(bBroadcast);
	RefreshSelectedIndicator();
}

void UFF_BuildTopButton::RefreshSelectedIndicator()
{
	if (Image_SelectedIndicator)
	{
		Image_SelectedIndicator->SetVisibility(
			GetSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
