// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Time/FF_FestivalStage.h"
#include "UI/PrepHUD/FF_PrepHudTypes.h"

// !! 전방 선언으로는 안 된다 !!
// `HandleWorldChanged` 가 `UFUNCTION` 이고 인자가 `const FFFZSWorldChangeEvent&` 라
// UHT 가 전체 정의를 요구한다. 시설 수 변화를 구독하는 유일한 창구라 폴링으로 대체하지 않는다 —
// 시설을 놓았는데 개장 버튼이 그대로 잠겨 있으면 원인을 찾을 수 없다.
#include "FF_FFZSTypes.h"

#include "FF_PrepHudBar.generated.h"

class UBorder;
class UButton;
class UPanelWidget;
class USizeBox;
class UTextBlock;
class UWidget;
class UWidgetAnimation;
class UFF_FestivalViewModelSubsystem;
class UFF_PrepHudTabButton;
class UFF_PrepPanelBase;
class UFF_PrepPlaceholderPanel;
class UFF_UIFlowSubsystem;
class UFFZSWorldSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFF_OnPrepTabSelectionChanged, FFF_PrepTabSelection, Selection);

// 개장 판정이 바뀌었다. 점검 패널의 `개장하기` 가 이걸 받아 자기 버튼을 켜고 끈다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFF_OnPrepCtaStateChanged);

/**
 * 준비 단계(셋업 · 홍보 · 개장 준비)의 하단 HUD.
 *
 * 목업 `docs/ui/mockups/live-hud-menu-states-v2-58px` 와 같은 골격이다 —
 * 총 58px(메뉴 29 + 상태 29)이고 화면 바닥에 붙는다. 정확한 픽셀은 에디터에서 맞춘다.
 *
 * ```
 * ┌ 메뉴 행 ─ 배치 · 프로그램 · 인력 · 홍보 · 피날레 · 점검 ────────────────┐
 * ├ 상태 행 ─ 축제 | 금화 | 관객 | 스태프 | 1일차·셋업 | 예상 | [CTA] ┤
 * └──────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 준비 단계에도 시간이 흐르는 구간(홍보)이 있고, HUD 가 입력 모드를 가져가면 월드 조작이 막힌다.
 * 일반 `UUserWidget` 이다. 근거: docs/ui/ui-tech-decision.md 7절
 *
 * === 🔴 HUDLayer 는 `Self Hit Test Invisible` 이어야 한다 ===
 * `Visible` 이면 화면 전체가 월드 클릭을 먹어 **건축 배치가 죽는다.** 2026-08-12 `WBP_TimeHud`
 * 에서 실제로 발생했다. 이 위젯도 같은 함정을 갖고 있으므로 `bEnforceHitTestInvisibleRoot` 가
 * 시작할 때 스스로 확인하고 고친다.
 *
 * === 패널 배타 규칙의 소유자는 여기다 ===
 * 한 번에 하나만 열리고, 같은 탭을 다시 누르면 닫힌다. **패널이 스스로 닫지 않는다.**
 * 규칙 자체는 `FFF_PrepHudRules` 에 순수 함수로 있고 테스트가 붙어 있다.
 *
 * === 값이 없는 칸은 숨긴다 ===
 * `0` 이나 `-` 를 쓰지 않는다. 관객 수를 모르는 상태에서 `0` 을 띄우면 "관객이 없다"는
 * **틀린 사실**이 된다. 소유자 API 가 없는 값은 `UI_API.md` 에 요청으로 남긴다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep HUD Bar"))
class FESTAFIESTA_API UFF_PrepHudBar : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_PrepHudBar(const FObjectInitializer& ObjectInitializer);

	// --- 조작 ---

	/**
	 * 탭을 누른 것과 같게 동작한다. **토글이다** — 이미 그 탭이 열려 있으면 닫힌다.
	 * 지금 보이지 않는 탭이면 아무것도 하지 않고 이유를 로그에 남긴다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void SelectTab(EFF_PrepHudTab Tab);

	/** 열린 패널을 닫는다. 이미 닫혀 있으면 아무 일도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void CloseCurrentPanel();

	/**
	 * 열린 패널을 화면에서 끌어 옮긴다. 패널 베이스의 드래그가 이걸 부른다.
	 *
	 * @param SlateDelta **슬레이트 단위** 이동량이다. 화면 픽셀을 넣으면 UI 배율이 1 이 아닌
	 *                   환경에서만 어긋난다 — 호출부가 배율을 한 번 나눠서 넣는다.
	 *
	 * 이동량은 **그 탭 레이아웃의 `Offset` 에 바로 쌓인다.** 그래서 패널을 닫고 다시 열어도
	 * 옮긴 자리가 유지되고, `FF.PrepHud.Layout` 이 찍어 주는 숫자를 그대로 확정할 수 있다.
	 * 늘어난 축의 이동은 버린다 — 그 방향은 화면이 정하므로 값이 커지면 반대로 밀린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void NudgePanelPosition(FVector2D SlateDelta);

	/** 옮긴 것을 되돌린다. 그 탭의 `Offset` 을 0 으로 만든다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void ResetPanelPosition();

	// --- 화면을 보면서 맞추기 위한 창구 ---
	//
	// 콘솔 명령이 이걸 부른다. **바꾼 값은 이 판이 돌아가는 동안만 유지된다** —
	// 인스턴스의 표를 고치는 것이고 에셋의 기본값은 그대로다. 확정은 WBP 기본값에 적어서 한다.

	/** 지금 열린 탭의 레이아웃. 열린 것이 없으면 `DefaultPanelLayout`. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Layout")
	FFF_PrepPanelLayout GetOpenPanelLayout() const;

	/** 지금 열린 탭의 레이아웃을 바꾸고 즉시 반영한다. 열린 것이 없으면 아무것도 하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Layout")
	void SetOpenPanelLayout(const FFF_PrepPanelLayout& NewLayout);

	/** 지금 열린 탭의 값을 `PanelLayouts` 에 적어 넣을 형태의 한 줄로 만든다. */
	FString DescribeOpenPanelLayout() const;

#if !UE_BUILD_SHIPPING
	/**
	 * 지금 화면에 올라와 있는 바. **콘솔 명령이 찾아갈 창구다.**
	 *
	 * 준비 단계 HUD 는 화면에 하나뿐이고, 콘솔 명령은 위젯 인스턴스를 인자로 받을 방법이
	 * 없어서 이렇게 둔다. `NativeConstruct` 에서 넣고 `NativeDestruct` 에서 비운다 —
	 * 약참조라 위젯이 죽은 뒤에 부르면 그냥 실패한다.
	 */
	static TWeakObjectPtr<UFF_PrepHudBar> ActiveBar;
#endif

	/** 지금 패널을 끌어 옮길 수 있는지. `디자이너 지정` 도킹에서는 코드가 자리를 안 만지므로 false. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	bool IsPanelDragAllowed() const;

	/**
	 * 다음 단계로 보내 달라고 요청한다. **CTA 버튼과 같은 경로다.**
	 *
	 * 점검 패널의 `개장하기` 처럼 패널 안에도 같은 행동을 하는 버튼이 있어서 공개해 둔다.
	 * 패널이 Stage 시스템을 직접 부르면 두 곳이 각자 판정해, 한쪽이 막힌 상태에서 다른 쪽만
	 * 통과하는 화면이 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void RequestStageAdvance();

	/**
	 * 지금 개장할 수 있는지. **판정은 바가 소유한다** — 시설 수와 단계 표를 아는 것이 바뿐이다.
	 *
	 * 🔴 바에는 이제 CTA 버튼이 없다 (2026-08-27). 화면에 버튼이 둘이라 하나로 줄였고,
	 * 남은 하나는 점검 패널의 `개장하기` 다. 그쪽이 이 값으로 자기 버튼을 켜고 끈다.
	 * 막힌 이유 문구는 탭을 열지 않아도 보여야 해서 바에 그대로 남아 있다.
	 */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	EFF_PrepCtaState GetCtaState() const { return LastCtaState; }

	/** 단계에 맞는 진행 버튼 문구. 셋업은 `확정하고 Day 1 시작`, 개장 준비는 `개장하기`. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	FText GetCtaLabelText() const;

	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Prep")
	FFF_OnPrepCtaStateChanged OnCtaStateChanged;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep")
	FFF_PrepTabSelection GetSelection() const { return Selection; }

	/** 모든 표시를 현재값으로 다시 그린다. 부분 갱신하지 않아 이벤트 순서에 의존하지 않는다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep")
	void RefreshAll();

	/** 디자이너 SizeBox 행 높이를 읽는다. 바 크기와 패널 도킹을 따로 맞출 필요가 없다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Layout")
	float GetBottomBarHeight() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Layout")
	float GetBottomStatusRowHeight() const;

	/** 열린 탭이 바뀔 때. 패널 밖에서 반응해야 하는 쪽(예: 배치 모드)이 구독한다. */
	UPROPERTY(BlueprintAssignable, Category = "FestaFiesta|UI|Prep")
	FFF_OnPrepTabSelectionChanged OnTabSelectionChanged;

	// --- 아직 소유자가 없는 값의 임시 창구 ---

	/**
	 * 관객 수·예상 방문객을 외부에서 넣는다. **둘 다 소유자 API 가 없다** (2026-08-26).
	 * `INDEX_NONE` 을 넣으면 그 칸을 숨긴다. 소유자 API 가 생기면 이 함수를 지우고
	 * ViewModel 구독으로 바꾼다 — 위젯 트리는 그대로 둔다.
	 */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Debug")
	void Debug_SetPlaceholders(int32 InVisitorCount, int32 InExpectedVisitorsMin, int32 InExpectedVisitorsMax);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 델리게이트 핸들러 ---

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	/** CTA 사유의 남은 시간을 초마다 갱신한다. */
	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

	UFUNCTION()
	void HandleWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleCtaClicked();

	/** 탭 버튼 클릭. 베이스가 `int32` 로 주므로 여기서 되돌려 해석한다. */
	void HandleTabButtonClicked(int32 TabId);

protected:
	// =========================================================================
	// 위젯 바인딩
	//
	// 탭 버튼만 이름이 규칙(`Tab_<enum 이름>`)에 묶이고 나머지는 전부 Optional 이다.
	// 목업의 일부만 만든 WBP 도 통과시키고, 소유자 값이 없는 칸을 나중에 붙일 수 있게 한다.
	// 빠진 것은 조용히 넘기지 않고 로그로 알린다.
	// =========================================================================

	// --- 메뉴 행 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Build;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Program;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Promotion;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Finale;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Tabs")
	TObjectPtr<UFF_PrepHudTabButton> Tab_Check;

	// --- 배경 (팔레트 색을 입힐 자리) ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UBorder> Border_MenuRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UBorder> Border_StatusRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Layout")
	TObjectPtr<USizeBox> SizeBox_MenuRow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Layout")
	TObjectPtr<USizeBox> SizeBox_StatusRow;

	// --- 패널이 들어가는 자리 ---

	/**
	 * 패널을 넣는 컨테이너. `Overlay` 나 `SizeBox` 를 쓴다.
	 *
	 * 이 위젯이 `CanvasPanel` 의 자식이면 `PanelDock` 값에 따라 앵커를 코드가 맞춘다.
	 * 다른 컨테이너면 위치는 디자이너가 소유하고 코드는 손대지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UPanelWidget> Panel_PanelHost;

	/** 패널 열기 애니메이션. 없으면 즉시 표시한다. `움직임 감소` 가 켜지면 재생하지 않는다. */
	UPROPERTY(Transient, BlueprintReadOnly, meta = (BindWidgetAnimOptional), Category = "FestaFiesta|UI|Prep")
	TObjectPtr<UWidgetAnimation> Anim_PanelOpen;

	// --- 상태 행 ---
	//
	// 값이 없으면 `Box_*` 묶음째 접는다. 숫자만 숨기면 옆의 아이콘과 구분선이 남아
	// "관객 아이콘 옆에 아무것도 없는" 화면이 된다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_FestivalName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UWidget> Box_FestivalName;

	/** 잔액. 매출과 다른 값이다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_Gold;

	/** 관객 수. **소유자 없음** — 준비 단계에는 관객이 없으므로 보통 숨겨져 있다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_VisitorCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UWidget> Box_VisitorCount;

	/** 사람 아이콘 옆에 총원만 표시한다. 기본/고용 상세는 인력 패널에서 확인한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_Staff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UWidget> Box_Staff;

	/**
	 * `1일차 · 셋업`. Day 와 Stage 표시명을 합친 한 줄이다.
	 *
	 * ★ 상태 행에서 **화면 정중앙**에 놓는다. 왼쪽 나열에 끼워 두면 앞의 칸(관객처럼
	 * 값이 없을 때 숨는 칸)이 접힐 때마다 위치가 흔들려 눈이 매번 다시 찾아야 한다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_DayStage;

	/**
	 * 일차·단계 뒤에 깔리는 어두운 판. 밝은 상태 행 위에서 이 한 줄만 도드라지게 한다.
	 * 없어도 동작한다 — 그때는 글자만 뜬다.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UBorder> Border_DayStage;

	/** 예상 방문객. **소유자 없음** — 범위로 보여준다. 없으면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UTextBlock> Text_ExpectedVisitors;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Status")
	TObjectPtr<UWidget> Box_ExpectedVisitors;

	// --- CTA ---
	//
	// 단계마다 하나다. 셋업은 `확정하고 Day N 시작`, 개장 준비는 `개장하기`.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Cta")
	TObjectPtr<UButton> Button_Cta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Cta")
	TObjectPtr<UTextBlock> Text_CtaLabel;

	/** 못 누르는 이유. **비활성 흐려짐만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Cta")
	TObjectPtr<UTextBlock> Text_CtaReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Cta")
	TObjectPtr<UWidget> Box_Cta;

	// =========================================================================
	// 설정
	// =========================================================================

	/** 켜면 색·배경·폰트·패딩은 Designer가 소유한다. 런타임에는 값과 선택 상태만 갱신한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Appearance")
	bool bUseDesignerStyle = false;

	/** {Amount}에 현재 보유 금화가 들어간다. 글꼴/크기는 Text_Gold에서 수정한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Text")
	FText GoldTextFormat;

	/** {Count}에 총 인원이 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Text")
	FText StaffTextFormat;

	/** {Day}와 {Stage}에 현재 일차와 단계가 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Text")
	FText DayStageTextFormat;

	/**
	 * ★ **탭 하나 = 한 줄.** 그 탭을 열었을 때 패널이 어디에 얼마나 크게 열릴지 정한다.
	 *
	 * 방향마다 쓰이는 성분이 다르다 — `FFF_PrepPanelLayout` 주석의 표를 먼저 본다.
	 * 표에 없는 탭은 `DefaultPanelLayout` 을 쓴다.
	 *
	 * === 화면을 보면서 맞추는 순서 ===
	 * 1. 게임에서 그 탭을 연다
	 * 2. 패널의 빈 곳을 끌어 자리를 잡고, 콘솔 `FF.PrepHud.SetSize <폭> <높이>` 로 크기를 바꿔 본다
	 * 3. 마음에 들면 `FF.PrepHud.Layout` 이 찍어 주는 숫자를 이 표에 적는다
	 *
	 * 2번의 변경은 **이 판이 돌아가는 동안만** 유지된다. 3번을 해야 확정된다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout")
	TMap<EFF_PrepHudTab, FFF_PrepPanelLayout> PanelLayouts;

	/** 표에 없는 탭이 쓰는 값. 크기가 0 이하로 들어온 경우의 폴백도 이것이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout")
	FFF_PrepPanelLayout DefaultPanelLayout;

	/**
	 * 하단 바 높이(px). **바의 실제 높이와 같게 유지한다.**
	 * 디자이너에서 바를 키우면 이 값도 같이 키운다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float BarHeight = 58.f;

	/**
	 * 상태 행 높이(px). **패널 아래 끝을 여기까지만 올린다.**
	 *
	 * 바 전체(58)만큼 띄우면 패널이 바 위에 동동 뜨고, 가운데 패널들과 아래 끝이 어긋난다.
	 * 탭 줄 밑으로 들어가야 "이 버튼이 이 패널을 연다" 가 눈에 붙는다
	 * (`docs/memory/prep-hud-reference.md` 2절).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float StatusRowHeight = 29.f;

	/** 패널과 화면 가장자리 사이 여백(px). 늘어나는 축에만 쓰인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float PanelMargin = 12.f;

	/**
	 * 메뉴 행·상태 행 배경을 코드가 칠할지.
	 *
	 * 🔴 켜 두는 것을 권한다. `UBorder` 의 기본 브러시는 `NoImage` 라 **색만 넣으면 아무것도
	 * 안 그려진다.** 이 두 행에 텍스처나 9-slice 를 쓰고 싶으면 끄고 WBP 에서 직접 지정한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout")
	bool bPaintRowBackgrounds = true;

	/**
	 * 상태 행 모서리 반경(px). 0 이면 각진 사각형이다.
	 * 화면 폭을 꽉 채우는 줄이라 기본은 0 이다 — 둥글리면 양 끝이 떠 보인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float RowCornerRadius = 0.f;

	/**
	 * 메뉴 행 칩 모서리 반경(px).
	 *
	 * 메뉴 행은 **버튼 폭까지만** 배경이 오는 떠 있는 칩이다.
	 * 기본은 0(각진 사각형)이다 — 2026-08-26 실기에서 둥근 쪽이 어색해 각진 것으로 정했다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float MenuRowCornerRadius = 0.f;

	/**
	 * 메뉴 행 배경을 **버튼 폭까지만** 오게 할지.
	 *
	 * 켜면 코드가 메뉴 행의 슬롯 정렬을 `Center` 로 맞춰 Border 가 자식(탭 행) 크기로 줄어든다.
	 * 끄면 정렬을 건드리지 않으므로 디자이너가 잡은 대로 간다 — 화면 폭을 꽉 채우고 싶으면 끈다.
	 *
	 * ★ 런타임에서 맞추는 이유는 **이미 만들어진 WBP 도 고쳐지게** 하기 위해서다.
	 *   생성기는 비파괴라 슬롯 정렬만 바꾸려고 WBP 를 지우고 다시 만들 이유가 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout")
	bool bMenuRowHugsButtons = true;

	/** `1일차 · 셋업` 칩 모서리 반경(px). 0 이면 각진 사각형이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout", meta = (ClampMin = "0.0"))
	float DayStageCornerRadius = 4.f;

	/**
	 * 패널을 빈 곳에서 끌어 옮길 수 있게 할지.
	 *
	 * 끄면 레이아웃 표의 `Offset` 값만으로 자리가 정해진다. 켜 두면 플레이어가 월드를
	 * 보려고 패널을 잠깐 밀어 둘 수 있다 — 비모달 화면이라 그게 자연스럽다.
	 *
	 * ★ 옮긴 자리는 **이 위젯이 살아 있는 동안만** 유지된다. 단계가 바뀌어 HUD 가 다시 만들어지면
	 *   설정값 자리로 돌아간다. 저장할 곳이 없어서가 아니라, 다음 단계에서 패널 구성이 달라지기 때문이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Layout")
	bool bAllowPanelDrag = true;

	/**
	 * 단계별 CTA 문구. **비어 있는 단계는 CTA 를 아예 숨긴다.**
	 * `{Day}` 를 쓰면 현재 일차가 들어간다 — `Day 1` 을 문구에 박지 않는다.
	 * 기본값은 생성자에서 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Cta")
	TMap<EFF_FestivalStage, FText> CtaLabels;

	/**
	 * 시설이 하나도 없으면 CTA 를 막을지.
	 *
	 * 기획 문구는 `개장 버튼`에 대한 것이지만 셋업 확정에도 같은 규칙을 적용한다 —
	 * 시설 0개로 시작한 축제는 관객이 할 것이 없어 어차피 되돌아온다.
	 * 이견이 있으면 이 값을 끄는 것으로 되돌릴 수 있다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Cta")
	bool bRequireFacilityForCta = true;

	/**
	 * 탭별 패널 클래스. **U5 가 여기를 채운다.**
	 * 비어 있는 탭은 `PlaceholderPanelClass` 로 "준비 중" 패널을 띄운다 — 아무것도 안 뜨면
	 * 탭이 고장 난 것으로 보인다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Panels")
	TMap<EFF_PrepHudTab, TSoftClassPtr<UFF_PrepPanelBase>> PanelClasses;

	/**
	 * `PanelClasses` 에 없는 탭은 **이름 규칙으로 찾는다.** 아이콘과 같은 방식이다.
	 *
	 * 이렇게 두면 U5 에서 패널을 하나 만들 때마다 코드를 고치거나 표를 채우지 않아도 된다 —
	 * 규칙에 맞는 이름으로 WBP 를 만들면 자동으로 붙는다. 규칙에서 벗어나는 하나만
	 * `PanelClasses` 에 예외로 넣는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Panels")
	FString PanelClassFolder = TEXT("/Game/FestaFiesta/UI/PrepHUD/Panels");

	/** `{0}` 에 `EFF_PrepHudTab` 의 항목 이름이 들어간다 (`Staff` → `WBP_FF_PrepPanelStaff`). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Panels")
	FString PanelClassNameFormat = TEXT("WBP_FF_PrepPanel{0}");

	/** 아직 만들지 않은 탭에 쓰는 자리표시 패널. 기본값은 생성자에서 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Panels")
	TSoftClassPtr<UFF_PrepPlaceholderPanel> PlaceholderPanelClass;

	/** 자리표시 패널에 띄울 탭별 한 줄. 비어 있으면 공통 문구를 쓴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Panels")
	TMap<EFF_PrepHudTab, FText> PlaceholderMessages;

	// --- 아이콘 ---
	//
	// 라이브 HUD 와 같은 규칙이다. 테마를 바꿀 때 폴더 한 줄만 고치면 6개가 함께 바뀐다.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Icons")
	FString TabIconFolder = TEXT("/Game/FestaFiesta/UI/LiveHUD/Themes/LightTeal/Icons/Navigation");

	/** `{0}` 에 `EFF_PrepHudTab` 의 항목 이름이 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Icons")
	FString TabIconNameFormat = TEXT("T_FF_LH_Icon_{0}_White_256");

	/** 아이콘 표시 크기(px). 목업 기준 13. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Icons")
	float TabIconSize = 13.f;

	/** 폴더 규칙에서 벗어나는 아이콘만 예외로 넣는다. 보통은 비워 둔다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Icons")
	TMap<EFF_PrepHudTab, FSlateBrush> TabIconOverrides;

	// --- 안전장치 ---

	/**
	 * 🔴 루트가 `Visible` 이면 `Self Hit Test Invisible` 로 고치고 경고를 남긴다.
	 *
	 * `Visible` 인 전체 화면 HUD 는 **월드 클릭을 전부 먹어 건축 배치를 죽인다.**
	 * 화면은 정상으로 보이므로 증상만 보고는 원인을 찾을 수 없다. 끄지 않는 것을 권한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Safety")
	bool bEnforceHitTestInvisibleRoot = true;

private:
	// --- 구독 ---

	UFF_FestivalViewModelSubsystem* GetViewModel() const;
	UFF_UIFlowSubsystem* GetFlow() const;

	/**
	 * 구독은 `NativeConstruct`, 해제는 `NativeDestruct` 다. **`NativeOnInitialized` 가 아니다.**
	 * CommonUI 화면 스택은 위젯을 풀에 넣고 재사용하므로 인스턴스당 1회인 곳에 구독을 두면
	 * 두 번째 진입부터 버튼이 죽는다 (2026-08-24에 화면 5종에서 실제로 발생).
	 * 이 위젯은 HUD 라 스택에 올라가지 않지만 규칙을 다르게 두지 않는다.
	 */
	void BindSources();
	void UnbindSources();

	// --- 갱신 ---

	void CollectTabs();
	FSlateBrush ResolveTabIcon(EFF_PrepHudTab Tab, const FString& EnumName) const;

	void RefreshTabVisibility();
	void RefreshStatusRow();

	/**
	 * `1일차 · 셋업` 한 줄만 다시 쓴다.
	 *
	 * `HandleStageViewChanged` 와 `RefreshStatusRow` 가 둘 다 이 줄을 갱신해야 하는데,
	 * 한쪽이 다른 쪽을 부르면 탭 갱신이 두 번 돌고 호출 순서에 결과가 달라진다
	 * (`제어 경로를 둘로 만들기` — ui-antipatterns-to-avoid 10번).
	 */
	void RefreshDayStageText(EFF_FestivalStage Stage, int32 Day);
	void RefreshCta();

	/** 상태를 넣고 **바뀌었을 때만** 알린다. 매 갱신마다 쏘면 패널이 헛돈다. */
	void SetCtaState(EFF_PrepCtaState NewState);

	/**
	 * 이 탭의 레이아웃. 표를 먼저 보고 없으면 `DefaultPanelLayout` 을 쓴다.
	 * 크기가 0 이하면 기본값으로 되돌리고 경고를 남긴다 — 0 이면 패널이 사라진다.
	 */
	FFF_PrepPanelLayout ResolveLayout(EFF_PrepHudTab Tab) const;

	/**
	 * 패널 자리를 이 방향으로 맞춘다. **패널을 열 때마다 부른다** — 탭마다 방향이 다르다.
	 * 호스트가 `CanvasPanel` 자식이 아니면 아무것도 하지 않는다 (위치를 디자이너가 소유한다).
	 */
	void ApplyPanelLayout(const FFF_PrepPanelLayout& Layout);

	/**
	 * 행 배경을 칠한다. **`SetBrushColor` 만으로는 안 된다** — `UBorder` 의 기본 브러시가
	 * `NoImage` 라 색을 넣어도 그려지지 않는다. 텍스처가 이미 지정돼 있으면 색만 덮는다.
	 */
	void PaintRowBackground(UBorder* Border, const FLinearColor& Color, float CornerRadius) const;

	/** 선택을 실제 화면에 반영한다. 값이 바뀌지 않았으면 아무것도 하지 않는다. */
	void ApplySelection(const FFF_PrepTabSelection& NewSelection);

	/** 없으면 만들어 호스트에 넣는다. 만들 수 없으면 nullptr 과 이유 로그. */
	UFF_PrepPanelBase* EnsurePanel(EFF_PrepHudTab Tab);

	/**
	 * 이 탭에 띄울 패널 클래스. 찾는 순서는 **예외 표 → 이름 규칙 → 자리표시** 다.
	 * 자리표시로 떨어졌으면 `bOutIsPlaceholder` 가 true 다.
	 */
	UClass* ResolvePanelClass(EFF_PrepHudTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const;

	// --- 시설 수 ---

	/**
	 * 설치된 시설 수를 다시 센다. 못 읽으면 `INDEX_NONE` 으로 둔다.
	 * **0 으로 두지 않는다** — 읽지 못한 것을 "없다"로 취급하면 시설을 놓고도 시작할 수 없다.
	 */
	void RefreshFacilityCount();

	// --- 표시 헬퍼 ---

	/** 값이 없으면 묶음째 접는다. 묶음이 없으면 TextBlock 만 접는다. */
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	/** 천 단위 구분 기호를 넣는다. 표시 형식을 위젯마다 다르게 만들지 않는다. */
	static FText FormatGold(int32 Amount);

	// --- 상태 ---

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepHudTabButton>> TabButtons;

	/** 한 번 만든 패널은 재사용한다. 단계가 끝나 위젯이 내려가면 함께 사라진다. */
	UPROPERTY()
	TMap<EFF_PrepHudTab, TObjectPtr<UFF_PrepPanelBase>> PanelWidgets;

	FFF_PrepTabSelection Selection;

	/**
	 * 마지막으로 로그를 남긴 CTA 상태. **상태가 바뀔 때만 찍기 위한 것이다.**
	 * `RefreshCta` 는 값이 바뀔 때마다 불리므로 매번 찍으면 로그가 같은 줄로 가득 찬다.
	 */
	EFF_PrepCtaState LastCtaState = EFF_PrepCtaState::Hidden;

	/** `INDEX_NONE` 이면 아직 읽지 못했다는 뜻이다. 0 과 구분한다. */
	int32 FacilityCount = INDEX_NONE;

	// 소유자 API 가 없는 값. `INDEX_NONE` 이면 칸을 숨긴다.
	int32 VisitorCount = INDEX_NONE;
	int32 ExpectedVisitorsMin = INDEX_NONE;
	int32 ExpectedVisitorsMax = INDEX_NONE;

	/** Zone 월드 구독 해제용. 월드가 바뀌면 이전 것은 이미 죽어 있다. */
	TWeakObjectPtr<UFFZSWorldSubsystem> BoundZoneWorldSubsystem;

	UFUNCTION()
	void HandleRequiredFacilitiesChanged();
	TWeakObjectPtr<class UFF_RequiredFacilitiesSubsystem> BoundRequiredFacilities;
};
