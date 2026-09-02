// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "UI/Atlas/FF_AtlasScreenGeo.h"
#include "FF_LivePanelBase.generated.h"

class UButton;
class UFF_LiveHudScreen;
class UTextBlock;

/**
 * 라이브 HUD 왼쪽에서 열리는 패널의 베이스. **U6 의 패널 7종이 여기서 파생한다.**
 *
 * === 패널은 자기 가시성을 소유하지 않는다 ===
 * 여는 것도 닫는 것도 `UFF_LiveHudScreen` 이다. 패널이 스스로 `SetVisibility` 를 부르면
 * 배타 규칙의 주체가 둘로 갈려 "어느 쪽이 이겼는지 알 수 없는" 상태가 된다
 * (ui-antipatterns-to-avoid 10번). 닫아야 할 일이 있으면 `RequestClose()` 로 **바에 요청**한다.
 *
 * === 🔴 여기는 절대 모달이 아니다 ===
 * 준비 단계 패널보다 이 규칙이 더 강하다. **Live 시간은 패널이 열려 있어도 계속 흐르고**,
 * 사건은 계속 발생한다 (AGENTS.md 규칙 1). 입력 모드를 가져가지 않으려고 CommonUI 도 쓰지 않는다.
 *
 * === 준비 단계 패널과 다른 점 ===
 * 끌어 옮기기가 없다. 라이브 패널은 전부 왼쪽 고정이고, 운영 중에 패널을 끌다가 월드 클릭을
 * 놓치는 쪽이 손해다. 자리를 바꾸고 싶으면 `FFF_LivePanelLayout` 의 폭을 조정한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel Base"))
class FESTAFIESTA_API UFF_LivePanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 바가 패널을 만든 직후 한 번 부른다. 어느 탭의 패널인지와 제목을 함께 넣는다.
	 * 따로 두면 제목만 바뀌고 탭이 남아 "대기열 패널에 SNS 제목"이 뜬다.
	 */
	void InitializePanel(UFF_LiveHudScreen* InOwningScreen, EFF_LiveTab InTab, const FText& InTitle);

	/** 패널이 화면에 올라올 때마다. 값 갱신·구독은 여기서 한다. */
	void HandlePanelOpened();

	/** 패널이 내려갈 때마다. 구독 해제는 여기서 한다. */
	void HandlePanelClosed();

	EFF_LiveTab GetTab() const { return Tab; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live")
	UFF_LiveHudScreen* GetOwningScreen() const { return OwningScreen.Get(); }

	/** 이 패널을 닫아 달라고 **바에 요청한다.** 직접 숨기지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live")
	void RequestClose();

protected:
	virtual void NativeOnInitialized() override;

	/** 오른쪽 위 닫기 버튼. 없으면 하단 바의 탭을 다시 눌러 닫는다. */
	UFUNCTION()
	void HandleCloseClicked();

	// =========================================================================
	// 끌어 옮기기 — 준비 단계(`UFF_PrepPanelBase`)와 같은 조작이다
	//
	// 패널의 **빈 곳**을 잡아 끌면 화면이 자리를 옮긴다. 버튼·목록 위에서는 그 위젯이 먼저
	// 입력을 먹으므로 여기까지 오지 않는다 — 그래서 별도 드래그 손잡이를 두지 않았다.
	//
	// 🔴 함정 두 개를 여기서 막는다 (준비 단계에서 이미 밟았다).
	// 1. **위젯 밖으로 나가면 드래그가 끊긴다.** 마우스 커서가 보이는 입력 모드는 캡처가
	//    `NoCapture` 다. `CaptureMouse` 를 돌려주지 않으면 커서가 패널을 벗어난 순간
	//    이동이 멈추고 버튼을 뗀 것도 못 받는다.
	// 2. **화면 픽셀을 슬레이트 단위로 바꿔야 한다.** 포인터 좌표는 픽셀이고 캔버스 슬롯은
	//    슬레이트 단위다. UI 배율이 1 인 환경에서는 티가 안 나고 다른 해상도에서만 어긋난다.
	// =========================================================================

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/**
	 * 🔴 **끄는 동안의 이동은 여기서 처리한다. 마우스 이동 이벤트로 하지 않는다.**
	 *
	 * 이 게임은 입력 모드가 `FInputModeGameOnly` 라 뷰포트가 마우스를 가져가는 구간이 있고,
	 * 그동안 `NativeOnMouseMove` 가 위젯까지 안 온다. 그래서 이벤트로 끌면 —
	 *   · 패널을 연 뒤 **첫 드래그가 통째로 먹힌다** (두 번째부터 되는 이유)
	 *   · 빠르게 끌어 커서가 패널 밖으로 나가면 거기서 끊긴다 (캡처를 안 쓰므로)
	 * 커서 위치와 버튼 상태를 매 틱 직접 읽으면 둘 다 사라진다 (2026-08-28).
	 */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** 끌어 옮기기를 이 패널에서 허용할지. 목록을 스크롤로 끄는 패널은 끈다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	bool bAllowDragToMove = true;

	/** C++ 파생용. `Super::` 를 부른 뒤 자기 것을 한다. */
	virtual void NativeOnPanelOpened() {}
	virtual void NativeOnPanelClosed() {}

	/** BP 파생용. 이름으로 BP 함수를 찾지 않기 위해 명시적으로 선언한다 (안티패턴 17번). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Live", meta = (DisplayName = "On Panel Opened"))
	void K2_OnPanelOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Live", meta = (DisplayName = "On Panel Closed"))
	void K2_OnPanelClosed();

	/** 제목. 바가 탭 라벨에서 넣어 주므로 WBP 에 문구를 박지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UTextBlock> Text_PanelTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UButton> Button_Close;

private:
	UPROPERTY()
	TWeakObjectPtr<UFF_LiveHudScreen> OwningScreen;

	/** 임계 판정과 이동량 누적. 아틀라스 지구본과 같은 것을 쓴다. */
	FFF_AtlasDragTracker DragTracker;

	EFF_LiveTab Tab = EFF_LiveTab::Operations;
};

/**
 * 아직 만들지 않은 탭에 띄우는 자리표시 패널. **패널을 만들면 매핑에서 빠진다.**
 *
 * 빈 패널을 띄우는 대신 "무엇이 여기 들어올 예정인지"를 적는다. 아무것도 안 뜨면 탭이
 * 고장 난 것으로 보이고, 빈 상자만 뜨면 만들다 만 것으로 보인다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Placeholder Panel"))
class FESTAFIESTA_API UFF_LivePlaceholderPanel : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	/** 바가 넣어 준다. `U6 에서 만듭니다` 같은 한 줄. */
	void SetPlaceholderMessage(const FText& InMessage);

protected:
	virtual void NativeOnPanelOpened() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UTextBlock> Text_Placeholder;

private:
	FText PlaceholderMessage;
};
