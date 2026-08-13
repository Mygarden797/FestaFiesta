// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/FF_LiveHudMenuBar.h"

#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"

#define LOCTEXT_NAMESPACE "FestaFiestaUI"

// =============================================================================
// UFF_LiveHudTabButton
// =============================================================================

void UFF_LiveHudTabButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_Root)
	{
		Button_Root->OnClicked.AddUniqueDynamic(this, &UFF_LiveHudTabButton::HandleClicked);
	}
	else
	{
		// 이게 없으면 탭이 눌리지 않는데 화면은 정상으로 보인다. 조용히 넘기면 원인을 못 찾는다.
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Button_Root가 없어 탭을 누를 수 없다."), *GetName());
	}

	RefreshVisualState();
}

void UFF_LiveHudTabButton::Configure(EFF_LiveHudTab InTab, const FText& InLabel, const FSlateBrush& InIconBrush)
{
	Tab = InTab;

	if (Text_Label)
	{
		Text_Label->SetText(InLabel);
	}

	// 리소스가 없는 브러시를 넣으면 아이콘이 빈 사각형이 된다. 그때는 WBP 기본값을 유지한다.
	if (Image_Icon && InIconBrush.GetResourceObject())
	{
		Image_Icon->SetBrush(InIconBrush);
	}

	RefreshVisualState();
}

void UFF_LiveHudTabButton::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
	RefreshVisualState();
}

void UFF_LiveHudTabButton::RefreshVisualState()
{
	// 상태 표현을 한 곳에서 "전체를 다시 선언"한다. 어떤 순서로 호출돼도 결과가 같다(멱등).
	const FLinearColor ContentColor = bSelected ? SelectedContentColor : NormalContentColor;

	if (Text_Label)
	{
		Text_Label->SetColorAndOpacity(FSlateColor(ContentColor));
	}

	if (Image_Icon)
	{
		// 흰색 단색 아이콘을 틴트한다. 상태별로 텍스처를 따로 두지 않는다.
		Image_Icon->SetColorAndOpacity(ContentColor);
	}

	if (!Button_Root)
	{
		return;
	}

	// 배경은 버튼 스타일이 그린다. 선택 상태에서는 Normal 브러시를 Selected 브러시로 바꿔 끼운다.
	// 색만 바꾸지 않고 배경 형태가 함께 바뀌어야 한다 (AT-011).
	FButtonStyle Style = Button_Root->GetStyle();
	Style.SetNormal(bSelected ? Style.Pressed : Style.Hovered);
	Button_Root->SetStyle(Style);
}

void UFF_LiveHudTabButton::HandleClicked()
{
	OnTabButtonClicked.ExecuteIfBound(Tab);
}

// =============================================================================
// UFF_LiveHudMenuBar
// =============================================================================

void UFF_LiveHudMenuBar::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildTabs();

	// 초기 선택은 운영이다. 목업 01-operations 기준.
	SelectTab(EFF_LiveHudTab::Operations);
}

void UFF_LiveHudMenuBar::BuildTabs()
{
	if (!Box_Tabs)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("[%s] Box_Tabs가 없어 탭을 만들 수 없다."), *GetName());
		return;
	}

	if (!TabButtonClass)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] TabButtonClass가 비어 있다. Class Defaults에서 WBP_FF_LiveHudTabButton을 지정한다."), *GetName());
		return;
	}

	Box_Tabs->ClearChildren();
	TabButtons.Reset();

	// 화면 문구를 C++ 문자열로 하드코딩하지 않고 enum의 DisplayName을 읽는다.
	// 탭을 추가·삭제할 때 enum 한 곳만 고치면 된다.
	const UEnum* TabEnum = StaticEnum<EFF_LiveHudTab>();
	if (!TabEnum)
	{
		return;
	}

	// enum의 마지막 항목은 _MAX 자동 항목이므로 제외한다.
	const int32 TabCount = TabEnum->NumEnums() - 1;

	for (int32 Index = 0; Index < TabCount; ++Index)
	{
		const EFF_LiveHudTab TabValue = static_cast<EFF_LiveHudTab>(TabEnum->GetValueByIndex(Index));

		UFF_LiveHudTabButton* TabButton = CreateWidget<UFF_LiveHudTabButton>(this, TabButtonClass);
		if (!TabButton)
		{
			continue;
		}

		TabButton->Configure(
			TabValue,
			TabEnum->GetDisplayNameTextByIndex(Index),
			ResolveTabIcon(TabValue, TabEnum->GetNameStringByIndex(Index)));
		TabButton->OnTabButtonClicked.BindUObject(this, &UFF_LiveHudMenuBar::HandleTabButtonClicked);

		// 지역 변수 이름을 Slot으로 두면 UUserWidget::Slot을 가려 C4458이 난다(경고를 에러로 취급).
		if (UHorizontalBoxSlot* TabSlot = Box_Tabs->AddChildToHorizontalBox(TabButton))
		{
			// 마지막 탭 뒤에는 간격을 두지 않는다. 레일 오른쪽 여백이 어긋난다.
			const float RightGap = (Index == TabCount - 1) ? 0.f : TabGap;
			TabSlot->SetPadding(FMargin(0.f, 0.f, RightGap, 0.f));
			TabSlot->SetVerticalAlignment(VAlign_Center);
		}

		TabButtons.Add(TabButton);
	}
}

FSlateBrush UFF_LiveHudMenuBar::ResolveTabIcon(EFF_LiveHudTab Tab, const FString& EnumName) const
{
	// 예외로 지정된 것이 있으면 그것을 쓴다.
	if (const FSlateBrush* Override = TabIconOverrides.Find(Tab))
	{
		return *Override;
	}

	FSlateBrush Brush;

	if (TabIconFolder.IsEmpty() || TabIconNameFormat.IsEmpty())
	{
		return Brush;
	}

	// enum 이름으로 경로를 만든다. 탭을 추가하면 아이콘 파일만 규칙에 맞춰 넣으면 된다.
	const FString AssetName = FString::Format(*TabIconNameFormat, { EnumName });
	const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *TabIconFolder, *AssetName, *AssetName);

	// 작은 UI 아이콘 9개라 동기 로드로 둔다. 위젯 생성 시 한 번만 돈다.
	UTexture2D* IconTexture = LoadObject<UTexture2D>(nullptr, *ObjectPath);
	if (!IconTexture)
	{
		// 조용히 넘기면 아이콘 없는 탭이 왜 그런지 알 수 없다.
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("[%s] 탭 아이콘을 찾지 못했다 — %s. TabIconFolder를 확인한다."), *GetName(), *ObjectPath);
		return Brush;
	}

	Brush.SetResourceObject(IconTexture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetImageSize(FVector2D(TabIconSize, TabIconSize));
	return Brush;
}

void UFF_LiveHudMenuBar::SelectTab(EFF_LiveHudTab Tab)
{
	SelectedTab = Tab;

	// 전체를 다시 선언한다. 이전 선택을 지우고 새 것을 켜는 식으로 하면 상태가 두 개 켜질 수 있다.
	for (const TObjectPtr<UFF_LiveHudTabButton>& TabButton : TabButtons)
	{
		if (TabButton)
		{
			TabButton->SetSelected(TabButton->GetTab() == Tab);
		}
	}

	OnTabSelected.Broadcast(Tab);
}

void UFF_LiveHudMenuBar::HandleTabButtonClicked(EFF_LiveHudTab Tab)
{
	// 같은 탭을 다시 눌러도 방송한다. 패널을 닫았다가 다시 여는 동작이 있을 수 있다.
	SelectTab(Tab);
}

#undef LOCTEXT_NAMESPACE
