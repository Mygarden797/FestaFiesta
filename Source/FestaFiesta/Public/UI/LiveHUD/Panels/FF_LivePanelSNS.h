// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelSNS.generated.h"

class UTextBlock;
class UVerticalBox;
class UButton;
class UScrollBox;
class UFF_FestivalViewModelSubsystem;
class UFF_LiveSnsPostEntry;

/** 원본 반응의 방향. 강도 0·음수 1과 미확정 센티널을 구분한다. */
UENUM(BlueprintType)
enum class EFF_LiveSnsSentiment : uint8 { Unknown, Positive, Negative, Neutral };

UENUM(BlueprintType)
enum class EFF_LiveSnsFilter : uint8 { All, Positive, Negative };

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

/** 새 글은 대기시키고 명시적 동기화에서만 현재 목록을 교체한다. */
struct FESTAFIESTA_API FFF_LiveSnsFeedState
{
	EFF_LiveSnsFilter Filter = EFF_LiveSnsFilter::All;
	TArray<FFF_LiveSocialPost> DisplayedPosts;
	int32 PendingCount = 0;
	static EFF_LiveSnsSentiment ResolveSentiment(const FFF_LiveSocialPost& Post);
	static bool Matches(const FFF_LiveSocialPost& Post, EFF_LiveSnsFilter InFilter);
	void Sync(const TArray<FFF_LiveSocialPost>& RecentPosts, int32 Limit);
	void RecordArrival(const FFF_LiveSocialPost& Post);
};

/**
 * SNS 실데이터 패널. 열 때 기존 목록을 동기화하고 닫기·소멸 시 구독을 해제한다.
 * 기본 최근 30건을 표시하며 새 글은 읽던 행을 밀지 않고 버튼으로 알린다.
 * 반응 강도는 만족도 점수가 아니므로 방향과 문장만 표시한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - SNS"))
class FESTAFIESTA_API UFF_LivePanelSNS : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	/** 테스트·기존 디자이너 호출 호환 전용. 실제 열기에서는 ViewModel로 동기화한다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Debug")
	void Debug_SetPosts(const TArray<FFF_LiveSocialPost>& InPosts);

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;
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

	/** 원본 보관 한도 이내의 UI 표시 정책. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|SNS", meta = (ClampMin = "1", ClampMax = "80"))
	int32 DisplayLimit = 30;
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|SNS")
	TSubclassOf<UFF_LiveSnsPostEntry> PostEntryClass;

private:
	void BindControls();
	void Unbind();
	void SyncFromViewModel();
	void SetFilter(EFF_LiveSnsFilter Filter);
	void RefreshPending();
	void RefreshFilters();
	void RefreshAll();

	FFF_LiveSnsFeedState Feed;
	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;
};
