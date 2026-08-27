// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepPromotion"

void UFF_PrepPromotionCard::ConfigureOption(const FFF_PromotionOptionView& InOption, const FText& InLabel)
{
	Option = InOption;

	ConfigureChoice(static_cast<int32>(InOption.Choice), InLabel);

	// `안 함` 의 0 Gold 는 확정값이다. 칸을 비우면 "아직 모른다"로 읽힌다.
	SetTextOrHide(Text_Cost, (InOption.Choice == EFF_PromotionChoice::None)
		? LOCTEXT("PrepPromotion_CostNone", "0 Gold")
		: FText::Format(LOCTEXT("PrepPromotion_Cost", "{0} Gold"), FText::AsNumber(InOption.CostGold)));

	// 사람 수가 아니라 배수만 쓴다. 예상 방문객 API 가 없어서 `+N명` 을 만들 수 없다 —
	// 없는 숫자를 지어내지 않는다.
	FNumberFormattingOptions MultiplierFormat;
	MultiplierFormat.SetMinimumFractionalDigits(2);
	MultiplierFormat.SetMaximumFractionalDigits(2);

	SetTextOrHide(Text_Effect, (InOption.Choice == EFF_PromotionChoice::None)
		? LOCTEXT("PrepPromotion_EffectNone", "방문객 그대로")
		: FText::Format(
			LOCTEXT("PrepPromotion_Effect", "방문객 ×{0}"),
			FText::AsNumber(InOption.VisitorMultiplier, &MultiplierFormat)));

	ApplyContentColor();
}

void UFF_PrepPromotionCard::CollectStateTexts(TArray<UTextBlock*>& OutTexts) const
{
	OutTexts.Add(Text_Cost);
	OutTexts.Add(Text_Effect);
}

#undef LOCTEXT_NAMESPACE
