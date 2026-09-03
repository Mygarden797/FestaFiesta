// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_LivePanelQueues.generated.h"

class UTextBlock;
class UVerticalBox;

/**
 * 대기열 전체를 한눈에 보는 값 한 벌. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveQueueSummary
{
	GENERATED_BODY()

	/** 지금 줄 서 있는 총 인원. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 TotalWaiting = 0;

	/** 지금 이용 중인 총 인원. */
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

	/** 가장 긴 줄의 인원. 줄이 하나도 없으면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 LongestQueueWaiting = INDEX_NONE;

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

	/** 한 번에 보여줄 최대 줄 수. 넘치면 아래에 몇 개가 더 있는지 적는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Queues", meta = (ClampMin = "1"))
	int32 MaxRows = 8;

private:
	void RefreshAll();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	FFF_LiveQueueSummary Summary;

	float TimeSinceRefresh = 0.f;
};
