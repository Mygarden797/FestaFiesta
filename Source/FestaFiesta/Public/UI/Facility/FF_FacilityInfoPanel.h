// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/Anchors.h"
#include "UI/Facility/FF_FacilityInfoTypes.h"
#include "UI/Facility/FF_FacilityLightingControls.h"
#include "FF_FacilityInfoPanel.generated.h"

class UBorder;
class UButton;
class UCommonTextBlock;
class UImage;
class UWidget;
class UVerticalBox;
class UFF_FacilitySynergyEntryWidget;

/**
 * Native contract for WBP_FF_FacilityInfoPanel.
 *
 * The WBP owns layout and styling; this class owns data projection and button events. Keeping
 * gameplay queries outside the widget prevents Blueprint bindings from running every frame.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Facility Info Panel"))
class FESTAFIESTA_API UFF_FacilityInfoPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	FSimpleMulticastDelegate OnMoveRequested;
	FSimpleMulticastDelegate OnRemoveRequested;
	FSimpleMulticastDelegate OnProgramScheduleRequested;
	FSimpleMulticastDelegate OnUpgradeRequested;
	FSimpleMulticastDelegate OnMinigameRequested;
	FSimpleMulticastDelegate OnCloseRequested;
	FSimpleMulticastDelegate OnFavoriteRequested;
	FSimpleMulticastDelegate OnRevenueRankingRequested;
	FFF_OnFacilityLightingRequested OnLightingRequested;
	FFF_OnFacilityLightingRequested OnLightingRollbackRequested;
	FFF_OnFacilityLightingResetRequested OnLightingResetRequested;

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void ApplyViewData(const FFF_FacilityInfoViewData& ViewData);

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void SetInfoPage(EFF_FacilityInfoPage Page);

	/** Optional authored animation or presentation; the native class never builds a widget tree. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|Facility Info")
	void OnViewDataApplied(const FFF_FacilityInfoViewData& ViewData);

	/** Opens without making the full-screen wrapper a hit target; closes by collapsing it. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Facility Info")
	void SetPanelOpen(bool bOpen);

	/** True only while the cursor is over the visible card, not the full-screen overlay. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Facility Info")
	bool IsPointerOverInteractivePanel() const;
	bool IsLightingTextInputFocused() const;
	void CancelLightingEdit();

	const FFF_FacilityInfoViewData& GetCurrentViewData() const { return CurrentViewData; }

protected:
	virtual void NativeOnInitialized() override;   
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& Event) override;

private:
	UFUNCTION()
	void HandleMoveClicked();

	UFUNCTION()
	void HandleRemoveClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleMinigameClicked();

	UFUNCTION()
	void HandleProgramScheduleClicked();

	UFUNCTION()
	void HandleUpgradeClicked();

	UFUNCTION() void HandleOverviewClicked();
	UFUNCTION() void HandleSynergyClicked();
	UFUNCTION() void HandleOperationClicked();
	UFUNCTION() void HandleUpgradeTabClicked();
	UFUNCTION() void HandleLightingTabClicked();
	UFUNCTION() void HandleRevenueTabClicked();
	UFUNCTION() void HandleProgramsTabClicked();
	UFUNCTION() void HandleSynergyPreviousClicked();
	UFUNCTION() void HandleSynergyNextClicked();
	UFUNCTION() void HandleLightingMinimizeClicked();
	void HandleLightingEditFinished();
	void RefreshLightingPanelLayout();
	void HandleLightingRollbackRequested(FGuid FacilityId, const FFFZSFacilityLightingState& State);
	UFUNCTION() void HandleFavoriteClicked();
	UFUNCTION() void HandleRemoveConfirmClicked();
	UFUNCTION() void HandleRemoveCancelClicked();
	UFUNCTION() void HandleRevenueRankingClicked();
	void RefreshPageVisibility();
	void RefreshSynergyEntries();
	void HandleSynergyPartnerCatalogRequested(FName FacilityDefinitionId);
	EFF_FacilityInfoPage ActivePage = EFF_FacilityInfoPage::Overview;
	int32 SynergyPageIndex = 0;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_TabRevenue;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_TabPrograms;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_SynergyPrevious;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_SynergyNext;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_SynergyPage;
	bool bLightingMinimized = false;
	bool bDraggingLighting = false;
	FVector2D LightingDragStart;
	FVector2D LightingPanelTranslation;
	FVector2D LightingDragTranslation;
	FMargin ExpandedLightingOffsets;
	FAnchors ExpandedLightingAnchors;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_TabLighting;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LightingMinimize;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_LightingMinimize;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> Panel_LightingCompact;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_LightingCompactSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_TabOverview;
	/** Optional only to support additive migration of an older panel WBP. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_TabSynergy;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_SynergySection;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_SynergySummary;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UVerticalBox> VerticalBox_SynergyEntries;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Synergy",
		meta = (AllowPrivateAccess = "true", DisplayName = "조합 행 위젯", ToolTip = "조합 탭의 각 항목을 그리는 WBP입니다. 글꼴·색·간격은 이 WBP의 Designer에서 수정합니다."))
	TSoftClassPtr<UFF_FacilitySynergyEntryWidget> SynergyEntryWidgetClass =
		TSoftClassPtr<UFF_FacilitySynergyEntryWidget>(FSoftObjectPath(
			TEXT("/Game/FestaFiesta/UI/Facility/WBP_FF_FacilitySynergyEntry.WBP_FF_FacilitySynergyEntry_C")));
	UPROPERTY(Transient) TArray<TObjectPtr<UFF_FacilitySynergyEntryWidget>> SynergyEntryPool;
	UPROPERTY(Transient) TSubclassOf<UFF_FacilitySynergyEntryWidget> LoadedSynergyEntryClass;
	bool bSynergyEntryLoadFailed = false;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_TabOperation;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_TabUpgrade;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Favorite;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Favorite;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_LevelSummary;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_RemoveConfirmation;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RemoveConfirmation;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_RemoveConfirm;
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_RemoveCancel;

	void SetText(UCommonTextBlock* Target, const FText& Text) const;
	void HandleLightingRequested(FGuid FacilityId, const FFFZSFacilityLightingState& State);
	void HandleLightingResetRequested(FGuid FacilityId);

	/** Authored child WBP, migrated by FF.BuildFacilityInfoUMG without replacing designer layout. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UFF_FacilityLightingControls> Widget_LightingControls;

	UPROPERTY(Transient)
	FFF_FacilityInfoViewData CurrentViewData;
	/** Preserve authored inactive/selected brushes while selection remains an enabled state. */
	UPROPERTY(Transient)
	TMap<EFF_FacilityInfoPage, FButtonStyle> AuthoredTabStyles;

	/** A double-click must not also purchase the newly displayed next level. */
	double LastUpgradeClickSeconds = -1.0;

	/** Designer-tunable feedback states; edit these in the panel WBP Class Defaults. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackErrorColor =
		FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor FeedbackSuccessColor =
		FLinearColor::FromSRGBColor(FColor(0x3F, 0x8F, 0x6D));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor InstanceLimitNormalColor =
		FLinearColor::FromSRGBColor(FColor(0x24, 0x33, 0x2E));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance",
		meta = (AllowPrivateAccess = "true"))
	FLinearColor InstanceLimitReachedColor =
		FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	/** Today's earnings number switches to this color once the facility has settled a payment today. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Revenue",
		meta = (AllowPrivateAccess = "true", DisplayName = "오늘 수익 있음 색",
			ToolTip = "이 시설이 오늘 결제를 한 건이라도 받았을 때 오늘 매출 숫자에 쓰는 색입니다."))
	FLinearColor RevenueEarnedColor =
		FLinearColor::FromSRGBColor(FColor(0x2F, 0x7D, 0x5C));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Revenue",
		meta = (AllowPrivateAccess = "true", DisplayName = "오늘 수익 없음 색",
			ToolTip = "오늘 결제가 아직 없을 때 오늘 매출 숫자에 쓰는 색입니다."))
	FLinearColor RevenueIdleColor =
		FLinearColor::FromSRGBColor(FColor(0x31, 0x57, 0x47));
	UPROPERTY(EditDefaultsOnly, Category = "Appearance|Tabs")
	FLinearColor SelectedTabTextColor = FLinearColor::White;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance|Tabs")
	FLinearColor InactiveTabTextColor = FLinearColor::FromSRGBColor(FColor(0x31, 0x57, 0x47));

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Panel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Move;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Remove;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_StartMinigame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_StartMinigame;

	/** Optional during the non-destructive WBP migration; the builder authors it for new assets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_ProgramSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ScheduledProgramSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ProgramScheduleReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_ScheduleProgram;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ScheduleProgram;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_UpgradeSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityLevel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityRevenue;

	/**
	 * Per-facility earnings card, added once by FF.MigrateFacilityRevenueSection. Layout, labels
	 * and styling stay in the WBP; the native class only projects the numbers.
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_RevenueSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityTodayRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityTotalRevenue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityRevenueCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityRevenueShare;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityRevenueStatus;

	/** Optional one-line alternative for compact designer layouts that skip the stat cards. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityRevenueSummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_UpgradeEffect;

	/** Optional so existing designer panels keep the complete legacy effect text. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_UpgradeComparison;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_UpgradeCurrent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_UpgradeNext;

	/** V2 named cells remain optional so older authored panels keep their original presentation. */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_UpgradeMetricRows;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_UpgradeQuote;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_UpgradeActions;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeCurrentRevenue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeNextRevenue;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeCurrentTime;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeNextTime;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeCurrentThroughput;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeNextThroughput;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeCurrentSize;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeNextSize;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradePrice;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeBalance;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_UpgradeBalanceLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_PersistentIdentity;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> Panel_RemoveActions;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_RemoveRecovery;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_RemoveInstallCost;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_RevenueRanking;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_UpgradeCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_UpgradeReason;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Upgrade;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Upgrade;

	/** Fixed footer surfaces in the LightTeal WBP; optional for legacy designer assets. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_Actions;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_BuildActions;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_LiveActions;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ThumbnailPlaceholder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_BuildSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UWidget> Panel_LiveSection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Context;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Category;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Zone;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_FacilityState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RelocationWarning;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Footprint;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Clearance;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_InstallCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RecoveryCost;

	/** Optional until the non-destructive WBP builder migration has added the new row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_InstanceLimit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_QueueCapacity;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_QueueCapacityValue;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCommonTextBlock> Text_ServiceCapacityValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ServiceDuration;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_OperationState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_WaitingParties;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Occupancy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_RemainingService;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_ExpectedWait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Throughput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget, AllowPrivateAccess = "true"))
	TObjectPtr<UCommonTextBlock> Text_Feedback;
};
