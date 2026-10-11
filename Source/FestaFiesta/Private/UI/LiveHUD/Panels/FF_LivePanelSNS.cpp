#include "UI/LiveHUD/Panels/FF_LivePanelSNS.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/LiveHUD/OperationAlert/FF_OperationAlertSubsystem.h"
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
		|| (InFilter == EFF_LiveSnsFilter::Social && !Post.bOperationAlert)
		|| (InFilter == EFF_LiveSnsFilter::Operation && Post.bOperationAlert)
		|| (InFilter == EFF_LiveSnsFilter::Positive && Direction == EFF_LiveSnsSentiment::Positive)
		|| (InFilter == EFF_LiveSnsFilter::Negative && Direction == EFF_LiveSnsSentiment::Negative);
}

void FFF_LiveSnsFeedState::Sync(const TArray<FFF_LiveSocialPost>& RecentPosts, int32 Limit)
{
	DisplayedPosts.Reset();
	DisplayedSerials.Reset();
	const int32 Capacity = FMath::Clamp(Limit, 1, 80);
	for (int32 Index = 0; Index < RecentPosts.Num(); ++Index)
	{
		if (Matches(RecentPosts[Index], Filter))
		{
			DisplayedPosts.Add(RecentPosts[Index]);
			// 원본은 최신순이고 새 글이 맨 앞에 들어온다. 앞에서 i 번째 글은 i 개 전에 도착했다.
			DisplayedSerials.Add(TotalArrivals - Index);
			if (DisplayedPosts.Num() >= Capacity) { break; }
		}
	}
	PendingCount = 0;
}

bool FFF_LiveSnsFeedState::InsertArrival(const FFF_LiveSocialPost& Post, int32 Limit)
{
	if (!Matches(Post, Filter)) { return false; }
	DisplayedPosts.Insert(Post, 0);
	DisplayedSerials.Insert(TotalArrivals, 0);
	const int32 Capacity = FMath::Clamp(Limit, 1, 80);
	if (DisplayedPosts.Num() > Capacity)
	{
		DisplayedPosts.SetNum(Capacity);
		DisplayedSerials.SetNum(Capacity);
	}
	return true;
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

void UFF_LivePanelSNS::NativeConstruct()
{
	Super::NativeConstruct();
	if (bPreserveFeedWhileClosed)
	{
		// 첫 열기 전의 새 글도 도크 알림에 포함한다. 목록은 열 때 한 번 읽는다.
		BindControls();
		BindViewModel();
	}
}

void UFF_LivePanelSNS::ConfigureFlyoutDefaults(TSubclassOf<UFF_LiveSnsPostEntry> InEntryClass)
{
	PostEntryClass = InEntryClass;
	bPreserveFeedWhileClosed = true;
	bAllowDragToMove = false;
}

void UFF_LivePanelSNS::ConfigureUnifiedNewsDefaults()
{
	bUnifiedNews = true;
	bPreserveFeedWhileClosed = true;
	bTintFilterBackground = true;
	SelectedFilterTextColor = FLinearColor::White;
	UnselectedFilterTextColor = FLinearColor::FromSRGBColor(FColor(24, 70, 58));
	DisplayLimit = 80;
	FilterLabels.Add(EFF_LiveSnsFilter::Social, LOCTEXT("Social", "SNS"));
	FilterLabels.Add(EFF_LiveSnsFilter::Operation, LOCTEXT("Operation", "운영 알림"));
	NewPostsTextFormat = LOCTEXT("NewNews", "↑ 새 소식 {Count}개 보기");
	NoPostsText = LOCTEXT("NoNews", "아직 도착한 소식이 없습니다.");
	FilterEmptyText = LOCTEXT("NoFilteredNews", "이 종류의 소식이 아직 없습니다.");
	SourceUnavailableText = LOCTEXT("NewsUnavailable", "소식을 아직 읽을 수 없습니다.");
}

void UFF_LivePanelSNS::ConfigureTabFilterDefaults(const FLinearColor& SelectedText, const FLinearColor& UnselectedText)
{
	bTintFilterBackground = false;
	SelectedFilterTypeface = TEXT("SemiBold");
	SelectedFilterTextColor = SelectedText;
	UnselectedFilterTextColor = UnselectedText;
	// 행이 아래 구분선과 위아래 여백을 가진다.
	PostSpacing = 0.f;
}

void UFF_LivePanelSNS::NativeOnPanelOpened()
{
	Super::NativeOnPanelOpened();
	if (!bPreserveFeedWhileClosed) { Unbind(); }
	BindControls();
	BindViewModel();
	bPanelOpen = true;
	// 미읽음이 남아 있다는 이유만으로 목록·스크롤을 초기화하지 않는다.
	const bool bArrivedWhileClosed = bAutoShowNewPosts && (bUnifiedNews
		? Feed.TotalArrivals != NewsSyncedArrival : Feed.PendingCount > 0);
	if (!bPreserveFeedWhileClosed || !bFeedInitialized || bArrivedWhileClosed) { SyncFromViewModel(); }
	else
	{
		if (bUnifiedNews && NewsReadingPositions.Contains(Feed.Filter)) { NewsRestoreFrames = 2; }
		RefreshPending();
	}
}

void UFF_LivePanelSNS::NativeOnPanelClosed()
{
	if (bUnifiedNews) { SaveNewsReadingPosition(); }
	bPanelOpen = false;
	ScrollAnchor.FramesLeft = 0;
	NewsRestoreFrames = 0;
	if (!bUnifiedNews && NewMarkClear == EFF_LiveSnsNewMarkClear::OnPanelClose) { ClearNewMarks(); }
	else
	{
		// 시간으로 지우는 방식이면 다음에 열었을 때 다시 그 시간만큼 보인다.
		NewMarkShownAt.Reset();
	}
	if (!bPreserveFeedWhileClosed) { Unbind(); }
	Super::NativeOnPanelClosed();
}

void UFF_LivePanelSNS::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	TickScrollAnchor();
	RestoreNewsReadingPosition();
	TickNewMarks();
}

void UFF_LivePanelSNS::NativeDestruct()
{
	Unbind();
	bFeedInitialized = false;
	bPanelOpen = false;
	ScrollAnchor = FScrollAnchor();
	NewMarkShownAt.Reset();
	PostEntries.Reset();
	RecentNews.Reset();
	UnreadNewsSerials.Reset();
	NewNewsSerials.Reset();
	bNewsInitialized = false;
	NewsReadingPositions.Reset();
	NewsRestoreFrames = 0;
	NewsSyncedArrival = 0;
	Super::NativeDestruct();
}

void UFF_LivePanelSNS::BindViewModel()
{
	UGameInstance* GI = GetGameInstance();
	UFF_FestivalViewModelSubsystem* Current = GI ? GI->GetSubsystem<UFF_FestivalViewModelSubsystem>() : nullptr;
	if (Current != CachedViewModel)
	{
		if (CachedViewModel) { CachedViewModel->OnSnsPostAdded.RemoveDynamic(this, &ThisClass::HandlePostAdded); }
		CachedViewModel = Current;
		bFeedInitialized = false;
		// 도착 번호는 구독한 원본 기준이다. 원본이 바뀌면 새로 센다.
		Feed.ResetArrivals();
		bNewsInitialized = false;
	}
	if (CachedViewModel) { CachedViewModel->OnSnsPostAdded.AddUniqueDynamic(this, &ThisClass::HandlePostAdded); }
	if (bUnifiedNews)
	{
		UFF_OperationAlertSubsystem* Alerts = GetWorld() ? GetWorld()->GetSubsystem<UFF_OperationAlertSubsystem>() : nullptr;
		if (CachedOperationAlerts != Alerts)
		{
			if (CachedOperationAlerts) { CachedOperationAlerts->OnOperationAlertRaised.RemoveDynamic(this, &ThisClass::HandleOperationAlert); }
			CachedOperationAlerts = Alerts;
			bNewsInitialized = false;
		}
		if (CachedOperationAlerts) { CachedOperationAlerts->OnOperationAlertRaised.AddUniqueDynamic(this, &ThisClass::HandleOperationAlert); }
		if (!bNewsInitialized && (CachedViewModel || CachedOperationAlerts))
		{
			RecentNews.Reset();
			Feed.ResetArrivals();
			bFeedInitialized = false;
			NewsSyncedArrival = 0;
			NewsRestoreFrames = 0;
			NewsReadingPositions.Reset();
			UnreadNewsSerials.Reset(); NewNewsSerials.Reset();
			// 늦게 열린 도크도 기존 기록을 잃지 않는다. 공통 시각이 없는 과거 기록은 수신 전 기록으로만 복원한다.
			if (CachedViewModel) { RecentNews = CachedViewModel->GetRecentSnsPosts(); }
			if (CachedOperationAlerts)
			{
				const auto& History = CachedOperationAlerts->GetHistory();
				// SNS가 원본 한도(80건)를 채웠어도 운영 기록이 통째로 잘리지 않게 양쪽 자리를 확보한다.
				const int32 OperationCount = FMath::Min(History.Num(), 80 - FMath::Min(RecentNews.Num(), 40));
				if (RecentNews.Num() > 80 - OperationCount) { RecentNews.SetNum(80 - OperationCount); }
				for (int32 Index = History.Num() - 1; Index >= History.Num() - OperationCount; --Index)
				{
					RecentNews.Add(MakeOperationPost(History[Index]));
				}
			}
			if (RecentNews.Num() > 80) { RecentNews.SetNum(80); }
			bNewsInitialized = true;
		}
	}
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
	if (CachedOperationAlerts) { CachedOperationAlerts->OnOperationAlertRaised.RemoveDynamic(this, &ThisClass::HandleOperationAlert); }
	CachedOperationAlerts = nullptr;
	if (Button_FilterAll) { Button_FilterAll->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterAll); }
	if (Button_FilterPositive) { Button_FilterPositive->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterPositive); }
	if (Button_FilterNegative) { Button_FilterNegative->OnClicked.RemoveDynamic(this, &ThisClass::HandleFilterNegative); }
	if (Button_NewPosts) { Button_NewPosts->OnClicked.RemoveDynamic(this, &ThisClass::HandleNewPosts); }
}

void UFF_LivePanelSNS::SyncFromViewModel()
{
	bSourceAvailable = IsValid(CachedViewModel) || (bUnifiedNews && IsValid(CachedOperationAlerts));
	const TArray<FFF_LiveSocialPost> NoPosts;
	const TArray<FFF_LiveSocialPost>& RecentPosts = bUnifiedNews ? RecentNews
		: CachedViewModel ? CachedViewModel->GetRecentSnsPosts() : NoPosts;
	bHasSourcePosts = !RecentPosts.IsEmpty();
	Feed.Sync(RecentPosts, DisplayLimit);
	if (bUnifiedNews) { NewsSyncedArrival = Feed.TotalArrivals; }
	bFeedInitialized = bSourceAvailable;
	RefreshAll();
	if (Scroll_Posts)
	{
		Scroll_Posts->EndInertialScrolling();
		if (bUnifiedNews && NewsReadingPositions.Contains(Feed.Filter))
		{
			// 행의 새 높이가 계산된 뒤 같은 글·글 안의 위치로 복원한다.
			NewsRestoreFrames = 2;
			Scroll_Posts->SetScrollOffset(NewsReadingPositions[Feed.Filter].Offset);
		}
		else { Scroll_Posts->ScrollToStart(); }
	}
	OnFeedAcknowledged.Broadcast();
}

void UFF_LivePanelSNS::HandlePostAdded(FFF_LiveSocialPost NewPost)
{
	if (bUnifiedNews) { RecordNews(NewPost); return; }
	Feed.NoteArrival();
	if (bAutoShowNewPosts && bPanelOpen && bFeedInitialized)
	{
		InsertLivePost(NewPost);
		return;
	}
	// 닫혀 있는 동안은 도크 배지로 센다. 열 때 한 번에 반영한다.
	Feed.RecordArrival(NewPost);
	RefreshPending();
}

FFF_LiveSocialPost UFF_LivePanelSNS::MakeOperationPost(const FFF_OperationAlert& Alert)
{
	FFF_LiveSocialPost Post;
	Post.bOperationAlert = true;
	Post.OperationDay = Alert.Day;
	FString Headline, Detail;
	if (Alert.Message.ToString().Split(TEXT(". "), &Headline, &Detail))
	{
		Post.Author = FText::FromString(Headline);
		Post.Body = FText::FromString(Detail);
	}
	else
	{
		Post.Author = FText::Format(LOCTEXT("OperationDay", "운영 알림 · {0}일차"), FText::AsNumber(Alert.Day));
		Post.Body = Alert.Message;
	}
	return Post;
}

void UFF_LivePanelSNS::HandleOperationAlert(const FFF_OperationAlert& Alert)
{
	if (bUnifiedNews) { RecordNews(MakeOperationPost(Alert)); }
}

void UFF_LivePanelSNS::RecordNews(const FFF_LiveSocialPost& Post)
{
	Feed.NoteArrival();
	RecentNews.Insert(Post, 0);
	if (RecentNews.Num() > 80) { RecentNews.SetNum(80); }
	NewNewsSerials.Add(Feed.TotalArrivals);
	UnreadNewsSerials.Add(Feed.TotalArrivals);
	const bool bShownNow = bPanelOpen && bFeedInitialized && bAutoShowNewPosts && FFF_LiveSnsFeedState::Matches(Post, Feed.Filter);
	const int64 Oldest = Feed.TotalArrivals - RecentNews.Num() + 1;
	for (auto It = UnreadNewsSerials.CreateIterator(); It; ++It) { if (*It < Oldest) { It.RemoveCurrent(); } }
	for (auto It = NewNewsSerials.CreateIterator(); It; ++It) { if (*It < Oldest) { It.RemoveCurrent(); } }
	// 다른 종류의 글이 들어와도 보관 한도 밖의 행·점은 함께 제거한다.
	bool bRowsExpired = false;
	for (int32 Index = Feed.DisplayedSerials.Num() - 1; Index >= 0; --Index)
	{
		if (Feed.DisplayedSerials[Index] < Oldest)
		{
			Feed.DisplayedSerials.RemoveAt(Index);
			Feed.DisplayedPosts.RemoveAt(Index);
			bRowsExpired = true;
		}
	}
	if (bShownNow) { InsertLivePost(Post); }
	else if (bRowsExpired) { BindRows(); RefreshSummary(); }
	if (bPanelOpen && bFeedInitialized && bAutoShowNewPosts) { NewsSyncedArrival = Feed.TotalArrivals; }
	RefreshPending();
	RefreshFilters();
}

void UFF_LivePanelSNS::HandleNewsRead(int64 Serial)
{
	if (!bUnifiedNews || !Feed.DisplayedSerials.Contains(Serial) || !UnreadNewsSerials.Remove(Serial)) { return; }
	NewNewsSerials.Remove(Serial);
	// 행 재생성이나 목록 동기화를 하지 않아 클릭한 위치가 움직이지 않는다.
	RefreshNewMarks();
	RefreshFilters();
	RefreshPending();
}

int32 UFF_LivePanelSNS::CountUnreadNews(bool bOperation) const
{
	int32 Count = 0;
	for (int32 Index = 0; Index < RecentNews.Num(); ++Index)
	{
		if (RecentNews[Index].bOperationAlert == bOperation && UnreadNewsSerials.Contains(Feed.TotalArrivals - Index)) { ++Count; }
	}
	return Count;
}

void UFF_LivePanelSNS::InsertLivePost(const FFF_LiveSocialPost& Post)
{
	// 지금 스크롤 위치는 행 내용을 바꾸기 전에 읽는다.
	const float Offset = Scroll_Posts ? Scroll_Posts->GetScrollOffset() : 0.f;
	if (!Feed.InsertArrival(Post, DisplayLimit)) { return; }
	bHasSourcePosts = true;
	RefreshSummary();
	if (!BindRows())
	{
		SetTextOrHide(Text_MissingNote, EntryUnavailableText);
		return;
	}
	// 맨 위 근처면 새 글이 그대로 보인다. 내려 읽는 중이면 보던 글을 제자리에 둔다.
	// 여러 글이 연달아 오면 처음 위치를 기준으로 들어온 수만큼 더한다.
	if (Scroll_Posts && (ScrollAnchor.FramesLeft > 0 || Offset > KeepReadingOffset))
	{
		if (ScrollAnchor.FramesLeft <= 0)
		{
			ScrollAnchor = FScrollAnchor();
			ScrollAnchor.OffsetBefore = Offset;
		}
		ScrollAnchor.InsertedCount = FMath::Min(ScrollAnchor.InsertedCount + 1, PostEntries.Num());
		ScrollAnchor.FramesLeft = 2;
	}
}

void UFF_LivePanelSNS::SetFilter(EFF_LiveSnsFilter Filter)
{
	if (Feed.Filter == Filter) { return; }
	if (bUnifiedNews) { SaveNewsReadingPosition(); }
	Feed.Filter = Filter;
	SyncFromViewModel();
}

void UFF_LivePanelSNS::HandleFilterAll() { SetFilter(EFF_LiveSnsFilter::All); }
void UFF_LivePanelSNS::HandleFilterPositive() { SetFilter(bUnifiedNews ? EFF_LiveSnsFilter::Social : EFF_LiveSnsFilter::Positive); }
void UFF_LivePanelSNS::HandleFilterNegative() { SetFilter(bUnifiedNews ? EFF_LiveSnsFilter::Operation : EFF_LiveSnsFilter::Negative); }
void UFF_LivePanelSNS::HandleNewPosts()
{
	if (bUnifiedNews) { NewsReadingPositions.Remove(Feed.Filter); }
	SyncFromViewModel();
}

void UFF_LivePanelSNS::Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts)
{
	bSourceAvailable = true;
	bHasSourcePosts = !InPosts.IsEmpty();
	Feed.Sync(InPosts, DisplayLimit);
	RefreshAll();
}

bool UFF_LivePanelSNS::BindRows()
{
	if (!Box_Posts) { return Feed.DisplayedPosts.IsEmpty(); }
	if (!PostEntryClass && !Feed.DisplayedPosts.IsEmpty())
	{
		PostEntryClass = LoadClass<UFF_LiveSnsPostEntry>(nullptr,
			TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LiveSnsPostEntry.WBP_FF_LiveSnsPostEntry_C"));
	}
	const int32 Wanted = Feed.DisplayedPosts.Num();
	while (PostEntries.Num() > Wanted)
	{
		if (UFF_LiveSnsPostEntry* Extra = PostEntries.Pop())
		{
			Extra->OnPostActivated.RemoveAll(this);
			Extra->RemoveFromParent();
		}
	}
	bool bOk = true;
	while (PostEntries.Num() < Wanted)
	{
		UFF_LiveSnsPostEntry* Entry = PostEntryClass ? CreateWidget<UFF_LiveSnsPostEntry>(this, PostEntryClass) : nullptr;
		if (!Entry) { bOk = false; break; }
		// 아래에 붙이는 것은 실행 중에도 바로 반영된다. 기존 행은 다시 만들지 않는다.
		if (UVerticalBoxSlot* EntrySlot = Box_Posts->AddChildToVerticalBox(Entry))
		{
			EntrySlot->SetHorizontalAlignment(HAlign_Fill);
			EntrySlot->SetPadding(FMargin(0.f, 0.f, 0.f, FMath::Max(0.f, PostSpacing)));
		}
		PostEntries.Add(Entry);
		Entry->OnPostActivated.AddUObject(this, &ThisClass::HandleNewsRead);
	}
	for (int32 Index = 0; Index < PostEntries.Num(); ++Index)
	{
		if (PostEntries[Index])
		{
			PostEntries[Index]->ConfigureReadInteraction(Feed.DisplayedSerials[Index], bUnifiedNews);
			PostEntries[Index]->ConfigurePost(Feed.DisplayedPosts[Index]);
		}
	}
	RefreshNewMarks();
	Box_Posts->SetVisibility(PostEntries.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	return bOk;
}

void UFF_LivePanelSNS::RefreshNewMarks()
{
	for (int32 Index = 0; Index < PostEntries.Num(); ++Index)
	{
		if (PostEntries[Index]) { PostEntries[Index]->SetNewMark(bUnifiedNews
			? NewNewsSerials.Contains(Feed.DisplayedSerials[Index]) : Feed.IsNew(Index)); }
	}
}

void UFF_LivePanelSNS::ClearNewMarks()
{
	if (bUnifiedNews) { return; }
	Feed.MarkAllSeen();
	NewMarkShownAt.Reset();
	RefreshNewMarks();
}

void UFF_LivePanelSNS::TickNewMarks()
{
	if (bUnifiedNews || NewMarkClear != EFF_LiveSnsNewMarkClear::AfterSeconds || !bPanelOpen) { return; }
	// 화면 시간(실시간)으로 센다. 게임 배속과 무관하게 읽을 시간을 준다.
	const double Now = FPlatformTime::Seconds();
	int64 ExpiredUpTo = Feed.SeenSerial;
	for (int32 Index = 0; Index < Feed.DisplayedSerials.Num(); ++Index)
	{
		if (!Feed.IsNew(Index)) { continue; }
		const int64 Serial = Feed.DisplayedSerials[Index];
		const double ShownAt = NewMarkShownAt.FindOrAdd(Serial, Now);
		if (Now - ShownAt >= NewMarkSeconds)
		{
			ExpiredUpTo = FMath::Max(ExpiredUpTo, Serial);
		}
	}
	if (ExpiredUpTo > Feed.SeenSerial)
	{
		// 더 오래된 글은 먼저 보였으므로 이 번호까지 읽은 것으로 친다. 필터를 바꿔도 점이 되살아나지 않는다.
		Feed.MarkSeenUpTo(ExpiredUpTo);
		for (auto It = NewMarkShownAt.CreateIterator(); It; ++It) { if (It.Key() <= ExpiredUpTo) { It.RemoveCurrent(); } }
		RefreshNewMarks();
	}
}

void UFF_LivePanelSNS::TickScrollAnchor()
{
	if (ScrollAnchor.FramesLeft <= 0) { return; }
	--ScrollAnchor.FramesLeft;
	if (!Scroll_Posts) { ScrollAnchor.FramesLeft = 0; return; }
	// 이번 프레임의 배치 계산(prepass)이 끝난 뒤라 위 행들의 높이가 새 글 기준으로 맞다.
	// 행을 다시 만들지 않아 줄바꿈 폭도 이미 알고 있다.
	float Shift = 0.f;
	for (int32 Index = 0; Index < ScrollAnchor.InsertedCount && PostEntries.IsValidIndex(Index); ++Index)
	{
		if (const UFF_LiveSnsPostEntry* Row = PostEntries[Index]) { Shift += Row->GetDesiredSize().Y + FMath::Max(0.f, PostSpacing); }
	}
	Scroll_Posts->SetScrollOffset(ScrollAnchor.OffsetBefore + Shift);
}

void UFF_LivePanelSNS::SaveNewsReadingPosition()
{
	if (!Scroll_Posts || !bFeedInitialized) { return; }
	// 필터를 빠르게 연속 전환해도 아직 복원 중인 위치를 덮어쓰지 않는다.
	if (NewsRestoreFrames > 0) { return; }
	FNewsReadingPosition Position;
	Position.Offset = Scroll_Posts->GetScrollOffset();
	float RowTop = 0.f;
	for (int32 Index = 0; Index < PostEntries.Num(); ++Index)
	{
		const float Height = PostEntries[Index] ? PostEntries[Index]->GetDesiredSize().Y : 0.f;
		if (Height <= 0.f) { break; }
		const float RowExtent = Height + FMath::Max(0.f, PostSpacing);
		if (Position.Offset < RowTop + RowExtent)
		{
			Position.Serial = Feed.DisplayedSerials[Index];
			Position.OffsetInRow = Position.Offset - RowTop;
			Position.bHasRow = true;
			break;
		}
		RowTop += RowExtent;
	}
	NewsReadingPositions.Add(Feed.Filter, Position);
}

void UFF_LivePanelSNS::RestoreNewsReadingPosition()
{
	if (NewsRestoreFrames <= 0 || !Scroll_Posts) { return; }
	--NewsRestoreFrames;
	const FNewsReadingPosition* Position = NewsReadingPositions.Find(Feed.Filter);
	if (!Position) { return; }
	float Offset = Position->Offset;
	const int32 RowIndex = Position->bHasRow ? Feed.DisplayedSerials.Find(Position->Serial) : INDEX_NONE;
	if (RowIndex != INDEX_NONE)
	{
		Offset = Position->OffsetInRow;
		for (int32 Index = 0; Index < RowIndex; ++Index)
		{
			if (PostEntries[Index]) { Offset += PostEntries[Index]->GetDesiredSize().Y + FMath::Max(0.f, PostSpacing); }
		}
	}
	Scroll_Posts->SetScrollOffset(Offset);
}

void UFF_LivePanelSNS::RefreshPending()
{
	OnPendingPostsChanged.Broadcast(GetPendingPostCount());
	// 수동 '새 글 보기'는 아직 목록에 없는 글 수다. 이미 보이는 미읽음 글과 구분한다.
	int32 Pending = Feed.PendingCount;
	if (bUnifiedNews)
	{
		Pending = 0;
		for (int32 Index = 0; Index < RecentNews.Num(); ++Index)
		{
			if (FFF_LiveSnsFeedState::Matches(RecentNews[Index], Feed.Filter)
				&& Feed.TotalArrivals - Index > NewsSyncedArrival) { ++Pending; }
		}
	}
	if (Button_NewPosts)
	{
		// 자동 표시면 버튼이 필요 없다. 접어서 자리까지 없앤다(감싼 SizeBox 도 0 이 된다).
		// 버튼 방식이면 높이를 예약해 새 글 버튼이 나타날 때 목록을 밀어내지 않는다.
		Button_NewPosts->SetVisibility(bAutoShowNewPosts ? ESlateVisibility::Collapsed
			: Pending > 0 ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
	SetTextOrHide(Text_NewPosts, FText::Format(NewPostsTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Pending)}}));
}

void UFF_LivePanelSNS::RefreshFilters()
{
	const auto Apply = [this](UButton* Button, UTextBlock* Label, UWidget* Underline, EFF_LiveSnsFilter Filter)
	{
		const bool bSelected = Feed.Filter == Filter;
		if (Button && bTintFilterBackground)
		{
			// 원본 브러시는 유지하므로 필터를 왕복해도 디자이너 스타일이 누적 변형되지 않는다.
			Button->SetBackgroundColor(bSelected ? SelectedFilterBackgroundColor : UnselectedFilterBackgroundColor);
		}
		if (Underline)
		{
			// 숨겨도 자리를 지켜 탭 높이가 흔들리지 않는다.
			Underline->SetVisibility(bTintFilterBackground ? ESlateVisibility::Collapsed
				: bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (Label)
		{
			const FText* ConfiguredText = FilterLabels.Find(Filter);
			FText Text = ConfiguredText ? *ConfiguredText : FText::GetEmpty();
			if (bUnifiedNews)
			{
				const int32 Unread = Filter == EFF_LiveSnsFilter::All ? GetPendingPostCount()
					: CountUnreadNews(Filter == EFF_LiveSnsFilter::Operation);
				if (Unread > 0) { Text = FText::Format(LOCTEXT("SourceUnread", "{0}  {1}"), Text, FText::AsNumber(Unread)); }
			}
			Label->SetText(bSelected ? FText::Format(SelectedFilterTextFormat,
				FFormatNamedArguments{{TEXT("Label"), Text}}) : Text);
			Label->SetColorAndOpacity(FSlateColor(bSelected ? SelectedFilterTextColor : UnselectedFilterTextColor));
			if (!SelectedFilterTypeface.IsNone())
			{
				const FName DesignerTypeface = DesignerFilterTypefaces.FindOrAdd(Label, Label->GetFont().TypefaceFontName);
				FSlateFontInfo Font = Label->GetFont();
				const FName Wanted = bSelected ? SelectedFilterTypeface : DesignerTypeface;
				if (Font.TypefaceFontName != Wanted)
				{
					Font.TypefaceFontName = Wanted;
					Label->SetFont(Font);
				}
			}
		}
	};
	Apply(Button_FilterAll, Text_FilterAll, Underline_FilterAll, EFF_LiveSnsFilter::All);
	Apply(Button_FilterPositive, Text_FilterPositive, Underline_FilterPositive, bUnifiedNews ? EFF_LiveSnsFilter::Social : EFF_LiveSnsFilter::Positive);
	Apply(Button_FilterNegative, Text_FilterNegative, Underline_FilterNegative, bUnifiedNews ? EFF_LiveSnsFilter::Operation : EFF_LiveSnsFilter::Negative);
}

void UFF_LivePanelSNS::RefreshSummary()
{
	SetTextOrHide(Text_Count, bSourceAvailable ? FText::Format(CountTextFormat,
		FFormatNamedArguments{{TEXT("Count"), FText::AsNumber(Feed.DisplayedPosts.Num())}}) : MissingCountText);
	const bool bEmpty = Feed.DisplayedPosts.IsEmpty();
	SetTextOrHide(Text_Empty, !bSourceAvailable ? SourceUnavailableText
		: bEmpty ? (bHasSourcePosts && Feed.Filter != EFF_LiveSnsFilter::All ? FilterEmptyText : NoPostsText)
		: FText::GetEmpty());
}

void UFF_LivePanelSNS::RefreshAll()
{
	RefreshFilters();
	RefreshSummary();
	SetTextOrHide(Text_MissingNote, FText::GetEmpty());
	RefreshPending();
	ScrollAnchor.FramesLeft = 0;
	NewsRestoreFrames = 0;
	// 행 위젯은 다시 쓰고 내용만 바꾼다.
	if (!BindRows()) { SetTextOrHide(Text_MissingNote, EntryUnavailableText); }
}

#undef LOCTEXT_NAMESPACE
