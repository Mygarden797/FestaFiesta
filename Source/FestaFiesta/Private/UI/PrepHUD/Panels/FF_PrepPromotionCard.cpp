// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepPromotion"

void UFF_PrepPromotionCard::ConfigureOption(const FFF_PromotionOptionView& InOption, const FText& InLabel)
{
	Option = InOption;

	const FSlateBrush* IconBrush = ChoiceIconBrushes.Find(InOption.Choice);
	ConfigureTab(static_cast<int32>(InOption.Choice), InLabel,
		IconBrush ? *IconBrush : FSlateBrush());

	// `안 함` 의 0 Gold 는 확정값이다. 칸을 비우면 "아직 모른다"로 읽힌다.
	SetTextOrHide(Text_Cost, InOption.CostGold != INDEX_NONE
		? FText::Format(CostFormat, FText::AsNumber(InOption.CostGold))
		: UnknownValueText);

	// 미구매 등급에도 배수가 있다. '방문객 그대로'로 고정하면 실제 예상치와 달라진다.
	FNumberFormattingOptions MultiplierFormat;
	MultiplierFormat.SetMinimumFractionalDigits(2);
	MultiplierFormat.SetMaximumFractionalDigits(2);

	SetTextOrHide(Text_Effect, InOption.VisitorMultiplier > 0.f
		? FText::Format(EffectFormat, FText::AsNumber(InOption.VisitorMultiplier, &MultiplierFormat))
		: UnknownValueText);

	ApplyContentColor();
}

void UFF_PrepPromotionCard::CollectStateTexts(TArray<UTextBlock*>& OutTexts) const
{
	OutTexts.Add(Text_Cost);
	OutTexts.Add(Text_Effect);
}

#undef LOCTEXT_NAMESPACE
