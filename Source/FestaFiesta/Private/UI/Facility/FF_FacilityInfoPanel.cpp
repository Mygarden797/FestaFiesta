// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformTime.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoPanel"

void UFF_FacilityInfoPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Layout is authored in WBP, including optional tabs and confirmation. Use the editor
	// migration command for old assets; never append runtime controls over designer work.
	if (Button_TabOverview) Button_TabOverview->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOverviewClicked);
	if (Button_TabSynergy) Button_TabSynergy->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSynergyClicked);
	if (Button_TabOperation) Button_TabOperation->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOperationClicked);
	if (Button_TabUpgrade) Button_TabUpgrade->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUpgradeTabClicked);
	if (Button_Favorite) Button_Favorite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFavoriteClicked);
	if (Button_RemoveConfirm) Button_RemoveConfirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveConfirmClicked);
	if (Button_RemoveCancel) Button_RemoveCancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveCancelClicked);
	if (Button_RevenueRanking) Button_RevenueRanking->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRevenueRankingClicked);
	if (Widget_LightingControls)
	{
		Widget_LightingControls->OnLightingRequested.AddUObject(this, &ThisClass::HandleLightingRequested);
		Widget_LightingControls->OnLightingResetRequested.AddUObject(this, &ThisClass::HandleLightingResetRequested);
	}

	if (Button_Move)
	{
		Button_Move->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMoveClicked);
	}
	if (Button_Remove)
	{
		Button_Remove->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveClicked);
	}
	if (Button_Close)
	{
		Button_Close->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (Button_StartMinigame)
	{
		Button_StartMinigame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMinigameClicked);
	}
	if (Button_ScheduleProgram)
	{
		Button_ScheduleProgram->OnClicked.AddUniqueDynamic(
			this, &ThisClass::HandleProgramScheduleClicked);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUpgradeClicked);
	}
}

FReply UFF_FacilityInfoPanel::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Card chrome and text consume left-clicks so they cannot select the world behind the panel.
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UFF_FacilityInfoPanel::ApplyViewData(const FFF_FacilityInfoViewData& ViewData)
{
	const bool bSelectionChanged = CurrentViewData.FacilityInstanceId != ViewData.FacilityInstanceId ||
		CurrentViewData.Context != ViewData.Context;
	CurrentViewData = ViewData;
	if (bSelectionChanged) ActivePage = EFF_FacilityInfoPage::Overview;
	if (Widget_LightingControls) Widget_LightingControls->ApplyViewData(ViewData);
	if (bSelectionChanged && WidgetTree)
	{
		if (UScrollBox* Details = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details"))))
		{
			Details->ScrollToStart();
		}
	}

	SetText(Text_Context, ViewData.ContextTitle);
	SetText(Text_FacilityName, ViewData.DisplayName);
	if (Text_FacilityName)
	{
		Text_FacilityName->SetToolTipText(ViewData.DisplayName);
	}
	SetText(Text_Category, ViewData.CategoryText);
	SetText(Text_Zone, ViewData.ZoneText);
	if (Text_Category) Text_Category->SetToolTipText(ViewData.CategoryText);
	if (Text_Zone) Text_Zone->SetToolTipText(ViewData.ZoneText);
	SetText(Text_FacilityState, ViewData.FacilityStateText);
	SetText(Text_FacilityRevenue, ViewData.FacilityRevenueText);
	if (Text_FacilityRevenue)
	{
		Text_FacilityRevenue->SetVisibility(ViewData.FacilityInstanceId.IsValid()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	const bool bShowRevenue = ViewData.bShowRevenueSection && ViewData.FacilityInstanceId.IsValid();
	SetText(Text_FacilityTodayRevenue, ViewData.TodayRevenueText);
	if (Text_FacilityTodayRevenue)
	{
		Text_FacilityTodayRevenue->SetColorAndOpacity(FSlateColor(
			ViewData.TodayRevenueGold > 0 ? RevenueEarnedColor : RevenueIdleColor));
	}
	SetText(Text_FacilityTotalRevenue, ViewData.TotalRevenueText);
	SetText(Text_FacilityRevenueCount, ViewData.RevenuePaymentCountText);
	SetText(Text_FacilityRevenueShare, ViewData.RevenueShareText);
	SetText(Text_FacilityRevenueStatus, ViewData.RevenueStatusText);
	if (Text_FacilityRevenueStatus)
	{
		Text_FacilityRevenueStatus->SetVisibility(ViewData.RevenueStatusText.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetText(Text_FacilityRevenueSummary, ViewData.RevenueSummaryText);
	if (Text_FacilityRevenueSummary)
	{
		Text_FacilityRevenueSummary->SetVisibility(bShowRevenue
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Panel_RevenueSection)
	{
		// Legacy panels without tabs keep this; tabbed panels refine it per page in RefreshPageVisibility.
		Panel_RevenueSection->SetVisibility(bShowRevenue
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetText(Text_RelocationWarning, LOCTEXT("NeedsRelocation", "! 재배치가 필요합니다."));
	SetText(Text_Footprint, ViewData.FootprintText);
	SetText(Text_Clearance, ViewData.ClearanceText);
	SetText(Text_InstallCost, ViewData.InstallCostText);
	SetText(Text_RecoveryCost, ViewData.RecoveryCostText);
	SetText(Text_InstanceLimit, ViewData.InstanceLimitText);
	if (Text_InstanceLimit)
	{
		Text_InstanceLimit->SetColorAndOpacity(FSlateColor(
			ViewData.bInstanceLimitReached
				? InstanceLimitReachedColor
				: InstanceLimitNormalColor));
	}
	SetText(Text_QueueCapacity, ViewData.QueueCapacityText);
	SetText(Text_ServiceDuration, ViewData.ServiceDurationText);
	SetText(Text_OperationState, ViewData.OperationStateText);
	SetText(Text_WaitingParties, ViewData.WaitingPartiesText);
	SetText(Text_Occupancy, ViewData.OccupancyText);
	SetText(Text_RemainingService, ViewData.RemainingServiceText);
	SetText(Text_ExpectedWait, ViewData.ExpectedWaitText);
	SetText(Text_Throughput, ViewData.ThroughputText);
	SetText(Text_ProgramStatus, ViewData.ProgramStatusText);
	SetText(Text_ScheduledProgramSummary, ViewData.ScheduledProgramSummaryText);
	SetText(Text_ProgramScheduleReason, ViewData.ProgramScheduleDisabledReasonText);
	SetText(Text_Feedback, ViewData.FeedbackText);
	SetText(Text_ThumbnailPlaceholder, ViewData.ThumbnailPlaceholderText);

	const bool bBuildContext = ViewData.Context == EFF_FacilityInfoContext::Build;
	SetText(Text_FacilityLevel, ViewData.FacilityLevelText);
	FText Effects = ViewData.UpgradeEffectText;
	if (!ViewData.UpgradeRevenueEffectText.IsEmpty())
	{
		Effects = FText::Format(LOCTEXT("UpgradeEffectsWithRevenue", "{0}\n{1}"),
			ViewData.UpgradeRevenueEffectText, Effects);
	}
	SetText(Text_UpgradeEffect, ViewData.UpgradeSizeEffectText.IsEmpty()
		? Effects : FText::Format(LOCTEXT("UpgradeEffectsWithSize", "{0}\n{1}"),
			Effects, ViewData.UpgradeSizeEffectText));
	SetText(Text_UpgradeCurrent, ViewData.UpgradeCurrentText);
	SetText(Text_UpgradeNext, ViewData.UpgradeNextText);
	SetText(Text_UpgradeCurrentRevenue, ViewData.UpgradeCurrentValues.Revenue);
	SetText(Text_UpgradeNextRevenue, ViewData.UpgradeNextValues.Revenue);
	SetText(Text_UpgradeCurrentTime, ViewData.UpgradeCurrentValues.ServiceTime);
	SetText(Text_UpgradeNextTime, ViewData.UpgradeNextValues.ServiceTime);
	SetText(Text_UpgradeCurrentThroughput, ViewData.UpgradeCurrentValues.Throughput);
	SetText(Text_UpgradeNextThroughput, ViewData.UpgradeNextValues.Throughput);
	SetText(Text_UpgradeCurrentSize, ViewData.UpgradeCurrentValues.Footprint);
	SetText(Text_UpgradeNextSize, ViewData.UpgradeNextValues.Footprint);
	SetText(Text_UpgradePrice, ViewData.UpgradePriceText);
	SetText(Text_UpgradeBalance, ViewData.UpgradeBalanceText);
	SetText(Text_UpgradeBalanceLabel, ViewData.UpgradeBalanceLabel);
	for (UWidget* Surface : {Panel_UpgradeMetricRows.Get(), Panel_UpgradeQuote.Get()})
		if (Surface) Surface->SetVisibility(bBuildContext && ViewData.bShowUpgradeDetails
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	const bool bHasComparison = Panel_UpgradeComparison && Text_UpgradeCurrent && Text_UpgradeNext &&
		!ViewData.UpgradeCurrentText.IsEmpty() && !ViewData.UpgradeNextText.IsEmpty();
	if (Panel_UpgradeComparison)
	{
		Panel_UpgradeComparison->SetVisibility(!Panel_UpgradeMetricRows && bHasComparison && bBuildContext && ViewData.bShowUpgradeDetails
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		// Keep authored multipliers, signed changes and footprint details available without
		// making the first screen taller. Legacy WBP assets continue to show the full text.
		Panel_UpgradeComparison->SetToolTipText(Text_UpgradeEffect ? Text_UpgradeEffect->GetText() : Effects);
	}
	SetText(Text_UpgradeCost, ViewData.UpgradeCostText);
	SetText(Text_UpgradeReason, ViewData.UpgradeDisabledReasonText);
	SetText(Text_Upgrade, ViewData.UpgradeActionText);
	if (Panel_UpgradeSection)
	{
		Panel_UpgradeSection->SetVisibility(ViewData.bHasFacilityLevels && ViewData.FacilityInstanceId.IsValid()
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	for (UCommonTextBlock* Detail : {Text_UpgradeEffect.Get(), Text_UpgradeCost.Get()})
	{
		if (Detail)
		{
			Detail->SetVisibility(bBuildContext && ViewData.bShowUpgradeDetails
				&& (Detail != Text_UpgradeEffect.Get() || (!bHasComparison && !Panel_UpgradeMetricRows))
				&& (Detail != Text_UpgradeCost.Get() || !Panel_UpgradeQuote)
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	}
	if (Text_UpgradeReason)
	{
		Text_UpgradeReason->SetVisibility(bBuildContext && ViewData.bShowUpgradeAction &&
			!ViewData.UpgradeDisabledReasonText.IsEmpty()
				? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->SetVisibility(bBuildContext && ViewData.bShowUpgradeAction
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Upgrade->SetIsEnabled(bBuildContext && ViewData.bShowUpgradeAction && ViewData.bCanUpgrade);
	}
	if (Panel_BuildSection)
	{
		Panel_BuildSection->SetVisibility(bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_LiveSection)
	{
		Panel_LiveSection->SetVisibility(!bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	const bool bShowLiveActions = !bBuildContext && ViewData.bHasMinigame;
	if (Panel_BuildActions)
	{
		Panel_BuildActions->SetVisibility(
			bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_LiveActions)
	{
		Panel_LiveActions->SetVisibility(
			bShowLiveActions ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_Actions)
	{
		Panel_Actions->SetVisibility(
			(bBuildContext || bShowLiveActions)
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (Text_RelocationWarning)
	{
		Text_RelocationWarning->SetVisibility(
			ViewData.bNeedsRelocation ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_Feedback)
	{
		Text_Feedback->SetVisibility(
			ViewData.FeedbackText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
		Text_Feedback->SetColorAndOpacity(FSlateColor(
			ViewData.bFeedbackIsError ? FeedbackErrorColor : FeedbackSuccessColor));
	}

	if (Button_Move)
	{
		Button_Move->SetIsEnabled(bBuildContext && ViewData.bCanMove);
	}
	if (Button_Remove)
	{
		Button_Remove->SetIsEnabled(bBuildContext && ViewData.bCanRemove);
	}
	if (Panel_ProgramSection)
	{
		Panel_ProgramSection->SetVisibility(
			ViewData.bShowProgramSection
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
	}
	if (Text_ScheduledProgramSummary)
	{
		Text_ScheduledProgramSummary->SetVisibility(
			ViewData.bHasScheduledProgram && !ViewData.ScheduledProgramSummaryText.IsEmpty()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (Text_ProgramScheduleReason)
	{
		Text_ProgramScheduleReason->SetVisibility(
			ViewData.bShowProgramScheduleAction && !ViewData.ProgramScheduleDisabledReasonText.IsEmpty()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (Button_ScheduleProgram)
	{
		Button_ScheduleProgram->SetVisibility(
			ViewData.bShowProgramScheduleAction
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		Button_ScheduleProgram->SetIsEnabled(
			ViewData.bShowProgramScheduleAction && ViewData.bCanScheduleProgram);
	}
	SetText(Text_ScheduleProgram,
		ViewData.ProgramScheduleActionText.IsEmpty()
			? LOCTEXT("ScheduleProgram", "유료 프로그램 편성")
			: ViewData.ProgramScheduleActionText);
	if (Button_StartMinigame)
	{
		Button_StartMinigame->SetVisibility(
			!bBuildContext && ViewData.bHasMinigame
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		Button_StartMinigame->SetIsEnabled(
			!bBuildContext && ViewData.bCanStartMinigame);
	}
	SetText(Text_StartMinigame,
		ViewData.MinigameActionText.IsEmpty()
			? LOCTEXT("StartMinigame", "게임 시작")
			: ViewData.MinigameActionText);

	const bool bHasThumbnail = IsValid(ViewData.Thumbnail);
	if (Image_Thumbnail)
	{
		if (bHasThumbnail)
		{
			Image_Thumbnail->SetBrushFromTexture(ViewData.Thumbnail, true);
		}
		else
		{
			Image_Thumbnail->SetBrushFromTexture(nullptr, true);
		}
		Image_Thumbnail->SetVisibility(
			bHasThumbnail ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Text_ThumbnailPlaceholder)
	{
		Text_ThumbnailPlaceholder->SetVisibility(
			bHasThumbnail ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	SetText(Text_Favorite, Panel_PersistentIdentity
		? (ViewData.bIsFavorite ? FText::FromString(TEXT("★")) : FText::FromString(TEXT("☆")))
		: (ViewData.bIsFavorite ? LOCTEXT("FavoriteSaved", "★ 즐겨찾기") : LOCTEXT("FavoriteAdd", "☆ 즐겨찾기")));
	if (Button_Favorite) Button_Favorite->SetToolTipText(ViewData.bIsFavorite
		? LOCTEXT("FavoriteRemoveTooltip", "즐겨찾기 해제") : LOCTEXT("FavoriteAddTooltip", "즐겨찾기 추가"));
	if (Button_Favorite) Button_Favorite->SetIsEnabled(!ViewData.FacilityDefinitionId.IsNone());
	SetText(Text_LevelSummary, ViewData.FacilityLevelText);
	if (Text_LevelSummary) Text_LevelSummary->SetVisibility(ViewData.bHasFacilityLevels
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	SetText(Text_RemoveConfirmation, Text_RemoveRecovery
		? FText::Format(LOCTEXT("RemoveQuestion", "{0} 시설을 철거할까요?"), ViewData.DisplayName)
		: FText::Format(
		LOCTEXT("RemoveConfirmation", "{0} 시설을 철거할까요?\n회수 금액 {1}\n강화에 사용한 비용은 환불되지 않습니다."),
		ViewData.DisplayName, ViewData.RecoveryCostText));
	SetText(Text_RemoveRecovery, ViewData.RecoveryCostText);
	SetText(Text_RemoveInstallCost, ViewData.InstallCostText);
	if (Button_RemoveConfirm) Button_RemoveConfirm->SetIsEnabled(bBuildContext && ViewData.bCanRemove);
	RefreshPageVisibility();
	OnViewDataApplied(ViewData);
}

void UFF_FacilityInfoPanel::SetInfoPage(const EFF_FacilityInfoPage Page)
{
	const bool bPageChanged = ActivePage != Page;
	ActivePage = Page;
	RefreshPageVisibility();
	if (bPageChanged && WidgetTree)
		if (UScrollBox* Details = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details"))))
			Details->ScrollToStart();
}

void UFF_FacilityInfoPanel::RefreshPageVisibility()
{
	// Legacy designer panels keep their previous presentation until explicitly migrated.
	if (!Button_TabOverview || !Button_TabOperation || !Button_TabUpgrade) return;
	const bool bBuild = CurrentViewData.Context == EFF_FacilityInfoContext::Build;
	if (ActivePage == EFF_FacilityInfoPage::Upgrade && (!bBuild || !CurrentViewData.bHasFacilityLevels))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::RemoveConfirmation && (!bBuild || !CurrentViewData.bCanRemove))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::Synergy && (!Button_TabSynergy || !Panel_SynergySection))
		ActivePage = EFF_FacilityInfoPage::Overview;
	auto Show = [](UWidget* Widget, const bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	Show(Panel_BuildSection, ActivePage == EFF_FacilityInfoPage::Overview);
	Show(Panel_SynergySection, ActivePage == EFF_FacilityInfoPage::Synergy);
	if (ActivePage == EFF_FacilityInfoPage::Synergy) RefreshSynergyEntries();
	Show(Panel_LiveSection, ActivePage == EFF_FacilityInfoPage::Operation);
	Show(Panel_UpgradeSection, ActivePage == EFF_FacilityInfoPage::Upgrade);
	Show(Panel_ProgramSection, ActivePage == EFF_FacilityInfoPage::Overview && CurrentViewData.bShowProgramSection);
	// Earnings matter both when planning (overview) and while watching live operation.
	Show(Panel_RevenueSection, CurrentViewData.bShowRevenueSection && CurrentViewData.FacilityInstanceId.IsValid() &&
		(ActivePage == EFF_FacilityInfoPage::Overview || ActivePage == EFF_FacilityInfoPage::Operation));
	Show(Panel_RemoveConfirmation, ActivePage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(Panel_RemoveActions, ActivePage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(Panel_UpgradeActions, ActivePage == EFF_FacilityInfoPage::Upgrade && CurrentViewData.bShowUpgradeAction);
	Show(Button_RevenueRanking, ActivePage == EFF_FacilityInfoPage::Operation &&
		CurrentViewData.FacilityInstanceId.IsValid());
	// The persistent header still identifies the facility. Give task-specific pages their
	// full scroll area instead of repeating the overview thumbnail above every action.
	if (WidgetTree) Show(WidgetTree->FindWidget(TEXT("Summary_Card")), ActivePage == EFF_FacilityInfoPage::Overview);
	// Lighting remains available in the overview; its own data controls applicability.
	if (Widget_LightingControls)
	{
		Widget_LightingControls->ApplyViewData(CurrentViewData);
		if (ActivePage != EFF_FacilityInfoPage::Overview) Show(Widget_LightingControls, false);
	}
	Button_TabOverview->SetIsEnabled(ActivePage != EFF_FacilityInfoPage::Overview);
	if (Button_TabSynergy) Button_TabSynergy->SetIsEnabled(ActivePage != EFF_FacilityInfoPage::Synergy);
	Button_TabOperation->SetIsEnabled(ActivePage != EFF_FacilityInfoPage::Operation);
	Button_TabUpgrade->SetIsEnabled(bBuild && CurrentViewData.bHasFacilityLevels && ActivePage != EFF_FacilityInfoPage::Upgrade);
	Show(Button_TabUpgrade, bBuild && CurrentViewData.bHasFacilityLevels);
	if (Panel_PersistentIdentity)
	{
		for (UButton* Tab : {Button_TabOverview.Get(), Button_TabSynergy.Get(), Button_TabOperation.Get(), Button_TabUpgrade.Get()})
			if (Tab)
				if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(Tab->GetContent()))
					Label->SetColorAndOpacity(FSlateColor(Tab->GetIsEnabled() ? InactiveTabTextColor : SelectedTabTextColor));
	}
	Show(Panel_Actions, ActivePage != EFF_FacilityInfoPage::RemoveConfirmation &&
		(bBuild || CurrentViewData.bHasMinigame));
}

void UFF_FacilityInfoPanel::RefreshSynergyEntries()
{
	SetText(Text_SynergySummary, CurrentViewData.SynergySummaryText);
	if (!VerticalBox_SynergyEntries) return;
	const int32 EntryCount = CurrentViewData.SynergyEntries.Num();
	if (EntryCount > SynergyEntryPool.Num() && !LoadedSynergyEntryClass && !bSynergyEntryLoadFailed)
	{
		UClass* RowClass = SynergyEntryWidgetClass.LoadSynchronous();
		if (RowClass && RowClass->IsChildOf(UFF_FacilitySynergyEntryWidget::StaticClass()))
		{
			LoadedSynergyEntryClass = RowClass;
		}
		else
		{
			bSynergyEntryLoadFailed = true;
			UE_LOG(LogTemp, Error, TEXT("Facility synergy row WBP is unavailable: %s"),
				*SynergyEntryWidgetClass.ToSoftObjectPath().ToString());
		}
	}
	while (SynergyEntryPool.Num() < EntryCount && LoadedSynergyEntryClass)
	{
		UFF_FacilitySynergyEntryWidget* Row = GetOwningPlayer()
			? CreateWidget<UFF_FacilitySynergyEntryWidget>(GetOwningPlayer(), LoadedSynergyEntryClass)
			: CreateWidget<UFF_FacilitySynergyEntryWidget>(GetWorld(), LoadedSynergyEntryClass);
		if (!Row) break;
		Row->OnPartnerCatalogRequested.AddUObject(this, &ThisClass::HandleSynergyPartnerCatalogRequested);
		// Spacing and typography belong to the child WBP. Keep pooled rows attached so
		// repeated queue/world refreshes change data without rebuilding the Slate tree.
		VerticalBox_SynergyEntries->AddChildToVerticalBox(Row);
		SynergyEntryPool.Add(Row);
	}
	for (int32 Index = 0; Index < SynergyEntryPool.Num(); ++Index)
	{
		UFF_FacilitySynergyEntryWidget* Row = SynergyEntryPool[Index];
		if (Index < EntryCount)
		{
			Row->ApplyEntry(CurrentViewData.SynergyEntries[Index]);
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else Row->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (EntryCount > 0 && SynergyEntryPool.Num() < EntryCount)
	{
		SetText(Text_SynergySummary, LOCTEXT("SynergyRowsUnavailable", "조합 정보를 표시할 수 없습니다."));
	}
}

void UFF_FacilityInfoPanel::HandleSynergyPartnerCatalogRequested(const FName FacilityDefinitionId)
{
	// A pooled row can retain old data while hidden. Only the currently displayed
	// selection may request navigation, and BuildMode owns all state/permission checks.
	if (ActivePage != EFF_FacilityInfoPage::Synergy || FacilityDefinitionId.IsNone() ||
		GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden ||
		!CurrentViewData.SynergyEntries.ContainsByPredicate([FacilityDefinitionId](const FFF_FacilityInfoSynergyEntry& Entry)
			{ return Entry.PartnerFacilityDefinitionId == FacilityDefinitionId; })) return;
	UWorld* World = GetWorld();
	UFF_BuildModeSubsystem* BuildMode = World ? World->GetSubsystem<UFF_BuildModeSubsystem>() : nullptr;
	if (BuildMode && BuildMode->OpenCatalogForFacility(FacilityDefinitionId)) return;
	const FText Failure = LOCTEXT("PartnerCatalogUnavailable", "지금은 시설 건축 목록을 열 수 없습니다. 건축 가능한 단계에서 다시 선택해 주세요.");
	if (UFF_FacilityInfoSubsystem* Info = World ? World->GetSubsystem<UFF_FacilityInfoSubsystem>() : nullptr)
		Info->SetFeedback(Failure, true);
	CurrentViewData.FeedbackText = Failure;
	CurrentViewData.bFeedbackIsError = true;
	SetText(Text_Feedback, Failure);
	if (Text_Feedback)
	{
		Text_Feedback->SetColorAndOpacity(FeedbackErrorColor);
		Text_Feedback->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UFF_FacilityInfoPanel::HandleOverviewClicked() { SetInfoPage(EFF_FacilityInfoPage::Overview); }
void UFF_FacilityInfoPanel::HandleSynergyClicked() { SetInfoPage(EFF_FacilityInfoPage::Synergy); }
void UFF_FacilityInfoPanel::HandleOperationClicked() { SetInfoPage(EFF_FacilityInfoPage::Operation); }
void UFF_FacilityInfoPanel::HandleUpgradeTabClicked() { SetInfoPage(EFF_FacilityInfoPage::Upgrade); }
void UFF_FacilityInfoPanel::HandleFavoriteClicked()
{
	if (!CurrentViewData.FacilityDefinitionId.IsNone()) OnFavoriteRequested.Broadcast();
}
void UFF_FacilityInfoPanel::HandleRevenueRankingClicked()
{
	if (ActivePage == EFF_FacilityInfoPage::Operation && CurrentViewData.FacilityInstanceId.IsValid())
		OnRevenueRankingRequested.Broadcast();
}
void UFF_FacilityInfoPanel::HandleRemoveCancelClicked()
{
	SetInfoPage(EFF_FacilityInfoPage::Overview);
	ApplyViewData(CurrentViewData);
}
void UFF_FacilityInfoPanel::HandleRemoveConfirmClicked()
{
	if (ActivePage == EFF_FacilityInfoPage::RemoveConfirmation &&
		CurrentViewData.Context == EFF_FacilityInfoContext::Build && CurrentViewData.bCanRemove)
	{
		// Leave confirmation before broadcasting so repeated clicks cannot submit twice.
		ActivePage = EFF_FacilityInfoPage::Overview;
		OnRemoveRequested.Broadcast();
	}
}

void UFF_FacilityInfoPanel::SetPanelOpen(const bool bOpen)
{
	// AddToPlayerScreen gives this UUserWidget a full-screen outer slot. SelfHitTestInvisible keeps
	// that wrapper transparent to world clicks while the visible Border and its buttons remain live.
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

bool UFF_FacilityInfoPanel::IsPointerOverInteractivePanel() const
{
	if (GetVisibility() == ESlateVisibility::Collapsed ||
		GetVisibility() == ESlateVisibility::Hidden || !Border_Panel)
	{
		return false;
	}
	if (Border_Panel->IsHovered())
	{
		return true;
	}

	// BuildMode polls the raw mouse-down state during the world tick, while UMG buttons normally
	// click on mouse-up. A newly replaced PrepHUD can own Slate's hover path even when this card is
	// visibly above it, so IsHovered alone lets the same press fall through and clear the selected
	// facility before the button callback runs. Cached geometry is independent of that hover owner.
	return FSlateApplication::IsInitialized() &&
		Border_Panel->GetCachedGeometry().IsUnderLocation(
			FSlateApplication::Get().GetCursorPos());
}

void UFF_FacilityInfoPanel::HandleMoveClicked()
{
	if (CurrentViewData.Context == EFF_FacilityInfoContext::Build && CurrentViewData.bCanMove)
	{
		OnMoveRequested.Broadcast();
	}
}

void UFF_FacilityInfoPanel::HandleRemoveClicked()
{
	if (CurrentViewData.Context == EFF_FacilityInfoContext::Build && CurrentViewData.bCanRemove)
	{
		// An old WBP without the authored confirmation is deliberately not destructive.
		if (Panel_RemoveConfirmation && Button_RemoveConfirm)
			SetInfoPage(EFF_FacilityInfoPage::RemoveConfirmation);
	}
}

void UFF_FacilityInfoPanel::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

void UFF_FacilityInfoPanel::HandleMinigameClicked()
{
	if (CurrentViewData.Context == EFF_FacilityInfoContext::Live &&
		CurrentViewData.bCanStartMinigame)
	{
		OnMinigameRequested.Broadcast();
	}
}

void UFF_FacilityInfoPanel::HandleProgramScheduleClicked()
{
	if (CurrentViewData.Context == EFF_FacilityInfoContext::Build &&
		CurrentViewData.FacilityInstanceId.IsValid() &&
		CurrentViewData.bShowProgramScheduleAction &&
		CurrentViewData.bCanScheduleProgram)
	{
		OnProgramScheduleRequested.Broadcast();
	}
}

void UFF_FacilityInfoPanel::HandleUpgradeClicked()
{
	if (CurrentViewData.Context == EFF_FacilityInfoContext::Build &&
		CurrentViewData.bHasFacilityLevels && CurrentViewData.FacilityInstanceId.IsValid() &&
		CurrentViewData.bShowUpgradeDetails && CurrentViewData.bShowUpgradeAction &&
		CurrentViewData.bCanUpgrade)
	{
		const double Now = FPlatformTime::Seconds();
		const double DoubleClickSeconds = FMath::Max(0.25f, GetDefault<UInputSettings>()->DoubleClickTime);
		if (LastUpgradeClickSeconds >= 0.0 && Now - LastUpgradeClickSeconds <= DoubleClickSeconds)
		{
			return;
		}
		LastUpgradeClickSeconds = Now;
		// The coordinator reads the displayed level, then the command validates it again.
		OnUpgradeRequested.Broadcast();
	}
}

void UFF_FacilityInfoPanel::SetText(UCommonTextBlock* Target, const FText& Text) const
{
	if (Target)
	{
		Target->SetText(Text);
	}
}

void UFF_FacilityInfoPanel::HandleLightingRequested(const FGuid FacilityId, const FFFZSFacilityLightingState& State)
{
	if (CurrentViewData.bCanEditLighting && CurrentViewData.FacilityInstanceId == FacilityId)
		OnLightingRequested.Broadcast(FacilityId, State);
}

void UFF_FacilityInfoPanel::HandleLightingResetRequested(const FGuid FacilityId)
{
	if (CurrentViewData.bCanEditLighting && CurrentViewData.FacilityInstanceId == FacilityId)
		OnLightingResetRequested.Broadcast(FacilityId);
}

#undef LOCTEXT_NAMESPACE
