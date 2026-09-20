// LightTeal PrepHUD facility catalog and placement-state panel.

#pragma once

#include "CoreMinimal.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Festival/FF_RequiredFacilityTypes.h"
#include "Styling/SlateTypes.h"
#include "UI/PrepHUD/Build/FF_PrepBuildPresentationTypes.h"
#include "UI/PrepHUD/FF_PrepPanelBase.h"
#include "FF_PrepPanelBuild.generated.h"

class UButton;
class UEditableTextBox;
class UHorizontalBox;
class UVerticalBox;
class UScrollBox;
class UTextBlock;
class UWidget;
class UFF_BuildCatalogEconomyReadModel;
class UFF_BuildCatalogSubsystem;
class UFF_BuildModeSubsystem;
class UFF_BuildFavoritesSubsystem;
class UFF_RequiredFacilitiesSubsystem;
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
 * external catalog presentation seam and owns a virtualized presentation model. Its tick only
 * adjusts page tile width when the actual list geometry changes; gameplay remains event driven.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "FF Prep Panel - Build"))
class FESTAFIESTA_API UFF_PrepPanelBuild : public UFF_PrepPanelBase
{
	GENERATED_BODY()

public:
	UFF_PrepPanelBuild(const FObjectInitializer& ObjectInitializer);

	/** Zero-based local page selection. It never changes filtering or starts placement. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build|Pages")
	void SetCatalogPage(int32 PageIndex);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build|Pages")
	void NextCatalogPage();

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build|Pages")
	void PreviousCatalogPage();

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Pages")
	int32 GetCatalogPageIndex() const;

	/** Empty results have zero pages; their page index remains zero. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Pages")
	int32 GetCatalogPageCount() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Pages")
	TArray<UObject*> GetCatalogPageItems() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Pages")
	bool CanGoToPreviousPage() const;

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Pages")
	bool CanGoToNextPage() const;

	/** Reveal one card once, clearing filters that would hide it. Never starts placement. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build")
	void RequestFacilityFocus(FName FacilityDefinitionId);

	/** 설치 조건을 충족하면 안내와 그 안내의 여백을 함께 접습니다. 상태를 변경하지 않는 표현 함수입니다. */
	void RefreshRequiredFacilityGuide(const FFF_RequiredFacilitiesStatus& Status);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
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
	void HandleSynergyCatalogContextChanged();

	UFUNCTION()
	void HandleCatalogFacilityFocusRequested(FName FacilityDefinitionId);

	UFUNCTION()
	void HandleRequiredFacilitiesChanged();

	UFUNCTION()
	void HandleZoneWorldChanged(const FFFZSWorldChangeEvent& Event);

	UFUNCTION()
	void HandleZoneWorldSnapshotRestored();

	UFUNCTION()
	void HandleReturnToCatalogClicked();

	UFUNCTION()
	void HandleExitBuildModeClicked();

	UFUNCTION()
	void HandleInspectFacilitiesClicked();

	UFUNCTION()
	void HandleFilterAllClicked();

	UFUNCTION()
	void HandleFilterBoothClicked();

	UFUNCTION()
	void HandleFilterPerformanceClicked();

	UFUNCTION()
	void HandleSearchTextChanged(const FText& NewText);

	UFUNCTION()
	void HandleFavoritesOnlyClicked();

	UFUNCTION()
	void HandleResetFiltersClicked();

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

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<class UBorder> Border_CatalogRequiredGuide;

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
	TObjectPtr<UButton> Button_PreviousPage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_NextPage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_PageIndex;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Pages", meta = (ClampMin = "1"))
	int32 CardsPerPage = 6;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Pages")
	bool bUsePagination = true;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_PlacementGuide;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_ReturnToCatalog;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_ExitBuildMode;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_InspectFacilities;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterAll;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterBooth;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FilterPerformance;

	/** 기존 WBP를 수정하지 않고 전체 카테고리 스크롤 행을 삽입하는 런타임 호스트입니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UHorizontalBox> HorizontalBox_Tools;

	/** Designer-owned second level; populated from the selected major category's catalog data. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UHorizontalBox> HorizontalBox_Subcategories;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UScrollBox> ScrollBox_Subcategories;

	/** Designer-owned major-category row. Legacy assets alone use the migration fallback. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UHorizontalBox> HorizontalBox_Categories;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_FavoritesOnly;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_FavoritesOnly;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_FavoriteCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UWidget> Panel_Empty;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_EmptyTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_EmptyHint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_ResetFilters;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Category")
	FMargin CategoryItemPadding = FMargin(0, 0, 5, 0);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_CategoryBreadcrumb;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UEditableTextBox> Search_Facilities;

	/** PrepHUD 공용 LightTeal 선택 칩. 설정하지 않으면 아래 안전한 프로젝트 경로를 사용합니다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	TSubclassOf<UFF_PrepChoiceButton> CategoryChoiceClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FestaFiesta|UI|Prep|Build")
	FString CategoryChoiceFallbackPath =
		TEXT("/Game/FestaFiesta/UI/PrepHUD/WBP_FF_PrepChoiceButton");

private:
	void HandleFavoritesChanged();
	void HandleTileClicked(UObject* ItemObject);
	void HandleCategoryChoiceClicked(int32 ChoiceId);
	void HandleMinorChoiceClicked(int32 ChoiceId);
	void RefreshMinorCategorySurface();
	void BindExternalSources();
	void UnbindExternalSources();
	void EndPanelSession();
	void BuildCategoryFilterSurface();
	void RefreshCategoryChoiceSelection();
	void RefreshLegacyCategoryButtonStyles();
	void FocusSelectedCategoryChoice();
	void RefreshPricedEntries();
	void ReconcilePresentation();
	void RefreshVisibleTiles();
	void ApplyPresentation(bool bReplaceListItems = true);
	void ApplyBuildModeState(EFF_BuildModeState State);
	void RevealSynergyRecommendations();
	void RefreshRequiredFacilitiesPresentation();
	void ApplyRequiredFacilitiesSnapshot();

	/** 카탈로그를 펼쳤을 때의 패널 높이(px). `FF_PrepHudBar` 의 `PanelLayouts` 표와 같아야 한다. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build", meta = (ClampMin = "0.0"))
	float CatalogHeight = 392.f;

	/** The teammate bar docks panels above its 29px status row; -41 reserves the remaining tab row and gap. */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build")
	float CatalogOffsetY = -41.f;

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
	float CompactHeight = 42.f;

	/**
	 * 배치 중 띠를 위로 더 올리는 양(px). 기본값은 탭 줄 높이만큼이라 **바 바로 위**에 붙는다.
	 *
	 * 패널은 원래 아래 끝을 탭 줄 밑(`StatusRowHeight`)에 맞춘다. 여기에 이 값이 더해진다 —
	 * `-29`(탭 줄 높이)를 주면 바 전체를 피해 그 위에 얹힌다.
	 *
	 * 0 으로 두면 예전처럼 탭 줄과 겹친다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "FestaFiesta|UI|Prep|Build")
	float CompactOffsetY = -41.f;
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
	TObjectPtr<UFF_BuildFavoritesSubsystem> FavoritesSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UFF_RequiredFacilitiesSubsystem> RequiredFacilitiesSubsystem;

	FFF_RequiredFacilitiesStatus RequiredFacilitiesSnapshot;
	FTimerHandle RequiredFacilitiesRefreshTimer;
	bool bRequiredFacilitiesDirty = true;

	UPROPERTY(Transient)
	TArray<FFF_BuildCatalogEntry> PricedEntries;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RuntimeCategoryScroll;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RuntimeCategoryBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PrepChoiceButton>> CategoryChoices;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFF_PrepChoiceButton>> MinorChoices;

	TArray<FName> MinorTokens;

	FFF_PrepBuildFilter Filter;
	int32 CatalogPageIndex = 0;
	float LastResponsiveListWidth = -1.0f;
	int32 LastResponsiveColumnCount = 0;
	FButtonStyle LegacySelectedCategoryStyle;
	FButtonStyle LegacyIdleCategoryStyle;
	FButtonStyle FavoritesFilterStyle;
	bool bHasFavoritesFilterStyle = false;
	bool bPanelOpen = false;
	bool bClosingPanel = false;
	bool bHasLegacyCategoryStyles = false;
	bool bRevealSynergyOnCatalogOpen = false;
	FName PendingCatalogFocusId;
	bool bApplyingCatalogFocus = false;
};
