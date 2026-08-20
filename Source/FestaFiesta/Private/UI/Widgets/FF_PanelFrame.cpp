// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/FF_PanelFrame.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/FF_UILog.h"

void UFF_PanelFrame::NativePreConstruct()
{
	Super::NativePreConstruct();

	// PreConstruct 에서 칠하는 이유는 디자이너에서도 보여야 하기 때문이다.
	// 색의 출처는 팔레트 하나다 — 디자이너에서 손으로 덮으면 제어 경로가 둘이 된다.
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		// CDO 라 여기 올 일이 없지만, 조용히 넘어가면 색이 안 나오는 이유를 찾을 수 없다.
		UE_LOG(LogFestaFiestaUI, Error, TEXT("UFF_PanelFrame: 팔레트를 읽을 수 없어 색을 적용하지 않았다."));
		return;
	}

	if (Border_Header)
	{
		Border_Header->SetBrushColor(Palette->InGame.Teal);
	}

	if (Text_Title)
	{
		// 머리 바가 Teal 이므로 제목은 White 다.
		Text_Title->SetColorAndOpacity(FSlateColor(Palette->InGame.White));
	}

	if (Border_Body)
	{
		Border_Body->SetBrushColor(Palette->InGame.Paper);
	}
}

void UFF_PanelFrame::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Close)
	{
		// AddDynamic 이 아니라 AddUniqueDynamic 을 쓴다. 비용은 사실상 0 이고
		// 재구성 시 중복 구독으로 닫기가 두 번 발생하는 것을 막는다.
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_PanelFrame::HandleCloseClicked);
	}
}

void UFF_PanelFrame::NativeDestruct()
{
	// **해제는 조건부, 정리는 무조건.** 여기서 조기 반환하면 아래 정리를 못 한다.
	// OnClicked.Clear() 를 쓰지 않는다 — BP 디자이너가 연결한 이벤트까지 지워진다.
	if (Button_Close)
	{
		Button_Close->OnClicked.RemoveDynamic(this, &UFF_PanelFrame::HandleCloseClicked);
	}

	Super::NativeDestruct();
}

void UFF_PanelFrame::HandleCloseClicked()
{
	// 자기를 제거하지 않는다. 무엇이 열려 있는지는 패널을 띄운 쪽이 소유한다.
	OnCloseRequested.Broadcast();
}

void UFF_PanelFrame::SetTitle(const FText& InTitle)
{
	if (!Text_Title)
	{
		UE_LOG(LogFestaFiestaUI, Warning,
			TEXT("UFF_PanelFrame::SetTitle: Text_Title 바인딩이 없다. WBP 위젯 이름을 확인한다."));
		return;
	}

	Text_Title->SetText(InTitle);
}

UWidget* UFF_PanelFrame::AddContent(UWidget* Content)
{
	if (!ScrollBox_Body)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_PanelFrame::AddContent: ScrollBox_Body 바인딩이 없어 내용을 붙이지 못했다."));
		return nullptr;
	}

	if (!Content)
	{
		UE_LOG(LogFestaFiestaUI, Warning, TEXT("UFF_PanelFrame::AddContent: 붙일 위젯이 null 이다."));
		return nullptr;
	}

	ScrollBox_Body->AddChild(Content);
	return Content;
}

void UFF_PanelFrame::ClearContent()
{
	if (!ScrollBox_Body)
	{
		UE_LOG(LogFestaFiestaUI, Error,
			TEXT("UFF_PanelFrame::ClearContent: ScrollBox_Body 바인딩이 없다."));
		return;
	}

	ScrollBox_Body->ClearChildren();
}

void UFF_PanelFrame::SetFooterVisible(bool bVisible)
{
	if (!Panel_Footer)
	{
		// 발이 없는 패널도 정상이다. 경고까지 올리지 않는다.
		return;
	}

	// Hidden 이 아니라 Collapsed 다. Hidden 은 자리를 남겨 빈 칸이 생긴다.
	Panel_Footer->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFF_PanelFrame::SetCloseButtonVisible(bool bVisible)
{
	if (!Button_Close)
	{
		return;
	}

	Button_Close->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
