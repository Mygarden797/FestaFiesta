// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_FocusableWidgetBase.generated.h"

class UImage;

/**
 * 게임패드·키보드 포커스를 **눈에 보이게** 만드는 베이스.
 * 옵션 행 · 저장 카드 · 목록 행처럼 **버튼이 아닌** 조작 위젯이 이걸 상속한다.
 *
 * === 왜 필요한가 (C-024) ===
 * 이 프로젝트는 `Config/DefaultEngine.ini` 에서 **`RenderFocusRule=Never`** 다.
 * 엔진이 포커스 사각형을 그려주지 않으므로 **포커스가 어디 있는지 화면에 아무 표시가 없다.**
 * 게임패드로는 지금 무엇이 선택돼 있는지 알 수 없게 된다.
 *
 * === 버튼은 이걸 쓰지 않는다 ===
 * `UFF_CommonButtonBase` 는 이미 `Image_FocusRing` 을 갖고 자기 상태를 관리한다.
 * 이 클래스는 **그 밖의 것**을 위한 것이다 — 슬라이더 · 체크박스 · 리스트 행.
 *
 * === 왜 단독 `UFF_FocusRing` 위젯이 아닌가 ===
 * 링만 있는 위젯은 **자기가 언제 보여야 하는지 알 수 없다.** 포커스는 부모가 받고,
 * 자식 위젯은 부모의 포커스 상태를 알 방법이 없다. 그래서 포커스를 받는 쪽이 링을 소유해야 한다.
 *
 * === 색만으로 표시하지 않는다 (AT-011) ===
 * 링은 **모양**이다. 배경색만 바꾸는 방식은 쓰지 않는다 — 색약 사용자에게 전달되지 않고,
 * 선택 상태(지속)와 포커스 상태(순간)가 구분되지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Focusable Widget Base"))
class FESTAFIESTA_API UFF_FocusableWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	// UUserWidget 은 기본 생성자가 없다. FObjectInitializer 를 받는 형태만 있다.
	UFF_FocusableWidgetBase(const FObjectInitializer& ObjectInitializer);

	/** 지금 포커스를 갖고 있는지. 표시 상태를 다시 계산할 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI")
	bool HasUIFocus() const { return bHasUIFocus; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	/**
	 * 포커스 표시를 전부 다시 선언한다. 부분 갱신을 하지 않으므로 어떤 순서로 이벤트가 들어와도
	 * 결과가 같다(멱등). `UFF_CommonButtonBase::RefreshVisualState` 와 같은 방식이다.
	 */
	void RefreshFocusVisual();

	/** 링 말고 추가 연출이 필요하면 BP 에서 구현한다. 링 자체는 C++ 이 관리한다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI", meta = (DisplayName = "On Focus Visual Changed"))
	void BP_OnFocusVisualChanged(bool bFocused);

	/**
	 * 포커스 링. 없으면 경고를 남긴다 — 있어야 하는데 빠진 것이지 정상이 아니다.
	 * 브러시는 디자이너가 넣는다. 색은 팔레트에서 읽는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_FocusRing;

	/**
	 * true 면 프론트엔드 팔레트(Glass Orbit)의 색을 쓴다. false 면 인게임(LightTeal).
	 * 프론트엔드와 인게임에서 같은 행 위젯을 쓸 수 있으므로 WBP 별로 정한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	bool bUseFrontendPalette = true;

private:
	bool bHasUIFocus = false;
};
