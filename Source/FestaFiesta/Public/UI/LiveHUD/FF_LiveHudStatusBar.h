// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "FF_LiveHudStatusBar.generated.h"

class UButton;
class UProgressBar;
class UTextBlock;
class UFF_FestivalViewModelSubsystem;

/**
 * Live HUD 최하단 상태 바.
 *
 * 목업 `docs/ui/mockups/live-hud-menu-states-v2-58px`의 아래 행이다.
 * `FF | G 10,480 | 284 | 4/6 | 1일차·LIVE | 19:42 | 20:00 메인 공연 | ⏸ ▶ ⏩ ⏭`
 *
 * === CommonUI를 쓰지 않는다 ===
 * 실시간 HUD라 입력 모드를 가져가면 안 된다. 일반 `UUserWidget`이다.
 * 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 아직 소유자가 없는 값 3개 ===
 * 관객 수 · 스태프 가용 인원 · 다음 프로그램 이름/시각은 읽을 창구가 없다 (2026-08-12).
 * **비어 있으면 그 칸을 숨긴다.** 0이나 `-`를 넣으면 "관객이 0명"이라는 틀린 사실이 된다.
 * 요청은 `UI_API.md`에 적어 뒀다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live HUD Status Bar"))
class FESTAFIESTA_API UFF_LiveHudStatusBar : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 모든 표시를 현재값으로 다시 그린다. 부분 갱신하지 않아 이벤트 순서에 의존하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI")
	void RefreshAll();

	/**
	 * 아직 소유자가 없는 값을 외부에서 넣는 임시 창구.
	 * 소유자 API가 생기면 이 함수를 지우고 ViewModel 구독으로 바꾼다 — 위젯 트리는 그대로 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Debug")
	void Debug_SetPlaceholders(int32 InVisitorCount, int32 InStaffAvailable, int32 InStaffTotal, const FText& InNextProgram);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 델리게이트 핸들러 ---

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

	// --- 재생 컨트롤 ---

	UFUNCTION()
	void HandlePauseClicked();

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleFastForwardClicked();

	UFUNCTION()
	void HandleSkipClicked();

protected:
	// --- 위젯 바인딩. 전부 Optional이다 ---
	// 소유자 값이 없는 칸을 나중에 붙일 수 있게 하고, 목업의 일부만 만든 WBP도 통과시킨다.

	/** 잔액. 매출과 다른 값이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Gold;

	/** 관객 수. **소유자 없음** — 값이 없으면 이 칸을 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_VisitorCount;

	/** 스태프 `가용/전체`. **가용 인원 소유자 없음** — 없으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Staff;

	/** `1일차 · LIVE`. Day와 Stage 표시명을 합친 한 줄이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_DayStage;

	/** 게임 속 시각 `19:42`. Live가 아니면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_Clock;

	/** `20:00 메인 공연`. **소유자 없음** — 없으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_NextProgram;

	// --- 값이 없을 때 함께 접을 묶음 (아이콘·구분선 포함) ---
	//
	// 숫자만 숨기면 옆의 아이콘과 구분선이 남아 "관객 아이콘 옆에 아무것도 없는" 화면이 된다.
	// 아이콘 + 숫자를 HorizontalBox로 싸고 그 박스를 아래 이름으로 두면 묶음째 접힌다.
	// 없으면 예전처럼 TextBlock만 접으므로 WBP를 안 고쳐도 동작은 한다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> Box_VisitorCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> Box_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UWidget> Box_NextProgram;

	/** 남은 운영 시간 `01:18`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_RemainingTime;

	/** 오늘 진행률. 남은 시간에서 계산한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UProgressBar> Progress_Program;

	// 재생 컨트롤. 배속 잠금 구간에서는 비활성되고 이유 문구가 함께 뜬다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Pause;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Play;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_FastForward;

	/** 스킵. 스킵 가능한 단계에서만 활성된다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UButton> Button_Skip;

	/** 배속을 못 바꾸는 이유. 흐려짐만으로는 알 수 없다 (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI")
	TObjectPtr<UTextBlock> Text_TimeScaleHint;

	// --- 배속 값 ---

	/** `▶`가 요청하는 배속. 기획 후보는 1/2/4다. 하드코딩하지 않게 노출한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Time")
	float NormalTimeScale = 1.f;

	/** `⏩`가 요청하는 배속. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Time")
	float FastTimeScale = 4.f;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	void RefreshClock();
	void RefreshPlaybackControls();
	void RefreshPlaceholders();

	/** 초를 `m:ss`로 만든다. 계산은 C++에서 끝내고 위젯은 표시만 한다. */
	static FText FormatSeconds(int32 TotalSeconds);

	/** 값이 없는 칸을 숨긴다. 빈 문자열이나 0을 넣지 않는다. */
	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	/**
	 * 값이 없으면 묶음 전체를 접는다. 묶음이 없으면 TextBlock만 접는다.
	 * 아이콘·구분선이 숫자 없이 남는 것을 막는다.
	 */
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	// --- 소유자 없는 값의 임시 보관 ---
	// INDEX_NONE이면 "모름"이고 해당 칸을 숨긴다.

	int32 VisitorCount = INDEX_NONE;
	int32 StaffAvailable = INDEX_NONE;
	int32 StaffTotal = INDEX_NONE;
	FText NextProgramText;

	/** 오늘 운영 총 길이(초). 진행률 계산의 분모다. 첫 갱신에서 관측한 최대값을 쓴다. */
	int32 ObservedMaxRemainingSeconds = 0;
};
