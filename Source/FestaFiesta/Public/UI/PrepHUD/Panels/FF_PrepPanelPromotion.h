// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "UI/PrepHUD/Panels/FF_PrepPromotionCard.h"
#include "UI/ViewModel/FF_FestivalViewModelSubsystem.h"
#include "FF_PrepPanelPromotion.generated.h"

class UButton;
class UFF_ConfirmDialog;
class UPanelWidget;
class UTextBlock;

/**
 * 홍보를 살 수 없는 이유.
 *
 * !! 하나로 합치지 않는다 !! 이유마다 플레이어가 할 행동이 다르다 — 잔액은 지출을 줄이면 되고,
 * 단계는 지나가면 끝이며, 이미 산 것은 되돌릴 수 없다. 카드를 흐리게만 하면 셋을 구분할 수 없다 (AT-011).
 */
UENUM(BlueprintType)
enum class EFF_PrepPromotionBlockReason : uint8
{
	None			UMETA(DisplayName = "가능"),

	/** 아직 값을 못 읽었다. 축제가 시작되기 전이면 정상이다. */
	Unknown			UMETA(DisplayName = "모름"),

	/** 셋업이 지났다. 홍보는 3일 전체에 적용되는 1회 선택이라 나중에 살 수 없다. */
	NotSetupStage	UMETA(DisplayName = "셋업이 지남"),

	/** 이미 샀다. 축제당 1회다. */
	AlreadyPurchased UMETA(DisplayName = "이미 구매함"),

	/** 등급을 안 골랐거나 `안 함` 을 골랐다. 막힌 것이 아니라 다음 행동 안내다. */
	NoChoiceSelected UMETA(DisplayName = "등급 미선택"),

	/** 홍보비를 낼 잔액이 없다. */
	NotEnoughGold	UMETA(DisplayName = "잔액 부족")
};

/**
 * 고른 등급 한 줄의 견적.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepPromotionQuote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	EFF_PromotionChoice Choice = EFF_PromotionChoice::None;

	/** 홍보비. 모르면 `INDEX_NONE`. **`안 함` 의 0 은 모름이 아니라 확정값이다.** */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 CostGold = INDEX_NONE;

	/** 방문객 배수. 모르면 0 이하. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	float VisitorMultiplier = 0.f;

	/**
	 * 구매 후 잔액. `bHasGoldAfterPurchase` 가 false 면 이 값은 의미가 없다.
	 *
	 * 🔴 **`INDEX_NONE` 으로 `모름` 을 나타낼 수 없다.** 모자라는 양을 보여주려고 음수를
	 * 자르지 않는데, `잔액 499 - 비용 500 = -1` 이 `모름` 과 구별되지 않는다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 GoldAfterPurchase = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	bool bHasGoldAfterPurchase = false;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	EFF_PrepPromotionBlockReason BlockReason = EFF_PrepPromotionBlockReason::Unknown;

	bool CanPurchase() const { return BlockReason == EFF_PrepPromotionBlockReason::None; }
};

/**
 * 홍보 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * 홍보 탭은 **셋업에서 한 번만** 열린다. 축제를 새로 시작하지 않으면 다시 못 보는 화면이라
 * "이미 샀는데 셋업이 지났다" 같은 조합을 손으로 밟기가 가장 어렵다.
 *
 * !! 여기서 구매 허용 여부를 최종 판정하지 않는다 !!
 * 잔액 판정의 소유자는 경제 시스템(`TrySpend`)이다. 이 struct 는 화면 표시용이다.
 */
struct FESTAFIESTA_API FFF_PrepPromotionRules
{
	/**
	 * 막힌 이유를 고른다.
	 *
	 * ★ **순서** — ① 모름 → ② 이미 구매 → ③ 셋업 아님 → ④ 등급 미선택 → ⑤ 잔액 부족
	 *
	 * ② 가 ③ 보다 먼저인 이유는, 이미 산 사람에게 `셋업이 지났습니다` 는 못 산 것처럼 읽히기
	 * 때문이다. 산 사실을 먼저 알려 준다.
	 *
	 * @param CostGold `INDEX_NONE` 이면 모름. `안 함`(0)은 `NoChoiceSelected` 로 접힌다
	 * @param Gold     현재 잔액. `INDEX_NONE` 이면 잔액 판정을 건너뛴다
	 */
	static EFF_PrepPromotionBlockReason ResolveBlockReason(
		EFF_PromotionChoice Choice,
		EFF_PromotionChoice Purchased,
		bool bCanPurchaseNow,
		int32 CostGold,
		int32 Gold);

	/**
	 * 견적을 만든다. 값이 없으면 `INDEX_NONE` 으로 둔다 — **0 을 넣지 않는다.**
	 * 다만 `안 함` 의 0 Gold 는 확정값이라 그대로 채운다.
	 */
	static FFF_PrepPromotionQuote BuildQuote(
		const FFF_PromotionOptionView* Option,
		EFF_PromotionChoice Purchased,
		bool bCanPurchaseNow,
		int32 Gold);

	/** 구매 완료 후에는 현재 잔액을 보여준다. 기존 견적에서 홍보비를 다시 빼지 않는다. */
	static TOptional<int32> ResolveSummaryBalance(
		const FFF_PrepPromotionQuote& Quote, EFF_PromotionChoice Purchased, int32 Gold);
};

/**
 * 준비 단계 `홍보` 패널.
 *
 * === 예상치와 실제 입장 인원을 구분한다 ===
 * 등급 비용·배수와 Gold 차감, 예상 방문객 반영은 연결되어 있다.
 * 실제 입장 인원과의 연동은 별개이므로 해당 안내를 화면에 남긴다.
 *
 * === 확인창을 거친다 ===
 * 피날레와 같다. 축제당 1회이고 셋업이 지나면 되돌릴 수 없다.
 * 프로그램(하루짜리)과 달라서 확인창이 값어치를 한다.
 *
 * === `안 함` 카드는 살 수 없다 ===
 * 미구매는 상태이지 상품이 아니다. 고르면 견적만 보여주고 구매 버튼은 사유와 함께 꺼진다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel - Promotion"))
class FESTAFIESTA_API UFF_PrepPanelPromotion : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Promotion")
	FFF_PrepPromotionQuote GetQuote() const { return Quote; }

	bool UsesCompactLayout() const { return bUseCompactLayout; }

	/** 저장된 제목 형식에 실제 홍보 등급을 넣는다. 확인창·미리보기가 같은 문구를 쓴다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Promotion")
	FText GetPurchaseConfirmationTitle(EFF_PromotionChoice Choice) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION()
	void HandlePurchaseClicked();

	/** 카드 클릭. `ChoiceId` 는 `EFF_PromotionChoice` 를 캐스팅한 값이다. */
	void HandleCardClicked(int32 ChoiceId);

	// --- 확인창 ---

	UFUNCTION()
	void HandlePurchaseConfirmed();

	UFUNCTION()
	void HandlePurchaseCancelled();

	// --- 결과 이벤트 ---

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleBuzzChanged(float NewBuzz, float PreviousBuzz);

	UFUNCTION()
	void HandleWeatherChanged(EFF_WeatherType NewWeather);

	UFUNCTION()
	void HandlePurchasedPromotionChanged(EFF_PromotionChoice NewChoice);

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

protected:
	// --- 위젯 바인딩. 전부 Optional 이다 ---

	/** 실제 등급 목록이 들어갈 자리. `WrapBox` 를 권한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UPanelWidget> Box_PromotionCards;

	/** 예상 방문객과 실제 입장 인원 연동 범위를 설명하는 안내. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_NotAppliedNotice;

	/** 축제 전체에 한 번 적용된다는 안내. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_OnceNotice;

	/** 카드마다 반복하지 않고 한 번만 표시하는 예상 방문객 기준. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_ForecastBasis;

	/** `1,200 Gold · 방문객 ×1.18`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_CostPreview;

	/** `구매 후 잔액 6,400`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_GoldAfter;

	/** 새 요약 영역. 기존 견적 바인딩은 이전 WBP 호환용으로 유지한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_SelectedPromotion;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_PurchaseCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_GoldAfterValue;

	/** 구매 전/완료 후의 잔액 의미를 구분한다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_GoldAfterLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UButton> Button_Purchase;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_PurchaseAction;

	/** 못 누르는 이유. **비활성 흐려짐만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Promotion")
	TObjectPtr<UTextBlock> Text_BlockReason;

	// --- 설정 ---

	/** 간소화 WBP에만 적용한다. 기존 화면은 원래 견적 표시를 유지한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion")
	bool bUseCompactLayout = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion")
	TSubclassOf<UFF_PrepPromotionCard> CardWidgetClass;

	/** `CardWidgetClass` 가 비었을 때 쓸 경로. 생성기의 패키지 이름과 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion")
	FString CardWidgetFallbackPath = TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepPromotionCard");

	/** 확인창 클래스. 비면 확인창 없이 바로 사지 않고 **거절한다** — 되돌릴 수 없는 결정이다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion")
	FString ConfirmDialogPath = TEXT("/Game/FestaFiesta/UI/Frontend/WBP_FF_ConfirmDialog");

	/** {0}: 구매하려는 등급 이름. 백틱은 FText 이스케이프이므로 인용부호로 쓰지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText ConfirmTitleFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "ConfirmTitleFormat", "‘{0}’ 홍보를 구매할까요?");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText NoPromotionSelectedText = NSLOCTEXT("FestaFiestaPrepPromotion", "NoPromotionSelected", "등급을 선택하세요");

	/** {0}: 선택한 등급 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText SelectedPromotionFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "SelectedPromotionFormat", "{0}");

	/** {0}: 이미 구매한 등급 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText PurchasedPromotionFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "PurchasedPromotionFormat", "{0} · 구매 완료");

	/** {0}: 실제 홍보비. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText PurchaseCostFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "PurchaseCostFormat", "{0} Gold");

	/** {0}: 구매 후 예상 잔액 또는 구매 완료 후 현재 잔액. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText GoldAfterValueFormat = NSLOCTEXT("FestaFiestaPrepPromotion", "GoldAfterValueFormat", "{0} Gold");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText GoldAfterLabel = NSLOCTEXT("FestaFiestaPrepPromotion", "GoldAfterLabel", "구매 후 잔액");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText CurrentGoldLabel = NSLOCTEXT("FestaFiestaPrepPromotion", "CurrentGoldLabel", "현재 잔액");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText MissingValueText = NSLOCTEXT("FestaFiestaPrepPromotion", "MissingValue", "—");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText SelectedBadgeText = NSLOCTEXT("FestaFiestaPrepPromotion", "SelectedBadge", "✓ 선택됨");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText OnceNoticeText = NSLOCTEXT("FestaFiestaPrepPromotion", "OnceNotice", "축제당 1회, 셋업에서 구매할 수 있으며 3일 전체에 적용됩니다.");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Promotion|Text")
	FText NotAppliedNoticeText = NSLOCTEXT("FestaFiestaPrepPromotion", "NotAppliedNotice", "홍보 효과는 예상 방문객에 반영됩니다. 실제 입장 인원은 운영 상황에 따라 달라집니다.");

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	void RebuildCards();
	void RefreshAll();

	/** 지금 고른 등급의 데이터. 없으면 `nullptr`. */
	const FFF_PromotionOptionView* FindSelectedOption() const;

	/** 확인창을 띄운다. 못 띄우면 false — 그때는 구매를 진행하지 않는다. */
	bool OpenConfirmDialog();

	/** 확인창 구독을 끊는다. **결과를 받은 시점에도 부른다** — 풀에서 재사용되기 때문이다. */
	void ReleaseConfirmDialog();

	FText BuildBlockReasonText() const;

	/** 등급 이름. enum `DisplayName` 을 런타임에 읽지 않기 위해 여기서 만든다. */
	static FText GetChoiceLabel(EFF_PromotionChoice Choice);

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY()
	TArray<TObjectPtr<UFF_PrepPromotionCard>> CardWidgets;

	/**
	 * 띄워 둔 확인창. **약참조다** — CommonUI 스택이 수명을 소유하고 풀에서 재사용한다.
	 * 여기서 강참조로 들면 스택이 지운 뒤에도 살아 있게 된다.
	 */
	TWeakObjectPtr<UFF_ConfirmDialog> PendingConfirm;

	TArray<FFF_PromotionOptionView> Options;

	/** 고른 등급. 안 골랐으면 `None` 이 아니라 `SelectedIndex == INDEX_NONE` 이다 —
	 *  `안 함` 을 고른 것과 구분해야 한다. */
	int32 SelectedIndex = INDEX_NONE;

	FFF_PrepPromotionQuote Quote;
};
