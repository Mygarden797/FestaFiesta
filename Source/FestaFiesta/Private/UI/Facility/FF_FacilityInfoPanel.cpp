// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/Facility/FF_FacilityInfoPanel.h"

#include "CommonTextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FFFacilityInfoPanel"

void UFF_FacilityInfoPanel::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	EnsureMinigameButton();

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
	SetText(Text_RelocationWarning, LOCTEXT("NeedsRelocation", "재배치가 필요합니다."));
	SetText(Text_Footprint, ViewData.FootprintText);
	SetText(Text_Clearance, ViewData.ClearanceText);
	SetText(Text_InstallCost, ViewData.InstallCostText);
	SetText(Text_RecoveryCost, ViewData.RecoveryCostText);
	SetText(Text_QueueCapacity, ViewData.QueueCapacityText);
	SetText(Text_ServiceDuration, ViewData.ServiceDurationText);
	SetText(Text_OperationState, ViewData.OperationStateText);
	SetText(Text_WaitingParties, ViewData.WaitingPartiesText);
	SetText(Text_Occupancy, ViewData.OccupancyText);
	SetText(Text_RemainingService, ViewData.RemainingServiceText);
	SetText(Text_ExpectedWait, ViewData.ExpectedWaitText);
	SetText(Text_Throughput, ViewData.ThroughputText);
	SetText(Text_Feedback, ViewData.FeedbackText);
	SetText(Text_ThumbnailPlaceholder, ViewData.ThumbnailPlaceholderText);

	const bool bBuildContext = ViewData.Context == EFF_FacilityInfoContext::Build;
	if (Panel_BuildSection)
	{
		Panel_BuildSection->SetVisibility(bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Panel_LiveSection)
	{
		Panel_LiveSection->SetVisibility(!bBuildContext ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
			ViewData.bFeedbackIsError
				? FLinearColor(1.0f, 0.32f, 0.28f, 1.0f)
				: FLinearColor(0.25f, 1.0f, 0.67f, 1.0f)));
	}

	if (Button_Move)
	{
		Button_Move->SetIsEnabled(bBuildContext && ViewData.bCanMove);
	}
	if (Button_Remove)
	{
		Button_Remove->SetIsEnabled(bBuildContext && ViewData.bCanRemove);
	}
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
	return GetVisibility() != ESlateVisibility::Collapsed && Border_Panel && Border_Panel->IsHovered();
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

void UFF_FacilityInfoPanel::SetText(UCommonTextBlock* Target, const FText& Text) const
{
	if (Target)
	{
		Target->SetText(Text);
	}
}

#undef LOCTEXT_NAMESPACE
