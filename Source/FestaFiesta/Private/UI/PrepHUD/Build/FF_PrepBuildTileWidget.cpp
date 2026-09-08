// PrepBuild TileView entry presentation.

#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"
#include "Widgets/Layout/SBox.h"

namespace
{
	template <typename ItemType>
	class SFFPrepBuildTileView final : public STileView<ItemType>
	{
	public:
		virtual STableViewBase::FReGenerateResults ReGenerateItems(const FGeometry& MyGeometry) override
		{
			// UE 5.7 STileView skips its generation cleanup when the source is empty.
			// Release stale item/row bindings at that boundary only. SetListItems briefly
			// empties its source while replacing it, so OnItemsChanged is too early.
			// Keep this workaround local; remove if the engine adopts SListView's guard.
			if (this->GetItems().IsEmpty())
			{
				this->WidgetGenerator.Clear();
			}
			return STileView<ItemType>::ReGenerateItems(MyGeometry);
		}
	};
}

FVector2D FFPrepBuildTileLayout::FitThumbnailSize(const int32 TextureWidth, const int32 TextureHeight)
{
	if (TextureWidth <= 0 || TextureHeight <= 0)
	{
		return FVector2D(ThumbnailWidth, ThumbnailHeight);
	}
	const double Scale = FMath::Min(
		static_cast<double>(ThumbnailWidth) / TextureWidth,
		static_cast<double>(ThumbnailHeight) / TextureHeight);
	return FVector2D(TextureWidth * Scale, TextureHeight * Scale);
}

void UFF_PrepBuildTileView::ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass)
{
	if (!InEntryClass || !InEntryClass->ImplementsInterface(UUserObjectListEntry::StaticClass()))
	{
		return;
	}

	EntryWidgetClass = InEntryClass;
	SetSelectionMode(ESelectionMode::Single);
	EntryWidth = FFPrepBuildTileLayout::CardWidth + FFPrepBuildTileLayout::HorizontalSpacing;
	EntryHeight = FFPrepBuildTileLayout::CardHeight;
	SetHorizontalEntrySpacing(FFPrepBuildTileLayout::HorizontalSpacing);
	SetVerticalEntrySpacing(0.0f);
	TileAlignment = EListItemAlignment::LeftAligned;
	// UE's setter only updates this property after Slate construction.
	AllowOverscroll = false;
	SetAllowOverScroll(false);
	bEnableScrollAnimation = false;
	bInEnableTouchAnimatedScrolling = false;
	bEnableFixedLineOffset = false;
}

TSharedRef<STableViewBase> UFF_PrepBuildTileView::RebuildListWidget()
{
	// UE's saved "entry size includes spacing" option and runtime size setters use
	// different margin conventions. Define the Slate pitch explicitly: 170px card
	// plus 5px on either side. A partial last row keeps these same dimensions.
	ConfigureEntryClass(EntryWidgetClass);
	FTileViewConstructArgs Args;
	Args.bAllowFocus = bIsFocusable;
	Args.SelectionMode = SelectionMode;
	Args.bClearSelectionOnClick = bClearSelectionOnClick;
	Args.ConsumeMouseWheel = ConsumeMouseWheel;
	Args.bReturnFocusToSelection = bReturnFocusToSelection;
	Args.TileAlignment = TileAlignment;
	Args.bWrapDirectionalNavigation = bWrapHorizontalNavigation;
	Args.Orientation = Orientation;
	Args.ScrollBarStyle = &ScrollBarStyle;
	Args.ScrollbarDisabledVisibility = UWidget::ConvertSerializedVisibilityToRuntime(ScrollbarDisabledVisibility);
	Args.EntryWidth = FFPrepBuildTileLayout::CardWidth + FFPrepBuildTileLayout::HorizontalSpacing;
	Args.EntryHeight = FFPrepBuildTileLayout::CardHeight;
	MyListView = MyTileView = ITypedUMGListView<UObject*>::ConstructTileView<SFFPrepBuildTileView>(this, ListItems, Args);
	MyTileView->SetOnEntryInitialized(SListView<UObject*>::FOnEntryInitialized::CreateUObject(
		this, &UFF_PrepBuildTileView::HandleOnEntryInitializedInternal));
	return StaticCastSharedRef<STableViewBase>(MyTileView.ToSharedRef());
}

TSharedRef<SWidget> UFF_PrepBuildTileWidget::RebuildWidget()
{
	// Async image loading and recycled entry contents must never change the row size.
	return SNew(SBox)
		.WidthOverride(FFPrepBuildTileLayout::CardWidth)
		.HeightOverride(FFPrepBuildTileLayout::CardHeight)
		[Super::RebuildWidget()];
}

void UFF_PrepBuildTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Border_Card)
	{
		Border_Card->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	}
	for (UTextBlock* Text : { Text_CategoryPath.Get(), Text_Name.Get(), Text_Cost.Get(),
		Text_State.Get(), Text_BuildLimit.Get() })
	{
		if (Text)
		{
			Text->SetAutoWrapText(false);
			Text->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		}
	}
	RefreshItemPresentation();
}

void UFF_PrepBuildTileWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	Item = Cast<UFF_PrepBuildTileData>(ListItemObject);
	RefreshItemPresentation();
}

void UFF_PrepBuildTileWidget::NativeOnItemSelectionChanged(const bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	RefreshVisualState(bIsSelected);
}

void UFF_PrepBuildTileWidget::NativeOnEntryReleased()
{
	Item = nullptr;
	bCardHovered = false;
	// Released entries retain their fixed layout but no previous data or selection.
	RefreshFromItem();
	RefreshVisualState(false);
	IUserObjectListEntry::NativeOnEntryReleased();
}

void UFF_PrepBuildTileWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bCardHovered = true;
	RefreshVisualState(IsListItemSelected());
}

void UFF_PrepBuildTileWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bCardHovered = false;
	RefreshVisualState(IsListItemSelected());
}

void UFF_PrepBuildTileWidget::RefreshItemPresentation()
{
	RefreshFromItem();
	RefreshVisualState(Item && IsListItemSelected());
}

void UFF_PrepBuildTileWidget::RefreshFromItem()
{
	if (Text_Name)
	{
		Text_Name->SetText(Item ? Item->GetDisplayName() : FText::GetEmpty());
	}
	if (Text_CategoryPath)
	{
		Text_CategoryPath->SetText(Item ? Item->GetCategoryPathText() : FText::GetEmpty());
	}
	if (Text_Cost)
	{
		Text_Cost->SetText(Item ? Item->GetCostText() : FText::GetEmpty());
	}
	if (Text_BuildLimit)
	{
		Text_BuildLimit->SetText(Item ? Item->GetBuildLimitText() : FText::GetEmpty());
	}
	if (Text_State)
	{
		Text_State->SetText(Item ? Item->GetStateLabelText() : FText::GetEmpty());
	}

	if (Item)
	{
		FText NameAndCategory = FText::Format(
			NSLOCTEXT("FestaFiestaPrepBuild", "TileNameAndCategory", "{0}\n{1}"),
			Item->GetDisplayName(), Item->GetCategoryPathText());
		const FString UniqueName = Item->GetUniqueNameToken();
		if (!UniqueName.IsEmpty() && !UniqueName.Equals(Item->GetDisplayName().ToString(), ESearchCase::IgnoreCase))
		{
			NameAndCategory = FText::Format(
				NSLOCTEXT("FestaFiestaPrepBuild", "TileUniqueName", "{0}\n고유 이름: {1}"),
				NameAndCategory, FText::FromString(UniqueName));
		}
		SetToolTipText(FText::Format(
			NSLOCTEXT("FestaFiestaPrepBuild", "TileFullDescription", "{0}\n{1} · {2}\n{3}"),
			NameAndCategory, Item->GetCostText(), Item->GetBuildLimitText(), Item->GetStateText()));
	}
	else
	{
		SetToolTipText(FText::GetEmpty());
	}

	// Asset Manager owns loading. Texture native size never participates in tile layout.
	UTexture2D* LoadedThumbnail = Item ? Item->GetThumbnail().Get() : nullptr;
	if (Image_Thumbnail)
	{
		Image_Thumbnail->SetBrushFromTexture(LoadedThumbnail, false);
		Image_Thumbnail->SetDesiredSizeOverride(LoadedThumbnail
			? FFPrepBuildTileLayout::FitThumbnailSize(LoadedThumbnail->GetSizeX(), LoadedThumbnail->GetSizeY())
			: FVector2D(FFPrepBuildTileLayout::ThumbnailWidth, FFPrepBuildTileLayout::ThumbnailHeight));
		Image_Thumbnail->SetColorAndOpacity(FLinearColor::White);
		Image_Thumbnail->SetVisibility(LoadedThumbnail
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Text_ThumbnailPlaceholder)
	{
		Text_ThumbnailPlaceholder->SetVisibility(Item && !LoadedThumbnail
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UFF_PrepBuildTileWidget::RefreshVisualState(const bool bIsSelected)
{
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	const FFF_UIPaletteInGame& Colors = Palette->InGame;
	const bool bAvailable = Item && Item->IsAffordable();
	if (Border_Card)
	{
		const FLinearColor Fill = bIsSelected ? Colors.TealSoft
			: bCardHovered ? Colors.Hover : bAvailable ? Colors.Paper : Colors.PaperSunken;
		const FLinearColor Outline = bIsSelected ? Colors.TealDeep : bCardHovered ? Colors.Amber : Colors.Line;
		const FSlateRoundedBoxBrush Brush(Fill, 7.0f, Outline, bIsSelected || bCardHovered ? 2.0f : 1.0f);
		Border_Card->SetBrush(Brush);
		Border_Card->SetBrushColor(FLinearColor::White);
	}
	if (Text_Name)
	{
		Text_Name->SetColorAndOpacity(FSlateColor(Colors.Ink));
	}
	for (UTextBlock* Text : { Text_CategoryPath.Get(), Text_BuildLimit.Get(), Text_ThumbnailPlaceholder.Get() })
	{
		if (Text)
		{
			Text->SetColorAndOpacity(FSlateColor(Colors.InkMuted));
		}
	}
	for (UTextBlock* Text : { Text_Cost.Get(), Text_State.Get() })
	{
		if (Text)
		{
			Text->SetColorAndOpacity(FSlateColor(bAvailable ? Colors.TealDeep : Colors.Danger));
		}
	}
}
