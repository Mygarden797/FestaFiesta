// LightTeal PrepHUD facility catalog and placement-state panel.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Styling/SlateTypes.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationTypes.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "FF_PrepPanelBuild.generated.h"

class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UWidget;
class UFF_BuildCatalogEconomyReadModel;
class UFF_BuildCatalogSubsystem;
class UFF_BuildModeSubsystem;
class UFF_PrepChoiceButton;
class UFF_PrepBuildPresentationModel;
class UFF_PrepBuildTileView;
class UFFZSWorldSubsystem;

namespace FFPrepBuildPauseModel
{
	inline bool ShouldHandlePauseMenuKey(
		const FKey& Key,
		const bool bIsRepeat,
		const bool bPanelOpen,
		const bool bPanelClosing,
		const EFF_BuildModeState State,
		const TArray<FKey>& OpenCloseKeys)
	{
		return !bIsRepeat && bPanelOpen && !bPanelClosing &&
			State == EFF_BuildModeState::CatalogOpen && OpenCloseKeys.Contains(Key);
	}
}

/**
 * PrepHUD's production build panel.
 *
 * Domain state and placement remain owned by BuildCatalog/BuildMode. This class opts into the
 * external catalog presentation seam, owns a virtualized presentation model, and never ticks.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "FF Prep Panel - Build"))
class FESTAFIESTA_API UFF_PrepPanelBuild : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	UFF_PrepPanelBuild(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(
		const FGeometry& InGeometry,
		const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnPanelOpened() override;
	virtual void NativeOnPanelClosed() override;

	UFUNCTION()
	void HandleCatalogChanged();

	UFUNCTION()
	void HandleCatalogThumbnailsChanged();

	UFUNCTION()
	void HandleGoldChanged(int32 NewGold);

	UFUNCTION()
	void HandleBuildModeStateChanged(EFF_BuildModeState PreviousState, EFF_BuildModeState NewState);

	UFUNCTION()
	void HandleBuildCatalogFocusRestoreRequested();

	UFUNCTION()
	void HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleZoneWorldSnapshotRestored();

	UFUNCTION()
	void HandleReturnToCatalogClicked();

	UFUNCTION()
	void HandleExitBuildModeClicked();

	UFUNCTION()
	void HandleFilterAllClicked();

	UFUNCTION()
	void HandleFilterBoothClicked();

	UFUNCTION()
	void HandleFilterPerformanceClicked();

	UFUNCTION()
	void HandleSearchTextChanged(const FText& NewText);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UWidget> Panel_Catalog;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UWidget> Panel_PlacementCompact;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UFF_PrepBuildTileView> TileView_Facilities;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Gold;

	/**
	 * 제목·보유 금화 줄.
	 *
	 * 🔴 배치 중에는 **접는다.** 시설을 놓는 동안 화면을 가리면 안 되고, 그때 제목과
	 * 금화는 볼 일이 없다 — 목록에서 이미 보고 들어왔다 (2026-09-03 백종태 요청).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UWidget> Box_Header;

	/**
	 * 패널 바깥 테두리.
	 *
	 * 🔴 배치 중에는 **상하 여백을 0 으로** 만든다. 여백 14 를 남겨 두면 흰 판이
	 * 탭 줄보다 위아래로 튀어나와, 초록 칩이 흰 배경의 아래쪽에 붙은 것처럼 보인다
	 * (2026-09-03 백종태 지적).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<class UBorder> Border_PanelRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_ItemCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_PlacementGuide;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_ReturnToCatalog;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_ExitBuildMode;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterAll;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterBooth;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterPerformance;

	/** 기존 WBP를 수정하지 않고 전체 카테고리 스크롤 행을 삽입하는 런타임 호스트입니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UHorizontalBox> HorizontalBox_Tools;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UEditableTextBox> Search_Facilities;

	/** PrepHUD 공용 LightTeal 선택 칩. 설정하지 않으면 아래 안전한 프로젝트 경로를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	TSubclassOf<UFF_PrepChoiceButton> CategoryChoiceClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	FString CategoryChoiceFallbackPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepChoiceButton");

private:
	void HandleTileClicked(UObject* ItemObject);
	void HandleCategoryChoiceClicked(int32 ChoiceId);
	void BindExternalSources();
	void UnbindExternalSources();
	void EndPanelSession();
	void BuildCategoryFilterSurface();
	void RefreshCategoryChoiceSelection();
	void RefreshLegacyCategoryButtonStyles();
	void FocusSelectedCategoryChoice();
	void RefreshPricedEntries();
	void ReconcilePresentation();
	void ApplyPresentation(bool bReplaceListItems = true);
	void ApplyBuildModeState(EFF_BuildModeState State);

	/** 카탈로그를 펼쳤을 때의 패널 높이(px). `FF_PrepHudBar` 의 `PanelLayouts` 표와 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build", meta = (ClampMin = "0.0"))
	float CatalogHeight = 320.f;

	/**
	 * 배치 중(목록을 접었을 때)의 패널 높이(px).
	 *
	 * 🔴 **작게 유지한다.** 배치 중에는 지면이 보여야 시설을 놓을 수 있는데, 안내 네 줄
	 * 아래로 빈 흰 판이 남으면 그만큼 화면을 가린다 (2026-09-03 백종태 지적).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build", meta = (ClampMin = "0.0"))
	// 🔴 탭 줄과 **겹치지 않게** 그 위에 올린다 (`CompactOffsetY` 참고).
	//   겹치면 흰 띠 안에 초록 칩이 들어가고, 띠를 키울수록 칩이 아래로 밀린다 —
	//   아래는 상태 행이라 늘릴 수 없어서 가운데로 맞출 방법이 없다 (2026-09-03).
	//   겹치지 않게 두면 그 문제 자체가 사라지고 높이를 자유롭게 잡을 수 있다.
	float CompactHeight = 32.f;

	/**
	 * 배치 중 띠를 위로 더 올리는 양(px). 기본값은 탭 줄 높이만큼이라 **바 바로 위**에 붙는다.
	 *
	 * 패널은 원래 아래 끝을 탭 줄 밑(`StatusRowHeight`)에 맞춘다. 여기에 이 값이 더해진다 —
	 * `-29`(탭 줄 높이)를 주면 바 전체를 피해 그 위에 얹힌다.
	 *
	 * 0 으로 두면 예전처럼 탭 줄과 겹친다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build")
	float CompactOffsetY = -29.f;
	void SetCategoryFilter(bool bEnabled, EFFZSFacilityCategory Category);
	TArray<FFF_BuildCatalogEntry> MakePricedEntries() const;

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildCatalogSubsystem> CatalogSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildCatalogEconomyReadModel> EconomyReadModel;

	UPROPERTY(Transient)
	TObjectPtr<UFF_BuildModeSubsystem> BuildModeSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UFFZSWorldSubsystem> ZoneSystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_PrepBuildPresentationModel> PresentationModel;

	UPROPERTY(Transient)
	TArray<FFF_BuildCatalogEntry> PricedEntries;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RuntimeCategoryScroll;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> RuntimeCategoryBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PrepChoiceButton>> CategoryChoices;

	FFF_PrepBuildFilter Filter;
	FButtonStyle LegacySelectedCategoryStyle;
	FButtonStyle LegacyIdleCategoryStyle;
	bool bPanelOpen = false;
	bool bClosingPanel = false;
	bool bHasLegacyCategoryStyles = false;
};
