#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
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

void UFF_LiveSnsPostEntry::ConfigureIconRowDefaults(const TMap<EFF_LiveSnsSentiment, UTexture2D*>& Icons,
	const FLinearColor& Positive, const FLinearColor& Negative, const FLinearColor& Other)
{
	SentimentIcons.Reset();
	for (const TPair<EFF_LiveSnsSentiment, UTexture2D*>& Pair : Icons) { SentimentIcons.Add(Pair.Key, Pair.Value); }
	PositiveColor = Positive;
	NegativeColor = Negative;
	NeutralColor = UnknownColor = Other;
	bTintSentimentIcon = true;
}

void UFF_LiveSnsPostEntry::SetNewMark(bool bNew)
{
	bNewMark = bNew;
	// 점은 이름 바로 뒤에 붙는다. 없을 때는 자리도 없앤다(뒤 칸이 남은 폭을 채운다).
	if (Image_NewDot) { Image_NewDot->SetVisibility(bNew ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void UFF_LiveSnsPostEntry::ConfigureNewsRowDefaults(UTexture2D* InSocialIcon, UTexture2D* InOperationIcon)
{
	bShowSourceLabel = true;
	SocialIcon = InSocialIcon;
	OperationIcon = InOperationIcon;
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
	SetToolTipText(CurrentPost.bOperationAlert && CurrentPost.OperationDay > 0
		? FText::Format(LOCTEXT("OperationDay", "{0}일차 운영 알림"), FText::AsNumber(CurrentPost.OperationDay)) : FText::GetEmpty());
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
	if (CurrentPost.bOperationAlert) { Color = FLinearColor::FromSRGBColor(FColor(183, 109, 0)); }
	if (Text_Sentiment)
	{
		const FText* Label = SentimentLabels.Find(Sentiment);
		const FText Mood = Label ? *Label : LOCTEXT("Unknown", "반응");
		Text_Sentiment->SetText(CurrentPost.bOperationAlert ? LOCTEXT("Operation", "운영 알림")
			: bShowSourceLabel ? FText::Format(LOCTEXT("SocialMood", "SNS · {0}"), Mood) : Mood);
		Text_Sentiment->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Text_SentimentSymbol)
	{
		const FText* Symbol = SentimentSymbols.Find(Sentiment);
		Text_SentimentSymbol->SetText(Symbol ? *Symbol : LOCTEXT("UnknownSymbol", "·"));
		Text_SentimentSymbol->SetColorAndOpacity(FSlateColor(Color));
	}
	if (Image_Sentiment)
	{
		// 브러시를 복사해 그림만 바꾼다. Designer 의 크기·여백은 그대로다.
		const TObjectPtr<UTexture2D>* Icon = SentimentIcons.Find(Sentiment);
		UTexture2D* Texture = Icon ? Icon->Get() : nullptr;
		FSlateBrush IconBrush = Image_Sentiment->GetBrush();
		IconBrush.SetResourceObject(Texture);
		Image_Sentiment->SetBrush(IconBrush);
		if (bTintSentimentIcon) { Image_Sentiment->SetColorAndOpacity(Color); }
		Image_Sentiment->SetVisibility(Texture && !CurrentPost.bOperationAlert ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (bShowSourceLabel && Image_Avatar)
	{
		FSlateBrush Avatar = Image_Avatar->GetBrush();
		Avatar.SetResourceObject(CurrentPost.bOperationAlert ? OperationIcon.Get() : SocialIcon.Get());
		Image_Avatar->SetBrush(Avatar);
		Image_Avatar->SetColorAndOpacity(CurrentPost.bOperationAlert ? Color : NeutralColor);
	}
	if (bShowSourceLabel && Border_Avatar)
	{
		Border_Avatar->SetBrushColor(CurrentPost.bOperationAlert
			? FLinearColor::FromSRGBColor(FColor(255, 245, 224)) : FLinearColor::FromSRGBColor(FColor(236, 243, 244)));
	}
	// 색과 기호·라벨을 동시에 사용한다.
	if (Border_Sentiment)
	{
		const FLinearColor* Background = SentimentBackgroundColors.Find(Sentiment);
		Border_Sentiment->SetBrushColor(CurrentPost.bOperationAlert
			? FLinearColor::FromSRGBColor(FColor(255, 243, 218)) : Background ? *Background : FLinearColor::White);
	}
}
#undef LOCTEXT_NAMESPACE
