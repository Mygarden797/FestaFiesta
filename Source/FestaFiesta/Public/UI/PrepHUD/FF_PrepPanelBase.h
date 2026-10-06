// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

// 클릭과 드래그를 임계 거리로 구분하는 순수 구조체. **Atlas 전용이 아니다** —
// 이름만 Atlas 이고 지구본 드래그에서 테스트까지 끝난 코드라 그대로 쓴다.
// TODO: Atlas 작업이 정리되면 `UI/Widgets/` 로 옮긴다 (지금 옮기면 진행 중인 작업과 충돌한다).
#include "UI/Atlas/FF_AtlasScreenGeo.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"
#include "UI/Widgets/FF_PanelScrollStyle.h"

#include "FF_PrepPanelBase.generated.h"

class UButton;
class UFF_PrepHudBar;
class UTextBlock;

/**
 * 준비 단계 하단 바에서 열리는 패널의 베이스. **U5 의 패널 6종이 여기서 파생한다.**
 *
 * === 패널은 자기 가시성을 소유하지 않는다 ===
 * 여는 것도 닫는 것도 `UFF_PrepHudBar` 다. 패널이 스스로 `SetVisibility` 를 부르면 배타 규칙의
 * 주체가 둘로 갈려 "어느 쪽이 이겼는지 알 수 없는" 상태가 된다 (ui-antipatterns-to-avoid 10번).
 * 닫아야 할 일이 있으면 `RequestClose()` 로 **바에 요청**한다.
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 준비 단계에도 Live 시간이 흐르는 단계(홍보)가 있고, HUD 가 입력 모드를 가져가면 월드 조작이
 * 막힌다. 전체 화면 시트가 아니라 HUD 위의 패널이므로 일반 `UUserWidget` 이다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel Base"))
class FESTAFIESTA_API UFF_PrepPanelBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 바가 패널을 만든 직후 한 번 부른다. 어느 탭의 패널인지와 제목을 함께 넣는다.
	 * 따로 두면 제목만 바뀌고 탭이 남아 "인력 패널에 홍보 제목"이 뜬다.
	 */
	void InitializePanel(UFF_PrepHudBar* InOwningBar, EFF_PrepHudTab InTab, const FText& InTitle);

	/** 패널이 화면에 올라올 때마다. 값 갱신·구독은 여기서 한다. */
	void HandlePanelOpened();

	/** 패널이 내려갈 때마다. 구독 해제는 여기서 한다. */
	void HandlePanelClosed();

	EFF_PrepHudTab GetTab() const { return Tab; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	UFF_PrepHudBar* GetOwningBar() const { return OwningBar.Get(); }

	/**
	 * 이 패널을 닫아 달라고 **바에 요청한다.** 점검 패널의 `배치 계속하기` 처럼 패널 안에서
	 * 닫아야 하는 버튼이 쓴다. 직접 숨기지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void RequestClose();

	/** 패널 안 스크롤바 모양. 끄면 각 ScrollBox 의 Designer 스타일을 그대로 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Scroll")
	FFF_PanelScrollStyle ScrollStyle;

	/**
	 * 패널 위에서 굴린 휠을 패널이 가진다. 끝까지 스크롤해 목록이 안 받은 휠이 뒤의 게임
	 * 화면까지 내려가 카메라가 확대·축소되던 것을 막는다 (2026-10-06 사용자 보고).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Scroll")
	bool bConsumeMouseWheel = true;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	/** 오른쪽 위 닫기 버튼. 없으면 하단 바의 탭을 다시 눌러 닫는다. */
	UFUNCTION()
	void HandleCloseClicked();

	// =========================================================================
	// 끌어 옮기기
	//
	// 패널의 **빈 곳**을 잡아 끌면 바가 자리를 옮긴다. 버튼·목록 위에서는 그 위젯이 먼저
	// 입력을 먹으므로 여기까지 오지 않는다 — 그래서 별도 드래그 손잡이를 두지 않았다.
	//
	// 🔴 함정 두 개를 여기서 막는다.
	// 1. **위젯 밖으로 나가면 드래그가 끊긴다.** 프론트엔드는 `bShowMouseCursor=true` 라
	//    캡처가 `NoCapture` 다. `CaptureMouse` 를 돌려주지 않으면 커서가 패널을 벗어난 순간
	//    이동이 멈추고 버튼을 뗀 것도 못 받는다.
	// 2. **화면 픽셀을 슬레이트 단위로 바꿔야 한다.** 포인터 좌표는 픽셀이고 캔버스 슬롯은
	//    슬레이트 단위다. UI 배율이 1 인 개발 환경에서는 티가 안 나고 다른 해상도에서만 어긋난다.
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
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	bool bAllowDragToMove = true;

	/** C++ 파생용. `Super::` 를 부른 뒤 자기 것을 한다. */
	virtual void NativeOnPanelOpened() {}
	virtual void NativeOnPanelClosed() {}

	/**
	 * 런타임에 만들어 넣을 자식 위젯의 클래스를 정한다 — 설정값이 있으면 그것을, 없으면 경로에서.
	 *
	 * C++ 기본값에 블루프린트 클래스를 넣을 수 없어서 경로 폴백이 필요하다.
	 * `UFF_PrepHudBar::ResolvePanelClass` 가 패널을 찾는 방식과 같고, 그래서
	 * `FF.PrepHudUMG` 로 만든 WBP 가 손으로 배선하지 않아도 붙는다.
	 *
	 * !! 실패해도 기본 타입으로 대체하지 않는다 !! 그러면 `BindWidget` 이 통과해 버리고
	 * 런타임에 아무 반응이 없다. `nullptr` 을 돌려주고 호출부가 로그를 남긴다.
	 */
	static UClass* ResolveWidgetClass(UClass* Configured, const FString& FallbackPath);

	/** BP 파생용. 이름으로 BP 함수를 찾지 않기 위해 명시적으로 선언한다 (안티패턴 17번). */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep", meta = (DisplayName = "On Panel Opened"))
	void K2_OnPanelOpened();

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep", meta = (DisplayName = "On Panel Closed"))
	void K2_OnPanelClosed();

	/** 제목. 바가 탭 라벨에서 넣어 주므로 WBP 에 문구를 박지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UTextBlock> Text_PanelTitle;

	/**
	 * 오른쪽 위 닫기 버튼. **패널을 직접 숨기지 않고 바에 닫아 달라고 요청한다** —
	 * 스스로 숨기면 하단 바의 탭은 눌린 채로 남아 화면과 상태가 갈라진다.
	 *
	 * 모든 패널이 공용으로 갖는다. WBP 에 `Button_Close` 라는 이름으로 놓으면 자동으로 붙는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UButton> Button_Close;

private:
	UPROPERTY()
	TWeakObjectPtr<UFF_PrepHudBar> OwningBar;

	EFF_PrepHudTab Tab = EFF_PrepHudTab::Build;

	/** 클릭과 드래그를 임계로 구분한다. 임계 안의 이동을 버려서 잡는 순간 툭 튀지 않는다. */
	FFF_AtlasDragTracker DragTracker;
};

/**
 * 아직 만들지 않은 탭에 띄우는 자리표시 패널. **U5 가 실제 패널을 만들면 매핑에서 빠진다.**
 *
 * 빈 패널을 띄우는 대신 "무엇이 여기 들어올 예정인지"를 적는다. 아무것도 안 뜨면 탭이
 * 고장 난 것으로 보이고, 빈 상자만 뜨면 만들다 만 것으로 보인다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Placeholder Panel"))
class FESTAFIESTA_API UFF_PrepPlaceholderPanel : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	/** 바가 넣어 준다. `U5 에서 만듭니다` 같은 한 줄. */
	void SetPlaceholderMessage(const FText& InMessage);

protected:
	virtual void NativeOnPanelOpened() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UTextBlock> Text_Placeholder;

private:
	FText PlaceholderMessage;
};
