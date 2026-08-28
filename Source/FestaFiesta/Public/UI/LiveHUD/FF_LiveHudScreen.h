// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "FF_LiveHudScreen.generated.h"

class UBorder;
class UButton;
class UFF_FestivalViewModelSubsystem;
class UFF_LivePanelBase;
class UFF_LivePlaceholderPanel;
class UFF_LiveTabButton;
class UPanelWidget;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnLiveTabSelectionChanged, FFF_LiveTabSelection, Selection);

/**
 * 라이브 운영 HUD 의 루트. **`StageScreens` 의 `Live` 에 이것을 배선한다.**
 *
 * 구성 —
 *   상태 행 : 관객 · 관객 만족도 · 안전 · 오늘 매출 · 보유 금화 · 시계 · 남은 시간
 *   메뉴 행 : 탭 7종 (운영 · 시설 · 관객 · 대기열 · SNS · 리포트 · 스태프)
 *   패널 호스트 : 왼쪽. 한 번에 하나만 열린다
 *   배속 : x1 · x2 · x4
 *
 * === 준비 단계 바(`UFF_PrepHudBar`)와 같은 골격이다 ===
 * 탭 배타·패널 호스트·상태 행 갱신이 같은 모양이라 읽는 사람이 두 번 배우지 않아도 된다.
 * 판정은 전부 `FFF_LiveHudRules` 에 있고 여기는 그 결과를 그린다.
 *
 * 다른 점 셋 —
 *   1. 패널이 **전부 왼쪽**이다. 방향 값이 없고 폭만 정한다
 *   2. CTA 가 없다. 라이브는 시간이 끝나면 저절로 넘어간다
 *   3. 배속 줄이 있다. 준비 단계에는 없다
 *
 * === 🔴 시간을 멈추지 않는다 ===
 * 패널이 열려 있어도 Live 시계는 계속 흐른다. 입력 모드를 가져가지 않으려고 CommonUI 를
 * 쓰지 않고 일반 `UUserWidget` 이다 (AGENTS.md 규칙 1 · ui-tech-decision 7절).
 *
 * === 기존 `UFF_LiveHudMenuBar` 3종과 별개다 ===
 * 그쪽은 2026-08-12 목업 기준 9탭이고 손으로 만든 WBP 가 붙어 있다. 건드리지 않고 남겨 뒀다
 * (2026-08-27 백종태 지시). 이쪽이 자리를 잡으면 그때 정리한다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Live HUD Screen"))
class FESTAFIESTA_API UFF_LiveHudScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 탭을 눌렀을 때와 같은 경로. 콘솔·디버그에서도 쓴다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live")
	void ToggleTab(EFF_LiveTab Tab);

	/** 열려 있는 패널을 닫는다. **패널이 스스로 닫지 않는다** — 여기가 유일한 경로다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live")
	void CloseCurrentPanel();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live")
	FFF_LiveTabSelection GetSelection() const { return Selection; }

	/** 모든 표시를 현재값으로 다시 그린다. 부분 갱신하지 않아 이벤트 순서에 의존하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live")
	void RefreshAll();

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Live")
	FFF_OnLiveTabSelectionChanged OnTabSelectionChanged;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 결과 이벤트 ---

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleSimSatisfactionChanged(int32 NewValue);

	UFUNCTION()
	void HandleSafetyChanged(int32 NewValue);

	/** 탭 버튼 클릭. 베이스가 `int32` 로 주므로 여기서 되돌려 해석한다. */
	void HandleTabButtonClicked(int32 TabId);

	/** 배속 버튼. `1`·`2`·`4` 를 그대로 넘긴다. */
	UFUNCTION()
	void HandleSpeed1Clicked();

	UFUNCTION()
	void HandleSpeed2Clicked();

	UFUNCTION()
	void HandleSpeed4Clicked();

protected:
	// =========================================================================
	// 위젯 바인딩 — 전부 Optional 이다.
	// 목업의 일부만 만든 WBP 도 통과시키고, 소유자 값이 없는 칸을 나중에 붙일 수 있게 한다.
	// 빠진 것은 조용히 넘기지 않고 로그로 알린다.
	// =========================================================================

	// --- 메뉴 행 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Operations;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Facilities;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Guests;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Queues;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_SNS;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Report;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Staff;

	// --- 배경 (팔레트 색을 입힐 자리) ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UBorder> Border_MenuRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UBorder> Border_StatusRow;

	/**
	 * 패널을 넣는 컨테이너. `CanvasPanel` 의 자식이면 코드가 왼쪽 앵커를 맞춘다.
	 * 다른 컨테이너면 위치는 디자이너가 소유하고 코드는 손대지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UPanelWidget> Panel_PanelHost;

	// --- 상태 행 ---
	//
	// 값이 없으면 `Box_*` 묶음째 접는다. 숫자만 숨기면 옆의 아이콘과 구분선이 남아
	// "관객 아이콘 옆에 아무것도 없는" 화면이 된다.

	/** 현재 관객 수. **소유자 없음** — 값이 오기 전에는 칸을 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_VisitorCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_VisitorCount;

	/**
	 * ★ 라벨은 `관객 만족도` 다. **`SimSatisfaction` 하나만 쓴다.**
	 * Live 중에는 FinaleBonus 가 없어 `ScoreSatisfaction` 과 값이 같다 — 같은 숫자를
	 * 두 번 띄우면 다른 지표로 읽힌다 (U6 계획).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Satisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Satisfaction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Safety;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Safety;

	/** 오늘 매출. **보유 금화와 다른 값이다** — 잔액은 지출로 줄어든다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Gold;

	/**
	 * 게임 속 시각 (`19:42`)과 남은 시간을 합친 한 줄. 화면 정중앙에 놓는다.
	 *
	 * 준비 단계 바의 `1일차 · 셋업` 과 같은 자리다 — 앞의 칸이 접힐 때마다 위치가
	 * 흔들리지 않게 가운데에 고정한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Clock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UBorder> Border_Clock;

	// --- 배속 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Speed")
	TObjectPtr<UButton> Button_Speed1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Speed")
	TObjectPtr<UButton> Button_Speed2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Speed")
	TObjectPtr<UButton> Button_Speed4;

	/** 왜 못 바꾸는지. **비활성 흐려짐만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Speed")
	TObjectPtr<UTextBlock> Text_SpeedReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Speed")
	TObjectPtr<UWidget> Box_Speed;

	// --- 설정 ---

	/** 탭 하나 = 한 줄. 없는 탭은 `DefaultPanelLayout` 으로 연다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	TMap<EFF_LiveTab, FFF_LivePanelLayout> PanelLayouts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	FFF_LivePanelLayout DefaultPanelLayout;

	/** 하단 바 전체 높이(px). 패널이 그 위에서 끝나게 하는 데 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	float BarHeight = 58.f;

	/** 화면 가장자리와 패널 사이 여백(px). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	float PanelMargin = 12.f;

	/**
	 * 이름 규칙에서 벗어나는 패널만 여기 넣는다.
	 * 보통은 비워 두고 `PanelClassNameFormat` 으로 찾게 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Panels")
	TMap<EFF_LiveTab, TSoftClassPtr<UFF_LivePanelBase>> PanelClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Panels")
	FString PanelClassFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels");

	/** `{0}` 에 `EFF_LiveTab` 의 항목 이름이 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Panels")
	FString PanelClassNameFormat = TEXT("WBP_FF_LivePanel{0}");

	/** 아직 안 만든 탭에 띄울 패널. 비면 그 탭은 아무것도 안 뜬다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Panels")
	TSoftClassPtr<UFF_LivePlaceholderPanel> PlaceholderPanelClass;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	/** 탭 버튼을 모아 한 번에 돈다. 매번 7개를 손으로 나열하지 않기 위해서다. */
	void ForEachTabButton(TFunctionRef<void(EFF_LiveTab, UFF_LiveTabButton&)> Visitor) const;

	void ApplySelection(const FFF_LiveTabSelection& NewSelection);

	/** 없으면 만들어 호스트에 넣는다. 만든 것은 캐시한다. */
	UFF_LivePanelBase* EnsurePanel(EFF_LiveTab Tab);

	UClass* ResolvePanelClass(EFF_LiveTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const;

	FFF_LivePanelLayout ResolveLayout(EFF_LiveTab Tab) const;
	void ApplyPanelLayout(const FFF_LivePanelLayout& Layout);

	void RefreshTabs();
	void RefreshStatusRow();
	void RefreshSpeed();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY()
	TMap<EFF_LiveTab, TObjectPtr<UFF_LivePanelBase>> PanelWidgets;

	FFF_LiveTabSelection Selection;

	/** 소유자 API 가 없는 값. `Debug_SetPlaceholders` 로만 들어온다. */
	int32 VisitorCount = INDEX_NONE;

public:
	/**
	 * 소유자 API 가 없는 칸을 손으로 채운다. **API 가 생기면 이 함수를 지운다.**
	 * 넣지 않으면 그 칸은 숨겨진 채로 남는다 — 0 을 띄우면 틀린 사실이 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Debug")
	void Debug_SetPlaceholders(int32 InVisitorCount);
};
