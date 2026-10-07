// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_LivePanelQueues.generated.h"

class UTextBlock;
class UVerticalBox;
class UFF_LiveQueueRowWidget;

/**
 * 대기열 전체를 한눈에 보는 값 한 벌. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveQueueSummary
{
	GENERATED_BODY()

	/** 지금 줄 서 있는 총 팀 수. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 TotalWaiting = 0;

	/** 지금 이용 중인 총 팀 수. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 TotalInService = 0;

	/**
	 * 평균 대기 시간(초).
	 *
	 * 🔴 **아직 아무도 못 끝낸 시설은 평균에서 뺀다.** 그 값을 0 으로 세면 평균이
	 * 실제보다 낮아져서, 줄이 긴데도 "평균 20초" 같은 안심되는 숫자가 뜬다.
	 * 셀 것이 하나도 없으면 `INDEX_NONE` 이다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 AverageWaitSeconds = INDEX_NONE;

	/** 평균에 실제로 들어간 시설 수. `0` 이면 아직 아무도 줄을 끝내지 못한 것이다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 AverageSampleCount = 0;

	/** 가장 긴 줄의 시설 이름. 줄이 하나도 없으면 비어 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText LongestQueueName;

	/** 가장 긴 줄의 팀 수. 줄이 하나도 없으면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 LongestQueueWaiting = INDEX_NONE;

	// --- 지금 서면 가장 오래 기다리는 곳 ---
	//
	// 🔴 **`가장 긴 줄` 과 다른 값이다.** 사람이 많아도 처리가 빠르면 금방 빠지고,
	// 줄이 짧아도 처리가 느리면 오래 걸린다. 고칠 곳을 고르는 근거는 이쪽이다.
	// (`UI_API.md` 요청 4번 — 큐↔시설 다리가 2026-09-03 에 확인돼 붙였다)

	/** 예상 대기가 가장 긴 시설 이름. 읽은 곳이 하나도 없으면 비어 있다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FText WorstExpectedWaitName;

	/** 그 시설의 예상 대기(초). 아무 곳도 못 읽었으면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 WorstExpectedWaitSeconds = INDEX_NONE;

	/** 예상 대기를 실제로 읽은 시설 수. `0` 이면 다리가 없거나 큐에 등록된 시설이 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 ExpectedWaitSampleCount = 0;

	// --- 큐 상태별 개수 ---

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 OpenCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 FullCount = 0;

	/** 닫힘·비우는 중. 지금 새 손님을 안 받는다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 NotAcceptingCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 BlockedCount = 0;

	/** 상태를 못 읽은 시설 수. **`닫힘` 에 섞지 않는다.** */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 UnknownCount = 0;

	/** 메인 공연 직전·공연 중이라 잠시 안 받는 시설 수. `정원 참` 과 다르다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 PausedCount = 0;

	/** 줄 대신 관람석으로 받는 시설 수. 위의 대기열 집계에는 넣지 않는다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 SeatedVenueCount = 0;

	/** 집계에 들어간 줄 서는 시설 수. 관람석만 있으면 0이다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 QueueFacilityCount = 0;

	bool HasQueueFacility() const { return QueueFacilityCount > 0; }

	bool HasAnyQueue() const { return LongestQueueWaiting != INDEX_NONE; }
};

/**
 * 라이브 `대기열` 패널의 판단 규칙. **순수 함수만 둔다.**
 */
struct FESTAFIESTA_API FFF_LiveQueueRules
{
	/**
	 * 시설 목록에서 대기열 요약을 만든다.
	 *
	 * ★ 시설 패널과 **같은 원본**을 쓰지만 보는 각도가 다르다. 시설 패널은
	 * "어디가 고장났나" 를, 여기는 "줄이 어떻게 흐르나" 를 답한다.
	 */
	static FFF_LiveQueueSummary BuildSummary(const TArray<FFF_LiveFacilityView>& Facilities);

	/** 줄이 긴 순으로 세운다. 같으면 이름으로 고정해 갱신할 때 순서가 흔들리지 않게 한다. */
	static void SortByWaiting(TArray<FFF_LiveFacilityView>& Facilities);

	/**
	 * 대기열 화면에 올릴 시설인가. 줄을 받는 시설과 관람석만 올린다.
	 * 길·타일·조명은 대기열이 없어 늘 `미접수` 로 떠서 목록과 집계를 가린다.
	 */
	static bool IsShownInQueuePanel(const FFF_LiveFacilityView& Facility)
	{
		return Facility.bUsesQueue || Facility.bSeatedVenue;
	}
};

/**
 * 라이브 `대기열` 패널.
 *
 * === 시설 패널과 무엇이 다른가 ===
 * 원본은 같지만 답하는 질문이 다르다.
 *   시설  : 어디가 **고장났나** (재배치·폐쇄·막힘)
 *   대기열: 줄이 **어떻게 흐르나** (총 대기·평균·가장 긴 줄)
 * 그래서 정렬 기준도 다르다 — 여기서는 급한 순이 아니라 **줄이 긴 순**이다.
 *
 * === 🔴 읽기 전용이다 ===
 * 줄을 열고 닫는 것은 존·큐 시스템이 소유한다. UI 는 상태만 알린다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Queues"))
class FESTAFIESTA_API UFF_LivePanelQueues : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Queues")
	FFF_LiveQueueSummary GetSummary() const { return Summary; }

	/** 행 크기·열 폭·글꼴을 편집할 WBP. 비우면 기존 텍스트 행을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues")
	TSubclassOf<UFF_LiveQueueRowWidget> RowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveQueues", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Count}: 팀 수"))
	FText TeamCountTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "TeamCountFormat", "{Count}팀");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Seconds}: 대기 시간"))
	FText WaitSecondsTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "WaitSecondsFormat", "{Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Count}: 평균 대기 기록이 있는 시설 수. 고객 수 가중 평균이 아니다."))
	FText AverageNoteTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "AverageNoteFormat", "완료 기록이 있는 {Count}곳의 시설별 평균을 평균한 값입니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText AverageNoSampleText = NSLOCTEXT("FestaFiestaLiveQueues", "AverageNoSample", "완료 기록이 없어 평균을 집계하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Name}: 시설 이름, {Count}: 대기 팀 수"))
	FText LongestTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "LongestFormat", "가장 긴 줄 · {Name} {Count}팀");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText NoQueueText = NSLOCTEXT("FestaFiestaLiveQueues", "NoQueue", "현재 대기 중인 팀이 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Name}: 시설 이름, {Seconds}: 지금 줄을 서면 예상되는 대기 시간"))
	FText WorstWaitTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "WorstWaitFormat", "지금 서면 가장 오래 · {Name} {Seconds}초");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText ExpectedWaitUnknownText = NSLOCTEXT("FestaFiestaLiveQueues", "ExpectedWaitUnknown", "지금 서면 예상 대기 · —");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Open}, {Full}, {Paused}, {NotAccepting}, {Blocked}, {Unknown}: 상태별 시설 수. 관람석은 세지 않는다."))
	FText StateCountsTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "StateCountsFormatV2", "접수 중 {Open} · 정원 참 {Full} · 공연 중 중단 {Paused} · 미접수/마감 {NotAccepting} · 막힘 {Blocked} · 미확인 {Unknown}");

	/** 관람석(메인 무대)만 있고 줄을 서는 시설이 없을 때. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText NoQueueFacilityText = NSLOCTEXT("FestaFiestaLiveQueues", "NoQueueFacility", "줄을 서는 시설이 아직 없습니다. 부스나 체험 시설을 지으면 대기 흐름이 보입니다.");

	/** 행 WBP가 없을 때 관람석 한 줄. {Name}: 시설 이름 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText SeatedFallbackLineFormat = NSLOCTEXT("FestaFiestaLiveQueues", "SeatedFallbackLine", "{Name}  —  관람석 · 공연 시간에 좌석으로 입장합니다");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText CheckingText = NSLOCTEXT("FestaFiestaLiveQueues", "Checking", "대기열 정보를 확인하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText EmptyText = NSLOCTEXT("FestaFiestaLiveQueues", "Empty", "대기열을 확인할 시설이 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text")
	FText FooterNoteText = NSLOCTEXT("FestaFiestaLiveQueues", "FooterNote", "대기·이용 수는 팀 기준입니다. 줄을 포기한 팀 수는 아직 읽을 수 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues|Text", meta = (ToolTip = "{Count}: 표시되지 않은 시설 수"))
	FText MoreRowsTextFormat = NSLOCTEXT("FestaFiestaLiveQueues", "MoreRowsFormat", "…외 {Count}개는 목록에 없습니다");

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	/** 라이브 중에는 값이 계속 바뀐다. 이벤트가 없어 주기적으로 다시 읽는다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// --- 요약 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_TotalWaiting;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_InService;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_AverageWait;

	/** 평균이 몇 곳을 센 것인지. 한 곳만 센 평균과 여섯 곳을 센 평균은 뜻이 다르다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_AverageNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_Longest;

	/** 지금 서면 가장 오래 걸리는 곳. 못 읽었으면 —로 구분한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_WorstWait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_StateCounts;

	// --- 목록 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UVerticalBox> Box_Queues;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Queues")
	TObjectPtr<UTextBlock> Text_MissingNote;

	// --- 설정 ---

	/** 목록을 다시 읽는 간격(초). 시설 패널과 같은 이유로 매 틱 읽지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 1.f;

	/** 한 번에 보여줄 최대 줄 수. 0이면 전체 표시. 넘치면 아래에 몇 개가 더 있는지 적는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues", meta = (ClampMin = "0"))
	int32 MaxRows = 8;

private:
	void RefreshAll();
	void RefreshRowWidgets();
	void UpdateFallbackLine(UTextBlock& Line, const FFF_LiveFacilityView& Row) const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	FFF_LiveQueueSummary Summary;
	TArray<FFF_LiveFacilityView> Rows;

	/** 갱신 때 순서별 행을 재사용해 스크롤 위치와 위젯 트리를 유지한다. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_LiveQueueRowWidget>> CachedRowWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CachedFallbackLines;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MoreRowsText;

	float TimeSinceRefresh = 0.f;
};
