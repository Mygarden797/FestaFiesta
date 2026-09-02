// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "UI/ClosingHUD/FF_ClosingHudTypes.h"
#include "FF_ClosingHudScreen.generated.h"

class UButton;
class UFF_ClosingPanelBase;
class UFF_ClosingPlaceholderPanel;
class UFF_FestivalViewModelSubsystem;
class UFF_LiveTabButton;
class UPanelWidget;
class UTextBlock;
class UWidget;

/**
 * 마감·리포트 단계의 하단 바.
 *
 * === 2026-09-02 결정 — 화면 세 장 대신 바 하나 ===
 * 원래 계획은 `마감`·`리포트`·`피날레` 를 각각 전체 화면으로 만드는 것이었다. 실제로
 * 필요한 것은 "하루가 끝났을 때 결과를 본다" 하나뿐이라, 준비·라이브와 **같은 모양**으로
 * 줄인다 — 바 하나에 탭 둘(`리포트`·`피날레`), 탭 하나당 패널 하나.
 *
 * === 마감과 리포트가 같은 화면인 이유 ===
 * 두 단계에서 볼 것이 같다. 다른 것은 **`다음 날로` 를 누를 수 있는가** 하나뿐이고,
 * 그것은 `AdvanceToNextDay()` 가 리포트 단계 밖에서 무시되기 때문이다. 화면을 나누면
 * 같은 표를 두 번 그리게 된다.
 *
 * 🔴 `Finale` 단계에는 **뜨지 않는다.** 거기는 불꽃이 터지는 연출 구간이다.
 *
 * === CommonUI 를 쓰지 않는다 ===
 * 준비·라이브 바와 같은 이유다. 이 바는 입력 모드를 가져가면 안 된다.
 */
UCLASS(Abstract, meta = (DisplayName = "FF Closing HUD Screen"))
class FESTAFIESTA_API UFF_ClosingHudScreen : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 열려 있는 패널을 닫는다. **패널이 스스로 닫지 않는다** — 여기가 유일한 경로다. */
	void CloseCurrentPanel();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Closing")
	FFF_ClosingTabSelection GetSelection() const { return Selection; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Closing")
	EFF_ClosingCtaState GetCtaState() const { return CtaState; }

	/**
	 * 열려 있는 패널을 슬레이트 단위만큼 옮긴다. 패널이 자기 빈 곳을 끌 때 부른다.
	 *
	 * 🔴 **자리는 화면이 소유한다.** 패널이 자기 슬롯을 직접 만지면 탭을 닫았다 열 때
	 * `ApplyPanelLayout` 이 덮어써서 옮긴 자리가 사라진다 (준비 단계와 같은 판단).
	 */
	void NudgePanelPosition(FVector2D SlateDelta);

	bool IsPanelDragAllowed() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStageViewChanged(EFF_FestivalStage NewStage, int32 NewDay);

	UFUNCTION()
	void HandleGoldChanged(int32 NewValue);

	UFUNCTION()
	void HandleRemainingSecondsChanged(int32 NewValue);

	UFUNCTION()
	void HandleFinaleGradeChanged(EFF_FinaleGrade NewGrade);

	UFUNCTION()
	void HandleCtaClicked();

protected:
	// === 탭 ===
	//
	// 라이브 바의 탭 버튼(`UFF_LiveTabButton`)을 그대로 쓴다. 생김새와 선택 표시가 같아야
	// 하는데 클래스를 하나 더 만들면 둘이 조용히 갈라진다.

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Report;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Tabs")
	TObjectPtr<UFF_LiveTabButton> Tab_Finale;

	/** 열린 패널이 들어가는 자리. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing")
	TObjectPtr<UPanelWidget> Panel_PanelHost;

	// === 상태 행 ===

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Status")
	TObjectPtr<UTextBlock> Text_DayStage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Status")
	TObjectPtr<UTextBlock> Text_Gold;

	/**
	 * 이 단계가 끝나기까지 남은 시간.
	 *
	 * 🔴 마감·리포트도 **시간이 흐르고 자동으로 넘어간다.** 남은 시간을 안 보여 주면
	 * `남은 마감 시간이 지나면 리포트로 넘어갑니다` 라는 안내가 언제까지인지 알 수 없는
	 * 빈말이 된다 (2026-09-02).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Status")
	TObjectPtr<UTextBlock> Text_Remaining;

	// === CTA ===

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Cta")
	TObjectPtr<UButton> Button_Cta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Cta")
	TObjectPtr<UTextBlock> Text_CtaLabel;

	/** !! 흐려짐만으로는 왜 못 누르는지 알 수 없다 !! (AT-011) 막힌 이유를 여기 적는다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Closing|Cta")
	TObjectPtr<UTextBlock> Text_CtaReason;

	// === 설정 ===

	/** 이름 규칙에서 벗어나는 패널만 여기 넣는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Panels")
	TMap<EFF_ClosingTab, TSoftClassPtr<UFF_ClosingPanelBase>> PanelClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Panels")
	FString PanelClassFolder = TEXT("/Game/FestaFiesta/UI/ClosingHUD/Panels");

	/** `{0}` 에 `EFF_ClosingTab` 의 항목 이름이 들어간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Panels")
	FString PanelClassNameFormat = TEXT("WBP_FF_ClosingPanel{0}");

	/**
	 * 아직 안 만든 탭에 띄울 패널.
	 *
	 * 🔴 **기본값을 여기서 준다.** 비워 두면 미구현 탭이 무반응이 되어 버튼이 고장난 것처럼
	 * 보인다 (라이브 HUD 에서 실제로 밟았다).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Panels")
	TSoftClassPtr<UFF_ClosingPlaceholderPanel> PlaceholderPanelClass = TSoftClassPtr<UFF_ClosingPlaceholderPanel>(
		FSoftObjectPath(TEXT("/Game/FestaFiesta/UI/ClosingHUD/Panels/WBP_FF_ClosingPlaceholderPanel.WBP_FF_ClosingPlaceholderPanel_C")));

	/**
	 * 패널 크기(px)와 자리.
	 *
	 * 🔴 **두 축 다 점 앵커 + 명시 크기다.** 늘어난 축을 쓰면 크기를 부모가 정하는데
	 * HUD 루트가 화면 높이를 못 받아 음수 높이가 된다 (`docs/memory/prep-hud-reference.md`).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Layout", meta = (ClampMin = "1"))
	FVector2D PanelSize = FVector2D(1080.0, 620.0);

	/** 하단 바 중 상태 행의 높이(px). 패널 아래 끝이 탭 줄 밑으로 들어가게 한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Layout")
	float StatusRowHeight = 29.f;

	/** 패널의 빈 곳을 끌어 자리를 옮길 수 있게 할지. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Closing|Layout")
	bool bAllowPanelDrag = true;

private:
	UFF_FestivalViewModelSubsystem* GetViewModel() const;

	void BindViewModel();
	void UnbindViewModel();

	void ForEachTabButton(TFunctionRef<void(EFF_ClosingTab, UFF_LiveTabButton&)> Visitor) const;

	void HandleTabButtonClicked(int32 TabId);
	void ToggleTab(EFF_ClosingTab Tab);
	void ApplySelection(const FFF_ClosingTabSelection& NewSelection);

	UFF_ClosingPanelBase* EnsurePanel(EFF_ClosingTab Tab);
	UClass* ResolvePanelClass(EFF_ClosingTab Tab, const FString& EnumName, bool& bOutIsPlaceholder) const;
	void ApplyPanelLayout();

	void RefreshAll();
	void RefreshTabs();
	void RefreshStatusRow();
	void RefreshCta();

	/** 피날레를 샀는가. 탭을 숨길지 정한다. */
	bool HasPurchasedFinale() const;

	static void SetTextOrHide(UTextBlock* TextBlock, const FText& Value);

	UPROPERTY(Transient)
	TObjectPtr<UFF_FestivalViewModelSubsystem> CachedViewModel;

	UPROPERTY(Transient)
	TMap<EFF_ClosingTab, TObjectPtr<UFF_ClosingPanelBase>> PanelWidgets;

	FFF_ClosingTabSelection Selection;

	EFF_ClosingCtaState CtaState = EFF_ClosingCtaState::Hidden;

	/** 끌어서 옮긴 양. 탭을 닫았다 열어도 유지된다. */
	FVector2D PanelOffset = FVector2D::ZeroVector;
};
