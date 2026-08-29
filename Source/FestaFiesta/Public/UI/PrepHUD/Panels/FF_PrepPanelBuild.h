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
