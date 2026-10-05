#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#define LOCTEXT_NAMESPACE "FestaFiestaLiveSnsEntry"

UFF_LiveSnsPostEntry::UFF_LiveSnsPostEntry(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SentimentLabels = {
		{EFF_LiveSnsSentiment::Positive, LOCTEXT("Positive", "긍정")},
		{EFF_LiveSnsSentiment::Negative, LOCTEXT("Negative", "아쉬움")},
		{EFF_LiveSnsSentiment::Neutral, LOCTEXT("Neutral", "중립")},
		{EFF_LiveSnsSentiment::Unknown, LOCTEXT("Unknown", "반응")}};
	SentimentSymbols = {
		{EFF_LiveSnsSentiment::Positive, LOCTEXT("PositiveSymbol", "+")},
		{EFF_LiveSnsSentiment::Negative, LOCTEXT("NegativeSymbol", "!")},
		{EFF_LiveSnsSentiment::Neutral, LOCTEXT("NeutralSymbol", "·")},
		{EFF_LiveSnsSentiment::Unknown, LOCTEXT("UnknownSymbol", "·")}};
	SentimentBackgroundColors = {
		{EFF_LiveSnsSentiment::Positive, FLinearColor::FromSRGBColor(FColor(229, 245, 233))},
		{EFF_LiveSnsSentiment::Negative, FLinearColor::FromSRGBColor(FColor(255, 243, 218))},
		{EFF_LiveSnsSentiment::Neutral, FLinearColor::FromSRGBColor(FColor(231, 244, 250))},
		{EFF_LiveSnsSentiment::Unknown, FLinearColor::FromSRGBColor(FColor(238, 242, 243))}};
}

void UFF_LiveSnsPostEntry::ConfigurePost(const FFF_LiveSocialPost& Post)
{
	CurrentPost = Post;
	bHasPost = true;
	RefreshPost();
}
void UFF_LiveSnsPostEntry::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (bHasPost) { RefreshPost(); }
}
void UFF_LiveSnsPostEntry::RefreshPost()
{
	if (Text_Author) { Text_Author->SetText(CurrentPost.Author); }
	if (Text_Body) { Text_Body->SetText(CurrentPost.Body); }
	const EFF_LiveSnsSentiment Sentiment = FFF_LiveSnsFeedState::ResolveSentiment(CurrentPost);
	FLinearColor Color = UnknownColor;
	switch (Sentiment)
	{
	case EFF_LiveSnsSentiment::Positive:
		Color = PositiveColor; break;
	case EFF_LiveSnsSentiment::Negative:
		Color = NegativeColor; break;
	case EFF_LiveSnsSentiment::Neutral:
		Color = NeutralColor; break;
	default: break;
	}
	if (Text_Sentiment)
	{
		const FText* Label = SentimentLabels.Find(Sentiment);
		Text_Sentiment->SetText(Label ? *Label : LOCTEXT("Unknown", "반응"));
		Text_Sentiment->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Text_SentimentSymbol)
	{
		const FText* Symbol = SentimentSymbols.Find(Sentiment);
		Text_SentimentSymbol->SetText(Symbol ? *Symbol : LOCTEXT("UnknownSymbol", "·"));
		Text_SentimentSymbol->SetColorAndOpacity(FSlateColor(Color));
	}
	// 색과 기호·라벨을 동시에 사용한다.
	if (Border_Sentiment)
	{
		const FLinearColor* Background = SentimentBackgroundColors.Find(Sentiment);
		Border_Sentiment->SetBrushColor(Background ? *Background : FLinearColor::White);
	}
}
#undef LOCTEXT_NAMESPACE
