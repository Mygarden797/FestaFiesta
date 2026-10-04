// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Time/FF_FestivalStage.h"
#include "UI/LiveHUD/FF_LivePanelBase.h"
#include "FF_LivePanelOperations.generated.h"

class UFF_FestivalViewModelSubsystem;
class UProgressBar;
class UTextBlock;
class UWidget;

/**
 * 일일 목표 진행 상태.
 *
 * !! 진행 막대만 두지 않는다 !! 막대 길이는 "얼마나 남았는지"를 못 말한다.
 * 상태마다 숫자와 문구가 함께 붙는다 (AT-011).
 */
UENUM(BlueprintType)
enum class EFF_LiveGoalState : uint8
{
	/** 목표나 관객 수를 아직 못 읽었다. **0% 가 아니다.** */
	Unknown		UMETA(DisplayName = "모름"),

	/** 진행 중. */
	InProgress	UMETA(DisplayName = "진행 중"),

	/** 목표를 넘겼다. 보너스를 받는다. */
	Reached		UMETA(DisplayName = "달성")
};

/**
 * 일일 목표 진행 한 벌. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_LiveGoalProgress
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	EFF_LiveGoalState State = EFF_LiveGoalState::Unknown;

	/** 0~1. 막대에 그대로 넣는다. 모르면 0 이고 그때는 막대를 숨긴다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	float Ratio = 0.f;

	/** 목표까지 남은 관객 수. 달성했거나 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Live")
	int32 Remaining = INDEX_NONE;

	bool IsKnown() const { return State != EFF_LiveGoalState::Unknown; }
};

/**
 * 라이브 운영 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "목표를 막 넘긴 순간" 이나 "관객 수를 아직 못 읽은 상태" 는 라이브 중 특정 시점에만
 * 나타나서 손으로 밟기 어렵다. 여기 있으면 자동화 테스트가 못으로 박는다.
 */
struct FESTAFIESTA_API FFF_LiveOperationsRules
{
	/**
	 * 일일 목표 진행을 만든다.
	 *
	 * 못 읽은 값을 0으로 바꾸지 않는다. 입장객 미집계와 실제 0명을 구분한다.
	 *
	 * @param VisitorCount 오늘 누적 입장객. `INDEX_NONE`이면 모름 (현재 체류 인원과 다름)
	 * @param DailyGoal    하루 목표. `INDEX_NONE` 이거나 0 이하면 모름
	 */
	static FFF_LiveGoalProgress BuildGoalProgress(int32 VisitorCount, int32 DailyGoal);
};

/**
 * 라이브 `운영` 패널. 탭 7종 중 첫 번째이고 기본으로 보게 되는 화면이다.
 *
 * 구성 (U6 계획) —
 *   관객 만족도 · 안전 · 오늘 매출 · 보유 금화
 *   일일 목표 진행
 *   주의 항목
 *
 * === 하단 바와 나누는 기준 ===
 * 바에는 **한눈에 계속 봐야 하는 것**(축제·돈·인력·관객·시계)을 두고, 여기에는
 * **들여다볼 때 보는 지표**를 둔다. 같은 값을 두 곳에 두지 않는다 —
 * 그래서 만족도·안전이 바에서 이쪽으로 왔다 (2026-08-27).
 *
 * === ★ 만족도는 `SimSatisfaction` 하나만 쓴다 ===
 * 라벨은 `관객 만족도` 다. Live 중에는 FinaleBonus 가 없어 `ScoreSatisfaction` 과 값이
 * 같으므로 같은 숫자를 두 번 띄우면 다른 지표로 읽힌다 (U6 계획).
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live Panel - Operations"))
class FESTAFIESTA_API UFF_LivePanelOperations : public UFF_LivePanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Operations")
	FFF_LiveGoalProgress GetGoalProgress() const { return GoalProgress; }

	/** 동적 문구는 각 운영 WBP Class Defaults에서 편집한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaLiveOperations", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Day}: 현재 일차"))
	FText DayTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "DayFormat", "{Day}일차");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Current}: 오늘 누적 입장객, {Goal}: 하루 목표"))
	FText GoalProgressTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalProgressFormat", "오늘 입장객 {Current} / {Goal}명");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Percent}: 목표 진행률(0~100)"))
	FText GoalPercentTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalPercentFormat", "{Percent}%");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Remaining}: 남은 입장객, {Bonus}: 목표 보너스"))
	FText GoalRemainingTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalRemainingFormat", "{Remaining}명 더 오면 보너스 +{Bonus}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Remaining}: 남은 입장객"))
	FText GoalRemainingNoBonusTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalRemainingNoBonusFormat", "목표까지 {Remaining}명 남음");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Bonus}: 목표 보너스"))
	FText GoalReachedTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalReachedFormat", "목표 달성 · 보너스 +{Bonus}");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text", meta = (ToolTip = "{Bonus}: 지급된 목표 보너스"))
	FText GoalBonusPaidTextFormat = NSLOCTEXT("FestaFiestaLiveOperations", "GoalBonusPaidFormat", "목표 달성 · 보너스 +{Bonus} 지급 완료");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text")
	FText GoalReachedNoBonusText = NSLOCTEXT("FestaFiestaLiveOperations", "GoalReachedNoBonus", "목표 달성");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text")
	FText GoalUnknownText = NSLOCTEXT("FestaFiestaLiveOperations", "GoalUnknown", "집계 준비 중");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text")
	FText SatisfactionNoteText = NSLOCTEXT("FestaFiestaLiveOperations", "SatisfactionNote", "실시간 관객 만족도");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Operations|Text")
	FText FooterNoteText = NSLOCTEXT("FestaFiestaLiveOperations", "FooterNote", "관객 현황과 대기열은 각 탭에서 확인할 수 있습니다.");

protected:
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleSimSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleAdmittedVisitorsChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

protected:
	// --- 지표 4종 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_Satisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_SatisfactionNote;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_Safety;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_Gold;

	// --- 일일 목표 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_Day;

	/** 오늘 누적 입장객과 목표. 공급이 없으면 `—`다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_GoalProgress;

	/** `180명 더 오면 +300` 또는 `달성 · +300`. **막대만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_GoalDetail;

	/** 숫자 진행률. 실제 0%와 미집계 `—`를 구분한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_GoalPercent;

	/** 모르면 숨긴다. **0% 로 그리지 않는다** — 안 온 것과 못 읽은 것은 다르다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UProgressBar> Bar_GoalProgress;

	// --- 주의 항목 ---

	/** 지금 손볼 것. 없으면 줄째 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_Warning;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UWidget> Box_Warning;

	/** 하단 안내. 기존 WBP 바인딩 이름을 유지한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Operations")
	TObjectPtr<UTextBlock> Text_DummyNotice;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	void RefreshAll();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	FFF_LiveGoalProgress GoalProgress;
};
