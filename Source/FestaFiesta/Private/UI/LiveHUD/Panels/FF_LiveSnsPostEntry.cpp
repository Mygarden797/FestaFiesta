#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#define LOCTEXT_NAMESPACE "FestaFiestaLiveSnsEntry"

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
	FLinearColor Color = NeutralColor;
	FText Label = LOCTEXT("Unknown", "반응");
	FText Symbol = LOCTEXT("UnknownSymbol", "·");
	switch (FFF_LiveSnsFeedState::ResolveSentiment(CurrentPost))
	{
	case EFF_LiveSnsSentiment::Positive:
		Color = PositiveColor; Label = LOCTEXT("Positive", "긍정"); Symbol = LOCTEXT("PositiveSymbol", "+"); break;
	case EFF_LiveSnsSentiment::Negative:
		Color = NegativeColor; Label = LOCTEXT("Negative", "아쉬움"); Symbol = LOCTEXT("NegativeSymbol", "!"); break;
	case EFF_LiveSnsSentiment::Neutral:
		Label = LOCTEXT("Neutral", "중립"); break;
	default: break;
	}
	if (Text_Sentiment) { Text_Sentiment->SetText(Label); Text_Sentiment->SetColorAndOpacity(FSlateColor(Color)); }
	if (Text_SentimentSymbol) { Text_SentimentSymbol->SetText(Symbol); Text_SentimentSymbol->SetColorAndOpacity(FSlateColor(Color)); }
	// 색과 기호·라벨을 동시에 사용한다.
	if (Border_Sentiment) { Border_Sentiment->SetBrushColor(FMath::Lerp(FLinearColor::White, Color, 0.13f)); }
}
#undef LOCTEXT_NAMESPACE
