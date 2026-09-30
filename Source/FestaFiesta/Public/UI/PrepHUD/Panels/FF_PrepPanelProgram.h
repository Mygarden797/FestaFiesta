// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"
#include "Building/FF_BuildCatalogTypes.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/PrepHUD/Panels/FF_PrepProgramCard.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_PrepPanelProgram.generated.h"

class UButton;
class UPanelWidget;
class UTextBlock;
class UWidget;
class UDataTable;
class UFF_FestivalProgressionSubsystem;

/**
 * 프로그램을 편성할 수 없는 이유.
 *
 * !! 하나로 합치지 않는다 !!
 * 이유마다 플레이어가 할 행동이 다르다 —
 * 시설은 `배치` 탭에서 부스를 지어야 하며, 단계는 기다리는 것 말고 할 수 있는 게 없다.
 * 카드를 흐리게만 하면 셋을 구분할 수 없다 (AT-011).
 */
UENUM(BlueprintType)
enum class EFF_PrepProgramBlockReason : uint8
{
	/** 편성할 수 있다. */
	None				UMETA(DisplayName = "가능"),

	/** 아직 값을 못 읽었다. 축제가 시작되기 전이면 정상이다. */
	Unknown				UMETA(DisplayName = "모름"),

	/** 지금은 편성할 수 있는 단계가 아니다 (셋업·개장 준비에서만 된다). */
	NotSchedulingStage	UMETA(DisplayName = "편성 단계 아님"),

	/** 카드를 아직 안 골랐다. 막힌 것이 아니라 다음 행동을 안내하는 상태다. */
	NoCardSelected		UMETA(DisplayName = "카드 미선택"),

	/** 오늘 이미 편성했다. 카드는 하루에 하나씩만 열린다. */
	AlreadyScheduled	UMETA(DisplayName = "오늘 이미 편성"),

	/** 이 카드를 열 수 있는 시설이 하나도 없다. 부스를 먼저 지어야 한다. */
	NoEligibleFacility	UMETA(DisplayName = "열 수 있는 시설 없음"),


	/** 비용을 낼 잔액이 없다. */
	NotEnoughGold		UMETA(DisplayName = "잔액 부족"),

	/** 카탈로그에 없는 카드다. 데이터 문제이므로 플레이어가 할 수 있는 것이 없다. */
	UnknownProgram		UMETA(DisplayName = "없는 프로그램")
};

/**
 * 고른 카드 한 장의 편성 견적. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepProgramQuote
{
	GENERATED_BODY()

	/** 고른 카드. 아무것도 안 골랐으면 `NAME_None`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	FName ProgramId;

	/** 편성 시 1회 차감. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 CostGold = INDEX_NONE;

	/**
	 * 정원을 다 채웠을 때 그날 체험비. **정원이 보장되므로 확정 매출이다** —
	 * 그래서 `예상` 이 아니라 그대로 쓴다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 MaxRevenue = INDEX_NONE;

	// 🔴 **아래 값은 `INDEX_NONE` 으로 `모름` 을 나타낼 수 없다.**
	// 모자라는 양을 보여주려고 음수를 자르지 않는데, 그러면 계산 결과가 정확히 -1 인 순간
	// (`잔액 199 - 비용 200`) 이 `모름` 과 구별되지 않는다.
	// 실제로 겪기 쉬운 값이라 **별도 플래그로 모름을 표현한다.** 표시부는 플래그만 본다.

	/** 편성 후 잔액. `bHasGoldAfterSchedule` 가 false 면 이 값은 의미가 없다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 GoldAfterSchedule = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	bool bHasGoldAfterSchedule = false;


	/** 이 카드를 열 수 있는 시설 수. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 EligibleFacilityCount = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	EFF_PrepProgramBlockReason BlockReason = EFF_PrepProgramBlockReason::Unknown;

	/** 편성 버튼을 켤지. 막힌 이유가 없을 때만. */
	bool CanSchedule() const { return BlockReason == EFF_PrepProgramBlockReason::None; }
};

/**
 * 프로그램 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "잔액은 되는데 열 시설이 없다" 같은 조합은 에디터에서 **부스를 실제로 지웠다 세워야** 재현된다.
 * 여기 있으면 자동화 테스트가 못으로 박는다. `FFF_PrepStaffRules` 와 같은 이유다.
 *
 * !! 여기서 편성 허용 여부를 최종 판정하지 않는다 !!
 * 실제 허용은 `UFF_FestivalProgressionSubsystem::TryScheduleProgram` 이 정한다.
 * 이 struct 는 **소유자가 이미 낸 답(`EFF_ProgramPurchaseResult`)을 화면 이유로 옮기고**,
 * 소유자가 보지 않는 두 가지(단계·시설)를 앞뒤에 끼워 넣을 뿐이다 (UI 규칙 2).
 */
struct FESTAFIESTA_API FFF_PrepProgramRules
{
	/**
	 * 막힌 이유를 고른다.
	 *
	 * ★ **순서가 곧 안내다.** 가장 바깥 조건부터 본다 —
	 *   ① 축제 전(모름) → ② 편성 단계 아님 → ③ 카드 미선택 → ④ 오늘 이미 편성
	 *   → ⑤ 열 수 있는 시설 없음 → ⑥ 잔액 부족
	 *
	 * ⑤ 를 ⑥ 보다 먼저 보는 이유는, 열 장소가 없으면 돈을 모아도
	 * 이 카드를 못 열기 때문이다. 더 근본적인 막힘을 먼저 알려 준다.
	 *
	 * ④ 가 ⑤ 보다 먼저인 이유는, 이미 편성한 카드에 대해 "부스를 지으세요" 는 틀린 안내라서다.
	 *
	 * @param OwnerResult            `CanScheduleProgram(ProgramId)` 결과. 카드 미선택이면 무시된다
	 * @param bCanScheduleNow        `CanScheduleProgramNow()` — 셋업·개장 준비에서만 true
	 * @param bHasCardSelected       카드를 골랐는가
	 * @param EligibleFacilityCount  열 수 있는 시설 수. `INDEX_NONE` 이면 아직 모른다
	 */
	static EFF_PrepProgramBlockReason ResolveBlockReason(
		EFF_ProgramPurchaseResult OwnerResult,
		bool bCanScheduleNow,
		bool bHasCardSelected,
		int32 EligibleFacilityCount);

	/**
	 * 견적을 만든다. 값이 없으면 그 칸을 `INDEX_NONE` 으로 둔다 — **0 을 넣지 않는다.**
	 * `0 Gold` 는 "공짜" 라는 틀린 사실이 된다.
	 *
	 * @param Card                   고른 카드. `nullptr` 이면 금액 칸이 전부 비고 이유만 채워진다
	 * @param CurrentGold            현재 잔액. `INDEX_NONE` 이면 잔액 계산을 건너뛴다
	 */
	static FFF_PrepProgramQuote BuildQuote(
		const FFF_ProgramCard* Card,
		EFF_ProgramPurchaseResult OwnerResult,
		bool bCanScheduleNow,
		int32 EligibleFacilityCount,
		int32 CurrentGold);
};

/**
 * 준비 단계 `프로그램` 패널.
 *
 * === 카드만 고르는 화면이 아니다 ===
 * `TryScheduleProgram` 은 **시설 핸들도** 받는다. 요가를 화장실에서 여는 것을 막으려고 카드마다
 * 허용 시설 종류가 있어서, 화면은 `카드 → 그 카드를 열 수 있는 시설 → 편성` 3단이다.
 *
 * === 카드를 WBP 에 박지 않는다 ===
 * 카탈로그는 축제 정의에서 오는 데이터다. 지금은 2장이지만 축제가 늘면 달라진다.
 * `Box_ProgramCards` 에 `CardWidgetClass` 로 런타임에 만들어 넣는다. 시설도 같다.
 *
 * === 확인창을 쓰지 않는다 ===
 * 하루짜리라 다음 날 다시 고를 수 있다. 되돌릴 수 없는 결정이 아니므로 확인창을 두지 않는다 —
 * 흔해지면 아무도 읽지 않게 된다. 되돌릴 수 없는 것은 피날레뿐이다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel - Program"))
class FESTAFIESTA_API UFF_PrepPanelProgram : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	/** 현재 견적. 표시용이고 이 값으로 편성을 막지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Program")
	FFF_PrepProgramQuote GetQuote() const { return Quote; }

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;
	virtual void NativeDestruct() override;

	// --- 버튼 ---

	UFUNCTION()
	void HandleScheduleClicked();

	/** 카드 클릭. `ChoiceId` 는 카탈로그 순번이다. */
	void HandleCardClicked(int32 ChoiceId);

	/** 시설 클릭. `ChoiceId` 는 시설 목록 순번이다. */
	void HandleFacilityClicked(int32 ChoiceId);

	UFUNCTION()
	void HandleFacilitySelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	UFUNCTION()
	UWidget* GenerateFacilityOptionWidget(FString Item);

	void HandleCatalogFacilityClicked(int32 ChoiceId);

	UFUNCTION()
	void HandleBuildCatalogChanged();

	// --- 결과 이벤트 ---

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	/** 편성 목록이 바뀌었다. 카드 잠금이 따라 바뀐다. */
	UFUNCTION()
	void HandleScheduledProgramsChanged();

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleFestivalDefinitionChanged();

protected:
	// --- 위젯 바인딩. 전부 Optional 이다 ---

	/** 카드가 들어갈 자리. `WrapBox` 를 권한다 — 카드가 늘어도 옆 카드를 덮지 않는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UPanelWidget> Box_ProgramCards;

	/** 고른 카드를 열 수 있는 시설이 들어갈 자리. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UPanelWidget> Box_FacilityChoices;

	/** 새 시안의 시설 선택. 이전 WBP의 버튼 목록은 계속 지원한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UComboBoxString> Combo_Facilities;

	/** 시설 줄 전체. 카드를 고르기 전에는 줄째 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UWidget> Box_FacilityRow;

	/** `열 수 있는 시설이 없습니다. 배치 탭에서 먼저 지으세요.` */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_FacilityNotice;

	/** Compatible facilities: each button opens its exact construction catalog entry. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UPanelWidget> Box_CatalogChoices;

	// --- 견적 ---

	/** `250 Gold`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_CostPreview;

	/** `편성 후 잔액 7,350`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_GoldAfter;

	/** `확정 매출 400`. 정원이 보장되므로 `예상` 이 아니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_RevenuePreview;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UButton> Button_Schedule;

	/** 못 누르는 이유. **비활성 흐려짐만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_BlockReason;

	/** 하루짜리라는 안내. 값으로 빼서 문구를 바꿀 수 있게 한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_DailyNotice;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_SelectedProgram;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_Day;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Program")
	TObjectPtr<UTextBlock> Text_EmptyCatalog;

	// --- 설정 ---

	/**
	 * 카드 위젯 클래스. 비워 두면 아래 경로에서 이름으로 찾는다.
	 *
	 * C++ 기본값으로 BP 클래스를 넣을 수 없어서 경로 폴백을 둔다 —
	 * `UFF_PrepHudBar::ResolvePanelClass` 가 패널을 찾는 방식과 같다.
	 * 그래야 `FF.PrepHudUMG` 로 만든 WBP 가 손으로 배선하지 않아도 붙는다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program")
	TSubclassOf<UFF_PrepProgramCard> CardWidgetClass;

	/** `CardWidgetClass` 가 비었을 때 쓸 경로. 블루프린트 클래스라 `_C` 는 코드가 붙인다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program")
	FString CardWidgetFallbackPath = TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepProgramCard");

	/** 시설 선택지 위젯 클래스. 날씨 선택지와 같은 `WBP_FF_PrepChoiceButton` 이면 된다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program")
	TSubclassOf<UFF_PrepChoiceButton> FacilityChoiceClass;

	/** `FacilityChoiceClass` 가 비었을 때 쓸 경로. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program")
	FString FacilityChoiceFallbackPath = TEXT("/Game/FestaFiesta/Building/FF_WBP_Building/Shared/WBP_FF_PrepChoiceButton");

	/** ★ 비우지 않는다. 안 쓰면 다음 날 편성이 사라진 이유를 알 수 없다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program")
	FText DailyNoticeText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program|Text")
	FText NoSelectionText = NSLOCTEXT("FestaFiestaPrepProgram", "SummaryNoSelection", "프로그램을 선택하세요");

	/** {0}: 축제의 실제 진행 일차. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program|Text")
	FText DayTextFormat = NSLOCTEXT("FestaFiestaPrepProgram", "DayFormat", "{0}일차");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program|Text")
	FText EmptyCatalogText = NSLOCTEXT("FestaFiestaPrepProgram", "EmptyCatalog", "현재 편성할 수 있는 프로그램이 없습니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program|Text")
	FText FacilityPlaceholderText = NSLOCTEXT("FestaFiestaPrepProgram", "FacilityPlaceholder", "시설을 선택하세요");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Program|Text")
	FText NoFacilityText = NSLOCTEXT("FestaFiestaPrepProgram", "NoFacilityOption", "편성 가능한 시설이 없습니다");

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FFF_PrepProgramFacilityNavigationTest;
#endif

	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();
	void BindRevenueTable();
	void HandleRevenueTableChanged();

	/** 카탈로그를 읽어 카드를 다시 만든다. 패널을 열 때와 편성이 바뀔 때만 부른다. */
	void RebuildCards();

	/** 고른 카드의 시설 목록을 다시 만든다. 카드가 바뀔 때마다 부른다. */
	void RebuildFacilityChoices();

	void RefreshFacilityCombo();

	void EnsureCatalogChoices();
	void RebuildCatalogChoices();

	/** 견적을 다시 만들고 화면 전체를 다시 그린다. 부분 갱신하지 않는다. */
	void RefreshAll();

	/** 지금 고른 카드. 없으면 `nullptr`. */
	const FFF_ProgramCard* FindSelectedCard() const;


	/** 막힌 이유를 화면 문구로 바꾼다. 문구를 늘리려면 여기만 고친다. */
	FText BuildBlockReasonText() const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalProgressionSubsystem> BoundProgression;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BoundRevenueTable;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepProgramCard>> CardWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepChoiceButton>> FacilityWidgets;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepChoiceButton>> CatalogChoiceWidgets;

	TArray<FFF_BuildCatalogEntry> CompatibleCatalogEntries;

	/** 화면에 그린 카탈로그 사본. 순번 → 카드를 되돌리는 데 쓴다. */
	TArray<FFF_ProgramCard> Catalog;

	/** 고른 카드의 카탈로그 순번. 안 골랐으면 `INDEX_NONE`. */
	int32 SelectedCardIndex = INDEX_NONE;

	/** 고른 카드를 열 수 있는 시설 목록. */
	TArray<FFF_PrepFacilityOption> FacilityOptions;

	/** 고른 시설의 순번. 안 골랐으면 `INDEX_NONE`. */
	int32 SelectedFacilityIndex = INDEX_NONE;

	/** 목록 갱신 중 발생하는 ComboBox의 Direct 이벤트가 선택을 다시 덮지 않게 한다. */
	bool bRefreshingFacilityCombo = false;

	FFF_PrepProgramQuote Quote;
};
