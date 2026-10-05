// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_LivePanelBase.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/FF_UILog.h"
#include "UI/LiveHUD/FF_LiveHudScreen.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

// =============================================================================
// UFF_LivePanelBase
// =============================================================================

UFF_LivePanelBase::UFF_LivePanelBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DynamicRowFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16);
}

void UFF_LivePanelBase::ApplyDynamicRowStyle(UTextBlock& TextBlock, const bool bNote) const
{
	TextBlock.SetFont(DynamicRowFont);
	TextBlock.SetColorAndOpacity(FSlateColor(bNote ? DynamicRowNoteColor : DynamicRowColor));
	TextBlock.SetAutoWrapText(true);
}

void UFF_LivePanelBase::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 내부 버튼이라 인스턴스 수명과 같다. 외부 시스템 구독만 패널을 열 때 붙인다.
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &UFF_LivePanelBase::HandleCloseClicked);

		// 🔴 툴팁을 여기서 비운다. 생성기에서 빼는 것만으로는 **이미 만들어진 WBP 에 남는다** —
		// `EnsureWidgetBlueprint` 가 기존 패키지를 건너뛰기 때문이다. 패널 6장을 지우고
		// 다시 만드는 것보다 이쪽이 싸고, 디자이너가 실수로 다시 넣어도 같이 막힌다.
		Button_Close->SetToolTipText(FText::GetEmpty());
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

void UFF_LivePanelBase::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
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

	// 🔴 **`CaptureMouse` 를 쓰지 않는다** (2026-08-28 실기 확인).
	//
	// 이 게임의 플레이어 컨트롤러는 `SetInputMode` 를 부르지 않아 기본값
	// `FInputModeGameOnly` 로 돈다. 그 모드는 `bHideCursorDuringCapture = true` 라
	// **슬레이트가 마우스를 캡처하는 순간 뷰포트가 커서를 숨기고 가둔다.** 그러면 —
	//   ① 커서가 사라지고
	//   ② 좌표가 안 변해서 패널이 안 움직이고
	//   ③ 뗀 이벤트가 이쪽으로 안 와서 `DragTracker` 가 눌린 채로 남는다
	//      → 손을 뗐는데도 패널이 커서를 따라다닌다
	// 패널을 닫았다 열면 그대로 재현됐다.
	//
	// 캡처를 안 하면 커서가 패널을 벗어난 동안 이동 이벤트가 끊긴다. 대신 패널이 커서를
	// 따라오므로 벗어나는 일이 드물고, 돌아오면 절대 좌표 차이만큼 따라붙어 이어진다.
	// 밖에서 뗀 경우는 `NativeOnMouseMove` 가 버튼 상태를 보고 정리한다.
	//
	// ⚠️ 입력 모드를 여기서 바꾸지 않는다. 건축 모드·미니게임이 그 값을 저장하고 되돌리므로
	//    UI 가 끼어들면 서로 덮어쓴다.
	return FReply::Handled();
}

FReply UFF_LivePanelBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 이동 자체는 `NativeTick` 이 처리한다 — 이벤트가 안 오는 구간이 있어서다.
	// 여기서는 이벤트를 먹기만 해서 아래 월드로 새지 않게 한다.
	return FReply::Handled();
}

void UFF_LivePanelBase::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!DragTracker.IsPressed() || !FSlateApplication::IsInitialized())
	{
		return;
	}

	const FSlateApplication& Slate = FSlateApplication::Get();

	// 🔴 **뗀 것을 이벤트로 기다리지 않는다.** 캡처를 안 쓰므로 패널 밖에서 떼면 그 이벤트가
	//    이쪽으로 안 온다. 그대로 두면 손을 뗀 뒤에도 패널이 커서를 따라다닌다.
	if (!Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
	{
		DragTracker.Release();
		return;
	}

	// 임계를 넘기 전에는 0 이 온다. 넘는 순간 쌓인 이동을 몰아 적용하지 않아 툭 튀지 않는다.
	const FVector2D PixelDelta = DragTracker.AddMove(Slate.GetCursorPos());
	if (PixelDelta.IsNearlyZero())
	{
		return;
	}

	UFF_LiveHudScreen* Screen = OwningScreen.Get();
	if (!Screen)
	{
		return;
	}

	// 🔴 픽셀 → 슬레이트 단위. 캔버스 슬롯은 슬레이트 단위를 쓴다.
	//    이 나눗셈을 빼면 UI 배율이 1 인 환경에서만 맞고 다른 해상도에서 어긋난다.
	//    배율이 0 이하로 올 일은 없지만, 0 으로 나누면 좌표가 NaN 이 되어 패널이 사라진다.
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	Screen->NudgePanelPosition(ViewportScale > 0.f ? PixelDelta / ViewportScale : PixelDelta);
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
	return FReply::Handled();
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
