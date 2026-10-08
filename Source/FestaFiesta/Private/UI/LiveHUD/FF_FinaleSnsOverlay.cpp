// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/FF_FinaleSnsOverlay.h"

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
#include "Engine/GameInstance.h"
#include "UI/LiveHUD/Incident/FF_IncidentUiStyle.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

TSharedRef<SWidget> UFF_FinaleSnsOverlay::RebuildWidget()
{
	BuildTree();
	return Super::RebuildWidget();
}

void UFF_FinaleSnsOverlay::BuildTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UiFont = FFIncidentUiStyle::LoadUiFont();
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_FinaleSns"));
	Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Canvas;

	USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SizeBox_FinaleSns"));
	Width->SetWidthOverride(ListWidth);
	List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VBox_FinaleSns"));
	Width->SetContent(List);

	// 왼쪽 아래에 붙여 두면 줄이 늘어날 때 위로 자란다
	if (UCanvasPanelSlot* ListSlot = Canvas->AddChildToCanvas(Width))
	{
		ListSlot->SetAnchors(FAnchors(0.f, 1.f));
		ListSlot->SetAlignment(FVector2D(0.f, 1.f));
		ListSlot->SetAutoSize(true);
		ListSlot->SetPosition(FVector2D(ScreenMargin.X, -ScreenMargin.Y));
	}
}

void UFF_FinaleSnsOverlay::NativeConstruct()
{
	Super::NativeConstruct();
	BindViewModel();
	ApplyVisibility();
}

void UFF_FinaleSnsOverlay::NativeDestruct()
{
	if (BoundViewModel)
	{
		BoundViewModel->OnFinaleSnsPostAdded.RemoveDynamic(this, &UFF_FinaleSnsOverlay::HandleFinalePostAdded);
		BoundViewModel = nullptr;
	}
	Super::NativeDestruct();
}

void UFF_FinaleSnsOverlay::BindViewModel()
{
	if (BoundViewModel)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	BoundViewModel = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	if (BoundViewModel)
	{
		BoundViewModel->OnFinaleSnsPostAdded.AddUniqueDynamic(this, &UFF_FinaleSnsOverlay::HandleFinalePostAdded);
	}
}

void UFF_FinaleSnsOverlay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 피날레 단계를 벗어나는 순간 남은 줄을 치운다. 다음 화면에 후기가 얹혀 있으면 안 된다.
	// 단계 자체가 아니라 벗어나는 순간을 보는 것은 FF.SNS.Finale 미리보기(Live 중)에도 줄이 보이게 하려는 것이다.
	const bool bFinaleStage = BoundViewModel && BoundViewModel->GetCurrentStage() == EFF_FestivalStage::Finale;
	const bool bLeftFinale = bWasFinaleStage && !bFinaleStage;
	bWasFinaleStage = bFinaleStage;
	if (bLeftFinale)
	{
		ClearPosts();
		return;
	}
	if (!Entries.IsEmpty())
	{
		AdvanceTime(InDeltaTime);
	}
}

void UFF_FinaleSnsOverlay::HandleFinalePostAdded(FFF_LiveSocialPost NewPost)
{
	AddPost(NewPost);
}

UBorder* UFF_FinaleSnsOverlay::MakeRow(const FFF_LiveSocialPost& Post)
{
	using namespace FFIncidentUiStyle;

	// 줄 뒤 반투명 띠. 모서리는 칩과 같은 4px
	UBorder* Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Row->SetBrush(MakeRounded(BackingColor, ChipRadius, FLinearColor::Transparent, 0.f));
	Row->SetPadding(FMargin(0.f, 3.f, 10.f, 3.f));

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Row->SetContent(Line);

	// 반응은 왼쪽 가는 띠로만 보인다. 글자색까지 바꾸면 불꽃 색과 섞여 읽기 어렵다
	const FLinearColor StripColor = Post.Sentiment == EFF_LiveSnsSentiment::Negative ? NegativeColor : PositiveColor;
	USizeBox* StripBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	StripBox->SetWidthOverride(2.f);
	UBorder* Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Strip->SetBrush(MakePill(StripColor));
	StripBox->SetContent(Strip);
	if (UHorizontalBoxSlot* StripSlot = Line->AddChildToHorizontalBox(StripBox))
	{
		StripSlot->SetVerticalAlignment(VAlign_Fill);
		StripSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));
	}

	const FVector2D ShadowOffset(1.f, 1.f);
	const FLinearColor ShadowColor(0.f, 0.f, 0.f, 0.7f);

	UTextBlock* Author = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Author->SetFont(FFIncidentUiStyle::MakeFont(UiFont, TEXT("SemiBold"), AuthorFontSize));
	Author->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.6f)));
	Author->SetShadowOffset(ShadowOffset);
	Author->SetShadowColorAndOpacity(ShadowColor);
	Author->SetText(Post.Author);
	if (UHorizontalBoxSlot* AuthorSlot = Line->AddChildToHorizontalBox(Author))
	{
		AuthorSlot->SetVerticalAlignment(VAlign_Center);
		AuthorSlot->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
	}

	UTextBlock* Body = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Body->SetFont(FFIncidentUiStyle::MakeFont(UiFont, TEXT("Regular"), BodyFontSize));
	Body->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Body->SetShadowOffset(ShadowOffset);
	Body->SetShadowColorAndOpacity(ShadowColor);
	Body->SetAutoWrapText(true);
	Body->SetText(Post.Body);
	if (UHorizontalBoxSlot* BodySlot = Line->AddChildToHorizontalBox(Body))
	{
		BodySlot->SetVerticalAlignment(VAlign_Center);
		BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	return Row;
}

void UFF_FinaleSnsOverlay::AddPost(const FFF_LiveSocialPost& Post)
{
	BuildTree();
	if (!WidgetTree || !List)
	{
		return;
	}

	while (Entries.Num() >= MaxEntries)
	{
		if (Entries[0].Row)
		{
			Entries[0].Row->RemoveFromParent();
		}
		Entries.RemoveAt(0);
	}

	UBorder* Row = MakeRow(Post);
	if (UVerticalBoxSlot* RowSlot = List->AddChildToVerticalBox(Row))
	{
		RowSlot->SetPadding(FMargin(0.f, EntryGap, 0.f, 0.f));
		// 줄 길이만큼만 띠를 깐다. 짧은 글에 340px 띠가 깔리면 불꽃을 괜히 가린다
		RowSlot->SetHorizontalAlignment(HAlign_Left);
	}

	FEntry Entry;
	Entry.Row = Row;
	Entries.Add(Entry);

	ApplyOpacities();
	ApplyVisibility();
}

void UFF_FinaleSnsOverlay::ClearPosts()
{
	if (List)
	{
		List->ClearChildren();
	}
	Entries.Reset();
	ApplyVisibility();
}

void UFF_FinaleSnsOverlay::AdvanceTime(float DeltaSeconds)
{
	for (FEntry& Entry : Entries)
	{
		Entry.AgeSeconds += DeltaSeconds;
	}
	ApplyOpacities();
}

void UFF_FinaleSnsOverlay::ApplyOpacities() const
{
	const int32 Count = Entries.Num();
	const int32 Steps = FMath::Max(1, MaxEntries - 1);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FEntry& Entry = Entries[Index];
		if (!Entry.Row)
		{
			continue;
		}
		// Index 0 이 가장 오래된 줄이다. 줄 수가 MaxEntries 에 닿았을 때 맨 위가 OldestOpacity 가 된다.
		const float Age = static_cast<float>(Count - 1 - Index) / static_cast<float>(Steps);
		const float Settled = FMath::Lerp(1.f, OldestOpacity, FMath::Clamp(Age, 0.f, 1.f));
		const float FadeIn = FadeInSeconds > 0.f ? FMath::Clamp(Entry.AgeSeconds / FadeInSeconds, 0.f, 1.f) : 1.f;
		Entry.Row->SetRenderOpacity(Settled * FadeIn);
	}
}

void UFF_FinaleSnsOverlay::ApplyVisibility()
{
	SetVisibility(Entries.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
