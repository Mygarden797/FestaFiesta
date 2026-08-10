// FF_BuildMinorSection.cpp

#include "Building/UI/FF_BuildMinorSection.h"

#include "CommonTextBlock.h"
#include "Components/Widget.h"
#include "Components/WrapBox.h"

void UFF_BuildMinorSection::SetSectionTitle(const FText& InTitle)
{
	// BindWidget이 아직 만들어지기 전 호출될 수 있으므로 먼저 상태를 저장한다.
	// 위젯이 이미 존재하는 일반 런타임 경로에서는 같은 호출에서 즉시 화면도 갱신한다.
	SectionTitle = InTitle;
	if (Text_MinorTitle)
	{
		Text_MinorTitle->SetText(SectionTitle);
	}
}

void UFF_BuildMinorSection::AddCard(UWidget* Card)
{
	// Box_Cards는 WBP 계약이고 Card는 화면에서 동적으로 생성된다. 어느 한쪽이라도 유효하지 않으면
	// 부분 생성 상태로 간주해 조용히 건너뛰며, AddChildToWrapBox 이후 자식 수명은 패널이 관리한다.
	if (Box_Cards && Card)
	{
		Box_Cards->AddChildToWrapBox(Card);
	}
}

void UFF_BuildMinorSection::ClearCards()
{
	// 섹션 재사용이나 수동 초기화를 지원한다. 패널이 없는 에디터 생성 중간 단계에서도 안전하다.
	if (Box_Cards)
	{
		Box_Cards->ClearChildren();
	}
}

void UFF_BuildMinorSection::NativePreConstruct()
{
	Super::NativePreConstruct();
	// SetSectionTitle이 WidgetTree 생성 전에 호출되었거나 디자이너 속성이 바뀐 경우,
	// 첫 표시 직전에 저장된 SectionTitle을 실제 텍스트 위젯으로 다시 투영한다.
	if (Text_MinorTitle)
	{
		Text_MinorTitle->SetText(SectionTitle);
	}
}
