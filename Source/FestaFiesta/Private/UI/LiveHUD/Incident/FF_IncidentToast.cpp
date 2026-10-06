// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Incident/FF_IncidentToast.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"

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

	UiFont = FFIncidentUiStyle::LoadUiFont();
	Canvas =WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Toast"));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_Toasts"));
	if (UCanvasPanelSlot* ListSlot = Canvas->AddChildToCanvas(List))
	{
		ListSlot->SetAnchors(FAnchors(0.5f, 0.f));
		ListSlot->SetAlignment(FVector2D(0.5f, 0.f));
		ListSlot->SetAutoSize(true);
		ListSlot->SetPosition(FVector2D(0.f, TopOffset));
	}
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

void UFF_IncidentToast::Show(const FText& Label, const FText& Message, bool bPositive)
{
	BuildTree();
	if (!WidgetTree || !List)
	{
		return;
	}

	while (Entries.Num() >= MaxEntries)
	{
		RemoveEntryAt(Entries.Num() - 1);
	}

	FEntry Entry;
	if (!MakeCard(Label, Message, bPositive, Entry))
	{
		return;
	}
	Entry.RemainingSeconds = FMath::Max(DisplaySeconds, KINDA_SMALL_NUMBER);

	List->InsertChildAt(0, Entry.Card);
	if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Entry.Card->Slot))
	{
		CardSlot->SetHorizontalAlignment(HAlign_Center);
	}
	Entries.Insert(Entry, 0);

	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (UVerticalBoxSlot* CardSlot = Cast<UVerticalBoxSlot>(Entries[Index].Card->Slot))
		{
			CardSlot->SetPadding(FMargin(0.f, Index == 0 ? 0.f : EntryGap, 0.f, 0.f));
		}
	}

	ApplyEntryOpacity(Entries[0]);
	ApplyVisibility();
}

UBorder* UFF_IncidentToast::MakeCard(const FText& Label, const FText& Message, bool bPositive, FEntry& OutEntry)
{
	using namespace FFIncidentUiStyle;
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return nullptr;
	}
	const FFPauseOptionsLightTeal::FTokens Tokens = FFPauseOptionsLightTeal::Get(*Palette);
	const FLinearColor StateColor = bPositive ? Palette->InGame.BandGood : Palette->InGame.Danger;

	// 카드: PaperStrong 바탕 · Line 테두리 · 카드 모서리 6px
	UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Card->SetBrush(MakeRounded(Tokens.PaperStrong, CardRadius, Tokens.Line, 1.f));
	Card->SetPadding(FMargin(8.f, 8.f, 18.f, 8.f));

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Card->SetContent(Row);

	// 결과 색 띠. 앞머리 글자와 함께 써서 색만으로 결과를 가르지 않는다(AT-011).
	USizeBox* StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetWidthOverride(4.f);
	UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Strip->SetBrush(MakePill(StateColor));
	StripBox->SetContent(Strip);
	if (UHorizontalBoxSlot* StripSlot = Row->AddChildToHorizontalBox(StripBox))
	{
		StripSlot->SetVerticalAlignment(VAlign_Fill);
		StripSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	LabelText->SetFont(MakeFont(TEXT("SemiBold"), 13));
	LabelText->SetColorAndOpacity(FSlateColor(StateColor));
	LabelText->SetText(Label);
	if (UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(LabelText))
	{
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
	}

	UTextBlock* MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	MessageText->SetFont(MakeFont(TEXT("Regular"), 14));
	MessageText->SetColorAndOpacity(FSlateColor(Tokens.Ink));
	MessageText->SetText(Message);
	if (UHorizontalBoxSlot* MessageSlot = Row->AddChildToHorizontalBox(MessageText))
	{
		MessageSlot->SetVerticalAlignment(VAlign_Center);
	}

	OutEntry.Card = Card;
	OutEntry.Label = LabelText;
	OutEntry.Text = MessageText;
	return Card;
}

FSlateFontInfo UFF_IncidentToast::MakeFont(const TCHAR* Typeface, int32 Size) const
{
	return FFIncidentUiStyle::MakeFont(UiFont, Typeface, Size);
}

FText UFF_IncidentToast::GetLabelAt(int32 Index) const
{
	return Entries.IsValidIndex(Index) && Entries[Index].Label ? Entries[Index].Label->GetText() : FText::GetEmpty();
}

FText UFF_IncidentToast::GetMessageAt(int32 Index) const
{
	return Entries.IsValidIndex(Index) && Entries[Index].Text ? Entries[Index].Text->GetText() : FText::GetEmpty();
}

float UFF_IncidentToast::GetOpacityAt(int32 Index) const
{
	return Entries.IsValidIndex(Index) && Entries[Index].Card ? Entries[Index].Card->GetRenderOpacity() : 0.f;
}

void UFF_IncidentToast::AdvanceTime(float DeltaSeconds)
{
	const float Step = FMath::Max(0.f, DeltaSeconds);
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		FEntry& Entry = Entries[Index];
		Entry.RemainingSeconds -= Step;
		if (Entry.RemainingSeconds <= 0.f)
		{
			RemoveEntryAt(Index);
			continue;
		}
		ApplyEntryOpacity(Entry);
	}
	if (!Entries.IsEmpty())
	{
		if (UVerticalBoxSlot* TopSlot = Cast<UVerticalBoxSlot>(Entries[0].Card->Slot))
		{
			TopSlot->SetPadding(FMargin(0.f));
		}
	}
	ApplyVisibility();
}

void UFF_IncidentToast::RemoveEntryAt(int32 Index)
{
	if (!Entries.IsValidIndex(Index))
	{
		return;
	}
	if (List && Entries[Index].Card)
	{
		List->RemoveChild(Entries[Index].Card);
	}
	Entries.RemoveAt(Index);
}

void UFF_IncidentToast::ApplyEntryOpacity(const FEntry& Entry) const
{
	if (Entry.Card)
	{
		Entry.Card->SetRenderOpacity(FadeSeconds > 0.f ? FMath::Clamp(Entry.RemainingSeconds / FadeSeconds, 0.f, 1.f) : 1.f);
	}
}

void UFF_IncidentToast::ApplyVisibility()
{
	SetVisibility(IsShowing() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
