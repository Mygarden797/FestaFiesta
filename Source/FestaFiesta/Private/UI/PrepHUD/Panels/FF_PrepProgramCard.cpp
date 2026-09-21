// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"

#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "FestaFiestaPrepProgram"

void UFF_PrepProgramCard::ConfigureCard(int32 InIndex, const FFF_ProgramCard& InCard, const FText& InTimeRangeText)
{
	Card = InCard;

	// 라벨과 id 를 함께 넣는다. 따로 두면 이름만 바뀌고 id 가 남아 다른 카드가 편성된다.
	ConfigureChoice(InIndex, InCard.DisplayName);

	SetTextOrHide(Text_Cost, (InCard.CostGold > 0)
		? FText::Format(LOCTEXT("PrepProgram_Cost", "{0} Gold"), FText::AsNumber(InCard.CostGold))
		: FText::GetEmpty());


	// 정원이 보장되므로 매출이 확정값이다. `예상` 이라고 쓰면 실제보다 약하게 읽힌다.
	SetTextOrHide(Text_Capacity, (InCard.Capacity > 0 && InCard.FeePerPerson > 0)
		? FText::Format(
			LOCTEXT("PrepProgram_Capacity", "정원 {0}명 · 확정 {1}"),
			FText::AsNumber(InCard.Capacity), FText::AsNumber(InCard.CalcMaxRevenue()))
		: FText::GetEmpty());


	SetTextOrHide(Text_TimeRange, InTimeRangeText);

	// 내용이 바뀌면 색도 다시 선언한다. 상태는 그대로 두고 색만 맞춘다.
	ApplyContentColor();
}

void UFF_PrepProgramCard::SetRevenuePreview(const FFF_ProgramCard* FacilityCard)
{
	if (Card.Capacity <= 0)
	{
		SetTextOrHide(Text_Capacity, FText::GetEmpty());
		return;
	}
	SetTextOrHide(Text_Capacity, FacilityCard
		? FText::Format(LOCTEXT("PrepProgram_FacilityCapacity", "정원 {0}명 · 최대 {1} Gold"),
			FText::AsNumber(FacilityCard->Capacity), FText::AsNumber(FacilityCard->CalcMaxRevenue()))
		: FText::Format(LOCTEXT("PrepProgram_CapacityBeforeFacility", "정원 {0}명 · 시설별 매출"),
			FText::AsNumber(Card.Capacity)));
}

void UFF_PrepProgramCard::CollectStateTexts(TArray<UTextBlock*>& OutTexts) const
{
	OutTexts.Add(Text_Cost);
	OutTexts.Add(Text_Capacity);
	OutTexts.Add(Text_TimeRange);
}

#undef LOCTEXT_NAMESPACE
