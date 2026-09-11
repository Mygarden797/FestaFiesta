// PrepBuild TileView entry presentation.

#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Building/Presentation/FF_BuildFavoritesSubsystem.h"
#include "Building/FF_BuildModeSubsystem.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
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

FVector2D FFPrepBuildTileLayout::FitThumbnailSize(const int32 TextureWidth, const int32 TextureHeight,
	const FVector2D Bounds)
{
	const FVector2D SafeBounds(
		FMath::IsFinite(Bounds.X) && Bounds.X > 0.0 ? Bounds.X : ThumbnailWidth,
		FMath::IsFinite(Bounds.Y) && Bounds.Y > 0.0 ? Bounds.Y : ThumbnailHeight);
	if (TextureWidth <= 0 || TextureHeight <= 0)
	{
		return SafeBounds;
	}
	const double Scale = FMath::Min(
		SafeBounds.X / TextureWidth,
		SafeBounds.Y / TextureHeight);
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
	Args.EntryWidth = EntryWidth;
	Args.EntryHeight = EntryHeight;
	MyListView = MyTileView = ITypedUMGListView<UObject*>::ConstructTileView<SFFPrepBuildTileView>(this, ListItems, Args);
	MyTileView->SetOnEntryInitialized(SListView<UObject*>::FOnEntryInitialized::CreateUObject(
		this, &UFF_PrepBuildTileView::HandleOnEntryInitializedInternal));
	return StaticCastSharedRef<STableViewBase>(MyTileView.ToSharedRef());
}

TSharedRef<SWidget> UFF_PrepBuildTileWidget::RebuildWidget()
{
	// The saved WBP SizeBox owns card dimensions. Match its size to the TileView entry size in Designer.
	return Super::RebuildWidget();
}

void UFF_PrepBuildTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	// Older saved WBP assets did not mark this SizeBox as a variable. Resolve its stable
	// Designer name without regenerating the asset or taking ownership of its layout.
	ThumbnailFrame = Cast<USizeBox>(GetWidgetFromName(TEXT("SizeBox_Thumbnail")));
	if (Button_Favorite)
	{
		Button_Favorite->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleFavoriteClicked);
	}
}

void UFF_PrepBuildTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (!bCapturedAppearance || IsDesignTime())
	{
		if (Border_Card) { AuthoredCardBrush = Border_Card->Background; }
		if (Text_Cost) { AuthoredCostColor = Text_Cost->GetColorAndOpacity(); }
		if (Text_State) { AuthoredStateColor = Text_State->GetColorAndOpacity(); }
		if (Text_Favorite) { AuthoredFavoriteColor = Text_Favorite->GetColorAndOpacity(); }
		bCapturedAppearance = true;
	}
	// Preview labels, clipping and wrapping are authored in the WBP, including Designer preview.
	if (!IsDesignTime()) { RefreshItemPresentation(); }
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
	RefreshFavorite();
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
	RefreshFavorite();
}

void UFF_PrepBuildTileWidget::HandleFavoriteClicked()
{
	if (!Item) { return; }
	if (UWorld* World = GetWorld())
	{
		if (auto* Mode = World->GetSubsystem<UFF_BuildModeSubsystem>()) { Mode->NotifyBuildToolsInteraction(); }
	}
	if (UGameInstance* Game = GetGameInstance())
	{
		if (auto* Favorites = Game->GetSubsystem<UFF_BuildFavoritesSubsystem>())
		{
			Favorites->ToggleFavorite(Item->GetFacilityDefinitionId());
		}
	}
	RefreshFavorite();
}

void UFF_PrepBuildTileWidget::RefreshFavorite()
{
	const auto* Favorites = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFF_BuildFavoritesSubsystem>() : nullptr;
	const bool bFavorite = Item && Favorites && Favorites->IsFavorite(Item->GetFacilityDefinitionId());
	if (Button_Favorite)
	{
		Button_Favorite->SetIsEnabled(Item != nullptr);
		Button_Favorite->SetToolTipText(bFavorite
			? NSLOCTEXT("FestaFiestaPrepBuild", "Unfavorite", "즐겨찾기 해제")
			: NSLOCTEXT("FestaFiestaPrepBuild", "Favorite", "즐겨찾기 등록"));
	}
	if (Text_Favorite)
	{
		Text_Favorite->SetText(FText::FromString(bFavorite ? TEXT("★") : TEXT("☆")));
		Text_Favorite->SetColorAndOpacity(bFavorite ? FSlateColor(FavoriteColor) : AuthoredFavoriteColor);
	}
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
		// Preserve each texture's aspect within the Designer's current frame. Neither
		// async loading nor pooled-row reuse may restore the old fixed 150 x 70 bounds.
		const FVector2D ThumbnailBounds = GetAuthoredThumbnailBounds();
		Image_Thumbnail->SetDesiredSizeOverride(LoadedThumbnail
			? FFPrepBuildTileLayout::FitThumbnailSize(LoadedThumbnail->GetSizeX(), LoadedThumbnail->GetSizeY(), ThumbnailBounds)
			: FFPrepBuildTileLayout::FitThumbnailSize(0, 0, ThumbnailBounds));
		Image_Thumbnail->SetVisibility(LoadedThumbnail
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Text_ThumbnailPlaceholder)
	{
		Text_ThumbnailPlaceholder->SetVisibility(Item && !LoadedThumbnail
			? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

FVector2D UFF_PrepBuildTileWidget::GetAuthoredThumbnailBounds() const
{
	FVector2D Bounds = ThumbnailFallbackBounds;
	if (const USizeBox* Frame = ThumbnailFrame.Get())
	{
		if (Frame->IsWidthOverride()) { Bounds.X = Frame->GetWidthOverride(); }
		if (Frame->IsHeightOverride()) { Bounds.Y = Frame->GetHeightOverride(); }
	}
	return Bounds;
}

void UFF_PrepBuildTileWidget::RefreshVisualState(const bool bIsSelected)
{
	const bool bAvailable = Item && Item->IsAffordable();
	if (Border_Card && bCapturedAppearance)
	{
		FSlateBrush Brush = AuthoredCardBrush;
		if (bIsSelected) { Brush.TintColor = SelectedTint; }
		else if (bCardHovered) { Brush.TintColor = HoverTint; }
		Border_Card->SetBrush(Brush);
	}
	if (Text_Cost) { Text_Cost->SetColorAndOpacity(Item && !bAvailable ? FSlateColor(UnavailableColor) : AuthoredCostColor); }
	if (Text_State) { Text_State->SetColorAndOpacity(Item && !bAvailable ? FSlateColor(UnavailableColor) : AuthoredStateColor); }
}
