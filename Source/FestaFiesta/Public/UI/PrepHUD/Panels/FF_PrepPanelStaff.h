// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "FF_PrepPanelStaff.generated.h"

class UButton;
class UTextBlock;
class UWidget;
class UFF_FestivalViewModelSubsystem;

/**
 * 알바를 더 고용할 수 없는 이유.
 *
 * !! 하나로 합치지 않는다 !!
 * `상한을 다 썼다` 와 `돈이 없다` 는 플레이어가 할 행동이 완전히 다르다 — 앞은 내일까지
 * 기다려야 하고 뒤는 돈을 벌거나 다른 지출을 줄여야 한다. 버튼을 흐리게만 하면 둘을 구분할
 * 수 없다 (AT-011).
 */
UENUM(BlueprintType)
enum class EFF_PrepHireBlockReason : uint8
{
	/** 고용할 수 있다. */
	None			UMETA(DisplayName = "가능"),

	/** 아직 값을 못 읽었다. 축제가 시작되기 전이면 정상이다. */
	Unknown			UMETA(DisplayName = "모름"),

	/** 오늘 상한을 다 썼다. 돈이 있어도 안 된다. */
	CapReached		UMETA(DisplayName = "오늘 상한 도달"),

	/** 상한은 남았지만 임금을 낼 잔액이 없다. */
	NotEnoughGold	UMETA(DisplayName = "잔액 부족")
};

/**
 * 고용 견적 한 벌. 화면은 이 값을 그대로 그리고 계산을 하지 않는다.
 */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepHireQuote
{
	GENERATED_BODY()

	/** 지금 고를 수 있는 최대 인원. 0 이면 `BlockReason` 이 이유를 갖는다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 MaxHireable = 0;

	/** 스텝퍼가 실제로 가리키는 인원. 상한을 넘겨 요청해도 여기서 잘린다. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 SelectedCount = 0;

	/** `SelectedCount` 명의 하루치 총 임금. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 WageTotal = INDEX_NONE;

	/** 고용 후 잔액. 모르면 `INDEX_NONE`. */
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	int32 GoldAfterHire = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|UI|Prep")
	EFF_PrepHireBlockReason BlockReason = EFF_PrepHireBlockReason::Unknown;

	/** 고용 버튼을 켤지. 인원이 1 이상이고 막힌 이유가 없을 때만. */
	bool CanHire() const { return SelectedCount > 0 && BlockReason == EFF_PrepHireBlockReason::None; }
};

/**
 * 인력 패널의 판단 규칙. **순수 함수만 둔다.**
 *
 * === 왜 위젯 밖으로 뺐나 ===
 * "상한은 남았는데 돈이 없다" 같은 조합은 **에디터에서 그 잔액을 실제로 만들어야만** 재현된다.
 * 여기 있으면 자동화 테스트가 못으로 박는다. U4 의 `FFF_PrepHudRules` 와 같은 이유다.
 *
 * !! 여기서 고용 허용 여부를 최종 판정하지 않는다 !!
 * 화면 표시용 계산이고, 실제 허용은 `UFF_FestivalProgressionSubsystem::TryHireTemporaryStaff`
 * 가 정한다. 두 곳에서 판정하면 규칙이 갈라진다 (UI 규칙 2).
 */
struct FESTAFIESTA_API FFF_PrepStaffRules
{
	/**
	 * 막힌 이유를 고른다.
	 *
	 * ★ **상한을 잔액보다 먼저 본다.** 상한을 다 쓴 상태에서 `돈을 모으세요` 라고 하면
	 *   돈을 모아도 안 되는 행동을 안내하게 된다.
	 *
	 * @param Affordable   `GetAffordableTemporaryStaffCount()` — 잔액·상한을 둘 다 반영한 값
	 * @param RemainingCap `GetRemainingTemporaryStaffCap()` — 상한만 본 값
	 *                     둘 중 하나라도 `INDEX_NONE` 이면 `Unknown`
	 */
	static EFF_PrepHireBlockReason ResolveBlockReason(int32 Affordable, int32 RemainingCap);

	/**
	 * 견적을 만든다. 값이 하나라도 없으면 금액 칸을 `INDEX_NONE` 으로 둔다 —
	 * **0 을 넣지 않는다.** `0 Gold` 는 "공짜"라는 틀린 사실이 된다.
	 *
	 * @param DesiredCount 스텝퍼가 원하는 인원. 0 미만이나 `MaxHireable` 초과면 잘린다
	 */
	static FFF_PrepHireQuote BuildQuote(
		int32 DesiredCount,
		int32 Affordable,
		int32 RemainingCap,
		int32 WagePerStaff,
		int32 Gold);
};

/**
 * 준비 단계 `인력` 패널.
 *
 * === U5 에서 가장 먼저 만든 패널이다 ===
 * 필요한 API 가 **전부 있는 유일한 패널**이라 더미가 하나도 없다. 패널 베이스와 하단 바의
 * 배타 규칙이 실제로 도는지 여기서 처음 검증된다.
 *
 * 구성 —
 *   현재 인원 `6명 (기본 6 · 알바 0)` · 오늘 최대 `12명`
 *   알바 고용 `[-] 3 [+]` → `3명 × 150 = 450 Gold` · `고용 후 잔액 7,150`
 *   고정비 표 `장소 임대료 2,400 / 알바 임금 450 / (미납 있으면) 미납 2,400`
 *   ★ `알바는 하루짜리입니다. 매일 다시 고용해야 합니다.`
 *
 * === ★ 하루짜리라는 것을 반드시 화면에 쓴다 ===
 * 안 쓰면 다음 날 인원이 줄어 있는 이유를 플레이어가 알 수 없다. 기본 팀만 무페이로 남는다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Prep Panel - Staff"))
class FESTAFIESTA_API UFF_PrepPanelStaff : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	/** 현재 견적. 표시용이고 이 값으로 고용을 막지 않는다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Staff")
	FFF_PrepHireQuote GetQuote() const { return Quote; }

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	// --- 버튼 ---

	UFUNCTION()
	void HandleDecreaseClicked();

	UFUNCTION()
	void HandleIncreaseClicked();

	UFUNCTION()
	void HandleHireClicked();

	// --- 결과 이벤트 ---

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleStaffCountChanged(int32 NewValue);

	/**
	 * 가용 인원만 바뀐 경우 — 다시 그리기만 한다.
	 *
	 * !! `HandleStaffCountChanged` 를 재사용하면 안 된다 !! 그쪽은 "고용이 성공했다"로 보고
	 * 스텝퍼 선택을 초기화한다. 다른 패널에서 프로그램을 편성했을 뿐인데 이 화면에서 고르던
	 * 인원이 조용히 사라지면 안 된다.
	 */
	UFUNCTION()
	void HandleAvailableStaffCountChanged(int32 NewValue);

	UFUNCTION()
	void HandleArrearsChanged(int32 NewValue);

protected:
	// --- 위젯 바인딩. 전부 Optional 이다 ---
	// 목업의 일부만 만든 WBP 도 통과시킨다. 빠진 것은 조용히 넘기지 않고 로그로 알린다.

	/** `6명 (기본 6 · 알바 0)`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_CurrentStaff;

	/** `오늘 최대 12명`. 상한을 모르면 숨긴다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_MaxStaff;

	// --- 스텝퍼 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UButton> Button_Decrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UButton> Button_Increase;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_SelectedCount;

	// --- 견적 ---

	/** `3명 × 150 = 450 Gold`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_WagePreview;

	/** `고용 후 잔액 7,150`. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_GoldAfterHire;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UButton> Button_Hire;

	/** 못 누르는 이유. **비활성 흐려짐만으로는 알 수 없다** (AT-011). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_HireBlockReason;

	// --- 고정비 표 ---

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_RentCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_WageCost;

	/** 미납이 0 이면 줄째 숨긴다. `미납 0` 은 없는 문제를 있는 것처럼 만든다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_ArrearsCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UWidget> Box_ArrearsRow;

	/** 하루짜리라는 안내. 문구를 바꿀 수 있게 값으로 뺀다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Staff")
	TObjectPtr<UTextBlock> Text_DailyNotice;

	// --- 설정 ---

	/**
	 * ★ 이 문구를 비우지 않는다. 안 쓰면 다음 날 인원이 줄어 있는 이유를 알 수 없다.
	 * WBP 에 박지 않고 값으로 두는 이유는 현지화와 문구 수정 때문이다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Staff")
	FText DailyNoticeText;

	/** 패널을 열 때 스텝퍼 기본값. 1 이면 바로 누를 수 있는 상태로 시작한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Staff", meta = (ClampMin = "0"))
	int32 DefaultSelectedCount = 1;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	/** 견적을 다시 만들고 화면 전체를 다시 그린다. 부분 갱신하지 않는다. */
	void RefreshAll();

	/** 스텝퍼를 움직인다. 상한을 넘어가면 `BuildQuote` 가 잘라 준다. */
	void ChangeSelection(int32 Delta);

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);
	static void SetGroupTextOrHide(UWidget* Group, UTextBlock* TextBlock, const FText& Value);

	UPROPERTY()
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	/**
	 * 플레이어가 고른 인원. **견적의 `SelectedCount` 와 다를 수 있다** —
	 * 잔액이 줄면 견적은 잘리지만 원하던 값은 유지해 두었다가 잔액이 회복되면 되돌린다.
	 */
	int32 DesiredCount = 1;

	FFF_PrepHireQuote Quote;
};
