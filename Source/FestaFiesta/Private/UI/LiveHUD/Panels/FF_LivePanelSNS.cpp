// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"

#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveSNS"

void UFF_LivePanelSNS::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();

	// ★ 이 패널에서 가장 중요한 문구다. 빈 화면만 두면 "고장났나" 가 되고,
	//   가짜 반응을 두면 "다 됐네" 가 된다. 둘 다 틀리다.
	SetTextOrHide(Text_MissingNote, LOCTEXT("LiveSNS_MissingNote",
		"관객 반응을 만드는 시스템이 아직 없습니다. 화면 자리만 잡아 둔 상태이고, "
		"반응 데이터가 생기면 여기에 그대로 들어갑니다."));

	RefreshAll();
}

void UFF_LivePanelSNS::Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts)
{
	Posts = InPosts;
	RefreshAll();
}

void UFF_LivePanelSNS::RefreshAll()
{
	if (!Box_Posts)
	{
		return;
	}

	Box_Posts->ClearChildren();

	for (const FFF_LiveSocialPost& Post : Posts)
	{
		UTextBlock* Line = NewObject<UTextBlock>(this);
		if (!Line)
		{
			continue;
		}

		// 점수를 모르면 자리를 비운다. `0` 을 그리면 `중립` 이라는 확정 사실이 된다.
		const FText Sentiment = (Post.SentimentScore != INDEX_NONE)
			? FText::Format(
				LOCTEXT("LiveSNS_Sentiment", " ({0})"), FText::AsNumber(Post.SentimentScore))
			: FText::GetEmpty();

		Line->SetText(FText::Format(
			LOCTEXT("LiveSNS_Post", "{0}{1}\n{2}"), Post.Author, Sentiment, Post.Body));
		Line->SetAutoWrapText(true);

		if (UVerticalBoxSlot* LineSlot = Box_Posts->AddChildToVerticalBox(Line))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
		}
	}

	Box_Posts->SetVisibility((Posts.Num() > 0)
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);

	SetTextOrHide(Text_Empty, (Posts.Num() > 0)
		? FText::GetEmpty()
		: LOCTEXT("LiveSNS_Empty", "아직 올라온 반응이 없습니다."));
}

void UFF_LivePanelSNS::SetTextOrHide(UTextBlock* TextBlock, const FText& Value)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(Value);
	TextBlock->SetVisibility(Value.IsEmpty()
		? ESlateVisibility::Collapsed
		: ESlateVisibility::SelfHitTestInvisible);
}

#undef LOCTEXT_NAMESPACE
