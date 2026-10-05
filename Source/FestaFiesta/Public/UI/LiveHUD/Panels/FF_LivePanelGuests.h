// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_LivePanelGuests.generated.h"

class UTextBlock;
class UVerticalBox;
class UFF_LiveNeedRowWidget;

/**
 * 욕구 하나가 얼마나 급한가.
 *
 * !! 색으로만 구분하지 않는다 !! (AT-011) 단계마다 문구와 수치가 함께 붙는다.
 */
UENUM(BlueprintType)
enum class EFF_LiveNeedSeverity : uint8
{
	/** 표본이 없다. **`0%` 가 아니다.** */
	Unknown	UMETA(DisplayName = "모름"),

	Ok		UMETA(DisplayName = "괜찮음"),
	Watch	UMETA(DisplayName = "주의"),
	Bad		UMETA(DisplayName = "나쁨")
};

/** 욕구 한 줄. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveNeedRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	FFF_LiveNeedView View;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveNeedSeverity Severity = EFF_LiveNeedSeverity::Unknown;
};

/**
 * 라이브 `관객` 패널의 판단 규칙. **순수 함수만 둔다.**
 */
struct FESTAFIESTA_API FFF_LiveGuestRules
{
	/**
	 * 미해소율을 급한 정도로 바꾼다.
	 *
	 * 🔴 **표본이 없으면 `모름` 이다.** `UFF_NpcNeedMetricsSubsystem` 은 표본이 0 일 때도
	 * 미해소율을 `0` 으로 돌려준다 (2026-08-28 김시리 주의). 그것을 `괜찮음` 으로 그리면
	 * 화면이 **"아무도 불편하지 않다" 는 틀린 사실**을 말한다.
	 *
	 * @param SampleCount   직전 집계에 들어간 관객 수. 0 이면 모름
	 * @param WatchPercent  이 값 이상이면 `주의`
	 * @param BadPercent    이 값 이상이면 `나쁨`
	 */
	static EFF_LiveNeedSeverity ResolveSeverity(
		float UnmetRatioPercent, int32 SampleCount, float WatchPercent, float BadPercent);

	/** 급한 순으로 세운다. 같으면 `EFF_NeedType` 순서로 고정한다. */
	static void SortBySeverity(TArray<FFF_LiveNeedRow>& Rows);
};

/**
 * 라이브 `관객` 패널.
 *
 * === 무엇을 답하나 ===
 * "지금 관객이 **무엇을 못 하고 있나**" 하나다. 인원 숫자는 하단 바에 이미 있으므로
 * 여기서는 **욕구 미해소율**이 본론이다.
 *
 * === 🔴 숫자 옆에 할 일을 적는다 ===
 * `배고픔 62%` 만 두면 무엇을 눌러야 할지 모른다. 급한 욕구에는 `먹을 곳을 늘리세요`
 * 처럼 대처를 함께 둔다 — 그것이 이 화면의 값어치다.
 *
 * === 없는 것 ===
 * 관객 개인·그룹 구성(가족·친구·혼자)은 표시하지 않는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Guests"))
class FESTAFIESTA_API UFF_LivePanelGuests : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Guests")
	TArray<FFF_LiveNeedRow> GetRows() const { return Rows; }

	/** 행의 크기·배치는 파생 WBP에서 편집한다. 비우면 텍스트 행을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests")
	TSubclassOf<UFF_LiveNeedRowWidget> RowWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveGuests", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text", meta = (ToolTip = "{Count}: 관객 수"))
	FText CountTextFormat = NSLOCTEXT("FestaFiestaLiveGuests", "CountFormat", "{Count}명");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText VisitorNoteText = NSLOCTEXT("FestaFiestaLiveGuests", "CurrentNote", "지금 축제장 안에 있는 관객입니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText AdmittedNoteText = NSLOCTEXT("FestaFiestaLiveGuests", "AdmittedNote", "오늘 입장한 누적 관객입니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText CheckingText = NSLOCTEXT("FestaFiestaLiveGuests", "Checking", "관객 정보를 확인하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText NoSampleText = NSLOCTEXT("FestaFiestaLiveGuests", "NoSampleReady", "아직 관객 표본이 없어 욕구를 집계하고 있습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText EmptyText = NSLOCTEXT("FestaFiestaLiveGuests", "EmptyReady", "욕구 집계가 아직 준비되지 않았습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text", meta = (ToolTip = "{Sample}: 집계 관객 수"))
	FText AllOkTextFormat = NSLOCTEXT("FestaFiestaLiveGuests", "AllOkFormat", "관객 {Sample}명 기준 · 급한 불편은 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text", meta = (ToolTip = "{Sample}: 집계 관객, {Bad}: 나쁨, {Watch}: 주의, {Unknown}: 미확인 항목 수"))
	FText SummaryTextFormat = NSLOCTEXT("FestaFiestaLiveGuests", "SummaryFormat", "관객 {Sample}명 기준 · 나쁨 {Bad} · 주의 {Watch} · 미확인 {Unknown}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText FooterNoteText = NSLOCTEXT("FestaFiestaLiveGuests", "FooterNote", "불편 비율은 집계된 관객을 기준으로 표시합니다. 주의·나쁨 항목의 안내를 확인하세요.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text", meta = (ToolTip = "{Count}: 불편 집계 표본 수"))
	FText SampleCountTextFormat = NSLOCTEXT("FestaFiestaLiveGuests", "SampleCountFormat", "집계 표본 {Count}명");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests|Text")
	FText SampleUnknownText = NSLOCTEXT("FestaFiestaLiveGuests", "SampleUnknown", "집계 준비 중");

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	/** 값이 계속 바뀐다. 델리게이트가 초당 수십 번 뜨므로 주기적으로 다시 읽는다. */
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	// --- 요약 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_VisitorCount;

	/** 동시 인원임을 밝힌다. 일일 목표(누적)와 다른 값이라 오해를 막는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_VisitorNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_AdmittedCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_AdmittedNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_SampleCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Summary;

	// --- 욕구 목록 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UVerticalBox> Box_Needs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Guests")
	TObjectPtr<UTextBlock> Text_MissingNote;

	// --- 설정 ---

	/**
	 * 미해소율 임계(%).
	 *
	 * `assumption` 이다 — 기획에서 확정된 값이 아니라 화면을 보고 정한 시작점이다.
	 * 미해소 판정 기준(`GetUnmetThreshold()`, 기본 0.7)도 소유자 쪽 `draft_assumption` 이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float WatchPercent = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float BadPercent = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Guests", meta = (ClampMin = "0.1"))
	float RefreshIntervalSeconds = 1.f;

private:
	void RefreshAll();
	void RefreshRowWidgets();
	void UpdateFallbackLine(UTextBlock& Line, const FFF_LiveNeedRow& Row) const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	TArray<FFF_LiveNeedRow> Rows;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_LiveNeedRowWidget>> CachedRowWidgets;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CachedFallbackLines;

	float TimeSinceRefresh = 0.f;
};
