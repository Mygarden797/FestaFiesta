// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/Panels/FF_PrepDetailCard.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_PrepPromotionCard.generated.h"

class UTextBlock;

/**
 * 준비 단계 `홍보` 패널의 카드 한 장.
 *
 * 상태·배지·색 재선언은 `UFF_PrepDetailCard` 가 갖는다. 여기는 비용과 효과 두 줄만 얹는다.
 *
 * === `안 함` 도 카드다 ===
 * 미구매는 데이터가 아니라 상태다. 그래도 카드로 두는 이유는 **안 사는 것도 선택지라는 사실**을
 * 화면에 남기기 위해서다. 비용과 배수는 `안 함` 도 실제 등급 데이터에서 읽는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Promotion Card"))
class FESTAFIESTA_API UFF_PrepPromotionCard : public UFF_PrepDetailCard
{
	GENERATED_BODY()

public:
	/**
	 * 카드 내용을 채운다. **선택 id 는 `EFF_PromotionChoice` 를 `int32` 로 캐스팅한 값**이다.
	 * 프로그램 카드와 달리 순번이 아니다 — 등급이 enum 이라 그대로 실을 수 있다.
	 *
	 * @param InLabel 등급 이름. **enum `DisplayName` 을 런타임에 읽지 않는다** — 패널이 넣어 준다
	 */
	void ConfigureOption(const FFF_PromotionOptionView& InOption, const FText& InLabel);

	EFF_PromotionChoice GetChoice() const { return Option.Choice; }

protected:
	virtual void CollectStateTexts(TArray<UTextBlock*>& OutTexts) const override;

	// --- 위젯 바인딩. 전부 Optional 이다 ---

	/** `500 Gold` · `안 함` 은 `0 Gold`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_Cost;

	/** 실제 등급 데이터의 `예상 방문객 ×1.08`. 실제 입장 인원을 보장하지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_Effect;

	/** 등급별 아이콘. 없는 등급은 WBP의 기본 아이콘을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion")
	TMap<EFF_PromotionChoice, FSlateBrush> ChoiceIconBrushes;

	/** {0}: 실제 등급 비용. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText CostFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "CardCostFormat", "{0} Gold");

	/** {0}: 실제 등급의 예상 방문객 배수. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText EffectFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "CardEffectFormat", "예상 방문객 ×{0}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText UnknownValueText = NSLOCTEXT("FestaFiestaPrepPromotion", "CardUnknownValue", "확인 중");

private:
	FFF_PromotionOptionView Option;
};
