// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelSNS.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;
class UWidget;
class UScrollBox;
class UFF_FestivalViewModelSubsystem;
class UFF_LiveSnsPostEntry;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLiveSnsPendingPostsChanged, int32);

/** 원본 반응의 방향. 강도 0·음수 1과 미확정 센티널을 구분한다. */
UENUM(BlueprintType)
enum class EFF_LiveSnsSentiment : uint8 { Unknown, Positive, Negative, Neutral };

UENUM(BlueprintType)
enum class EFF_LiveSnsFilter : uint8 { All, Positive, Negative };

/** 새 글 빨간 점을 언제 지우는지. */
UENUM(BlueprintType)
enum class EFF_LiveSnsNewMarkClear : uint8
{
	/** 패널을 닫을 때 그때까지 온 글을 모두 읽음으로 친다. 다음에 열면 그 사이 온 글에만 점이 있다. */
	OnPanelClose UMETA(DisplayName = "패널을 닫을 때"),
	/** 열린 패널에서 점이 보인 뒤 `New Mark Seconds` 가 지나면 지운다. */
	AfterSeconds UMETA(DisplayName = "보이고 몇 초 뒤"),
};

/**
 * SNS 반응 한 건.
 *
 * **2026-09-16 — 이 값을 채우는 시스템이 생겼다.** `UFF_CrowdInsightsSubsystem`(김시리)이
 * 3~10초마다 화제와 문장을 결정론적으로 골라 `FFF_SnsPostRow`를 만들고,
 * `UFF_FestivalViewModelSubsystem`이 그것을 이 구조체로 바꿔 중계한다
 * (`GetRecentSnsPosts()` · `OnSnsPostAdded`). 방향 필드를 보완해 실제 음수 1·강도 0 반응을
 * 미확정 센티널과 구분한다. 행의 외형은 별도 WBP에서 편집한다.
 *
 * 처음에 더미 글을 만들지 않은 이유를 남겨 둔다: 그럴듯한 가짜 반응이 떠 있으면 팀이
 * "SNS 는 됐다" 고 생각해서 진짜 API 가 안 만들어지기 때문이었다. 실제로 그렇게 됐다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveSocialPost
{
	GENERATED_BODY()

	/** 누가 썼는지. 현지화 대상이라 `FText` 다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText Author;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText Body;

	/** -100 ~ +100. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 SentimentScore = INDEX_NONE;

	/** 원본 방향이 있으면 점수 부호보다 우선한다. 예전 호출자는 점수로 호환한다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveSnsSentiment Sentiment = EFF_LiveSnsSentiment::Unknown;
};

/**
 * SNS 목록 상태. `Sync` 는 원본 최근 글로 목록을 다시 만들고, `RecordArrival` 은 닫혀 있는 동안 온 글 수(배지)를 센다.
 * 2026-10-07부터 열린 목록에는 `InsertArrival` 로 새 글을 바로 맨 위에 넣는다(자동 표시).
 *
 * 새 글 점: 구독한 뒤 도착한 글마다 번호(`TotalArrivals`)를 매기고, `SeenSerial` 보다 큰 번호가 새 글이다.
 * 원본 목록은 최신순이고 새 글이 맨 앞에 들어오므로 `Sync` 의 i 번째 글 번호는 `TotalArrivals - i` 다.
 * 원본 글에 고유 번호가 없어서 이렇게 도착 순서로 센다.
 */
struct FESTAFIESTA_API FFF_LiveSnsFeedState
{
	EFF_LiveSnsFilter Filter = EFF_LiveSnsFilter::All;
	TArray<FFF_LiveSocialPost> DisplayedPosts;
	/** `DisplayedPosts` 와 같은 순서의 도착 번호. 구독 전부터 있던 글은 0 이하다. */
	TArray<int64> DisplayedSerials;
	int32 PendingCount = 0;
	int64 TotalArrivals = 0;
	int64 SeenSerial = 0;
	static EFF_LiveSnsSentiment ResolveSentiment(const FFF_LiveSocialPost& Post);
	static bool Matches(const FFF_LiveSocialPost& Post, EFF_LiveSnsFilter InFilter);
	void Sync(const TArray<FFF_LiveSocialPost>& RecentPosts, int32 Limit);
	void RecordArrival(const FFF_LiveSocialPost& Post);
	/** 글 하나가 도착했다(필터와 무관). 원본 목록에 들어간 직후에 부른다. */
	void NoteArrival() { ++TotalArrivals; }
	/** 열린 목록 맨 위에 넣는다. 필터 밖이면 false. 한도를 넘는 아래쪽 글은 뺀다. `NoteArrival` 다음에 부른다. */
	bool InsertArrival(const FFF_LiveSocialPost& Post, int32 Limit);
	bool IsNew(int32 Index) const { return DisplayedSerials.IsValidIndex(Index) && DisplayedSerials[Index] > SeenSerial; }
	void MarkAllSeen() { SeenSerial = TotalArrivals; }
	void MarkSeenUpTo(int64 Serial) { SeenSerial = FMath::Max(SeenSerial, Serial); }
	void ResetArrivals() { TotalArrivals = SeenSerial = 0; }
};

/**
 * SNS 실데이터 패널. 기본 패널은 열 때 목록을 동기화하고 닫을 때 구독을 해제한다.
 * 도크용 보존 모드는 소멸 시에만 해제하여 닫힌 동안의 새 글과 읽던 위치를 유지한다.
 * 기본 최근 30건을 표시한다. 반응 강도는 만족도 점수가 아니므로 방향과 문장만 표시한다.
 *
 * 새 글 (2026-10-07 사용자 결정): 버튼을 누르지 않아도 목록에 바로 들어간다. 열 때는 그동안 온 글을,
 * 열려 있을 때는 오는 즉시 맨 위에 넣고 이름 옆에 빨간 점을 단다. 아래로 내려 읽는 중이면 새 글 높이만큼
 * 스크롤을 맞춰 보던 글이 밀리지 않는다. `Auto Show New Posts` 를 끄면 예전처럼 `새 글 N개 보기` 버튼으로 알린다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - SNS"))
class FESTAFIESTA_API UFF_LivePanelSNS : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFF_LivePanelSNS(const FObjectInitializer& ObjectInitializer);

	/** 도크 모드에서는 닫혀 있어도 새 글을 받고 재열기 시 읽던 목록·위치를 보존한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS")
	bool bPreserveFeedWhileClosed = false;

	/** 첫 열기·필터 변경·새 글 보기에서 실제 목록을 동기화했을 때 알린다. */
	FSimpleMulticastDelegate OnFeedAcknowledged;
	FOnLiveSnsPendingPostsChanged OnPendingPostsChanged;
	int32 GetPendingPostCount() const { return Feed.PendingCount; }

	/** 새 플라이아웃 최초 제작용. 기존 WBP에는 호출하지 않는다. */
	void ConfigureFlyoutDefaults(TSubclassOf<UFF_LiveSnsPostEntry> InEntryClass);
	/** 탭형 필터·구분선 행(2026-10-07 승인안) 최초 적용용. 이후 값은 Class Defaults 에서 고친다. */
	void ConfigureTabFilterDefaults(const FLinearColor& SelectedText, const FLinearColor& UnselectedText);

	/** 테스트·기존 디자이너 호출 호환 전용. 실제 열기에서는 ViewModel로 동기화한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Debug")
	void Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	UFUNCTION()
	void HandlePostAdded(FFF_LiveSocialPost NewPost);
	UFUNCTION()
	void HandleFilterAll();
	UFUNCTION()
	void HandleFilterPositive();
	UFUNCTION()
	void HandleFilterNegative();
	UFUNCTION()
	void HandleNewPosts();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UVerticalBox> Box_Posts;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UScrollBox> Scroll_Posts;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UButton> Button_FilterAll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UButton> Button_FilterPositive;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UButton> Button_FilterNegative;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_FilterAll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_FilterPositive;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_FilterNegative;
	/** 탭형 필터의 밑줄. 선택한 탭만 보이고 나머지는 자리만 지킨다(Hidden). 없으면 무시한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UWidget> Underline_FilterAll;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UWidget> Underline_FilterPositive;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UWidget> Underline_FilterNegative;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UButton> Button_NewPosts;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_NewPosts;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Count;

	/** 왜 비어 있는지. **이 패널의 본론이다.** */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|SNS")
	TObjectPtr<UTextBlock> Text_MissingNote;

	/** 켜면 새 글을 버튼 없이 바로 목록에 넣는다(열 때·열려 있을 때). 끄면 `새 글 N개 보기` 버튼으로 알린다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|NewPosts")
	bool bAutoShowNewPosts = true;
	/** 새 글 빨간 점을 지우는 때. 점 크기·색은 글 행 WBP 의 `Image_NewDot` 에서 고친다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|NewPosts")
	EFF_LiveSnsNewMarkClear NewMarkClear = EFF_LiveSnsNewMarkClear::OnPanelClose;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|NewPosts",
		meta = (EditCondition = "NewMarkClear == EFF_LiveSnsNewMarkClear::AfterSeconds", ClampMin = "0.5", UIMax = "30.0"))
	float NewMarkSeconds = 5.f;
	/**
	 * 목록을 이만큼(px) 넘게 내려 읽고 있으면 새 글이 위에 들어와도 보던 글을 제자리에 둔다.
	 * 맨 위 근처를 보고 있으면 새 글이 그대로 위에 보인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|NewPosts", meta = (ClampMin = "0.0"))
	float KeepReadingOffset = 8.f;

	/** 원본 보관 한도 이내의 UI 표시 정책. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS", meta = (ClampMin = "1", ClampMax = "80"))
	int32 DisplayLimit = 30;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS")
	TSubclassOf<UFF_LiveSnsPostEntry> PostEntryClass;

	/** 글 사이 간격. 글 자체의 크기·글꼴·내부 여백은 행 WBP Designer에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style", meta = (ClampMin = "0.0", UIMax = "40.0"))
	float PostSpacing = 10.f;
	/**
	 * 선택한 필터는 ✓ 없이 색을 뒤집어 보여 준다 (2026-10-06 사용자 결정).
	 * 버튼 브러시에 곱하므로 흰 바탕 브러시가 이 색이 된다. 글자와 밝기 차이가 커서 색맹에도 구분된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor SelectedFilterBackgroundColor = FLinearColor::FromSRGBColor(FColor(25, 139, 91));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor UnselectedFilterBackgroundColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor SelectedFilterTextColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FLinearColor UnselectedFilterTextColor = FLinearColor::FromSRGBColor(FColor(24, 70, 58));
	/**
	 * 끄면 필터 버튼 바탕색을 바꾸지 않는다. 도크 SNS의 탭은 꺼 두고 밑줄·글자 굵기로 선택을 보인다.
	 * 위 두 바탕색은 켜져 있을 때만 쓴다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	bool bTintFilterBackground = true;
	/**
	 * 선택한 필터 글자의 굵기(글꼴 Typeface 이름, 예: SemiBold). 비우면 굵기를 바꾸지 않는다.
	 * 글꼴·크기는 Designer 값을 쓰고, 선택이 풀리면 Designer 굵기로 되돌린다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Style")
	FName SelectedFilterTypeface;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	TMap<EFF_LiveSnsFilter, FText> FilterLabels;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text", meta = (ToolTip = "{Label}: 선택한 필터 이름"))
	FText SelectedFilterTextFormat = NSLOCTEXT("FestaFiestaLiveSNS", "SelectedFilterPlain", "{Label}");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text", meta = (ToolTip = "{Count}: 현재 표시 중인 글 수"))
	FText CountTextFormat = NSLOCTEXT("FestaFiestaLiveSNS", "DisplayedCount", "최근 {Count}개 · 최신순");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text", meta = (ToolTip = "{Count}: 선택 필터에 맞는 새 글 수"))
	FText NewPostsTextFormat = NSLOCTEXT("FestaFiestaLiveSNS", "PendingCount", "↑ 새 글 {Count}개 보기");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	FText MissingCountText = NSLOCTEXT("FestaFiestaLiveSNS", "MissingCount", "—");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	FText SourceUnavailableText = NSLOCTEXT("FestaFiestaLiveSNS", "SourceMissing", "관객 반응을 아직 읽을 수 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	FText NoPostsText = NSLOCTEXT("FestaFiestaLiveSNS", "Empty", "아직 올라온 반응이 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	FText FilterEmptyText = NSLOCTEXT("FestaFiestaLiveSNS", "FilterEmpty", "이 조건에 맞는 반응이 아직 없습니다.");
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS|Text")
	FText EntryUnavailableText = NSLOCTEXT("FestaFiestaLiveSNS", "EntryMissing", "반응 표시 위젯을 불러오지 못했습니다.");

private:
	void BindControls();
	void BindViewModel();
	void Unbind();
	void SyncFromViewModel();
	void SetFilter(EFF_LiveSnsFilter Filter);
	void RefreshPending();
	void RefreshFilters();
	void RefreshAll();
	void RefreshSummary();
	/**
	 * 행 수를 글 수에 맞추고 i 번째 행에 i 번째 글을 넣는다. 모자라면 아래에 붙이고 남으면 아래에서 뺀다.
	 * 맨 위에 새 행을 끼우지 않는 이유: UMG 는 실행 중 맨 위 삽입을 Slate 에 바로 반영하지 못해 전체를 다시
	 * 만들어야 하고, 그러면 모든 글이 한 프레임 동안 줄바꿈 없이 그려져 목록이 출렁인다. 행은 그대로 두고
	 * 내용만 한 칸씩 내린다. 그래서 새 글이 와도 행 높이가 바로 맞고 스크롤 보정도 같은 프레임에 끝난다.
	 */
	bool BindRows();
	void RefreshNewMarks();
	void InsertLivePost(const FFF_LiveSocialPost& Post);
	void ClearNewMarks();
	void TickNewMarks();
	void TickScrollAnchor();

	FFF_LiveSnsFeedState Feed;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_LiveSnsPostEntry>> PostEntries;
	bool bPanelOpen = false;
	/**
	 * 내려 읽는 중 새 글이 위에 들어왔을 때 보던 글을 제자리에 두는 보정.
	 * 새 글 k 개가 위 k 개 행에 들어왔으므로 그 행 높이 합만큼 스크롤을 더 내린다.
	 */
	struct FScrollAnchor
	{
		float OffsetBefore = 0.f;
		int32 InsertedCount = 0;
		int32 FramesLeft = 0;
	} ScrollAnchor;
	/** `보이고 몇 초 뒤` 방식에서 새 글(도착 번호)의 점이 처음 보인 시각. */
	TMap<int64, double> NewMarkShownAt;
	// 필터 글자의 Designer 굵기. 선택 강조를 풀 때 되돌린다.
	TMap<TObjectPtr<UTextBlock>, FName> DesignerFilterTypefaces;
	// 원본에서 읽은 빈 목록과 읽기 자체가 불가능한 상태를 구분한다.
	bool bSourceAvailable = false;
	bool bHasSourcePosts = false;
	bool bFeedInitialized = false;
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
