// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_SatisfactionBreakdownWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

#define LOCTEXT_NAMESPACE "FFSatisfactionBreakdownWidget"

namespace
{
	FLinearColor PaletteColor(FLinearColor FFF_UIPaletteInGame::* Member, const FLinearColor& Fallback)
	{
		const UFF_UIPalette* Palette = UFF_UIPalette::Get();
		return Palette ? Palette->InGame.*Member : Fallback;
	}
}

TSharedRef<SWidget> UFF_SatisfactionBreakdownWidget::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_SatisfactionBreakdownWidget::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}
	UiFont = FFIncidentUiStyle::LoadUiFont();
	Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_SatisfactionBreakdown"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	const FLinearColor Ink = PaletteColor(&FFF_UIPaletteInGame::Ink, FLinearColor::Black);
	const FLinearColor Muted = PaletteColor(&FFF_UIPaletteInGame::InkMuted, FLinearColor::Gray);
	Root->AddChildToVerticalBox(MakeText(TitleText, TEXT("SemiBold"), 16, Ink, false))->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));
	SummaryLabel = MakeText(FText::GetEmpty(), TEXT("Regular"), 13, Muted, true);
	Root->AddChildToVerticalBox(SummaryLabel)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_SatisfactionBreakdownRows"));
	Root->AddChildToVerticalBox(Rows);
	HintLabel = MakeText(FText::GetEmpty(), TEXT("SemiBold"), 14, PaletteColor(&FFF_UIPaletteInGame::TealDeep, Ink), true);
	Root->AddChildToVerticalBox(HintLabel)->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));
	RefreshRows();
}

UTextBlock* UFF_SatisfactionBreakdownWidget::MakeText(const FText& Text, const TCHAR* Typeface, int32 Size,
	const FLinearColor& Color, bool bWrap)
{
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Label->SetFont(FFIncidentUiStyle::MakeFont(UiFont, Typeface, Size));
	Label->SetColorAndOpacity(FSlateColor(Color));
	Label->SetAutoWrapText(bWrap);
	Label->SetText(Text);
	Label->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return Label;
}

void UFF_SatisfactionBreakdownWidget::SetBreakdown(const FFF_SatisfactionBreakdown& InBreakdown)
{
	Breakdown = InBreakdown;
	RefreshRows();
}

FText UFF_SatisfactionBreakdownWidget::GetSummaryText() const
{
	if (!Breakdown.bValid)
	{
		return FText::GetEmpty();
	}
	return FText::Format(SummaryFormat, FFormatNamedArguments{
		{ TEXT("Lost"), FText::AsNumber(100 - Breakdown.Satisfaction) },
		{ TEXT("Score"), FText::AsNumber(Breakdown.Satisfaction) } });
}

void UFF_SatisfactionBreakdownWidget::RefreshRows()
{
	if (!Rows)
	{
		return;
	}
	SetVisibility(Breakdown.bValid ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	Rows->ClearChildren();
	SummaryLabel->SetText(GetSummaryText());
	HintLabel->SetText(Breakdown.Hint);
	HintLabel->SetVisibility(Breakdown.Hint.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (!Breakdown.bValid)
	{
		return;
	}

	const FLinearColor Ink = PaletteColor(&FFF_UIPaletteInGame::Ink, FLinearColor::Black);
	const FLinearColor Muted = PaletteColor(&FFF_UIPaletteInGame::InkMuted, FLinearColor::Gray);
	const FLinearColor Loss = PaletteColor(&FFF_UIPaletteInGame::Danger, FLinearColor::Red);
	for (const FFF_SatisfactionLoss& Item : Breakdown.Losses)
	{
		UVerticalBox* Entry = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		LabelBox->SetWidthOverride(LabelWidth);
		LabelBox->SetContent(MakeText(FFF_SatisfactionBreakdown::GetFactorLabel(Item.Factor), TEXT("Medium"), 15, Ink, false));
		Line->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

		// 깎인 점수는 빨강, 하나도 안 깎였으면 흐린 0
		UTextBlock* Points = MakeText(Item.Points > 0 ? FText::Format(LOCTEXT("Lost", "−{0}"), FText::AsNumber(Item.Points)) : FText::AsNumber(0),
			TEXT("SemiBold"), 15, Item.Points > 0 ? Loss : Muted, false);
		Points->SetJustification(ETextJustify::Right);
		USizeBox* PointsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		PointsBox->SetWidthOverride(PointsWidth);
		PointsBox->SetContent(Points);
		Line->AddChildToHorizontalBox(PointsBox)->SetVerticalAlignment(VAlign_Center);
		Entry->AddChildToVerticalBox(Line);

		if (!Item.Evidence.IsEmpty())
		{
			Entry->AddChildToVerticalBox(MakeText(Item.Evidence, TEXT("Regular"), 13, Muted, true))->SetPadding(FMargin(0.f, 1.f, 0.f, 0.f));
		}
		Rows->AddChildToVerticalBox(Entry)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}
}

#undef LOCTEXT_NAMESPACE
