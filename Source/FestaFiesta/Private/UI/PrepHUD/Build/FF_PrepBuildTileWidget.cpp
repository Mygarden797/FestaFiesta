// PrepBuild TileView entry presentation.

#include "UI/PrepHUD/Build/FF_PrepBuildTileWidget.h"

#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UI/Data/FF_UIPalette.h"
#include "UI/PrepHUD/Build/FF_PrepBuildTileData.h"

void UFF_PrepBuildTileView::ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass)
{
	if (!InEntryClass || !InEntryClass->ImplementsInterface(UUserObjectListEntry::StaticClass()))
	{
		return;
	}

	EntryWidgetClass = InEntryClass;
	SetSelectionMode(ESelectionMode::Single);
	SetEntryWidth(174.0f);
	SetEntryHeight(194.0f);
	SetHorizontalEntrySpacing(10.0f);
	SetVerticalEntrySpacing(10.0f);
}

void UFF_PrepBuildTileWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);
	Item = Cast<UFF_PrepBuildTileData>(ListItemObject);
	RefreshFromItem();
	RefreshVisualState(IsListItemSelected());
}

void UFF_PrepBuildTileWidget::NativeOnItemSelectionChanged(const bool bIsSelected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(bIsSelected);
	RefreshVisualState(bIsSelected);
}

void UFF_PrepBuildTileWidget::NativeOnEntryReleased()
{
	Item = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}

void UFF_PrepBuildTileWidget::RefreshFromItem()
{
	if (!Item)
	{
		if (Text_Name)
		{
			Text_Name->SetText(FText::GetEmpty());
		}
		if (Text_Cost)
		{
			Text_Cost->SetText(FText::GetEmpty());
		}
		if (Text_State)
		{
			Text_State->SetText(FText::GetEmpty());
		}
		if (Image_Thumbnail)
		{
			Image_Thumbnail->SetBrush(FSlateBrush());
		}
		return;
	}

	if (Text_Name)
	{
		Text_Name->SetText(Item->GetDisplayName());
	}
	if (Text_Cost)
	{
		Text_Cost->SetText(Item->GetCostText());
	}
	if (Text_State)
	{
		const FText StateText = Item->GetStateText();
		Text_State->SetText(StateText.IsEmpty()
			? NSLOCTEXT("FestaFiestaPrepBuild", "Available", "배치 가능")
			: StateText);
	}
	if (Image_Thumbnail)
	{
		// Asset Manager/BuildCatalogSubsystem owns async loading. A tile only reads an already-loaded texture.
		if (UTexture2D* LoadedThumbnail = Item->GetThumbnail().Get())
		{
			Image_Thumbnail->SetBrushFromTexture(LoadedThumbnail, true);
			Image_Thumbnail->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			Image_Thumbnail->SetBrush(FSlateBrush());
		}
	}
}

void UFF_PrepBuildTileWidget::RefreshVisualState(const bool bIsSelected)
{
	const UFF_UIPalette* Palette = UFF_UIPalette::Get();
	if (!Palette)
	{
		return;
	}

	const bool bAvailable = Item && Item->IsAffordable();
	if (Border_Card)
	{
		Border_Card->SetBrushColor(bIsSelected
			? Palette->InGame.TealSoft
			: (bAvailable ? Palette->InGame.Paper : Palette->InGame.PaperSunken));
	}
	if (Text_Name)
	{
		Text_Name->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.Ink
			: Palette->InGame.InkFaint));
	}
	if (Text_Cost)
	{
		Text_Cost->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.TealDeep
			: Palette->InGame.Danger));
	}
	if (Text_State)
	{
		Text_State->SetColorAndOpacity(FSlateColor(bAvailable
			? Palette->InGame.TealDeep
			: Palette->InGame.Danger));
	}
}
