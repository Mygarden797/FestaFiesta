// FF_FacilityInfoPanel.cpp
// 역할: 선택한 시설의 정보·업그레이드·조명·프로그램을 표시하고 조작 의도를 전달한다.

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilitySynergyEntryWidget.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "UI/Facility/FF_FacilityInfoSubsystem.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformTime.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Misc/ConfigCacheIni.h"
#include "Building/Presentation/FF_BuildTypography.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoPanel"

namespace
{
	constexpr const TCHAR* LightingIntroductionSection = TEXT("FestaFiesta.FacilityLighting");
	constexpr const TCHAR* LightingIntroductionSeenKey = TEXT("bHasSeenColorIntroduction");
}

void UFF_FacilityInfoPanel::HideLegacyClearanceWidgets()
{
	if (!WidgetTree) return;
	// Retain optional bindings on existing assets while removing the complete row/card.
	for (const TCHAR* Name : {TEXT("Text_Clearance"), TEXT("Text_Clearance_Label"),
		TEXT("Text_Clearance_Card"), TEXT("Clearance_Row"), TEXT("Clearance_Row_Label"),
		TEXT("Clarity_Row_Text_Clearance"), TEXT("Clarity_Label_Text_Clearance"),
		TEXT("Aurora_Row_Text_Clearance"), TEXT("Aurora_Label_Text_Clearance")})
		if (UWidget* Widget = WidgetTree->FindWidget(Name))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
#if WITH_EDITOR
			Widget->bHiddenInDesigner = true;
#endif
		}
	if (Text_Clearance) Text_Clearance->SetText(FText::GetEmpty());
}

void UFF_FacilityInfoPanel::NativePreConstruct()
{
	Super::NativePreConstruct();
	HideLegacyClearanceWidgets();
	if (!IsDesignTime() || !bPreviewDesignerPage || !WidgetTree) return;
	auto Show = [this](const TCHAR* Name, bool bVisible)
	{
		if (UWidget* Widget = WidgetTree->FindWidget(Name))
		{
#if WITH_EDITOR
			Widget->bHiddenInDesigner = !bVisible;
#endif
			Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		}
	};
	// Preview only switches real authored sections. It does not rebuild the tree,
	// invent sample values, or replace the designer's brushes, fonts and padding.
	Show(TEXT("Panel_BuildSection"), DesignerPage == EFF_FacilityInfoPage::Overview);
	Show(TEXT("Panel_LiveSection"), DesignerPage == EFF_FacilityInfoPage::Operation);
	Show(TEXT("Panel_SynergySection"), DesignerPage == EFF_FacilityInfoPage::Synergy);
	Show(TEXT("Panel_StaffSection"), DesignerPage == EFF_FacilityInfoPage::Staff);
	Show(TEXT("Panel_UpgradeSection"), DesignerPage == EFF_FacilityInfoPage::Upgrade);
	Show(TEXT("Panel_UpgradeActions"), DesignerPage == EFF_FacilityInfoPage::Upgrade);
	Show(TEXT("Panel_RevenueSection"), DesignerPage == EFF_FacilityInfoPage::Revenue);
	Show(TEXT("Panel_ProgramSection"), DesignerPage == EFF_FacilityInfoPage::Programs);
	Show(TEXT("Widget_LightingControls"), DesignerPage == EFF_FacilityInfoPage::Lighting);
	Show(TEXT("Button_LightingMinimize"), DesignerPage == EFF_FacilityInfoPage::Lighting);
	Show(TEXT("Panel_InspectorNavigation"), true);
	Show(TEXT("Button_TabLighting"), DesignerPage == EFF_FacilityInfoPage::Lighting);
	Show(TEXT("Button_TabPrograms"), DesignerPage == EFF_FacilityInfoPage::Programs);
	Show(TEXT("Panel_RemoveConfirmation"), DesignerPage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(TEXT("Panel_RemoveActions"), DesignerPage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(TEXT("Panel_Actions"), DesignerPage != EFF_FacilityInfoPage::Lighting && DesignerPage != EFF_FacilityInfoPage::RemoveConfirmation);
}

void UFF_FacilityInfoPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Existing layout remains authored in WBP. Only the optional staff section gets a
	// fallback for older assets; an authored section always takes precedence.
	EnsureStaffControls();
	if (Button_AssignStaff) Button_AssignStaff->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleAssignStaffClicked);
	if (Button_UnassignStaff) Button_UnassignStaff->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnassignStaffClicked);
	// Legacy assets can still carry former per-person captions. The finalized Designer
	// stores the current captions itself, so subsequent authored text stays editable.
	if (WidgetTree && !WidgetTree->FindWidget(TEXT("Panel_RequiredBindings")))
	{
		auto SetMetricLabel = [this](const TCHAR* ValueName, const TCHAR* LegacyRow, const FText& Caption)
		{
			for (const FString& Name : {
				FString(ValueName) + TEXT("_Label"),
				FString(TEXT("Clarity_Label_")) + ValueName,
				FString(TEXT("Aurora_Label_")) + ValueName,
				FString(LegacyRow) + TEXT("_Label")})
			{
				if (auto* Label = Cast<UCommonTextBlock>(WidgetTree->FindWidget(FName(*Name))))
					Label->SetText(Caption);
			}
		};
		SetMetricLabel(TEXT("Text_ServiceDuration"), TEXT("ServiceDuration_Row"), LOCTEXT("TeamServiceDuration", "팀당 이용 시간"));
		SetMetricLabel(TEXT("Text_Occupancy"), TEXT("Occupancy_Row"), LOCTEXT("TeamOccupancy", "대기·처리 중 팀"));
		SetMetricLabel(TEXT("Text_RemainingService"), TEXT("RemainingService_Row"), LOCTEXT("RemainingTeams", "남은 서비스 팀"));
		SetMetricLabel(TEXT("Text_ExpectedWait"), TEXT("ExpectedWait_Row"), LOCTEXT("TeamExpectedWait", "한 팀 예상 대기"));
		if (auto* Label = Cast<UCommonTextBlock>(WidgetTree->FindWidget(TEXT("V2_UpgradeLabelTime"))))
			Label->SetText(LOCTEXT("TeamUpgradeTime", "팀당 시간"));
		if (Text_Occupancy)
			Text_Occupancy->SetToolTipText(LOCTEXT("TeamOccupancyTip", "대기·처리 중인 팀 수와 최대 팀 수입니다. 인원수와 관계없이 한 팀이 한 자리를 사용합니다."));
	}
	if (Button_TabOverview) Button_TabOverview->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOverviewClicked);
	if (Button_TabSynergy) Button_TabSynergy->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSynergyClicked);
	if (Button_TabOperation) Button_TabOperation->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOperationClicked);
	if (Button_TabStaff) Button_TabStaff->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStaffClicked);
	if (Button_TabUpgrade) Button_TabUpgrade->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUpgradeTabClicked);
	if (Button_TabLighting) Button_TabLighting->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightingTabClicked);
	if (Button_LightingColorHelp) Button_LightingColorHelp->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightingTabClicked);
	if (Button_DismissLightingIntroduction) Button_DismissLightingIntroduction->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleDismissLightingIntroduction);
	if (Button_TabRevenue) Button_TabRevenue->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRevenueTabClicked);
	if (Button_TabPrograms) Button_TabPrograms->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleProgramsTabClicked);
	if (Button_SynergyPrevious) Button_SynergyPrevious->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSynergyPreviousClicked);
	if (Button_SynergyNext) Button_SynergyNext->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSynergyNextClicked);
	if (Button_LightingMinimize) Button_LightingMinimize->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightingMinimizeClicked);
	if (Button_Favorite) Button_Favorite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFavoriteClicked);
	if (Button_RemoveConfirm) Button_RemoveConfirm->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveConfirmClicked);
	if (Button_RemoveCancel) Button_RemoveCancel->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRemoveCancelClicked);
	if (Button_RevenueRanking) Button_RevenueRanking->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRevenueRankingClicked);
	if (Widget_LightingControls)
	{
		Widget_LightingControls->OnLightingRequested.AddUObject(this, &ThisClass::HandleLightingRequested);
		Widget_LightingControls->OnLightingResetRequested.AddUObject(this, &ThisClass::HandleLightingResetRequested);
		Widget_LightingControls->OnLightingRollbackRequested.AddUObject(this, &ThisClass::HandleLightingRollbackRequested);
		Widget_LightingControls->OnObserveRequested.AddUObject(this, &ThisClass::HandleLightingMinimizeClicked);
		Widget_LightingControls->OnEditFinished.AddUObject(this, &ThisClass::HandleLightingEditFinished);
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

void UFF_FacilityInfoPanel::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	if (IsDesignTime()) return;
	const FVector2D Viewport = Geometry.GetLocalSize();
	if (Viewport.X > 1.f && Viewport.Y > 1.f && !Viewport.Equals(LastLayoutViewport, .5f))
	{
		LastLayoutViewport = Viewport;
		RefreshLightingPanelLayout();
	}
}

FReply UFF_FacilityInfoPanel::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (ActivePage == EFF_FacilityInfoPage::Lighting && Border_Panel &&
			Border_Panel->GetCachedGeometry().AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()).Y < 48.f)
		{
			bDraggingLighting=true;
			LightingDragStart=InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
			LightingDragTranslation=LightingPanelTranslation;
			return FReply::Handled().CaptureMouse(TakeWidget());
		}
		// Card chrome and text consume left-clicks so they cannot select the world behind the panel.
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

// 관리자가 모아 준 시설 정보를 각 행과 탭에 표시한다. 이 함수는 시설의 실제 상태를 변경하지 않는다.
void UFF_FacilityInfoPanel::ApplyViewData(const FFF_FacilityInfoViewData& ViewData)
{
	const bool bSelectionChanged = CurrentViewData.FacilityInstanceId != ViewData.FacilityInstanceId ||
		CurrentViewData.Context != ViewData.Context;
	if (bSelectionChanged) { CancelLightingEdit(); if (bLightingMinimized) HandleLightingMinimizeClicked(); }
	CurrentViewData = ViewData;
	if (bSelectionChanged)
	{
		ActivePage = EFF_FacilityInfoPage::Overview;
		SynergyPageIndex = 0;
		bLightingIntroductionVisible = false;
	}
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
	const bool bShowTeamRevenue = ViewData.FacilityInstanceId.IsValid() && !ViewData.FacilityRevenueText.IsEmpty();
	if (Text_FacilityRevenue)
	{
		Text_FacilityRevenue->SetToolTipText(LOCTEXT("TeamRevenueSettlementHint",
			"이용 요금·시설 강화·시너지를 반영한 팀별 예상 부스 매출입니다.\n이용 완료 시 실제 적립된 부스 매출이 오늘 매출에 누적됩니다.\n유료 프로그램 참가비는 별도로 정산됩니다."));
		Text_FacilityRevenue->SetVisibility(bShowTeamRevenue
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (UWidget* RevenueCard = GetWidgetFromName(TEXT("TeamRevenue_OverviewCard")))
		RevenueCard->SetVisibility(bShowTeamRevenue ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
	HideLegacyClearanceWidgets();
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
	SetText(Text_QueueCapacityValue, ViewData.QueueCapacityValueText);
	SetText(Text_ServiceCapacityValue, ViewData.ServiceCapacityValueText);
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
	if (UWidget* RevenueRow = GetWidgetFromName(TEXT("V2_UpgradeRowRevenue")))
		RevenueRow->SetVisibility(!ViewData.UpgradeCurrentValues.Revenue.IsEmpty() || !ViewData.UpgradeNextValues.Revenue.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
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
	if (Panel_BuildActions)
	{
		Panel_BuildActions->SetVisibility(
			bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_Actions)
	{
		Panel_Actions->SetVisibility(
			bBuildContext
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
	RefreshStaffControls();
	OnViewDataApplied(ViewData);
}

void UFF_FacilityInfoPanel::SetInfoPage(const EFF_FacilityInfoPage Page)
{
	if (ActivePage == Page) return;
	const bool bPageChanged = ActivePage != Page;
	if (bPageChanged && ActivePage == EFF_FacilityInfoPage::Lighting)
	{
		bLightingIntroductionVisible = false;
		CancelLightingEdit();
		if (bLightingMinimized) HandleLightingMinimizeClicked();
	}
	ActivePage = Page;
	if (Page == EFF_FacilityInfoPage::Lighting && Widget_LightingControls && !bLightingIntroductionVisible)
		Widget_LightingControls->BeginEdit();
	RefreshPageVisibility();
	if (bPageChanged && WidgetTree)
		if (UScrollBox* Details = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details"))))
			Details->ScrollToStart();
}

void UFF_FacilityInfoPanel::RefreshPageVisibility()
{
	// Legacy designer panels keep their previous presentation until explicitly migrated.
	if (!Button_TabOverview || !Button_TabOperation || !Button_TabUpgrade)
	{
		RefreshStaffControls();
		return;
	}
	const bool bBuild = CurrentViewData.Context == EFF_FacilityInfoContext::Build;
	if (ActivePage == EFF_FacilityInfoPage::Upgrade && (!bBuild || !CurrentViewData.bHasFacilityLevels))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::RemoveConfirmation && (!bBuild || !CurrentViewData.bCanRemove))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::Synergy && (!Button_TabSynergy || !Panel_SynergySection))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::Staff && (!Panel_StaffSection || !CurrentViewData.bShowStaffSection))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::Lighting && !CurrentViewData.bHasLightingRig)
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage != EFF_FacilityInfoPage::Lighting) bLightingIntroductionVisible = false;
	const bool bRevenueAvailable = CurrentViewData.bShowRevenueSection && CurrentViewData.FacilityInstanceId.IsValid();
	if (ActivePage == EFF_FacilityInfoPage::Revenue && (!Button_TabRevenue || !bRevenueAvailable))
		ActivePage = EFF_FacilityInfoPage::Overview;
	if (ActivePage == EFF_FacilityInfoPage::Programs && (!Button_TabPrograms || !CurrentViewData.bShowProgramSection))
		ActivePage = EFF_FacilityInfoPage::Overview;
	RefreshStaffControls();
	auto Show = [](UWidget* Widget, const bool bShow)
	{
		if (Widget) Widget->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	};
	Show(Panel_BuildSection, ActivePage == EFF_FacilityInfoPage::Overview);
	Show(Panel_SynergySection, ActivePage == EFF_FacilityInfoPage::Synergy);
	if (ActivePage == EFF_FacilityInfoPage::Synergy) RefreshSynergyEntries();
	Show(Panel_LiveSection, ActivePage == EFF_FacilityInfoPage::Operation);
	Show(Panel_UpgradeSection, ActivePage == EFF_FacilityInfoPage::Upgrade);
	Show(Panel_ProgramSection, CurrentViewData.bShowProgramSection && ActivePage ==
		(Button_TabPrograms ? EFF_FacilityInfoPage::Programs : EFF_FacilityInfoPage::Overview));
	// The explicit compact migration gives revenue its own direct-access page. Legacy
	// designer assets retain their existing combined overview/operation presentation.
	Show(Panel_RevenueSection, bRevenueAvailable && (Button_TabRevenue
		? ActivePage == EFF_FacilityInfoPage::Revenue
		: ActivePage == EFF_FacilityInfoPage::Overview || ActivePage == EFF_FacilityInfoPage::Operation));
	Show(Panel_RemoveConfirmation, ActivePage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(Panel_RemoveActions, ActivePage == EFF_FacilityInfoPage::RemoveConfirmation);
	Show(Panel_UpgradeActions, ActivePage == EFF_FacilityInfoPage::Upgrade && CurrentViewData.bShowUpgradeAction);
	Show(Button_RevenueRanking, ActivePage == (Button_TabRevenue ? EFF_FacilityInfoPage::Revenue : EFF_FacilityInfoPage::Operation) &&
		CurrentViewData.FacilityInstanceId.IsValid());
	// The persistent header still identifies the facility. Give task-specific pages their
	// full scroll area instead of repeating the overview thumbnail above every action.
	if (WidgetTree) Show(WidgetTree->FindWidget(TEXT("Summary_Card")), !WidgetTree->FindWidget(TEXT("Aurora_InfoV7")) && ActivePage == EFF_FacilityInfoPage::Overview);
	// The authored lighting tab shares the same facility identity and scroll surface.
	if (Widget_LightingControls)
	{
		Widget_LightingControls->ApplyViewData(CurrentViewData);
		Show(Widget_LightingControls, !bLightingIntroductionVisible && CurrentViewData.bHasLightingRig && (Button_TabLighting
			? ActivePage == EFF_FacilityInfoPage::Lighting : ActivePage == EFF_FacilityInfoPage::Overview));
	}
	Show(Button_TabLighting, CurrentViewData.bHasLightingRig);
	Show(Button_TabRevenue, bRevenueAvailable);
	Show(Button_TabPrograms, CurrentViewData.bShowProgramSection);
	Show(Button_LightingMinimize, ActivePage == EFF_FacilityInfoPage::Lighting && !bLightingIntroductionVisible);
	Show(Button_TabUpgrade, (bBuild && CurrentViewData.bHasFacilityLevels) || CurrentViewData.bHasLightingRig);
	if (Button_TabUpgrade)
	{
		Button_TabUpgrade->SetRenderOpacity(bBuild && CurrentViewData.bHasFacilityLevels ? 1.f : .42f);
		Button_TabUpgrade->SetToolTipText(CurrentViewData.bHasFacilityLevels ? FText::GetEmpty() : LOCTEXT("NoFacilityUpgrade", "이 시설은 강화가 지원되지 않습니다."));
	}
	auto UpdateTab=[this](UButton* Tab,const EFF_FacilityInfoPage Page,const bool bAvailable=true)
	{
		if(!Tab)return;
		if(!AuthoredTabStyles.Contains(Page))AuthoredTabStyles.Add(Page,Tab->GetStyle());
		FButtonStyle TabStyle=AuthoredTabStyles.FindChecked(Page);
		const bool bSelected=bAvailable&&ActivePage==Page;
		if(bSelected)TabStyle.Normal=TabStyle.Hovered=TabStyle.Pressed=TabStyle.Disabled;
		Tab->SetStyle(TabStyle);
		Tab->SetIsEnabled(bAvailable);
		if(Panel_PersistentIdentity)
			if(auto* Label=Cast<UCommonTextBlock>(Tab->GetContent()))
				Label->SetColorAndOpacity(FSlateColor(bSelected?SelectedTabTextColor:InactiveTabTextColor));
	};
	UpdateTab(Button_TabOverview,EFF_FacilityInfoPage::Overview);
	UpdateTab(Button_TabSynergy,EFF_FacilityInfoPage::Synergy);
	UpdateTab(Button_TabOperation,EFF_FacilityInfoPage::Operation);
	UpdateTab(Button_TabStaff,EFF_FacilityInfoPage::Staff,CurrentViewData.bShowStaffSection);
	UpdateTab(Button_TabUpgrade,EFF_FacilityInfoPage::Upgrade,bBuild&&CurrentViewData.bHasFacilityLevels);
	UpdateTab(Button_TabLighting,EFF_FacilityInfoPage::Lighting,CurrentViewData.bHasLightingRig);
	UpdateTab(Button_TabRevenue,EFF_FacilityInfoPage::Revenue,bRevenueAvailable);
	UpdateTab(Button_TabPrograms,EFF_FacilityInfoPage::Programs,CurrentViewData.bShowProgramSection);
	Show(Panel_Actions, ActivePage != EFF_FacilityInfoPage::Lighting && ActivePage != EFF_FacilityInfoPage::RemoveConfirmation &&
		bBuild);
	RefreshLightingIntroduction();
	RefreshLightingPanelLayout();
}

void UFF_FacilityInfoPanel::RefreshSynergyEntries()
{
	SetText(Text_SynergySummary, CurrentViewData.SynergySummaryText);
	if (!VerticalBox_SynergyEntries) return;
	const int32 EntryCount = CurrentViewData.SynergyEntries.Num();
	const bool bPaginated = Button_SynergyPrevious && Button_SynergyNext && Text_SynergyPage;
	SynergyPageIndex = FMath::Clamp(SynergyPageIndex, 0, FMath::Max(0, EntryCount - 1));
	const int32 VisibleCount = bPaginated ? FMath::Min(1, EntryCount) : EntryCount;
	if (bPaginated)
	{
		Button_SynergyPrevious->SetIsEnabled(SynergyPageIndex > 0);
		Button_SynergyNext->SetIsEnabled(SynergyPageIndex + 1 < EntryCount);
		SetText(Text_SynergyPage, FText::Format(LOCTEXT("SynergyPage", "{0} / {1}"),
			FText::AsNumber(EntryCount > 0 ? SynergyPageIndex + 1 : 0), FText::AsNumber(EntryCount)));
		if (WidgetTree)
			if (UWidget* Pager = WidgetTree->FindWidget(TEXT("Clarity_SynergyPager")))
				Pager->SetVisibility(EntryCount > 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (VisibleCount > SynergyEntryPool.Num() && !LoadedSynergyEntryClass && !bSynergyEntryLoadFailed)
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
	while (SynergyEntryPool.Num() < VisibleCount && LoadedSynergyEntryClass)
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
		if (Index < VisibleCount)
		{
			Row->ApplyEntry(CurrentViewData.SynergyEntries[bPaginated ? SynergyPageIndex : Index]);
			Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else Row->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (VisibleCount > 0 && SynergyEntryPool.Num() < VisibleCount)
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
void UFF_FacilityInfoPanel::HandleStaffClicked() { SetInfoPage(EFF_FacilityInfoPage::Staff); }
void UFF_FacilityInfoPanel::HandleUpgradeTabClicked() { SetInfoPage(EFF_FacilityInfoPage::Upgrade); }
bool UFF_FacilityInfoPanel::HasSeenLightingIntroduction()
{
	// This is one local player's learning history, shared by stages and lighting facilities.
	// Read the cached config as well so a recreated panel or another entry point stays in sync.
	if (!bHasSeenLightingIntroduction && GConfig && !GGameUserSettingsIni.IsEmpty())
		GConfig->GetBool(LightingIntroductionSection, LightingIntroductionSeenKey,
			bHasSeenLightingIntroduction, GGameUserSettingsIni);
	return bHasSeenLightingIntroduction;
}

void UFF_FacilityInfoPanel::RefreshLightingIntroduction()
{
	const bool bAvailable = CurrentViewData.bHasLightingRig && CurrentViewData.FacilityInstanceId.IsValid() &&
		CurrentViewData.Context != EFF_FacilityInfoContext::None;
	const bool bCanEdit = bAvailable && CurrentViewData.bCanEditLighting;
	if (Panel_LightingColorHelp) Panel_LightingColorHelp->SetVisibility(
		bAvailable && ActivePage == EFF_FacilityInfoPage::Overview
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (Button_LightingColorHelp)
	{
		Button_LightingColorHelp->SetIsEnabled(bCanEdit);
		Button_LightingColorHelp->SetToolTipText(bCanEdit
			? LOCTEXT("LightingColorHelpTip", "이 시설의 조명 색상과 밝기를 직접 바꿀 수 있어요.")
			: LOCTEXT("LightingColorUnavailableTip", "지금은 이 시설의 조명을 편집할 수 없습니다."));
	}
	if (Border_LightingIntroductionHint) Border_LightingIntroductionHint->SetVisibility(
		bCanEdit && !HasSeenLightingIntroduction() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Button_TabLighting) Button_TabLighting->SetToolTipText(bCanEdit
		? LOCTEXT("LightingTabAvailable", "조명 지원 시설 · 색상과 밝기를 바꿀 수 있습니다.")
		: LOCTEXT("LightingColorUnavailableTip", "지금은 이 시설의 조명을 편집할 수 없습니다."));
	if (Panel_LightingIntroduction) Panel_LightingIntroduction->SetVisibility(
		bLightingIntroductionVisible && ActivePage == EFF_FacilityInfoPage::Lighting && bAvailable
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UFF_FacilityInfoPanel::HandleLightingTabClicked()
{
	if (!CurrentViewData.bHasLightingRig || !CurrentViewData.FacilityInstanceId.IsValid() ||
		CurrentViewData.Context == EFF_FacilityInfoContext::None ||
		!CurrentViewData.bCanEditLighting || !IsVisible() || !GetIsEnabled() || !Widget_LightingControls) return;
	const bool bShowIntroduction = !HasSeenLightingIntroduction() &&
		Panel_LightingIntroduction && Button_DismissLightingIntroduction;
	if (bShowIntroduction) bLightingIntroductionVisible = true;
	SetInfoPage(EFF_FacilityInfoPage::Lighting);
	if (!bShowIntroduction)
	{
		// Closing cancels the draft but keeps the selected page. Re-entering that page
		// must start a fresh transaction even when SetInfoPage has nothing to switch.
		if (!bLightingIntroductionVisible) Widget_LightingControls->BeginEdit();
		return;
	}
	// Consume the reminder only after a valid click has revealed the authored explanation.
	RefreshPageVisibility();
	bHasSeenLightingIntroduction = true;
	if (GConfig && !GGameUserSettingsIni.IsEmpty())
	{
		GConfig->SetBool(LightingIntroductionSection, LightingIntroductionSeenKey, true, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
	RefreshLightingIntroduction();
}

void UFF_FacilityInfoPanel::HandleDismissLightingIntroduction()
{
	if (!bLightingIntroductionVisible || !IsVisible()) return;
	bLightingIntroductionVisible = false;
	if (Widget_LightingControls && CurrentViewData.bCanEditLighting) Widget_LightingControls->BeginEdit();
	RefreshPageVisibility();
	if (WidgetTree)
		if (UScrollBox* Details = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details"))))
			Details->ScrollToStart();
}
void UFF_FacilityInfoPanel::HandleRevenueTabClicked() { SetInfoPage(EFF_FacilityInfoPage::Revenue); }
void UFF_FacilityInfoPanel::HandleProgramsTabClicked() { SetInfoPage(EFF_FacilityInfoPage::Programs); }
void UFF_FacilityInfoPanel::HandleSynergyPreviousClicked()
{
	if (ActivePage != EFF_FacilityInfoPage::Synergy || SynergyPageIndex <= 0) return;
	--SynergyPageIndex;
	RefreshSynergyEntries();
}
void UFF_FacilityInfoPanel::HandleSynergyNextClicked()
{
	if (ActivePage != EFF_FacilityInfoPage::Synergy || SynergyPageIndex + 1 >= CurrentViewData.SynergyEntries.Num()) return;
	++SynergyPageIndex;
	RefreshSynergyEntries();
}
void UFF_FacilityInfoPanel::HandleFavoriteClicked()
{
	if (!CurrentViewData.FacilityDefinitionId.IsNone()) OnFavoriteRequested.Broadcast();
}
void UFF_FacilityInfoPanel::HandleRevenueRankingClicked()
{
	if (ActivePage == (Button_TabRevenue ? EFF_FacilityInfoPage::Revenue : EFF_FacilityInfoPage::Operation) &&
		CurrentViewData.FacilityInstanceId.IsValid())
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
	if (!bOpen)
	{
		bLightingIntroductionVisible = false;
		CancelLightingEdit();
	}
	// AddToPlayerScreen gives this UUserWidget a full-screen outer slot. SelfHitTestInvisible keeps
	// that wrapper transparent to world clicks while the visible Border and its buttons remain live.
	SetVisibility(bOpen ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	RefreshPageVisibility();
}

bool UFF_FacilityInfoPanel::IsPointerOverInteractivePanel() const
{
	if (GetVisibility() == ESlateVisibility::Collapsed ||
		GetVisibility() == ESlateVisibility::Hidden || !Border_Panel)
	{
		return false;
	}
	if (bDraggingLighting || HasMouseCapture() || (Widget_LightingControls && Widget_LightingControls->IsPointerCaptured()) || Border_Panel->IsHovered())
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

void UFF_FacilityInfoPanel::EnsureStaffControls()
{
	if (!WidgetTree || IsDesignTime()) return;
	// Keep older authored panels usable until the explicit WBP migration is run.
	if (!Button_TabStaff && Button_TabOperation)
	{
		if (auto* Tabs = Cast<UHorizontalBox>(Button_TabOperation->GetParent()))
		{
			Button_TabStaff = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_TabStaff"));
			Button_TabStaff->SetStyle(Button_TabOperation->GetStyle());
			auto* Label = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Text_TabStaff"));
			Label->SetText(LOCTEXT("StaffTab", "스태프"));
			if (auto* Reference = Cast<UCommonTextBlock>(Button_TabOperation->GetContent()))
			{
				Label->SetFont(Reference->GetFont());
				Label->SetColorAndOpacity(Reference->GetColorAndOpacity());
				Label->SetJustification(ETextJustify::Center);
			}
			Button_TabStaff->AddChild(Label);
			auto* StaffTabSlot = Tabs->AddChildToHorizontalBox(Button_TabStaff);
			StaffTabSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			StaffTabSlot->SetPadding(FMargin(2.f, 0.f));
			StaffTabSlot->SetVerticalAlignment(VAlign_Fill);
			if (const auto* ReferenceSlot = Cast<UHorizontalBoxSlot>(Button_TabOperation->Slot))
			{
				StaffTabSlot->SetSize(ReferenceSlot->GetSize());
				StaffTabSlot->SetPadding(ReferenceSlot->GetPadding());
				StaffTabSlot->SetHorizontalAlignment(ReferenceSlot->GetHorizontalAlignment());
				StaffTabSlot->SetVerticalAlignment(ReferenceSlot->GetVerticalAlignment());
			}
			Tabs->ShiftChild(Tabs->GetChildIndex(Button_TabOperation) + 1, Button_TabStaff);
		}
	}
	// Older inspector assets may have no secondary row, even when a staff section
	// has already been authored. Keep the same four-page contract as the migration.
	if (Button_TabStaff && Button_TabOperation)
		if (auto* Tabs = Cast<UHorizontalBox>(Button_TabOperation->GetParent()))
			{
				TArray<UWidget*> Optional;
				for (const TCHAR* Name : {TEXT("Button_TabUpgrade"), TEXT("Button_TabRevenue"),
					TEXT("Button_TabPrograms"), TEXT("Button_TabLighting")})
					if (UWidget* Tab = WidgetTree->FindWidget(Name))
						if (Tab->GetParent() == Tabs) Optional.Add(Tab);
				if (!Optional.IsEmpty())
				{
					auto* Tools = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Reference_AdditionalFacilityPages")));
					if (!Tools) Tools = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Clarity_SecondaryTabs")));
					if (!Tools) Tools = Cast<UHorizontalBox>(WidgetTree->FindWidget(TEXT("Panel_InspectorTools")));
					if (!Tools)
						for (const TCHAR* Name : {TEXT("Button_TabRevenue"), TEXT("Button_TabPrograms")})
							if (UWidget* Tab = WidgetTree->FindWidget(Name))
								if (auto* Row = Cast<UHorizontalBox>(Tab->GetParent()))
									if (Row != Tabs) { Tools = Row; break; }
					if (!Tools)
					{
						UWidget* NavigationAnchor = Tabs;
						while (NavigationAnchor->GetParent())
						{
							if (auto* NavigationColumn = Cast<UVerticalBox>(NavigationAnchor->GetParent()))
							{
								Tools = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Panel_InspectorTools"));
								NavigationColumn->AddChildToVerticalBox(Tools)->SetPadding(FMargin(8, 0, 8, 6));
								NavigationColumn->ShiftChild(NavigationColumn->GetChildIndex(NavigationAnchor) + 1, Tools);
								break;
							}
							NavigationAnchor = NavigationAnchor->GetParent();
						}
					}
					if (Tools && Tools != Tabs) for (UWidget* Tab : Optional)
					{
						Tab->RemoveFromParent();
						auto* ToolSlot = Tools->AddChildToHorizontalBox(Tab);
						ToolSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
						ToolSlot->SetPadding(FMargin(2.f, 0.f));
					}
				}
				int32 PrimaryIndex = 0;
				for (UButton* Tab : {Button_TabOverview.Get(), Button_TabSynergy.Get(), Button_TabOperation.Get(), Button_TabStaff.Get()})
					if (Tab && Tab->GetParent() == Tabs) Tabs->ShiftChild(PrimaryIndex++, Tab);
			}
	if (Panel_StaffSection) return;
	// Add within the existing scroll content, never replace/reparent the designer tree.
	UPanelWidget* Host = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Content")));
	if (!Host) Host = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("ScrollBox_Details")));
	if (!Host) return;

	UVerticalBox* Section = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Panel_StaffSection"));
	Text_StaffSummary = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), TEXT("Text_StaffSummary"));
	Text_StaffSummary->SetFont(Text_Footprint ? Text_Footprint->GetFont() : FFBuildTypography::Make(FFBuildTypography::ERole::Body, 14));
	if (Text_Footprint) Text_StaffSummary->SetColorAndOpacity(Text_Footprint->GetColorAndOpacity());
	Text_StaffSummary->SetAutoWrapText(true);
	Section->AddChildToVerticalBox(Text_StaffSummary)->SetPadding(FMargin(0, 6, 0, 4));
	UHorizontalBox* Actions = WidgetTree->ConstructWidget<UHorizontalBox>();
	Section->AddChildToVerticalBox(Actions);
	auto MakeButton = [this, Actions](const FName Name, const FText& Caption)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		if (Button_TabOverview) Button->SetStyle(Button_TabOverview->GetStyle());
		UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
		Label->SetText(Caption);
		Label->SetFont(Text_StaffSummary->GetFont());
		Label->SetColorAndOpacity(Text_StaffSummary->GetColorAndOpacity());
		Button->AddChild(Label);
		Actions->AddChildToHorizontalBox(Button)->SetPadding(FMargin(0, 3, 8, 8));
		return Button;
	};
	Button_AssignStaff = MakeButton(TEXT("Button_AssignStaff"), LOCTEXT("AssignOneStaff", "+ 배정"));
	Button_UnassignStaff = MakeButton(TEXT("Button_UnassignStaff"), LOCTEXT("UnassignOneStaff", "- 해제"));
	Host->AddChild(Section);
	Panel_StaffSection = Section;
	Panel_StaffSection->SetVisibility(ESlateVisibility::Collapsed);
}

void UFF_FacilityInfoPanel::RefreshStaffControls()
{
	if (Panel_StaffSection)
	{
		const bool bShow = CurrentViewData.bShowStaffSection &&
			ActivePage == EFF_FacilityInfoPage::Staff;
		Panel_StaffSection->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Panel_StaffSection->SetToolTipText(CurrentViewData.StaffAssignmentHint);
	}
	SetText(Text_StaffSummary, CurrentViewData.StaffSummaryText);
	if (Button_TabStaff)
	{
		Button_TabStaff->SetVisibility(CurrentViewData.bShowStaffSection
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_TabStaff->SetIsEnabled(CurrentViewData.bShowStaffSection);
		Button_TabStaff->SetToolTipText(CurrentViewData.bShowStaffSection
			? LOCTEXT("StaffTabTip", "이 시설의 스태프 배정 상태를 확인하고 + 배정 또는 - 해제로 변경합니다.")
			: FText::GetEmpty());
	}
	if (Button_AssignStaff)
	{
		Button_AssignStaff->SetIsEnabled(CurrentViewData.bCanAssignStaff);
		Button_AssignStaff->SetToolTipText(LOCTEXT("AssignStaffTip", "미배정 대기 스태프 1명을 이 안내 시설에 배정합니다. 빈자리와 보유 스태프가 필요합니다."));
	}
	if (Button_UnassignStaff)
	{
		Button_UnassignStaff->SetIsEnabled(CurrentViewData.bCanUnassignStaff);
		Button_UnassignStaff->SetToolTipText(LOCTEXT("UnassignStaffTip", "이 시설의 대기 스태프 1명을 미배정 상태로 돌립니다. 출동 중인 스태프는 복귀 후 해제할 수 있습니다."));
	}
}

void UFF_FacilityInfoPanel::HandleAssignStaffClicked()
{
	if (ActivePage == EFF_FacilityInfoPage::Staff && CurrentViewData.bShowStaffSection &&
		CurrentViewData.bCanAssignStaff && CurrentViewData.FacilityInstanceId.IsValid()) OnAssignStaffRequested.Broadcast();
}

void UFF_FacilityInfoPanel::HandleUnassignStaffClicked()
{
	if (ActivePage == EFF_FacilityInfoPage::Staff && CurrentViewData.bShowStaffSection &&
		CurrentViewData.bCanUnassignStaff && CurrentViewData.FacilityInstanceId.IsValid()) OnUnassignStaffRequested.Broadcast();
}

void UFF_FacilityInfoPanel::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
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

void UFF_FacilityInfoPanel::CancelLightingEdit() { if(Widget_LightingControls) Widget_LightingControls->CancelEdit(); }
bool UFF_FacilityInfoPanel::IsLightingTextInputFocused() const
{ return GetVisibility()!=ESlateVisibility::Collapsed && Widget_LightingControls && Widget_LightingControls->IsEditingText(); }
void UFF_FacilityInfoPanel::HandleLightingRollbackRequested(FGuid FacilityId,const FFFZSFacilityLightingState& State)
{ OnLightingRollbackRequested.Broadcast(FacilityId,State); }
void UFF_FacilityInfoPanel::HandleLightingEditFinished() { SetInfoPage(EFF_FacilityInfoPage::Overview); }

void UFF_FacilityInfoPanel::HandleLightingMinimizeClicked()
{
	if(!WidgetTree || ActivePage!=EFF_FacilityInfoPage::Lighting || bLightingIntroductionVisible) return;
	USizeBox* Size=Cast<USizeBox>(WidgetTree->FindWidget(TEXT("Panel_Size")));
	UCanvasPanelSlot* PanelSlot=Size?Cast<UCanvasPanelSlot>(Size->Slot):nullptr;
	if(!PanelSlot) return;
	bLightingMinimized=!bLightingMinimized;
	if(!bLightingMinimized)
	{
		// A compact card can be dragged to the bottom; expanding must keep its footer on screen.
		const double Top = PanelSlot->GetPosition().Y;
		const double MaxTranslation = GetCachedGeometry().GetLocalSize().Y - AuthoredPanelSize.Y - Top - 8.0;
		LightingPanelTranslation.Y=FMath::Clamp(LightingPanelTranslation.Y,
			8.0-Top,FMath::Max(8.0-Top,MaxTranslation));
	}
	RefreshLightingPanelLayout();
	if(FSlateApplication::IsInitialized()) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::Cleared);
}

void UFF_FacilityInfoPanel::RefreshLightingPanelLayout()
{
	if(!Button_TabLighting || !WidgetTree) return;
	const bool bLighting=ActivePage==EFF_FacilityInfoPage::Lighting;
	const bool bClarityV2=WidgetTree->FindWidget(TEXT("ClarityV2_Info"))!=nullptr;
	// NativeTick receives the current allocation before GetCachedGeometry catches
	// up. Using the previous frame here can leave the panel permanently clamped
	// to the old window height after a resize.
	const FVector2D Viewport = LastLayoutViewport.IsNearlyZero()
		? GetCachedGeometry().GetLocalSize() : LastLayoutViewport;
	if(USizeBox* Size=Cast<USizeBox>(WidgetTree->FindWidget(TEXT("Panel_Size"))))
	{
		if (AuthoredPanelSize.IsNearlyZero())
		{
			const auto* AuthoredCanvasSlot = Cast<UCanvasPanelSlot>(Size->Slot);
			AuthoredPanelSize.X = Size->IsWidthOverride() ? Size->GetWidthOverride() : (AuthoredCanvasSlot ? AuthoredCanvasSlot->GetSize().X : 456.f);
			AuthoredPanelSize.Y = Size->IsHeightOverride() ? Size->GetHeightOverride() : (AuthoredCanvasSlot ? AuthoredCanvasSlot->GetSize().Y : 620.f);
		}
		const float DesiredWidth = bLighting ? LightingPanelWidth : float(AuthoredPanelSize.X);
		const float DesiredHeight = bLightingMinimized ? LightingCompactHeight : float(AuthoredPanelSize.Y);
		const float PanelWidth = Viewport.X > 1.f ? FMath::Min(DesiredWidth, FMath::Max(200.f, float(Viewport.X) - 36.f)) : DesiredWidth;
		const float PanelHeight = Viewport.Y > 1.f ? FMath::Min(DesiredHeight, FMath::Max(100.f, float(Viewport.Y) - 36.f)) : DesiredHeight;
		if (!Size->IsWidthOverride() || !FMath::IsNearlyEqual(Size->GetWidthOverride(), PanelWidth)) Size->SetWidthOverride(PanelWidth);
		if (!Size->IsHeightOverride() || !FMath::IsNearlyEqual(Size->GetHeightOverride(), PanelHeight)) Size->SetHeightOverride(PanelHeight);
		if(UCanvasPanelSlot* PanelSlot=Cast<UCanvasPanelSlot>(Size->Slot))
		{
			// Respect the authored anchor, alignment and position. Only a fixed-size
			// canvas slot needs its allocated size updated alongside the SizeBox.
			if (!PanelSlot->GetAnchors().IsStretchedHorizontal() && !PanelSlot->GetAnchors().IsStretchedVertical() &&
				!PanelSlot->GetSize().Equals(FVector2D(PanelWidth, PanelHeight)))
				PanelSlot->SetSize(FVector2D(PanelWidth, PanelHeight));
		}
		Size->SetRenderTranslation(bLighting?LightingPanelTranslation:FVector2D::ZeroVector);
	}
	if(UWidget* Shadow=WidgetTree->FindWidget(TEXT("Panel_ShadowSize"))) Shadow->SetVisibility(bLighting?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
	if(UWidget* Body=WidgetTree->FindWidget(TEXT("Body_Surface"))) Body->SetVisibility(bLightingMinimized?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
	if(bClarityV2) if(UWidget* InspectorNavigation=WidgetTree->FindWidget(TEXT("Panel_InspectorNavigation")))
		InspectorNavigation->SetVisibility(bLightingMinimized?ESlateVisibility::Collapsed:ESlateVisibility::SelfHitTestInvisible);
	if(Panel_PersistentIdentity) Panel_PersistentIdentity->SetVisibility(bLightingMinimized?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
	if(Panel_LightingCompact) Panel_LightingCompact->SetVisibility(bLightingMinimized?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	SetText(Text_LightingMinimize,FText::FromString(bLightingMinimized?TEXT("펼치기"):TEXT("접기")));
	SetText(Text_LightingCompactSummary,FText::Format(LOCTEXT("CompactLightingV7","{0}\n{1}  ·  밝기 {2}%  ·  {3}\nWASD 이동  ·  Q / E 회전  ·  휠 확대 / 축소"),
		CurrentViewData.DisplayName,
		CurrentViewData.LightingState.bOverrideColor?FText::FromString(TEXT("#")+CurrentViewData.LightingState.Color.ToFColorSRGB().ToHex().Left(6)):LOCTEXT("OriginalLight","원래 조명"),
		FText::AsNumber(FMath::RoundToInt(CurrentViewData.LightingState.BrightnessMultiplier*100)),
		CurrentViewData.LightingState.bEnabled?LOCTEXT("LightOn","켜짐"):LOCTEXT("LightOff","꺼짐")));
}

FReply UFF_FacilityInfoPanel::NativeOnMouseMove(const FGeometry& Geometry,const FPointerEvent& Event)
{
	if(!bDraggingLighting || !Border_Panel) return Super::NativeOnMouseMove(Geometry,Event);
	const FVector2D Delta=Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition())-LightingDragStart;
	FVector2D Candidate=LightingDragTranslation+Delta;
	const FVector2D CurrentTop=Geometry.AbsoluteToLocal(Border_Panel->GetCachedGeometry().GetAbsolutePosition());
	const FVector2D BaseTop=CurrentTop-LightingPanelTranslation;
	const FVector2D PanelExtent=Border_Panel->GetCachedGeometry().GetLocalSize();
	Candidate.X=FMath::Clamp(Candidate.X,8.0-BaseTop.X,FMath::Max(8.0,Geometry.GetLocalSize().X-PanelExtent.X-8.0)-BaseTop.X);
	Candidate.Y=FMath::Clamp(Candidate.Y,8.0-BaseTop.Y,FMath::Max(8.0,Geometry.GetLocalSize().Y-PanelExtent.Y-8.0)-BaseTop.Y);
	LightingPanelTranslation=Candidate; RefreshLightingPanelLayout(); return FReply::Handled();
}
FReply UFF_FacilityInfoPanel::NativeOnMouseButtonUp(const FGeometry& Geometry,const FPointerEvent& Event)
{ if(bDraggingLighting && Event.GetEffectingButton()==EKeys::LeftMouseButton){bDraggingLighting=false;return FReply::Handled().ReleaseMouseCapture();}return Super::NativeOnMouseButtonUp(Geometry,Event); }
FReply UFF_FacilityInfoPanel::NativeOnMouseWheel(const FGeometry& Geometry,const FPointerEvent& Event)
{ return IsPointerOverInteractivePanel()?FReply::Handled():Super::NativeOnMouseWheel(Geometry,Event); }
void UFF_FacilityInfoPanel::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{ bDraggingLighting=false; Super::NativeOnMouseCaptureLost(Event); }

#undef LOCTEXT_NAMESPACE
