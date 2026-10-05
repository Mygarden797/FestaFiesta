// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentToast.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"

TSharedRef<SWidget> UFF_IncidentToast::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_IncidentToast::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Toast"));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Border_Toast"));
	Card->SetBrushColor(PositiveColor);
	Card->SetPadding(FMargin(18.f, 10.f));
	if (UCanvasPanelSlot* CardSlot = Canvas->AddChildToCanvas(Card))
	{
		CardSlot->SetAnchors(FAnchors(0.5f, 0.f));
		CardSlot->SetAlignment(FVector2D(0.5f, 0.f));
		CardSlot->SetAutoSize(true);
		CardSlot->SetPosition(FVector2D(0.f, TopOffset));
	}

	Text_Message = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Message"));
	FSlateFontInfo Font = Text_Message->GetFont();
	Font.Size = 14;
	Text_Message->SetFont(Font);
	Text_Message->SetJustification(ETextJustify::Center);
	Card->SetContent(Text_Message);
}

void UFF_IncidentToast::NativeConstruct()
{
	Super::NativeConstruct();
	ApplyVisibility();
}

void UFF_IncidentToast::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (IsShowing())
	{
		AdvanceTime(InDeltaTime);
	}
}

void UFF_IncidentToast::Show(const FText& Message, bool bPositive)
{
	BuildTree();
	if (Text_Message)
	{
		Text_Message->SetText(Message);
	}
	if (Card)
	{
		Card->SetBrushColor(bPositive ? PositiveColor : NegativeColor);
	}
	RemainingSeconds = FMath::Max(DisplaySeconds, KINDA_SMALL_NUMBER);
	ApplyVisibility();
}

FText UFF_IncidentToast::GetMessage() const
{
	return Text_Message ? Text_Message->GetText() : FText::GetEmpty();
}

void UFF_IncidentToast::AdvanceTime(float DeltaSeconds)
{
	RemainingSeconds = FMath::Max(0.f, RemainingSeconds - FMath::Max(0.f, DeltaSeconds));
	ApplyVisibility();
}

void UFF_IncidentToast::ApplyVisibility()
{
	if (!IsShowing())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetRenderOpacity(FadeSeconds > 0.f ? FMath::Clamp(RemainingSeconds / FadeSeconds, 0.f, 1.f) : 1.f);
}
