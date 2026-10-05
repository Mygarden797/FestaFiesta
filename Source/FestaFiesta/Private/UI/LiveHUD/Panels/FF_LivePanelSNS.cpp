#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "UI/LiveHUD/Panels/FF_LiveSnsPostEntry.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"

#define LOCTEXT_NAMESPACE "FestaFiestaLiveSNS"

EFF_LiveSnsSentiment FFF_LiveSnsFeedState::ResolveSentiment(const FFF_LiveSocialPost& Post)
{
	if (Post.Sentiment != EFF_LiveSnsSentiment::Unknown) { return Post.Sentiment; }
	if (Post.SentimentScore == INDEX_NONE) { return EFF_LiveSnsSentiment::Unknown; }
	return Post.SentimentScore > 0 ? EFF_LiveSnsSentiment::Positive
		: Post.SentimentScore < 0 ? EFF_LiveSnsSentiment::Negative : EFF_LiveSnsSentiment::Neutral;
}

bool FFF_LiveSnsFeedState::Matches(const FFF_LiveSocialPost& Post, EFF_LiveSnsFilter InFilter)
{
	const EFF_LiveSnsSentiment Direction = ResolveSentiment(Post);
	return InFilter == EFF_LiveSnsFilter::All
		|| (InFilter == EFF_LiveSnsFilter::Positive && Direction == EFF_LiveSnsSentiment::Positive)
		|| (InFilter == EFF_LiveSnsFilter::Negative && Direction == EFF_LiveSnsSentiment::Negative);
}

void FFF_LiveSnsFeedState::Sync(const TArray<FFF_LiveSocialPost>& RecentPosts, int32 Limit)
{
	DisplayedPosts.Reset();
	const int32 Capacity = FMath::Clamp(Limit, 1, 80);
	for (const FFF_LiveSocialPost& Post : RecentPosts)
	{
		if (Matches(Post, Filter))
		{
			DisplayedPosts.Add(Post);
			if (DisplayedPosts.Num() >= Capacity) { break; }
		}
	}
	PendingCount = 0;
}

void FFF_LiveSnsFeedState::RecordArrival(const FFF_LiveSocialPost& Post)
{
	// 현재 행·스크롤은 그대로 둔다. 원본 보관 한도를 넘는 미확인 건수는 표시하지 않는다.
	if (Matches(Post, Filter)) { PendingCount = FMath::Min(PendingCount + 1, 80); }
}

UFF_LivePanelSNS::UFF_LivePanelSNS(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FilterLabels = {
		{EFF_LiveSnsFilter::All, LOCTEXT("All", "전체")},
		{EFF_LiveSnsFilter::Positive, LOCTEXT("Positive", "긍정")},
		{EFF_LiveSnsFilter::Negative, LOCTEXT("Negative", "아쉬움")}};
}

void UFF_LivePanelSNS::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	Unbind();
	BindControls();
	if (UGameInstance* GI = GetGameInstance())
	{
		CachedViewModel = GI->GetSubsystem<UFF_FestivalViewModelSubsystem>();
	}
	if (CachedViewModel)
	{
		CachedViewModel->OnSnsPostAdded.AddUniqueDynamic(this, &ThisClass::HandlePostAdded);
	}
	SyncFromViewModel();
}

void UFF_LivePanelSNS::NativeOnPanelClosed()
{
	Unbind();
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelSNS::NativeDestruct()
{
	Unbind();
	Super::NativeDestruct();
}

void UFF_LivePanelSNS::BindControls()
{
	if (Button_FilterAll) { Button_FilterAll->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFilterAll); }
	if (Button_FilterPositive) { Button_FilterPositive->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFilterPositive); }
	if (Button_FilterNegative) { Button_FilterNegative->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFilterNegative); }
	if (Button_NewPosts) { Button_NewPosts->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNewPosts); }
}

void UFF_LivePanelSNS::Unbind()
{
	if (CachedViewModel) { CachedViewModel->OnSnsPostAdded.RemoveDynamic(this, &ThisClass::HandlePostAdded); }
	CachedViewModel = nullptr;
	if (Button_FilterAll) { Button_FilterAll->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterAll); }
	if (Button_FilterPositive) { Button_FilterPositive->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterPositive); }
	if (Button_FilterNegative) { Button_FilterNegative->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterNegative); }
	if (Button_NewPosts) { Button_NewPosts->OnClicked.RemoveDynamic(this, &ThisClass::HandleNewPosts); }
}

void UFF_LivePanelSNS::SyncFromViewModel()
{
	bSourceAvailable = IsValid(CachedViewModel);
	const TArray<FFF_LiveSocialPost> NoPosts;
	const TArray<FFF_LiveSocialPost>& RecentPosts = bSourceAvailable ? CachedViewModel->GetRecentSnsPosts() : NoPosts;
	bHasSourcePosts = !RecentPosts.IsEmpty();
	Feed.Sync(RecentPosts, DisplayLimit);
	RefreshAll();
	if (Scroll_Posts)
	{
		Scroll_Posts->EndInertialScrolling();
		Scroll_Posts->ScrollToStart();
	}
}

void UFF_LivePanelSNS::HandlePostAdded(FFF_LiveSocialPost NewPost)
{
	Feed.RecordArrival(NewPost);
	RefreshPending();
}

void UFF_LivePanelSNS::SetFilter(EFF_LiveSnsFilter Filter)
{
	Feed.Filter = Filter;
	SyncFromViewModel();
}

void UFF_LivePanelSNS::HandleFilterAll() { SetFilter(EFF_LiveSnsFilter::All); }
void UFF_LivePanelSNS::HandleFilterPositive() { SetFilter(EFF_LiveSnsFilter::Positive); }
void UFF_LivePanelSNS::HandleFilterNegative() { SetFilter(EFF_LiveSnsFilter::Negative); }
void UFF_LivePanelSNS::HandleNewPosts() { SyncFromViewModel(); }

void UFF_LivePanelSNS::Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts)
{
	bSourceAvailable = true;
	bHasSourcePosts = !InPosts.IsEmpty();
	Feed.Sync(InPosts, DisplayLimit);
	RefreshAll();
}

void UFF_LivePanelSNS::RefreshPending()
{
	if (Button_NewPosts)
	{
		// 높이를 예약해 새 글 버튼이 나타날 때 목록을 밀어내지 않는다.
		Button_NewPosts->SetVisibility(Feed.PendingCount > 0 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	SetTextOrHide(Text_NewPosts, FText::Format(NewPostsTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Feed.PendingCount)}}));
}

void UFF_LivePanelSNS::RefreshFilters()
{
	const auto Apply = [this](UButton* Button, UTextBlock* Label, EFF_LiveSnsFilter Filter)
	{
		const bool bSelected = Feed.Filter == Filter;
		if (Button)
		{
			// 원본 브러시는 유지하므로 필터를 왕복해도 디자이너 스타일이 누적 변형되지 않는다.
			Button->SetBackgroundColor(bSelected ? SelectedFilterBackgroundColor : UnselectedFilterBackgroundColor);
		}
		if (Label)
		{
			const FText* ConfiguredText = FilterLabels.Find(Filter);
			const FText Text = ConfiguredText ? *ConfiguredText : FText::GetEmpty();
			Label->SetText(bSelected ? FText::Format(SelectedFilterTextFormat,
				FFormatNamedArguments{{TEXT("Label"), Text}}) : Text);
			Label->SetColorAndOpacity(FSlateColor(bSelected ? SelectedFilterTextColor : UnselectedFilterTextColor));
		}
	};
	Apply(Button_FilterAll, Text_FilterAll, EFF_LiveSnsFilter::All);
	Apply(Button_FilterPositive, Text_FilterPositive, EFF_LiveSnsFilter::Positive);
	Apply(Button_FilterNegative, Text_FilterNegative, EFF_LiveSnsFilter::Negative);
}

void UFF_LivePanelSNS::RefreshAll()
{
	RefreshFilters();
	SetTextOrHide(Text_Count, bSourceAvailable ? FText::Format(CountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Feed.DisplayedPosts.Num())}}) : MissingCountText);
	SetTextOrHide(Text_MissingNote, FText::GetEmpty());
	RefreshPending();
	const bool bEmpty = Feed.DisplayedPosts.IsEmpty();
	SetTextOrHide(Text_Empty, !bSourceAvailable ? SourceUnavailableText
		: bEmpty ? (bHasSourcePosts && Feed.Filter != EFF_LiveSnsFilter::All ? FilterEmptyText : NoPostsText)
		: FText::GetEmpty());
	if (!Box_Posts)
	{
		if (!bEmpty) { SetTextOrHide(Text_MissingNote, EntryUnavailableText); }
		return;
	}
	Box_Posts->ClearChildren();

	if (!PostEntryClass && !Feed.DisplayedPosts.IsEmpty())
	{
		PostEntryClass = LoadClass<UFF_LiveSnsPostEntry>(nullptr,
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry.WBP_FF_LiveSnsPostEntry_C"));
	}
	bool bRenderFailed = false;
	for (const FFF_LiveSocialPost& Post : Feed.DisplayedPosts)
	{
		UFF_LiveSnsPostEntry* Entry = PostEntryClass ? CreateWidget<UFF_LiveSnsPostEntry>(this, PostEntryClass) : nullptr;
		if (!Entry) { bRenderFailed = true; break; }
		Entry->ConfigurePost(Post);
		if (UVerticalBoxSlot* EntrySlot = Box_Posts->AddChildToVerticalBox(Entry))
		{
			EntrySlot->SetHorizontalAlignment(HAlign_Fill);
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, FMath::Max(0.f, PostSpacing)));
		}
	}
	Box_Posts->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (bRenderFailed) { SetTextOrHide(Text_MissingNote, EntryUnavailableText); }
}

#undef LOCTEXT_NAMESPACE
