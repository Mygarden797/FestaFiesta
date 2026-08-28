// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonButtonBase.h"
#include "Styling/SlateBrush.h"
#include "FF_CommonButtonBase.generated.h"

class UImage;
class UCommonTextBlock;
class UFF_CommonButtonBase;
class USizeBox;

/** 클릭된 버튼 자신을 넘긴다. 화면 쪽에서 버튼별 핸들러를 따로 두지 않아도 되게 하기 위함. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFFCommonButtonClicked, UFF_CommonButtonBase*, Button);

/**
 * FestaFiesta 공통 버튼 베이스.
 *
 * WBP를 먼저 만들고 C++을 나중에 붙이지 않는다. 이 클래스를 부모로 지정해 WBP를 파생한다.
 *
 * === 이 클래스가 하지 않는 것 ===
 * 배경 브러시·패딩·최소 크기·사운드는 건드리지 않는다. 전부 `UCommonButtonStyle`(CBS_ 에셋)이 맡는다.
 * 여기서 배경을 또 관리하면 "버튼이 어떻게 보이는가"의 출처가 두 곳이 되어,
 * 테마를 바꿀 때 스타일 에셋만 고쳐서는 안 되는 상태가 된다.
 * 근거: docs/ui/ui-tech-decision.md 9절
 *
 * === 이 클래스가 하는 것 ===
 * CommonButtonStyle이 다루지 않는 부분만 담당한다.
 *   1. 클릭 알림 (OnFFButtonClicked)
 *   2. 아이콘·Chevron 틴트 — 스타일은 버튼 자신만 칠하고 임의의 자식 이미지는 안 건드린다
 *   3. 마우스 호버와 키보드·패드 포커스를 구분해 추적
 *   4. 비활성 시 불투명도
 *
 * 상태 표현은 RefreshVisualState() 한 곳에서 "전체를 다시 선언"한다.
 * 부분 갱신을 하지 않으므로 어떤 순서로 이벤트가 들어와도 결과가 같다(멱등).
 *
 * 규격: docs/ui/glass-orbit-unreal-spec.md 7절
 */
UCLASS(Abstract, meta = (DisplayName = "FF Common Button Base"))
class FESTAFIESTA_API UFF_CommonButtonBase : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	/**
	 * 클릭 알림.
	 * CommonUI 버전에 따라 기본 제공 델리게이트 이름이 달라지므로, 우리 쪽에서 하나를 고정해 쓴다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFFCommonButtonClicked OnFFButtonClicked;

	/** 라벨을 코드에서 바꾼다. 문구는 FText로만 다룬다(현지화 대상). */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void SetButtonLabel(const FText& InLabel);

	/** 현재 상태(호버·포커스·활성)를 읽어 표현 전체를 다시 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void RefreshVisualState();

	/** `IconBrush`가 지정돼 있으면 `Image_Icon`에 적용한다. 비어 있으면 WBP 설정을 유지한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void ApplyIconBrush();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	virtual void NativeOnClicked() override;
	virtual void NativeOnHovered() override;
	virtual void NativeOnUnhovered() override;
	virtual void NativeOnEnabled() override;
	virtual void NativeOnDisabled() override;

	// 키보드·게임패드 포커스는 마우스 호버와 별개다. 둘 다 처리해야 패드에서 선택 위치가 보인다.
	virtual void NativeOnAddedToFocusPath(const FFocusEvent& InFocusEvent) override;
	virtual void NativeOnRemovedFromFocusPath(const FFocusEvent& InFocusEvent) override;

	/**
	 * 추가 연출이 필요하면 BP에서 구현한다.
	 * 이름으로 BP 함수를 찾아 호출(FindFunction+ProcessEvent)하지 않기 위해 명시적 이벤트로 둔다.
	 *
	 * === bIsHighlighted와 bIsFocused를 나눠 넘기는 이유 ===
	 * `bIsHighlighted`는 **호버만** 본다. CBS_ 스타일의 배경 브러시가 호버에서만 바뀌므로,
	 * 색 기준에 포커스를 넣으면 "어두운 배경 + 남색 글자"가 되어 글자가 사라진다
	 * (RefreshVisualState의 주석 참고. 실제로 있었던 버그다).
	 *
	 * 하지만 **애니메이션은 색과 다르다.** 게임패드로 포커스를 옮길 때도 같은 연출이 돌아야 한다
	 * (규격 10절 — 3m TV 거리에서 선택 위치가 보여야 한다).
	 * 그래서 색 판정(`bIsHighlighted`)과 연출 판정(`bIsHighlighted || bIsFocused`)을 BP에서
	 * 따로 쓸 수 있도록 셋을 다 넘긴다.
	 *
	 * 근거: docs/ui/glass-orbit-v3-wiring.md 3절
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI", meta = (DisplayName = "On Visual State Refreshed"))
	void BP_OnVisualStateRefreshed(bool bIsHighlighted, bool bIsInteractable, bool bIsFocused);

protected:
	// --- 위젯 바인딩. WBP의 위젯 이름이 아래와 정확히 같아야 컴파일된다 ---
	// 배경 이미지는 없다. 버튼 배경은 CBS_ 스타일 에셋이 직접 그린다.

	/** UTextBlock이 아니라 UCommonTextBlock이어야 CTS_ 스타일 에셋을 지정할 수 있다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UCommonTextBlock> Text_Label;

	// 아이콘·Chevron이 없는 버튼도 있을 수 있으므로 Optional. 없으면 로그를 남기고 건너뛴다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_Chevron;

	/**
	 * 키보드·게임패드 포커스 표시.
	 *
	 * CBS_ 스타일의 배경 브러시는 **호버에서만** 바뀐다. 포커스는 안 바뀐다.
	 * 그래서 포커스 표시는 별도 위젯이 필요하다. 없으면 패드로 어디가 선택됐는지 알 수 없다.
	 * WBP에 테두리 이미지를 하나 넣고 이 이름을 주면 된다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UImage> Image_FocusRing;

	/**
	 * 버튼 WBP의 바깥 크기 상자. 부모 화면에서 자식 WBP 내부를 직접 편집할 수 없으므로
	 * 아래 `ButtonSizeOverride`를 이 슬롯에 적용한다. 없는 버튼 WBP도 허용한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<USizeBox> SizeBox_Root;

	// --- 내용. 버튼 인스턴스마다 다른 값 ---

	/** 버튼 문구. 화면 문자열을 C++에 하드코딩하지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Content")
	FText ButtonLabel;

	/**
	 * 버튼 아이콘.
	 *
	 * `Image_Icon`은 버튼 WBP 안에 있어서 부모 화면에서 인스턴스별로 편집할 수 없다.
	 * 이 프로퍼티를 거쳐야 타이틀 화면에서 버튼마다 다른 아이콘을 줄 수 있다.
	 * 비워 두면 WBP에 설정된 브러시를 그대로 쓴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Content")
	FSlateBrush IconBrush;

	/**
	 * 이 버튼 인스턴스의 가로·세로 크기 재정의.
	 * 각 축이 0 이하면 그 축은 WBP의 기존 SizeBox 값을 유지한다. 기본값이 (0, 0)이므로
	 * 기존 타이틀·핀 버튼의 크기를 바꾸지 않고 Atlas 같은 특정 인스턴스만 조절할 수 있다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FestaFiesta|UI|Layout", meta = (ClampMin = "0.0"))
	FVector2D ButtonSizeOverride = FVector2D::ZeroVector;

	// --- 아이콘·Chevron 틴트. 배경과 폰트는 CBS_/CTS_가 맡고 여기서는 건드리지 않는다 ---

	/** #ECF4FF */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor NormalContentColor = FLinearColor::FromSRGBColor(FColor(0xEC, 0xF4, 0xFF));

	/** #102B4E */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor HighlightedContentColor = FLinearColor::FromSRGBColor(FColor(0x10, 0x2B, 0x4E));

	/** #8192A8 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	FLinearColor DisabledContentColor = FLinearColor::FromSRGBColor(FColor(0x81, 0x92, 0xA8));

	/**
	 * 라벨 색까지 이 클래스가 칠할지.
	 *
	 * CBS_ 스타일의 상태별 텍스트 스타일이 자식 UCommonTextBlock에 자동 적용되면
	 * 이걸 false로 두고 스타일에 맡긴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	bool bTintLabel = true;

	/** 비활성 시 전체 불투명도 (규격 7절). */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DisabledOpacity = 0.35f;

	/**
	 * 마우스가 들어올 때 이 버튼으로 포커스를 옮길지. (규격 7절 "마우스 진입 시 포커스 이동")
	 *
	 * 2026-08-07 결정: 상태 표시를 `Image_FocusRing` 하나로 모으기로 했다.
	 * 호버에서 배경을 흰색으로 덮지 않고 링으로만 알린다. 그러려면 마우스 호버가
	 * 포커스를 만들어야 한다 — 링은 `bHasFocusPath`로만 켜지기 때문이다.
	 *
	 * 끄면 호버와 포커스가 별개로 동작한다. 그 경우 호버 쪽에 다른 시각 표현
	 * (CBS_ 의 Hovered 브러시 등)을 반드시 붙여야 한다. 안 그러면 마우스로는 아무 반응이 없다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Input")
	bool bFocusOnHover = true;

	/**
	 * 포커스·호버일 때만 Chevron을 보이게 할지.
	 * false면 항상 표시하고 색만 바뀐다(현재 목업 기준).
	 * 색상만으로 상태를 구분하면 안 되므로(AT-011) 스타일의 브러시 교체와 함께 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Tint")
	bool bShowChevronOnlyWhenHighlighted = false;

private:
	/** `ButtonSizeOverride`의 양수 축만 `SizeBox_Root`에 적용한다. */
	void ApplyButtonSizeOverride();

	/** 키보드·게임패드 포커스 보유 여부. 마우스 호버와 합쳐 Highlighted를 만든다. */
	bool bHasFocusPath = false;
};
