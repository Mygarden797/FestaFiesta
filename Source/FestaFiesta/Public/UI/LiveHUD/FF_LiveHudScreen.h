// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStageSubsystem.h"
#include "UI/LiveHUD/FF_LiveHudTypes.h"
#include "FF_LiveHudScreen.generated.h"

class UBorder;
class UImage;
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

	/**
	 * 메인 공연 잠금이 걸리거나 풀렸다. **단계 안에서** 바뀌므로 따로 받아야 한다.
	 * 이걸 안 받으면 라이브 도중 시간만 1배속이 되고 버튼은 4배속이 눌린 채로 남는다.
	 */
	UFUNCTION()
	void HandleTimeScaleLockChanged(bool bNewLocked);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleTodayRevenueChanged(int32 NewValue);

	UFUNCTION()
	void HandleArrearsChanged(int32 NewValue);

	/** 피날레 등급이 바뀌면 탭이 생기거나 사라진다. 값만 바뀌는 것이 아니다. */
	UFUNCTION()
	void HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade);

	UFUNCTION()
	void HandleCtaClicked();

	/** 전체 인원과 가용 인원 **양쪽**이 이 핸들러를 부른다. 인자를 믿지 않는다. */
	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Density;

	/** 결산 탭. 마감·리포트에서만 보인다 (2026-09-02 통합). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Finale;

	// --- 배경 (팔레트 색을 입힐 자리) ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live")
	TObjectPtr<UBorder> Border_MenuRow;

	/**
	 * 바 전체 높이를 쥔 상자.
	 *
	 * 🔴 **런타임에 높이를 바꿔야 한다.** 탭이 하나도 없는 단계(피날레 연출)에서는
	 * `Border_MenuRow` 를 접는데, 두 행이 모두 `Fill` 이라 남은 58 을 **상태 행이 전부
	 * 가져간다.** 그러면 배경만 두 배로 커지고 안의 글자는 `VAlign_Center` 라 그대로여서
	 * 위아래로 빈 띠가 생긴다 (2026-09-03 실측: 마감 `상태행 2020x29` → 피날레 `2020x58`).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<class USizeBox> SizeBox_Bar;

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

	// === 왼쪽 묶음 — 준비 단계 바와 같은 순서다 (2026-08-27 백종태 요청) ===
	// 축제 → 금화 → 미납 → 오늘 매출 → 인력 → 관객.
	// 두 화면이 같은 자리에 같은 값을 두면 단계가 바뀌어도 눈이 다시 찾지 않는다.
	//
	// ★ 관객 만족도·안전은 여기 없다. **운영 패널로 옮겼다** (U6 계획).
	//   바에 지표까지 늘어놓으면 왼쪽이 화면 절반을 넘어 가운데 칩과 겹친다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_FestivalName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_FestivalName;

	/** 지금 쓸 수 있는 잔액. 지출로 줄어든다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Gold;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Gold;

	/**
	 * ★ 밀린 임대료. **잔액 바로 옆에 둔다.**
	 * 미납이 있으면 매출이 들어와도 잔액이 안 늘어나므로 이 칸이 없으면 설명이 안 된다.
	 * 0 이면 칸을 숨긴다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Arrears;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Arrears;

	/** 오늘 매출. **보유 금화와 다른 값이다** — 잔액은 지출로 줄어든다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_TodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_TodayRevenue;

	/**
	 * `6명 중 4명 사용 가능`. **전체만 띄우지 않는다** — 프로그램에 묶인 인원은
	 * Event 대응에 못 쓴다. 라이브에서는 그 구분이 준비 단계보다 더 중요하다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Staff;

	/** 현재 관객 수. **소유자 없음** — 값이 오기 전에는 칸을 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_VisitorCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_VisitorCount;

	// === 가운데 칩 — `● 1일차 · 라이브` ===

	/**
	 * 준비 단계 바의 `1일차 · 셋업` 과 같은 자리다. 앞의 칸이 접힐 때마다 위치가
	 * 흔들리지 않게 화면 정중앙에 고정한다.
	 *
	 * 🔴 **단계 이름을 enum 에서 읽지 않는다.** `GetCurrentStageDisplayText()` 는
	 * `UMETA(DisplayName)` 을 런타임에 조회해서 패키징 빌드에 영문이 뜬다
	 * (2026-08-19 김시리 정정). 이 HUD 는 라이브에서만 뜨므로 `라이브` 를 그냥 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_DayStage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UBorder> Border_DayStage;

	/**
	 * 방송 중을 뜻하는 빨간 점.
	 *
	 * !! 이것만으로 상태를 말하지 않는다 !! 바로 옆에 `라이브` 라는 글자가 함께 있다 —
	 * 점은 그 글자를 거드는 장식이다 (AT-011).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UImage> Image_LiveDot;

	// === 오른쪽 — 시계 · 배속 ===

	/** 게임 속 시각과 남은 시간 (`19:42 · 남은 128초`). **배속 버튼 왼쪽**에 둔다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UTextBlock> Text_Clock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Status")
	TObjectPtr<UWidget> Box_Clock;

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

	// === CTA — 피날레 연출 구간에서만 띄운다 ===

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Cta")
	TObjectPtr<UButton> Button_Cta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Live|Cta")
	TObjectPtr<UTextBlock> Text_CtaLabel;

	// --- 설정 ---

	/** 탭 하나 = 한 줄. 없는 탭은 `DefaultPanelLayout` 으로 연다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	TMap<EFF_LiveTab, FFF_LivePanelLayout> PanelLayouts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	FFF_LivePanelLayout DefaultPanelLayout;

	/** 패널의 빈 곳을 끌어 자리를 옮길 수 있게 할지. 준비 단계와 같은 조작이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	bool bAllowPanelDrag = true;

	/** 하단 바 전체 높이(px). 메뉴 행 + 상태 행이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	float BarHeight = 58.f;

	/**
	 * 하단 바 중 **상태 행**의 높이(px). 패널의 아래 끝이 여기에 맞춰진다.
	 *
	 * === 🔴 왜 `BarHeight` 가 아니라 이 값인가 (2026-08-28) ===
	 * 처음에는 바 전체를 비웠더니 패널과 탭 버튼 사이에 틈이 생겼다. 준비 단계는
	 * **패널 아래 끝이 탭 버튼 줄 밑으로 들어가서** 버튼이 패널 위에 얹힌 것처럼 보인다 —
	 * 그래야 "이 버튼이 이 패널을 연다" 가 눈에 붙는다.
	 *
	 * 바가 패널보다 뒤에 추가되어 있어 탭이 위에 그려진다. 탭이 가리는 자리는 패널의
	 * 빈 아래쪽이라 내용이 잘리지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Layout")
	float StatusRowHeight = 29.f;

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

	/**
	 * 아직 안 만든 탭에 띄울 패널.
	 *
	 * 🔴 **기본값을 여기서 준다.** 처음에는 비워 뒀는데 그러면 아직 안 만든 탭을 눌렀을 때
	 * `EnsurePanel` 이 `nullptr` 을 돌려주고 **아무 일도 안 일어난다** — 버튼이 고장난 것처럼
	 * 보인다. 준비 단계 바는 같은 자리에 경로 상수를 두고 있어서 이 함정을 안 밟았다.
	 *
	 * `TSoftClassPtr` 이라 쿠커의 참조 그래프에도 잡힌다 (문자열 경로와 다른 점).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Live|Panels")
	TSoftClassPtr<UFF_LivePlaceholderPanel> PlaceholderPanelClass = TSoftClassPtr<UFF_LivePlaceholderPanel>(
		FSoftObjectPath(TEXT("/Game/FestaFiesta/UI/LiveHUD/Panels/WBP_FF_LivePlaceholderPanel.WBP_FF_LivePlaceholderPanel_C")));

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	/** 탭 버튼을 모아 한 번에 돈다. 매번 7개를 손으로 나열하지 않기 위해서다. */
	void ForEachTabButton(TFunctionRef<void(EFF_LiveTab, UFF_LiveTabButton&)> Visitor) const;

	void ApplySelection(const FFF_LiveTabSelection& NewSelection);

public:
	/**
	 * 열려 있는 패널을 슬레이트 단위만큼 옮긴다. 패널이 자기 빈 곳을 끌 때 부른다.
	 *
	 * 준비 단계와 같다 — 자리는 **화면이 소유**하고 패널은 요청만 한다. 패널이 자기 슬롯을
	 * 직접 만지면 탭을 닫았다 열 때 값이 되돌아가서 옮긴 자리가 사라진다.
	 */
	void NudgePanelPosition(FVector2D SlateDelta);

	// === 하단 바 CTA (2026-09-02 통합) ===
	//
	// 판정은 화면이 소유하고 **버튼은 리포트 패널이 그린다.** 바에도 두면 화면에
	// `다음 날로` 가 둘이 된다 — 준비 단계에서 `개장하기` 가 둘이었던 것과 같은 문제다.
	// 다만 피날레는 패널이 없는 단계라 그때만 바에 띄운다.

	/**
	 * 지금 넘길 수 있는 상태인지. **부를 때마다 다시 판정한다.**
	 *
	 * 🔴 캐시해 두면 안 된다. 리포트 패널이 이 값을 읽어 버튼을 그리는데, 단계가 바뀔 때
	 * 화면과 패널 중 **누가 먼저 신호를 받는지 정해져 있지 않다.** 패널이 먼저 깨어나면
	 * 한 단계 전의 값을 읽어 `다음 날로` 를 눌렀는데 아무 일도 안 일어나게 된다.
	 * 판정 자체는 bool 세 개짜리 순수 함수라 매번 불러도 싸다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Cta")
	EFF_LiveCtaState GetCtaState() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Cta")
	FText GetCtaLabelText() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Cta")
	FText GetCtaReasonText() const;

	/** 패널의 버튼이 눌렸을 때. 판정은 소유자가 한다 — 여기서 미리 막지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Live|Cta")
	void RequestAdvance();

	/** 피날레를 샀는가. 결산 탭을 숨길지 정한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Live|Cta")
	bool HasPurchasedFinale() const;

	/** 지금 끌어 옮길 수 있는 상태인지. `Designer` 도킹이면 코드가 자리를 안 잡으므로 막는다. */
	bool IsPanelDragAllowed() const;

private:

	/** 없으면 만들어 호스트에 넣는다. 만든 것은 캐시한다. */
	UFF_LivePanelBase* EnsurePanel(EFF_LiveTab Tab);

	UClass* ResolvePanelClass(EFF_LiveTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const;

	FFF_LivePanelLayout ResolveLayout(EFF_LiveTab Tab) const;
	void ApplyPanelLayout(const FFF_LivePanelLayout& Layout);

	/**
	 * 다음 틱에 패널·호스트의 **실제 크기**를 로그로 찍는다.
	 *
	 * 안 보이는 원인이 색인지 크기인지 부모인지를 눈으로는 못 가른다. 숫자로 가른다.
	 * 원인을 잡고 나면 지운다 (2026-08-28 임시).
	 */
	void LogPanelGeometryNextTick(UFF_LivePanelBase& Panel);

	/**
	 * 단계가 바뀔 때 바의 실측 높이를 남긴다.
	 *
	 * 🔴 "피날레에서 바가 두꺼워진다" 는 보고(2026-09-03)를 코드만 읽어서는 못 갈랐다.
	 * 바는 `SizeBox` 로 58 고정이고 두 행 모두 `Fill` 이라 이론상 안 변한다. 그러면
	 * ① 실제로는 안 변하는데 탭 줄이 사라져 그렇게 보이거나 ② 어딘가 내가 못 본 경로가
	 * 늘리고 있는 것인데, **둘의 대처가 완전히 다르다.** 추측으로 고치면 헛짚는다.
	 */
	void LogBarGeometryNextTick(EFF_FestivalStage Stage);

	/**
	 * 탭 버튼 줄의 높이(px). 생성기의 `MenuRowHeight` 와 같아야 한다.
	 * 상태 행 높이는 위의 `StatusRowHeight` 를 그대로 쓴다 — 값을 두 벌 두지 않는다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Live|Layout", meta = (ClampMin = "0.0"))
	float MenuRowHeight = 29.f;

	void RefreshTabs();
	void RefreshStatusRow();
	void RefreshSpeed();
	void RefreshCta();

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY()
	TMap<EFF_LiveTab, TObjectPtr<UFF_LivePanelBase>> PanelWidgets;

	FFF_LiveTabSelection Selection;

	// 캐시하지 않는다 — `GetCtaState()` 주석 참고.

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
