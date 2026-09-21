// Event-driven UMG collection screen. All layout is authored in the saved WBP.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Building/Codex/FF_FacilityCodexTypes.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "FF_FacilityCodexScreen.generated.h"

class UButton;
class UBorder;
class UComboBoxString;
class UEditableTextBox;
class UProgressBar;
class UFF_FacilityCodexImage;
class UTextBlock;
class USizeBox;
class UStackBox;
class UWidgetSwitcher;
class UFF_FacilityCodexItem;
class UFF_FacilityCodexSubsystem;
class UFF_FacilityCodexTileView;
class UFF_FacilityCodexPreview;

UCLASS(Abstract, Blueprintable, meta=(DisplayName="FF Facility Codex Screen"))
class FESTAFIESTA_API UFF_FacilityCodexScreen : public UUserWidget
{
	GENERATED_BODY()
public:
	FSimpleMulticastDelegate OnCloseRequested;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFacilityCatalogRequested, FName);
	FOnFacilityCatalogRequested OnFacilityCatalogRequested;
	/** Both the benefiting facility and its nearby partner match this filter. None means all. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void SetFacilityFilter(FName FacilityId);
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void SetDiscoveryFilter(EFF_FacilityCodexFilterState State);
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void SetSearchText(const FText& SearchText);
	/** Filter by the category of the facility receiving the recipe's bonus. */
	UFUNCTION(BlueprintCallable, Category="FestaFiesta|Facility Codex")
	void SetCategoryFilter(bool bEnabled, EFFZSFacilityCategory Category);
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	FName GetSelectedRuleId() const { return SelectedRuleId; }
	UFUNCTION(BlueprintPure, Category="FestaFiesta|Facility Codex")
	int32 GetVisibleEntryCount() const { return VisibleItems.Num(); }
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	/** Deterministic editor/test presentation without changing gameplay or discovery saves. */
	void SetPreviewEntries(const TArray<FFF_FacilityCodexEntry>& Entries);
#endif
protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UFF_FacilityCodexTileView> TileView_Entries;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UEditableTextBox> Search_Entries;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_Close;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_All;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_Discovered;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_Undiscovered;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Active;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RewardReady;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UComboBoxString> Combo_Category;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_CollectionRemaining;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_RewardFeedback;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_RewardFeedback;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UButton> Button_ClearFacility;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Summary;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> Progress_Discovery;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Filter;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Empty;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UWidget> Panel_Detail;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_DetailTitle;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_TargetName;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_PartnerName;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_TargetCatalog;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_PartnerCatalog;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Bonus;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Distance;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Discovery;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Condition;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Applied;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UFF_FacilityCodexImage> Image_Target;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UFF_FacilityCodexImage> Image_Partner;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_TargetFallback;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_PartnerFallback;
	/** Optional for compatibility with the original authored screen before its explicit migration. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UStackBox> Stack_Main;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UStackBox> Stack_GlassDetailHeading;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UStackBox> Stack_GlassRecipe;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_Detail;
	/** Full viewport bounds, independent of filtering and the number of collection entries. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_Dialog;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_Browse;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_Showcase;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_ShowcaseRewardInfo;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UFF_FacilityCodexPreview> Viewport_Showcase;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UFF_FacilityCodexImage> Image_Showcase;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ShowcaseFallback;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailStatus;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewFacilityName;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewCategory;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PreviewHint;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ShowcaseDistance;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailRewardAmount;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailRewardLabel;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_DetailRewardHint;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_PreviewTarget;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_PreviewPartner;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ShowcaseCatalog;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RotatePreviewLeft;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_RotatePreviewRight;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ResetPreview;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_DetailClaimReward;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_DetailRewardClaimed;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UBorder> Border_DetailRewardLocked;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<USizeBox> Size_DetailRewardState;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> Switcher_Detail;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_DetailRecipe;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_DetailPlacement;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_DetailEffect;
	/** Move detail below readable columns instead of reducing card typography. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|Responsive")
	bool bEnableResponsiveDetail = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|Responsive", meta=(ClampMin="1"))
	float MinimumCardWidth = 224.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|Responsive", meta=(ClampMin="80"))
	float CompactDetailHeight = 220.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Layout|Responsive", meta=(ClampMin="0"))
	float CompactDetailGap = 20.f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Tabs")
	FLinearColor SelectedFilterTextColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Tabs")
	FLinearColor InactiveFilterTextColor = FLinearColor::FromSRGBColor(FColor(0x20, 0x4c, 0x36));
private:
	void UpdateResponsiveLayout();
	UFUNCTION() void RefreshFromSubsystem();
	UFUNCTION() void HandleClose();
	UFUNCTION() void HandleAll();
	UFUNCTION() void HandleDiscovered();
	UFUNCTION() void HandleUndiscovered();
	UFUNCTION() void HandleActive();
	UFUNCTION() void HandleRewardReady();
	UFUNCTION() void HandleCategoryChanged(FString Selected, ESelectInfo::Type SelectInfo);
	UFUNCTION() void HandlePreviewTarget();
	UFUNCTION() void HandlePreviewPartner();
	UFUNCTION() void HandleShowcaseCatalog();
	UFUNCTION() void HandleRotateLeft();
	UFUNCTION() void HandleRotateRight();
	UFUNCTION() void HandleResetPreview();
	UFUNCTION() void HandleDetailClaimReward();
	UFUNCTION() void HandleClearFacility();
	UFUNCTION() void HandleDetailRecipe();
	UFUNCTION() void HandleDetailPlacement();
	UFUNCTION() void HandleDetailEffect();
	UFUNCTION() void HandleTargetCatalog();
	UFUNCTION() void HandlePartnerCatalog();
	void HandleClaimReward(FName RuleId);
	void ShowRewardFeedback(int32 PaidGold);
	void ClearRewardFeedback();
	void UpdateRewardFeedback(float DeltaTime);
	void SetDetailPage(int32 Page);
	void SetTabSelected(UButton* Button, bool bSelected);
	UFUNCTION() void HandleSearchChanged(const FText& Text);
	void ApplyEntries(const TArray<FFF_FacilityCodexEntry>& Entries);
	void RefreshFilter();
	void RefreshDetail();
	void RefreshCategoryChoices();
	void RefreshShowcase();
	void RefreshPreviewVisibility();
	void HandleSelectionChanged(UObject* SelectedItem);
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexSubsystem> Codex;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UFF_FacilityCodexItem>> ItemsByRule;
	UPROPERTY(Transient) TArray<TObjectPtr<UFF_FacilityCodexItem>> AllItems;
	UPROPERTY(Transient) TArray<TObjectPtr<UFF_FacilityCodexItem>> VisibleItems;
	UPROPERTY(Transient) TMap<FName, FButtonStyle> AuthoredTabStyles;
	int32 ActiveDetailPage = INDEX_NONE;
	FFF_FacilityCodexFilter Filter;
	FFF_FacilityCodexSummary Summary;
	FName SelectedRuleId;
	TSoftObjectPtr<UTexture2D> ShownTarget;
	TSoftObjectPtr<UTexture2D> ShownPartner;
	TSoftObjectPtr<UTexture2D> ShownShowcase;
	TArray<EFFZSFacilityCategory> CategoryChoices;
	FName ShownPreviewRuleId;
	float RewardFeedbackRemaining = 0.f;
	bool bPreviewPartner = false;
	bool bUpdatingCategoryChoices = false;
	bool bShowPreviewModel = false;
	bool bApplyingFilter = false;
	bool bPreviewData = false;
	bool bCapturedResponsiveLayout = false;
	bool bCompactDetail = false;
};
