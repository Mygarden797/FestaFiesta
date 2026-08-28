// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/Panels/FF_PrepChoiceButton.h"
#include "FF_PrepDetailCard.generated.h"

class UTextBlock;

/**
 * 준비 단계 카드 한 장의 표시 상태.
 *
 * !! 선택 여부와 별개다 !! 이미 확정한 카드는 선택돼 있지 않아도 잠긴다.
 */
UENUM(BlueprintType)
enum class EFF_PrepCardState : uint8
{
	/** 고를 수 있다. */
	Selectable	UMETA(DisplayName = "선택 가능"),

	/** 지금 고른 카드. 견적이 이 카드 기준으로 그려진다. */
	Selected	UMETA(DisplayName = "선택함"),

	/** 이미 확정했다 (프로그램은 `편성함`, 홍보는 `구매함`). 배지 문구로 구분한다. */
	Committed	UMETA(DisplayName = "확정함"),

	/** 지금은 고를 수 없다. 배지에 이유가 들어간다. */
	Locked		UMETA(DisplayName = "잠김")
};

/**
 * 수치 몇 줄과 배지를 가진 준비 단계 카드의 공용 베이스.
 * 프로그램 카드와 홍보 카드가 여기서 파생한다.
 *
 * === 왜 베이스를 하나 더 두나 ===
 * 🔴 `UFF_HudTabButtonBase::RefreshVisualState` 는 **`Text_Label` 과 `Image_Icon` 만** 색을
 * 다시 칠한다. 카드가 새로 얹는 비용·효과·배지 글자는 베이스가 모른다. 선택되면 배경이
 * 어두워지는데 글자가 `Ink` 로 남아 **안 읽힌다.**
 *
 * 그 처리를 카드마다 복사하면 반드시 한쪽이 빠지고, 빠진 쪽은 "색만으로 구분하는" 화면이
 * 된다 (AT-011). 그래서 상태·배지·색 재선언을 여기 한 벌만 둔다.
 * 파생 클래스는 `CollectStateTexts` 로 **자기 글자만 알려 주면** 된다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Detail Card"))
class FESTAFIESTA_API UFF_PrepDetailCard : public UFF_PrepChoiceButton
{
	GENERATED_BODY()

public:
	/**
	 * 표시 상태를 반영한다. **매번 전 상태를 다시 선언한다** — 어떤 순서로 불려도 결과가 같다.
	 *
	 * @param InBadgeText 배지 문구. 비어 있으면 배지를 접는다.
	 *                    **`Locked` 인데 비어 있으면 안 된다** — 흐려짐만으로는 이유를 알 수 없다
	 */
	void ApplyCardState(EFF_PrepCardState InState, const FText& InBadgeText);

	EFF_PrepCardState GetCardState() const { return CardState; }

protected:
	/**
	 * 상태에 따라 색을 다시 칠할 글자들. **파생 클래스가 자기 것을 넣는다.**
	 *
	 * `Text_Label` 은 넣지 않아도 된다 — `ApplyContentColor` 가 알아서 더한다.
	 * (`UFF_HudTabButtonBase` 도 라벨을 칠하지만 선택/비선택 2상태만 알아서
	 *  `확정함`·`잠김` 을 표현하지 못한다.)
	 */
	virtual void CollectStateTexts(TArray<UTextBlock*>& OutTexts) const {}

	/** 상태에 맞는 글자색을 전 텍스트에 다시 선언한다. 내용이 바뀐 뒤에도 부른다. */
	void ApplyContentColor();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	/** `✓ 편성함` · `잔액 부족` 등. 비어 있으면 접는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UTextBlock> Text_Badge;

	/**
	 * 잠긴 카드의 글자색. 베이스의 `NormalContentColor` 와 짝이다.
	 *
	 * !! 색만으로 구분하지 않는다 !! 이 색은 배지 문구를 **보조**할 뿐이고,
	 * 잠긴 이유는 항상 `Text_Badge` 에 글자로도 들어간다 (AT-011).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	FLinearColor DisabledContentColor = FLinearColor::FromSRGBColor(FColor(0x94, 0xA0, 0x9A));

private:
	EFF_PrepCardState CardState = EFF_PrepCardState::Selectable;
};
