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

	// 🔴 **`CaptureMouse` 를 쓰지 않는다** (2026-08-28 실기 확인. 전에는 캡처했다).
	//
	// `AFF_FestivalPlayerController` 는 `SetInputMode` 를 부르지 않아 기본값
	// `FInputModeGameOnly` 로 돈다. 그 모드는 `bHideCursorDuringCapture = true` 라
	// **슬레이트가 마우스를 캡처하는 순간 뷰포트가 커서를 숨기고 가둔다.** 그러면 —
	//   ① 커서가 사라지고
	//   ② 좌표가 안 변해서 패널이 안 움직이고
	//   ③ 뗀 이벤트가 이쪽으로 안 와서 `DragTracker` 가 눌린 채로 남는다
	//      → 손을 뗐는데도 패널이 커서를 따라다닌다
	//
	// 프론트엔드(타이틀·아틀라스)는 CommonUI 가 입력 모드를 잡아 줘서 캡처가 멀쩡했다.
	// 인게임 HUD 에는 그 전제가 없다 — 같은 코드인데 화면에 따라 다르게 동작했다.
	//
	// 캡처를 안 하면 커서가 패널을 벗어난 동안 이동 이벤트가 끊긴다. 대신 패널이 커서를
	// 따라오므로 벗어나는 일이 드물고, 돌아오면 절대 좌표 차이만큼 따라붙어 이어진다.
	// 밖에서 뗀 경우는 `NativeOnMouseMove` 가 버튼 상태를 보고 정리한다.
	//
	// ⚠️ 입력 모드를 여기서 바꾸지 않는다. 건축 모드·미니게임이 그 값을 저장하고 되돌리므로
	//    UI 가 끼어들면 서로 덮어쓴다.
	return FReply::Handled();
}

FReply UFF_PrepPanelBase::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!DragTracker.IsPressed())
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 🔴 캡처를 안 하므로 **패널 밖에서 뗀 것을 못 받는다.** 그대로 두면 눌린 상태가 남아
	//    손을 뗀 뒤에도 패널이 커서를 따라다닌다. 커서가 돌아온 첫 이동에서 정리한다.
	if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		DragTracker.Release();
		return FReply::Handled();
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

	return FReply::Handled();
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

UClass* UFF_PrepPanelBase::ResolveWidgetClass(UClass* Configured, const FString& FallbackPath)
{
	if (Configured)
	{
		return Configured;
	}

	if (FallbackPath.IsEmpty())
	{
		return nullptr;
	}

	// 블루프린트 클래스는 `_C` 가 붙는다. 이걸 빼면 에셋은 있는데 못 찾는다.
	const FString AssetName = FPaths::GetCleanFilename(FallbackPath);
	const FString ObjectPath = FString::Printf(TEXT("%s.%s_C"), *FallbackPath, *AssetName);
	return LoadObject<UClass>(nullptr, *ObjectPath);
}
