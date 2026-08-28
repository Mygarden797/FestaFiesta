// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LivePanelBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"

// =============================================================================
// UFF_LivePanelBase
// =============================================================================

void UFF_LivePanelBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_LivePanelBase::HandleCloseClicked);
	}
}

void UFF_LivePanelBase::HandleCloseClicked()
{
	RequestClose();
}

void UFF_LivePanelBase::InitializePanel(UFF_LiveHudScreen* InOwningScreen, EFF_LiveTab InTab, const FText& InTitle)
{
	OwningScreen = InOwningScreen;
	Tab = InTab;

	if (Text_PanelTitle)
	{
		Text_PanelTitle->SetText(InTitle);
	}
}

void UFF_LivePanelBase::HandlePanelOpened()
{
	NativeOnPanelOpened();
	K2_OnPanelOpened();
}

void UFF_LivePanelBase::HandlePanelClosed()
{
	NativeOnPanelClosed();
	K2_OnPanelClosed();
}

void UFF_LivePanelBase::RequestClose()
{
	if (UFF_LiveHudScreen* Screen = OwningScreen.Get())
	{
		Screen->CloseCurrentPanel();
		return;
	}

	// 스스로 숨기지 않는다. 그러면 하단 바의 탭이 눌린 채로 남아 화면과 상태가 갈라진다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 소유 화면이 없어 닫아 달라고 요청할 수 없다."), *GetName());
}

// =============================================================================
// UFF_LivePlaceholderPanel
// =============================================================================

void UFF_LivePlaceholderPanel::SetPlaceholderMessage(const FText& InMessage)
{
	PlaceholderMessage = InMessage;

	if (Text_Placeholder)
	{
		Text_Placeholder->SetText(PlaceholderMessage);
	}
}

void UFF_LivePlaceholderPanel::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 바가 `InitializePanel` 보다 먼저 문구를 넣었을 수 있다. 열 때 한 번 더 반영한다.
	if (Text_Placeholder)
	{
		Text_Placeholder->SetText(PlaceholderMessage);
	}
}
