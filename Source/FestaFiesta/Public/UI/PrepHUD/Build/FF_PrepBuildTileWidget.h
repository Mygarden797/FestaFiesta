// PrepBuild TileView and its virtualized entry presentation.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/TileView.h"
#include "FF_PrepBuildTileWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UFF_PrepBuildTileData;

/**
 * UTileView's entry class is protected because designers normally select it in the editor.
 * The one-shot WBP builder needs an explicit, type-safe setter while creating a brand-new asset.
 */
UCLASS(meta = (DisplayName = "FF Prep Build Tile View"))
class FESTAFIESTA_API UFF_PrepBuildTileView final : public UTileView
{
	GENERATED_BODY()

public:
	/** Builder-only configuration. Runtime code never changes the generated entry contract. */
	void ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass);
};

/**
 * Virtualized facility tile. It only projects UFF_PrepBuildTileData into named UMG children;
 * selection and activation remain owned by UTileView.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "FF Prep Build Tile"))
class FESTAFIESTA_API UFF_PrepBuildTileWidget
	: public UUserWidget
	, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Cost;

	/** Explicit cause text keeps disabled states distinguishable without relying on color. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_State;

private:
	void RefreshFromItem();
	void RefreshVisualState(bool bIsSelected);

	UPROPERTY(Transient)
	TObjectPtr<UFF_PrepBuildTileData> Item;
};
