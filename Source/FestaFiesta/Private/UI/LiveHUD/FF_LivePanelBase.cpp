// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LivePanelBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "Blueprint/WidgetLayoutLibrary.h"

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
// 끌어 옮기기 — 준비 단계와 같은 구현이다
// =============================================================================

FReply UFF_LivePanelBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const UFF_LiveHudScreen* Screen = OwningScreen.Get();
	if (!bAllowDragToMove || !Screen || !Screen->IsPanelDragAllowed()
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	DragTracker.Press(InMouseEvent.GetScreenSpacePosition());

	// 🔴 `CaptureMouse` 를 돌려주지 않으면 커서가 패널을 벗어나는 순간 이동이 멈추고
	//    버튼을 뗀 것도 받지 못한다. 캡처가 `NoCapture` 인 입력 모드라 반드시 필요하다.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UFF_LivePanelBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 임계를 넘기 전에는 0 이 온다. 넘는 순간 쌓인 이동을 몰아 적용하지 않아 툭 튀지 않는다.
	const FVector2D PixelDelta = DragTracker.AddMove(InMouseEvent.GetScreenSpacePosition());
	if (PixelDelta.IsNearlyZero())
	{
		return FReply::Handled();
	}

	UFF_LiveHudScreen* Screen = OwningScreen.Get();
	if (!Screen)
	{
		return FReply::Handled();
	}

	// 🔴 픽셀 → 슬레이트 단위. 캔버스 슬롯은 슬레이트 단위를 쓴다.
	//    이 나눗셈을 빼면 UI 배율이 1 인 환경에서만 맞고 다른 해상도에서 어긋난다.
	//    배율이 0 이하로 올 일은 없지만, 0 으로 나누면 좌표가 NaN 이 되어 패널이 사라진다.
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	Screen->NudgePanelPosition(ViewportScale > 0.f ? PixelDelta / ViewportScale : PixelDelta);
	return FReply::Handled();
}

FReply UFF_LivePanelBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	// 돌려주는 값은 "클릭으로 볼지" 인데 패널 배경 클릭에는 동작이 없어 쓰지 않는다.
	// 그래도 상태는 반드시 비운다 — 안 그러면 다음 이동이 이전 드래그로 이어진다.
	DragTracker.Release();
	return FReply::Handled().ReleaseMouseCapture();
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
