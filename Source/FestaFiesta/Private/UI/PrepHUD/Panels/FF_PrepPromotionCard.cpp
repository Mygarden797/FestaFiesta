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

	SetTextOrHide(Text_Buzz, FText::Format(
		LOCTEXT("CardStartingBuzz", "화제성 +{0}"), FText::AsNumber(InOption.StartingBuzz)));
	SetTextOrHide(Text_Forecast,
		InOption.ExpectedVisitors != INDEX_NONE && InOption.ForecastAdmissionFeeGold != INDEX_NONE
			? FText::Format(LOCTEXT("CardVisitorForecast", "예상 방문객 {0}명\n현재 입장료 {1} 기준"),
				FText::AsNumber(InOption.ExpectedVisitors), FText::AsNumber(InOption.ForecastAdmissionFeeGold))
			: LOCTEXT("CardVisitorForecastPending", "예상 방문객 확인 중"));
	if (Text_Forecast)
	{
		Text_Forecast->SetToolTipText(LOCTEXT("CardVisitorForecastHint",
			"현재 입장료·화제성·평판을 기준으로 이 홍보 등급을 적용했을 때의 하루 예상 방문객입니다. 실제 입장 인원과 다를 수 있습니다."));
	}

	ApplyContentColor();
}

void UFF_PrepPromotionCard::CollectStateTexts(TArray<UTextBlock*>& OutTexts) const
{
	OutTexts.Add(Text_Cost);
	OutTexts.Add(Text_Effect);
	OutTexts.Add(Text_Buzz);
	OutTexts.Add(Text_Forecast);
}

#undef LOCTEXT_NAMESPACE
