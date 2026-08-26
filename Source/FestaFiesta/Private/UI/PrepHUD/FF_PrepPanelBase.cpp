// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepPanelBase.h"

#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

// =============================================================================
// UFF_PrepPanelBase
// =============================================================================

void UFF_PrepPanelBase::InitializePanel(UFF_PrepHudBar* InOwningBar, EFF_PrepHudTab InTab, const FText& InTitle)
{
	OwningBar = InOwningBar;
	Tab = InTab;

	if (Text_PanelTitle)
	{
		Text_PanelTitle->SetText(InTitle);
	}
}

void UFF_PrepPanelBase::HandlePanelOpened()
{
	NativeOnPanelOpened();
	K2_OnPanelOpened();
}

void UFF_PrepPanelBase::HandlePanelClosed()
{
	NativeOnPanelClosed();
	K2_OnPanelClosed();
}

void UFF_PrepPanelBase::RequestClose()
{
	if (UFF_PrepHudBar* Bar = OwningBar.Get())
	{
		Bar->CloseCurrentPanel();
		return;
	}

	// 스스로 숨기지 않는다. 그러면 바의 탭은 눌린 채로 남아 화면과 상태가 갈라진다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 소유 바가 없어 패널을 닫을 수 없다. InitializePanel 이 불렸는지 확인한다."),
		*GetName());
}

// =============================================================================
// UFF_PrepPlaceholderPanel
// =============================================================================

void UFF_PrepPlaceholderPanel::SetPlaceholderMessage(const FText& InMessage)
{
	PlaceholderMessage = InMessage;

	if (Text_Placeholder)
	{
		Text_Placeholder->SetText(PlaceholderMessage);
	}
}

void UFF_PrepPlaceholderPanel::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 바가 `SetPlaceholderMessage` 를 위젯 생성 직후에 부르는데, 그때 `Text_Placeholder` 는
	// 이미 존재한다. 그래도 여기서 한 번 더 반영해 두면 열 때마다 화면이 값과 일치한다.
	if (Text_Placeholder)
	{
		Text_Placeholder->SetText(PlaceholderMessage);
	}
}
