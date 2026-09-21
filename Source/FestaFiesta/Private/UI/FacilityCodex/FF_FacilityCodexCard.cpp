#include "UI/FacilityCodex/FF_FacilityCodexCard.h"

#include "Components/Border.h"
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
	// An older serialized WBP can still contain the former three-column value.
	ColumnCount = 6;
	View->SetColumnCount(6);
	return View;
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
	if (Image_Target) bAuthoredTargetEnabled = Image_Target->GetIsEnabled();
	if (Image_Partner) bAuthoredPartnerEnabled = Image_Partner->GetIsEnabled();
	bCapturedAppearance = true;
}

void UFF_FacilityCodexCard::RefreshDiscoveryAppearance()
{
	CaptureAuthoredAppearance();
	const bool bUndiscovered = Item && !Item->Entry.bDiscovered;
	if (CardBackground)
	{
		FSlateBrush Brush = AuthoredCardBrush;
		if (bUndiscovered)
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
	// Do not disable the row or card: unknown recipes still open their full recipe details.
	// UE 5.7's default disabled shader lowers opacity; this deliberately promises dimming,
	// not grayscale conversion. Async image completion changes only the brush/visibility.
	if (Image_Target) Image_Target->SetIsEnabled(bAuthoredTargetEnabled && !(bUndiscovered && bDimUndiscoveredThumbnails));
	if (Image_Partner) Image_Partner->SetIsEnabled(bAuthoredPartnerEnabled && !(bUndiscovered && bDimUndiscoveredThumbnails));
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

#undef LOCTEXT_NAMESPACE
