// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/FF_ActivatableScreenBase.h"
#include "FF_OptionsScreen.generated.h"

class UFF_CommonButtonBase;
class UFF_OptionRowToggle;
class UTextBlock;
class UVerticalBox;

/** 옵션 분류 6종. `docs/ui/main-menu-and-save.md` 의 목록과 같다. */
UENUM(BlueprintType)
enum class EFF_OptionCategory : uint8
{
	Language		UMETA(DisplayName = "언어"),
	Accessibility	UMETA(DisplayName = "접근성"),
	Sound			UMETA(DisplayName = "사운드"),
	Graphics		UMETA(DisplayName = "그래픽"),
	Controls		UMETA(DisplayName = "조작"),
	Credits			UMETA(DisplayName = "크레딧")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnOptionsBackRequested);

/**
 * 옵션. 좌측 분류 6개 + 우측 설정 행.
 *
 * === 지금 실제로 동작하는 것은 `접근성 > 움직임 감소` 하나다 (2026-08-20 결정) ===
 * 나머지 다섯은 **자리만** 만들고 `반영 사항 없음` 을 띄운다.
 * - 언어: 현지화 데이터가 없다
 * - 사운드: SoundClass·SoundMix 에셋이 없고 소유자가 불분명하다
 * - 그래픽: `UGameUserSettings` 에 붙이면 되지만 이번 범위가 아니다
 * - 조작: Enhanced Input 리바인딩은 별도 작업이다
 * - 크레딧: `THIRD_PARTY_NOTICES.md` 를 화면으로 옮기는 작업이 남아 있다
 *
 * **동작하지 않는다는 사실을 화면에 적는다.** 만져도 아무 일이 없는 조작 UI 는
 * 고장으로 보이고 시연에서 질문을 만든다.
 *
 * === 분류 버튼을 런타임에 만들지 않는다 ===
 * 6개를 컨테이너에 런타임으로 채우면 **디자이너에서 하나도 보이지 않아 위치·크기를 손으로
 * 만질 수 없다.** Live HUD 탭 9개에서 실제로 겪고 명시 바인딩으로 되돌린 문제다
 * (`docs/memory/wbp-codegen-workflow.md`). 그래서 여섯 개를 각각 바인딩한다.
 * 설정 행은 분류마다 개수가 달라 런타임 생성이 맞다.
 *
 * === 값을 소유하지 않는다 ===
 * `움직임 감소` 의 값은 `UFF_UIAccessibilitySubsystem` 이 갖는다. 이 화면이 들고 있으면
 * 화면을 닫을 때 사라진다. 여기서는 읽고 요청만 한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Options Screen"))
class FESTAFIESTA_API UFF_OptionsScreen : public UFF_ActivatableScreenBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI")
	FFF_OnOptionsBackRequested OnBackRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnActivated() override;
	virtual void NativeDestruct() override;

	/** 처음 포커스는 선택된 분류 버튼이다. */
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleCategoryClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleBackClicked(UFF_CommonButtonBase* Button);

	UFUNCTION()
	void HandleReduceMotionToggled(bool bEnabled);

	/** 분류를 고르고 우측을 다시 그린다. */
	void SelectCategory(EFF_OptionCategory Category);

	/** 우측 설정 행을 전부 다시 만든다. 부분 갱신을 하지 않는다. */
	void RebuildRows();

	/** 분류에 해당하는 버튼. 없으면 nullptr. */
	UFF_CommonButtonBase* GetCategoryButton(EFF_OptionCategory Category) const;

	// --- 분류 버튼. 런타임 생성이 아니라 명시 바인딩이다 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Language;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Accessibility;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Sound;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Graphics;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Controls;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Credits;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "FestaFiesta|UI")
	TObjectPtr<UFF_CommonButtonBase> Button_Back;

	// --- 우측 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_CategoryTitle;

	/** 설정 행이 들어가는 곳. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UVerticalBox> VerticalBox_Rows;

	/** `반영 사항 없음`. 아직 동작하지 않는 분류에서 보인다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_NotImplemented;

	/** 켜고/끄는 행에 쓸 위젯. 보통 `WBP_FF_OptionRowToggle`. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	TSubclassOf<UFF_OptionRowToggle> ToggleRowClass;

	/**
	 * 처음 보여줄 분류. **기본을 `접근성` 으로 둔다** — 유일하게 실제로 동작하는 분류라
	 * 화면을 열자마자 `반영 사항 없음` 만 보이는 첫인상을 피한다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI")
	EFF_OptionCategory DefaultCategory = EFF_OptionCategory::Accessibility;

private:
	EFF_OptionCategory SelectedCategory = EFF_OptionCategory::Accessibility;
};
