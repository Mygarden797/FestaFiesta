// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoPanel.h"
#include "UI/Facility/FF_FacilityInfoUpgradeLayout.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/InputSettings.h"
#include "HAL/PlatformTime.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoPanel"

void UFF_FacilityInfoPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureMinigameButton();
	EnsureProgramSection();
	EnsureUpgradeSection();

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
	CurrentViewData = ViewData;

	SetText(Text_Context, ViewData.ContextTitle);
	SetText(Text_FacilityName, ViewData.DisplayName);
	SetText(Text_Category, ViewData.CategoryText);
	SetText(Text_Zone, ViewData.ZoneText);
	SetText(Text_FacilityState, ViewData.FacilityStateText);
	SetText(Text_FacilityRevenue, ViewData.FacilityRevenueText);
	if (Text_FacilityRevenue)
	{
		Text_FacilityRevenue->SetVisibility(ViewData.FacilityInstanceId.IsValid()
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
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
		OnRemoveRequested.Broadcast();
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

void UFF_FacilityInfoPanel::EnsureUpgradeSection()
{
	if (!WidgetTree)
	{
		return;
	}
	if (!Text_FacilityRevenue)
	{
		if (UVerticalBox* Meta = FFFacilityInfoUpgradeLayout::FindRevenueParent(*WidgetTree))
		{
			Text_FacilityRevenue = WidgetTree->ConstructWidget<UCommonTextBlock>(
				UCommonTextBlock::StaticClass(), TEXT("Text_FacilityRevenue"));
			Text_FacilityRevenue->SetAutoWrapText(true);
			if (Text_FacilityState)
			{
				FSlateFontInfo Font = Text_FacilityState->GetFont();
				Font.Size = 11;
				Text_FacilityRevenue->SetFont(Font);
				Text_FacilityRevenue->SetColorAndOpacity(Text_FacilityState->GetColorAndOpacity());
			}
			Meta->AddChildToVerticalBox(Text_FacilityRevenue);
			if (UWidget* State = FFFacilityInfoUpgradeLayout::FindContentChild(Text_FacilityState, *Meta))
			{
				Meta->ShiftChild(Meta->GetChildIndex(State) + 1, Text_FacilityRevenue);
			}
		}
	}
	UVerticalBox* Section = Cast<UVerticalBox>(Panel_UpgradeSection);
	if (!Section)
	{
		UVerticalBox* Content = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Content")));
		if (Panel_UpgradeSection || !Content)
		{
			return;
		}
		Section = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("Panel_UpgradeSection"));
		Panel_UpgradeSection = Section;
		Content->AddChildToVerticalBox(Section);
	}
	bool bOrderChanged = false;
	FFFacilityInfoUpgradeLayout::PlaceAfterSummary(*WidgetTree, *Section, bOrderChanged);
	auto EnsureText = [this, Section](TObjectPtr<UCommonTextBlock>& Target, const FName Name)
	{
		if (!Target)
		{
			Target = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass(), Name);
			Target->SetAutoWrapText(true);
			if (Text_ServiceDuration)
			{
				Target->SetFont(Text_ServiceDuration->GetFont());
				Target->SetColorAndOpacity(Text_ServiceDuration->GetColorAndOpacity());
			}
			Section->AddChildToVerticalBox(Target);
		}
	};
	EnsureText(Text_FacilityLevel, TEXT("Text_FacilityLevel"));
	EnsureText(Text_UpgradeEffect, TEXT("Text_UpgradeEffect"));
	EnsureText(Text_UpgradeCost, TEXT("Text_UpgradeCost"));
	EnsureText(Text_UpgradeReason, TEXT("Text_UpgradeReason"));
	if (!Button_Upgrade)
	{
		Button_Upgrade = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Upgrade"));
		if (Button_ScheduleProgram)
		{
			Button_Upgrade->SetStyle(Button_ScheduleProgram->GetStyle());
		}
		Section->AddChildToVerticalBox(Button_Upgrade);
	}
	if (!Text_Upgrade && !Button_Upgrade->GetContent())
	{
		Text_Upgrade = WidgetTree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), TEXT("Text_Upgrade"));
		if (Text_ScheduleProgram)
		{
			Text_Upgrade->SetFont(Text_ScheduleProgram->GetFont());
			Text_Upgrade->SetColorAndOpacity(Text_ScheduleProgram->GetColorAndOpacity());
		}
		Text_Upgrade->SetJustification(ETextJustify::Center);
		Button_Upgrade->SetContent(Text_Upgrade);
	}
}

void UFF_FacilityInfoPanel::EnsureMinigameButton()
{
	if (Button_StartMinigame || !WidgetTree)
	{
		return;
	}

	UVerticalBox* LiveSection = Cast<UVerticalBox>(Panel_LiveSection);
	if (!LiveSection)
	{
		return;
	}

	Button_StartMinigame = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("Button_StartMinigame"));
	Text_StartMinigame = WidgetTree->ConstructWidget<UCommonTextBlock>(
		UCommonTextBlock::StaticClass(), TEXT("Text_StartMinigame"));
	if (!Button_StartMinigame || !Text_StartMinigame)
	{
		Button_StartMinigame = nullptr;
		Text_StartMinigame = nullptr;
		return;
	}

	Text_StartMinigame->SetText(LOCTEXT("StartMinigameFallback", "게임 시작"));
	Text_StartMinigame->SetJustification(ETextJustify::Center);
	Button_StartMinigame->SetContent(Text_StartMinigame);
	LiveSection->AddChildToVerticalBox(Button_StartMinigame);
}

void UFF_FacilityInfoPanel::EnsureProgramSection()
{
	// This is a compatibility shim for an older designer asset. It creates only the missing
	// program block; the WBP builder/migration owns the normal authored layout and styling.
	UVerticalBox* ProgramSection = Cast<UVerticalBox>(Panel_ProgramSection);
	if (!ProgramSection)
	{
		if (Panel_ProgramSection || !WidgetTree)
		{
			return;
		}

		UVerticalBox* Content = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("Panel_Content")));
		if (!Content)
		{
			return;
		}

		ProgramSection = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("Panel_ProgramSection"));
		if (!ProgramSection)
		{
			return;
		}
		Panel_ProgramSection = ProgramSection;
		Content->AddChildToVerticalBox(ProgramSection);

		UCommonTextBlock* Title = WidgetTree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), TEXT("Text_ProgramTitleFallback"));
		if (Title)
		{
			Title->SetText(LOCTEXT("ProgramTitleFallback", "유료 프로그램"));
			ProgramSection->AddChildToVerticalBox(Title);
		}
	}

	auto EnsureText = [this, ProgramSection](
		TObjectPtr<UCommonTextBlock>& Target,
		const FName WidgetName)
	{
		if (Target || !WidgetTree)
		{
			return;
		}
		Target = WidgetTree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), WidgetName);
		if (Target)
		{
			Target->SetAutoWrapText(true);
			ProgramSection->AddChildToVerticalBox(Target);
		}
	};

	EnsureText(Text_ProgramStatus, TEXT("Text_ProgramStatus"));
	EnsureText(Text_ScheduledProgramSummary, TEXT("Text_ScheduledProgramSummary"));
	EnsureText(Text_ProgramScheduleReason, TEXT("Text_ProgramScheduleReason"));

	if (!Button_ScheduleProgram && WidgetTree)
	{
		Button_ScheduleProgram = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("Button_ScheduleProgram"));
		if (Button_ScheduleProgram)
		{
			ProgramSection->AddChildToVerticalBox(Button_ScheduleProgram);
		}
	}
	if (!Text_ScheduleProgram && Button_ScheduleProgram && WidgetTree &&
		!Button_ScheduleProgram->GetContent())
	{
		Text_ScheduleProgram = WidgetTree->ConstructWidget<UCommonTextBlock>(
			UCommonTextBlock::StaticClass(), TEXT("Text_ScheduleProgram"));
		if (Text_ScheduleProgram)
		{
			Text_ScheduleProgram->SetText(LOCTEXT("ScheduleProgramFallback", "유료 프로그램 편성"));
			Text_ScheduleProgram->SetJustification(ETextJustify::Center);
			Button_ScheduleProgram->SetContent(Text_ScheduleProgram);
		}
	}
}

void UFF_FacilityInfoPanel::SetText(UCommonTextBlock* Target, const FText& Text) const
{
	if (Target)
	{
		Target->SetText(Text);
	}
}

#undef LOCTEXT_NAMESPACE
