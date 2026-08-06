// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "FF_UIRootLayout.generated.h"

class UPanelWidget;
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

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	UPanelWidget* GetHUDLayer() const { return HUDLayer; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	UPanelWidget* GetToastLayer() const { return ToastLayer; }

protected:
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
	UFF_ActivatableScreenBase* PushToStack(
		UCommonActivatableWidgetStack* TargetStack,
		TSubclassOf<UFF_ActivatableScreenBase> WidgetClass,
		const TCHAR* StackLabel);

	bool PopFromStack(UCommonActivatableWidgetStack* TargetStack, const TCHAR* StackLabel);
};
