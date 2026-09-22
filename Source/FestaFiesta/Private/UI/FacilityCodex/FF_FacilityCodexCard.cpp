#include "UI/FacilityCodex/FF_FacilityCodexCard.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInterface.h"
#include "UI/FacilityCodex/FF_FacilityCodexPresentation.h"

#define LOCTEXT_NAMESPACE "FFFacilityCodexCard"

namespace
{
	template <typename ItemType>
	class SFFCodexTileView final : public STileView<ItemType>
	{
	public:
		void SetColumnCount(int32 Value) { Columns = FMath::Clamp(Value, 1, 6); }
		virtual STableViewBase::FReGenerateResults ReGenerateItems(const FGeometry& Geometry) override
		{
			// Geometry is the actual items panel, after its scrollbar has taken space.
			// Change Slate's layout pitch only: never overwrite the saved WBP entry settings.
			const float Width = Geometry.GetLocalSize().X;
			if (Width > 1.f)
			{
				const float Pitch = FMath::Max(1.f, Width / float(Columns) - .01f);
				if (!FMath::IsNearlyEqual(this->GetItemWidth(), Pitch)) this->SetItemWidth(Pitch);
			}
			// UE 5.7 skips row cleanup for an empty tile source. Release pooled bindings here,
			// at regeneration, rather than the temporary empty state during SetListItems.
			if (this->GetItems().IsEmpty()) this->WidgetGenerator.Clear();
			return STileView<ItemType>::ReGenerateItems(Geometry);
		}
	protected:
		virtual int32 GetNumItemsPerLine() const override { return Columns; }
	private:
		int32 Columns = 6;
	};
}

void UFF_FacilityCodexTileView::ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass)
{
	if (!InEntryClass || !InEntryClass->ImplementsInterface(UUserObjectListEntry::StaticClass())) return;
	EntryWidgetClass = InEntryClass;
	SetSelectionMode(ESelectionMode::Single);
	// One-time authoring defaults. Runtime derives only the horizontal pitch from available width.
	EntryWidth = 200.f;
	EntryHeight = 218.f;
	SetHorizontalEntrySpacing(10.f);
	SetVerticalEntrySpacing(10.f);
	TileAlignment = EListItemAlignment::LeftAligned;
	AllowOverscroll = false;
	bEnableScrollAnimation = false;
}

TSharedRef<STableViewBase> UFF_FacilityCodexTileView::RebuildListWidget()
{
	// Preserve all authored TileView options, including whether entry size includes spacing.
	auto View = ConstructTileView<SFFCodexTileView>();
	ColumnCount = FMath::Clamp(ColumnCount, 1, 6);
	View->SetColumnCount(ColumnCount);
	return View;
}

void UFF_FacilityCodexTileView::SetResponsiveColumns(int32 InColumns)
{
	InColumns = FMath::Clamp(InColumns, 1, 6);
	if (ColumnCount == InColumns) return;
	ColumnCount = InColumns;
	if (MyTileView)
	{
		StaticCastSharedPtr<SFFCodexTileView<UObject*>>(MyTileView)->SetColumnCount(ColumnCount);
		MyTileView->RequestListRefresh();
	}
}

void UFF_FacilityCodexCard::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_ClaimReward) Button_ClaimReward->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClaimReward);
}

void UFF_FacilityCodexCard::NativePreConstruct()
{
	Super::NativePreConstruct();
	// Designer preview keeps the current WBP colors. Runtime snapshots them once before any
	// discovery tint is applied, so pooled rows never capture the previous item's gray style.
	if (IsDesignTime()) bCapturedAppearance = false;
	CaptureAuthoredAppearance();
	if (!IsDesignTime()) RefreshDiscoveryAppearance();
}

void UFF_FacilityCodexCard::CaptureAuthoredAppearance()
{
	if (bCapturedAppearance) return;
	CardBackground = Cast<UBorder>(GetWidgetFromName(TEXT("Border_Card")));
	if (CardBackground)
	{
		AuthoredCardBrush = CardBackground->Background;
		AuthoredCardBrushColor = CardBackground->GetBrushColor();
	}
	StateBackground = Cast<UBorder>(GetWidgetFromName(TEXT("Border_State")));
	if (StateBackground)
	{
		AuthoredStateBrush = StateBackground->Background;
		AuthoredStateBrushColor = StateBackground->GetBrushColor();
	}
	if (Text_Name) AuthoredNameColor = Text_Name->GetColorAndOpacity();
	if (Text_Recipe) AuthoredRecipeColor = Text_Recipe->GetColorAndOpacity();
	if (Text_Bonus) AuthoredBonusColor = Text_Bonus->GetColorAndOpacity();
	if (Text_State) AuthoredStateColor = Text_State->GetColorAndOpacity();
	if (Text_ClaimReward) AuthoredClaimRewardColor = Text_ClaimReward->GetColorAndOpacity();
	bCapturedAppearance = true;
}

void UFF_FacilityCodexCard::RefreshDiscoveryAppearance()
{
	CaptureAuthoredAppearance();
	const bool bUndiscovered = Item && !Item->Entry.bDiscovered;
	if (CardBackground)
	{
		FSlateBrush Brush = AuthoredCardBrush;
		if (bUndiscovered && !Cast<UMaterialInterface>(Brush.GetResourceObject()))
		{
			Brush.TintColor = FSlateColor(UndiscoveredBackgroundColor);
			Brush.OutlineSettings.Color = FSlateColor(UndiscoveredOutlineColor);
		}
		CardBackground->SetBrush(Brush);
		CardBackground->SetBrushColor(bUndiscovered ? FLinearColor::White : AuthoredCardBrushColor);
	}
	if (StateBackground)
	{
		FSlateBrush Brush = AuthoredStateBrush;
		if (bUndiscovered) Brush.TintColor = FSlateColor(UndiscoveredOutlineColor);
		StateBackground->SetBrush(Brush);
		StateBackground->SetBrushColor(bUndiscovered ? FLinearColor::White : AuthoredStateBrushColor);
	}
	if (Text_Name) Text_Name->SetColorAndOpacity(bUndiscovered ? FSlateColor(UndiscoveredTitleColor) : AuthoredNameColor);
	if (Text_Recipe) Text_Recipe->SetColorAndOpacity(bUndiscovered ? FSlateColor(UndiscoveredBodyColor) : AuthoredRecipeColor);
	if (Text_Bonus) Text_Bonus->SetColorAndOpacity(bUndiscovered ? FSlateColor(UndiscoveredTitleColor) : AuthoredBonusColor);
	if (Text_State) Text_State->SetColorAndOpacity(bUndiscovered ? FSlateColor(UndiscoveredBodyColor) : AuthoredStateColor);
	// State belongs to the thumbnail shader, never Slate's disabled/low-opacity effect.
	// Installed facilities stay recognizable in gray; missing facilities offer a silhouette hint.
	const auto ApplyImageState = [bUndiscovered](UFF_FacilityCodexImage* Image, bool bInstalled)
	{
		if (!Image) return;
		Image->SetIsEnabled(true);
		Image->SetDiscoveryVisualState(!bUndiscovered ? EFF_FacilityCodexDiscoveryVisualState::Color
			: bInstalled ? EFF_FacilityCodexDiscoveryVisualState::Grayscale
			: EFF_FacilityCodexDiscoveryVisualState::Silhouette);
	};
	ApplyImageState(Image_Target, Item && Item->Entry.bTargetInstalled);
	ApplyImageState(Image_Partner, Item && Item->Entry.bPartnerInstalled);
}

void UFF_FacilityCodexCard::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	Item = Cast<UFF_FacilityCodexItem>(ListItemObject);
	RefreshPresentation();
}

void UFF_FacilityCodexCard::NativeOnItemSelectionChanged(bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	if (Border_Selection) Border_Selection->SetVisibility(bIsSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFF_FacilityCodexCard::NativeOnEntryReleased()
{
	Item = nullptr;
	RefreshPresentation();
	if (Border_Selection) Border_Selection->SetVisibility(ESlateVisibility::Collapsed);
	IUserObjectListEntry::NativeOnEntryReleased();
}

void UFF_FacilityCodexCard::RefreshPresentation()
{
	using namespace FFFacilityCodexPresentation;
	RefreshDiscoveryAppearance();
	RefreshRewardPresentation();
	// The selected recipe already has a dedicated detail region. A generic tooltip
	// merely repeats the title and ingredients while covering neighbouring cards.
	SetToolTip(nullptr);
	SetToolTipText(FText::GetEmpty());
	if (!Item)
	{
		Thumbnail(Image_Target, Text_TargetFallback, {}, ShownTarget);
		Thumbnail(Image_Partner, Text_PartnerFallback, {}, ShownPartner);
		if (Text_Name) Text_Name->SetText(FText::GetEmpty());
		if (Text_Recipe) Text_Recipe->SetText(FText::GetEmpty());
		if (Text_Bonus) Text_Bonus->SetText(FText::GetEmpty());
		if (Text_State) Text_State->SetText(FText::GetEmpty());
		SetToolTipText(FText::GetEmpty());
		return;
	}
	const FFF_FacilityCodexEntry& Entry = Item->Entry;
	if (Text_Name) Text_Name->SetText(Entry.DisplayName);
	if (Text_Recipe) Text_Recipe->SetText(FText::Format(LOCTEXT("Recipe", "{0} + {1}"), Entry.TargetName, Entry.PartnerName));
	if (Text_Bonus) Text_Bonus->SetText(FText::Format(LOCTEXT("Bonus", "수수료 +{0}%"), Number(Entry.ConfiguredBonusPercent)));
	if (Text_State) Text_State->SetText(Entry.bEffectApplied
		? LOCTEXT("AppliedCompact", "효과 적용 중") : Entry.bDiscovered
		? LOCTEXT("Discovered", "발견 완료") : LOCTEXT("UndiscoveredCompact", "미발견"));
	Thumbnail(Image_Target, Text_TargetFallback, Entry.TargetThumbnail, ShownTarget);
	Thumbnail(Image_Partner, Text_PartnerFallback, Entry.PartnerThumbnail, ShownPartner);
	if (Border_Selection) Border_Selection->SetVisibility(IsListItemSelected() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFF_FacilityCodexCard::RefreshRewardPresentation()
{
	const bool bDiscovered = Item && Item->Entry.bDiscovered;
	const bool bClaimed = bDiscovered && Item->Entry.bDiscoveryRewardClaimed;
	// A later reward-table change must not hide an existing claim from the player's history.
	const bool bHasReward = bDiscovered && (Item->Entry.DiscoveryRewardGold > 0 || bClaimed);
	const bool bHasClaimedBadge = Border_RewardClaimed != nullptr;
	const bool bShowLockedReward = Border_RewardLocked && Item && !bDiscovered && Item->Entry.DiscoveryRewardGold > 0;
	if (Size_RewardState)
	{
		Size_RewardState->SetVisibility(bHasReward || bShowLockedReward
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (Button_ClaimReward)
	{
		Button_ClaimReward->SetVisibility(bHasReward && !(bClaimed && bHasClaimedBadge)
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_ClaimReward->SetIsEnabled(bHasReward && !bClaimed);
		Button_ClaimReward->SetToolTipText(!bHasReward ? FText::GetEmpty() : bClaimed
			? LOCTEXT("RewardClaimedTip", "발견 보상 수령 완료")
			: FText::Format(LOCTEXT("RewardTip", "발견 보상 {0} 골드 받기 (1회)"), FText::AsNumber(Item->Entry.DiscoveryRewardGold)));
	}
	if (Border_RewardClaimed)
	{
		// A passive badge keeps the check and completion label at full contrast, without
		// looking like a dimmed gift that might become available again. Visible permits its tooltip.
		Border_RewardClaimed->SetVisibility(bClaimed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Border_RewardClaimed->SetIsEnabled(true);
		Border_RewardClaimed->SetToolTipText(bClaimed
			? LOCTEXT("RewardClaimedTip", "발견 보상 수령 완료") : FText::GetEmpty());
	}
	if (Border_RewardLocked)
	{
		// Show the future reward without exposing a claim action before discovery.
		// This passive surface stays enabled so its prerequisite and amount are readable.
		Border_RewardLocked->SetVisibility(bShowLockedReward ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Border_RewardLocked->SetIsEnabled(true);
		Border_RewardLocked->SetToolTipText(bShowLockedReward
			? FText::Format(LOCTEXT("RewardLockedTip", "발견하면 보상 {0} 골드 수령 가능 (1회)"),
				FText::AsNumber(Item->Entry.DiscoveryRewardGold)) : FText::GetEmpty());
	}
	// Child images never intercept interaction. Older assets retain their disabled-gift fallback.
	if (Image_ClaimReward) Image_ClaimReward->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Image_RewardClaimed) Image_RewardClaimed->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Text_ClaimReward)
	{
		// The new layout reserves room for a compact caption; older icon-only buttons do not.
		Text_ClaimReward->SetVisibility(bHasClaimedBadge || !Image_ClaimReward
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		Text_ClaimReward->SetText(bHasClaimedBadge ? LOCTEXT("ClaimRewardCompact", "받기")
			: bClaimed ? LOCTEXT("RewardClaimed", "수령 완료") : LOCTEXT("ClaimReward", "보상 받기"));
		Text_ClaimReward->SetColorAndOpacity(bClaimed ? FSlateColor(ClaimedRewardTextColor) : AuthoredClaimRewardColor);
	}
}

void UFF_FacilityCodexCard::HandleClaimReward()
{
	if (!Item || !Item->Entry.bDiscovered || Item->Entry.bDiscoveryRewardClaimed || Item->Entry.DiscoveryRewardGold <= 0) return;
	Item->OnClaimRewardRequested.ExecuteIfBound(Item->Entry.RuleId);
}

#undef LOCTEXT_NAMESPACE
