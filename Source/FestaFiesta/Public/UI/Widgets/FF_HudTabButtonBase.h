// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FF_HudTabButtonBase.generated.h"

class UButton;
class UImage;
class UTextBlock;

/**
 * HUD 하단 바의 탭 버튼 공용 베이스. 준비 단계 HUD 와 라이브 HUD 가 함께 쓴다.
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 실시간 HUD 는 입력 모드를 가져가면 안 된다. `UCommonActivatableWidget` 계열을 쓰면 화면 스택과
 * 입력 모드가 따라붙어 운영 중 월드 조작이 막힐 수 있다. 그래서 일반 `UUserWidget` + 일반 `UButton` 이다.
 * 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 탭 종류를 enum 으로 받지 않는 이유 ===
 * 준비 단계는 `EFF_PrepHudTab`, 라이브는 `EFF_LiveHudTab` 으로 탭 집합이 다르다. 베이스는 `int32`
 * 로만 들고 있고 **어느 enum 인지는 파생 클래스가 안다.** 그래야 같은 시각 처리를 두 벌 갖지 않는다.
 *
 * 브러시 4종(Default/Hover/Selected/Pressed)은 WBP 에서 지정한다.
 * **선택 상태 브러시는 `Pressed` 슬롯에 넣는다** — `RefreshVisualState` 가 선택 중에는 그것을
 * Normal·Hovered·Pressed 에 함께 적용한다.
 *
 * 🔴 `UFF_LiveHudTabButton` 은 아직 이 베이스를 쓰지 않는다. 같은 코드가 그쪽에 한 벌 더 있다.
 *    U6 이 라이브 HUD 탭을 9종에서 7종으로 줄이면서 그 파일을 어차피 고치므로, **그때 이 베이스로
 *    부모를 바꾼다.** 지금 옮기면 손으로 만든 `WBP_FF_LiveHudTabButton` 을 확인할 방법 없이 건드리게 된다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF HUD Tab Button Base"))
class FESTAFIESTA_API UFF_HudTabButtonBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 이 탭이 무엇인지·문구·아이콘을 함께 설정한다.
	 * 따로 두면 라벨만 바뀌고 아이콘이 남아 "인력 탭에 배치 아이콘" 같은 조합이 화면에 뜬다.
	 * `InIconBrush` 의 리소스가 비어 있으면 WBP 에 설정된 브러시를 유지한다.
	 */
	void ConfigureTab(int32 InTabId, const FText& InLabel, const FSlateBrush& InIconBrush);

	/**
	 * 선택 상태를 반영한다.
	 *
	 * !! 색만 바꾸지 않는다 !! (AT-011)
	 * 선택된 탭은 배경 브러시가 바뀌고 라벨·아이콘 색이 반전된다. 배경 교체가 형태 변화를 담당한다.
	 */
	void SetSelected(bool bInSelected);

	bool IsSelected() const { return bSelected; }

	/** 파생 클래스가 자기 enum 으로 되돌려 해석한다. */
	int32 GetTabId() const { return TabId; }

	/** 클릭 알림. 바가 구독해 선택을 바꾼다. */
	DECLARE_DELEGATE_OneParam(FOnHudTabButtonClicked, int32);
	FOnHudTabButtonClicked OnHudTabButtonClicked;

protected:
	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClicked();

	/** 상태 표현을 한 곳에서 전체 재선언한다. 어떤 순서로 불려도 결과가 같다(멱등). */
	virtual void RefreshVisualState();

protected:
	// --- 위젯 바인딩 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Root;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Label;

	// --- 선택 상태 표현 ---
	//
	// 색 자체는 팔레트에서 오는 값이고, 여기 기본값은 팔레트를 못 읽는 상황의 폴백이다.
	// 바가 `NativePreConstruct` 에서 팔레트 값으로 덮는다.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor NormalContentColor = FLinearColor::FromSRGBColor(FColor(0x24, 0x33, 0x2E));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor SelectedContentColor = FLinearColor::FromSRGBColor(FColor(0xFF, 0xFF, 0xFF));

public:
	/**
	 * 팔레트에서 읽은 색을 바가 내려준다. 위젯이 팔레트를 각자 읽으면 테마를 바꿀 때 하나가 빠진다.
	 * 값을 넣은 뒤 곧바로 다시 그린다.
	 */
	void SetContentColors(const FLinearColor& InNormal, const FLinearColor& InSelected);

private:
	int32 TabId = 0;
	bool bSelected = false;

	/**
	 * WBP 가 지정한 버튼 브러시 원본.
	 *
	 * 선택 상태에서는 Normal·Hovered·Pressed 를 선택 브러시로 함께 덮어쓴다.
	 * 원본을 보관하지 않으면 선택 해제 뒤 기본·호버 상태로 되돌릴 수 없다.
	 */
	FSlateBrush DefaultNormalBrush;
	FSlateBrush DefaultHoveredBrush;
	FSlateBrush DefaultPressedBrush;
	bool bCapturedDefaultBrushes = false;
};
