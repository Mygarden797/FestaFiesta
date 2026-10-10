// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Containers/Ticker.h"
#include "FF_UIRootLayout.generated.h"

class IInputProcessor;
class SFF_AtlasTransitionOverlay;
class UPanelWidget;
class UCommonActivatableWidgetContainerBase;
class UCommonActivatableWidgetStack;
class UFF_ActivatableScreenBase;

/**
 * UI 최상위 레이아웃. 뷰포트에 이것 하나만 올리고 나머지는 전부 이 아래로 들어간다.
 *
 *   HUDLayer     일반 UMG. 상단 자원 바·Overview HUD·Heat Map 범례. 입력을 가져가지 않는다.
 *   ScreenStack  전체 화면. 타이틀·플레이 허브·옵션·Atlas·Report.
 *   PanelStack   화면 위에 겹치는 상세 패널. 시설 상세·프로그램 슬롯·이벤트 상세.
 *   ToastLayer   일반 UMG. 경고·저장 완료·이벤트 알림. 항상 비모달.
 *
 * Lyra의 UPrimaryGameLayout / PushContentToLayer는 기본 CommonUI 기능이 아니라
 * Lyra의 CommonGame 플러그인 소속이다. 이 규모에서는 통째로 가져오지 않고 이 작은 레이아웃을 쓴다.
 *
 * 근거: docs/ui/ui-tech-decision.md 7·8절
 */
UCLASS(Abstract, meta = (DisplayName = "FF UI Root Layout"))
class FESTAFIESTA_API UFF_UIRootLayout : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** 전체 화면을 하나 밀어 넣는다. 실패하면 nullptr을 돌려주고 이유를 로그에 남긴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI",
		meta = (DeterminesOutputType = "ScreenClass"))
	UFF_ActivatableScreenBase* PushScreen(TSubclassOf<UFF_ActivatableScreenBase> ScreenClass);

	/* PushScreen 과 같지만 이번 한 번만 스택의 크로스페이드를 건너뛴다.
	   레벨에 들어오면서 지구본(스택 바닥용)과 국가 지도를 연달아 올릴 때, 페이드가 돌면
	   그 사이 지구본이 비쳐 보인다. 스택 설정값은 바로 되돌리므로 다른 전환은 그대로다.
	*/
	UFF_ActivatableScreenBase* PushScreenWithoutTransition(TSubclassOf<UFF_ActivatableScreenBase> ScreenClass);

	/** 화면 위에 겹치는 상세 패널을 밀어 넣는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI",
		meta = (DeterminesOutputType = "PanelClass"))
	UFF_ActivatableScreenBase* PushPanel(TSubclassOf<UFF_ActivatableScreenBase> PanelClass);

	/** 최상단 화면을 닫는다. 닫을 것이 있었으면 true. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	bool PopScreen();

	/** 최상단 패널을 닫는다. 닫을 것이 있었으면 true. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	bool PopPanel();

	/**
	 * **특정 화면만** 내린다. 최상단이 아니어도 그것만 제거한다.
	 *
	 * `PopScreen` 은 최상단을 내리므로, ESC 일시정지가 결산 시트 위에 올라간 상태에서 Stage 가
	 * 바뀌면 시트가 아니라 일시정지를 내린다. 흐름 서브시스템이 Stage 화면을 교체할 때 이걸 쓴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	bool RemoveScreen(UFF_ActivatableScreenBase* Screen);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	UPanelWidget* GetHUDLayer() const { return HUDLayer; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	UPanelWidget* GetToastLayer() const { return ToastLayer; }

	/** ESC·옵션·구매 확인처럼 일반 HUD 위를 덮는 화면이 현재 표시 중인지. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	bool HasForegroundStackContent() const;

	/** 불투명 프레임 뒤 한 번만 교체한다. 비활성 설정/준비 실패면 false로 즉시 전환을 허용한다. */
	bool BeginAtlasTransition(FSimpleDelegate SwapUnderCover, TFunction<bool()> IsReady);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Atlas Transition")
	bool IsAtlasTransitionActive() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Atlas Transition")
	float GetAtlasTransitionOpacity() const { return AtlasTransitionOpacity; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Atlas Transition")
	bool IsScreenStackTransitioning() const { return bScreenStackTransitioning; }

	/** 화면 이력은 수정하지 않고 페이드·예약 콜백·입력 차단만 정리한다. */
	void CancelAtlasTransition();

	virtual void BeginDestroy() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 위젯 바인딩. WBP_FF_UIRootLayout의 위젯 이름이 아래와 같아야 한다 ---

	/** 일반 UMG 컨테이너(Overlay 등). CommonUI 스택이 아니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> HUDLayer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UCommonActivatableWidgetStack> ScreenStack;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UCommonActivatableWidgetStack> PanelStack;

	/** 알림·토스트. 비모달이어야 하므로 스택이 아니라 일반 컨테이너다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UPanelWidget> ToastLayer;

private:
	enum class EAtlasTransitionPhase : uint8
	{
		Inactive,
		FadingOut,
		WaitingForOpaquePaint,
		HoldingOpaque,
		FadingIn
	};

	bool TickAtlasTransition(float DeltaTime);
	void SetAtlasTransitionOpacity(float Opacity);
	void HandleScreenStackTransitioning(UCommonActivatableWidgetContainerBase* Container, bool bIsTransitioning);

	TSharedPtr<SFF_AtlasTransitionOverlay> AtlasTransitionOverlay;
	TSharedPtr<IInputProcessor> AtlasTransitionInputProcessor;
	FTSTicker::FDelegateHandle AtlasTransitionTicker;
	FSimpleDelegate AtlasSwapUnderCover;
	TFunction<bool()> AtlasIsReady;
	EAtlasTransitionPhase AtlasTransitionPhase = EAtlasTransitionPhase::Inactive;
	uint32 AtlasTransitionGeneration = 0;
	float AtlasTransitionOpacity = 0.f;
	float AtlasPhaseElapsed = 0.f;
	float AtlasFadeOutDuration = 0.15f;
	float AtlasOpaqueHoldDuration = 0.05f;
	float AtlasFadeInDuration = 0.25f;
	FLinearColor AtlasTransitionColor = FLinearColor::Black;
	bool bScreenStackTransitioning = false;

	UFF_ActivatableScreenBase* PushToStack(
		UCommonActivatableWidgetStack* TargetStack,
		TSubclassOf<UFF_ActivatableScreenBase> WidgetClass,
		const TCHAR* StackLabel);

	bool PopFromStack(UCommonActivatableWidgetStack* TargetStack, const TCHAR* StackLabel);
};
