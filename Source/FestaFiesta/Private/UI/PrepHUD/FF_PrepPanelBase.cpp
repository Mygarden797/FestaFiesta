// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/FF_PrepPanelBase.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"
#include "UI/PrepHUD/FF_PrepHudBar.h"

// =============================================================================
// UFF_PrepPanelBase
// =============================================================================

void UFF_PrepPanelBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_PrepPanelBase::HandleCloseClicked);
	}
}

void UFF_PrepPanelBase::HandleCloseClicked()
{
	RequestClose();
}

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
	// 끌던 중에 단계가 바뀌어 패널이 닫히면 누른 상태가 남는다. 다음에 열 때
	// 첫 이동이 이전 드래그로 이어져 패널이 튀는 것을 막는다.
	DragTracker.Reset();

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
// 끌어 옮기기
// =============================================================================

FReply UFF_PrepPanelBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const UFF_PrepHudBar* Bar = OwningBar.Get();
	if (!bAllowDragToMove || !Bar || !Bar->IsPanelDragAllowed()
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	DragTracker.Press(InMouseEvent.GetScreenSpacePosition());

	// 🔴 `CaptureMouse` 를 돌려주지 않으면 커서가 패널을 벗어나는 순간 이동이 멈추고
	//    버튼을 뗀 것도 받지 못한다. 프론트엔드 입력 모드가 `NoCapture` 라 반드시 필요하다.
	return FReply::Handled().CaptureMouse(TakeWidget());
}

FReply UFF_PrepPanelBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

	UFF_PrepHudBar* Bar = OwningBar.Get();
	if (!Bar)
	{
		return FReply::Handled();
	}

	// 🔴 픽셀 → 슬레이트 단위. 캔버스 슬롯은 슬레이트 단위를 쓴다.
	//    이 나눗셈을 빼면 UI 배율이 1 인 환경에서만 맞고 다른 해상도에서 어긋난다.
	//    배율이 0 이하로 올 일은 없지만, 0 으로 나누면 좌표가 NaN 이 되어 패널이 사라진다.
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	Bar->NudgePanelPosition(ViewportScale > 0.f ? PixelDelta / ViewportScale : PixelDelta);
	return FReply::Handled();
}

FReply UFF_PrepPanelBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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
