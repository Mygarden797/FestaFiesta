// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ClosingHUD/FF_ClosingPanelBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/ClosingHUD/FF_ClosingHudScreen.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/FF_UILog.h"

// =============================================================================
// UFF_ClosingPanelBase
// =============================================================================

void UFF_ClosingPanelBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_ClosingPanelBase::HandleCloseClicked);

		// 🔴 `×` 에 툴팁을 달지 않는다. 모르는 사람이 없어서 안내가 아니라 가림이다
		// (2026-08-29 결정). 생성기에서 빼도 이미 만든 WBP 에는 남아 여기서 비운다.
		Button_Close->SetToolTipText(FText::GetEmpty());
	}
}

void UFF_ClosingPanelBase::InitializePanel(
	UFF_ClosingHudScreen* InOwningScreen, EFF_ClosingTab InTab, const FText& InTitle)
{
	OwningScreen = InOwningScreen;
	Tab = InTab;

	if (Text_PanelTitle)
	{
		Text_PanelTitle->SetText(InTitle);
	}
}

void UFF_ClosingPanelBase::HandlePanelOpened()
{
	NativeOnPanelOpened();
	K2_OnPanelOpened();
}

void UFF_ClosingPanelBase::HandlePanelClosed()
{
	NativeOnPanelClosed();
	K2_OnPanelClosed();
}

void UFF_ClosingPanelBase::HandleCloseClicked()
{
	if (UFF_ClosingHudScreen* Screen = OwningScreen.Get())
	{
		Screen->CloseCurrentPanel();
		return;
	}

	// 스스로 숨지 않는다. 그러면 하단 바의 탭이 눌린 채로 남아 화면과 상태가 갈라진다.
	UE_LOG(LogFestaFiestaUI, Warning,
		TEXT("[%s] 소유 화면이 없어 닫아 달라고 요청할 수 없다."), *GetName());
}

void UFF_ClosingPanelBase::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

// =============================================================================
// 끌어 옮기기 — 준비·라이브와 같은 구현이다
// =============================================================================

FReply UFF_ClosingPanelBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const UFF_ClosingHudScreen* Screen = OwningScreen.Get();
	if (!bAllowDragToMove || !Screen || !Screen->IsPanelDragAllowed()
		|| InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	DragTracker.Press(InMouseEvent.GetScreenSpacePosition());

	// 🔴 `CaptureMouse` 를 쓰지 않는다. 이 게임은 입력 모드가 `FInputModeGameOnly` 라
	// 캡처하는 순간 뷰포트가 커서를 숨기고 가둔다 (`prep-hud-reference.md` 3절).
	return FReply::Handled();
}

FReply UFF_ClosingPanelBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 이동 자체는 `NativeTick` 이 처리한다 — 이벤트가 안 오는 구간이 있어서다.
	return FReply::Handled();
}

FReply UFF_ClosingPanelBase::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	DragTracker.Release();
	return FReply::Handled();
}

void UFF_ClosingPanelBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!DragTracker.IsPressed() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const FSlateApplication& Slate = FSlateApplication::Get();

	// 🔴 캡처를 안 쓰므로 패널 밖에서 뗀 것을 못 받는다. 여기서 정리하지 않으면
	// 손을 뗀 뒤에도 패널이 커서를 따라다닌다.
	if (!Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		DragTracker.Release();
		return;
	}

	const FVector2D PixelDelta = DragTracker.AddMove(Slate.GetCursorPos());
	if (PixelDelta.IsNearlyZero())
	{
		return;
	}

	UFF_ClosingHudScreen* Screen = OwningScreen.Get();
	if (!Screen)
	{
		return;
	}

	// 🔴 픽셀 → 슬레이트 단위. 이 나눗셈을 빼면 다른 해상도에서 어긋난다.
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	Screen->NudgePanelPosition(ViewportScale > 0.f ? PixelDelta / ViewportScale : PixelDelta);
}

// =============================================================================
// UFF_ClosingPlaceholderPanel
// =============================================================================

void UFF_ClosingPlaceholderPanel::SetPlaceholderMessage(const FText& InMessage)
{
	PlaceholderMessage = InMessage;
	SetTextOrHide(Text_Placeholder, PlaceholderMessage);
}

void UFF_ClosingPlaceholderPanel::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// 화면이 `InitializePanel` 보다 먼저 문구를 넣었을 수 있다. 열 때 한 번 더 반영한다.
	SetTextOrHide(Text_Placeholder, PlaceholderMessage);
}
