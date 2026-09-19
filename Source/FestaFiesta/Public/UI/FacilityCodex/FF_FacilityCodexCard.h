// Facility collection presentation. Layout and default appearance belong to WBP assets.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/TileView.h"
#include "Building/Codex/FF_FacilityCodexTypes.h"
#include "FF_FacilityCodexCard.generated.h"

class UBorder;
class UFF_FacilityCodexImage;
class UTextBlock;

/** Stable list identity. Reused across world events so selection and virtualized rows survive. */
UCLASS()
class FESTAFIESTA_API UFF_FacilityCodexItem final : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, Category="Facility Codex") FFF_FacilityCodexEntry Entry;
};

/** Builder access to the otherwise protected WBP entry class. Runtime never rebuilds layout. */
UCLASS(meta=(DisplayName="FF Facility Codex Tile View"))
class FESTAFIESTA_API UFF_FacilityCodexTileView final : public UTileView
{
	GENERATED_BODY()
public:
	void ConfigureEntryClass(TSubclassOf<UUserWidget> InEntryClass);
protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
};

UCLASS(Abstract, Blueprintable, meta=(DisplayName="FF Facility Codex Card"))
class FESTAFIESTA_API UFF_FacilityCodexCard : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()
public:
	void RefreshPresentation();
protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Name;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Recipe;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_Bonus;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_State;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UFF_FacilityCodexImage> Image_Target;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UFF_FacilityCodexImage> Image_Partner;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UBorder> Border_Selection;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_TargetFallback;
	UPROPERTY(BlueprintReadOnly, meta=(BindWidget)) TObjectPtr<UTextBlock> Text_PartnerFallback;

	/** Only undiscovered cards use these colors; discovered cards keep their authored WBP style. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Undiscovered")
	FLinearColor UndiscoveredBackgroundColor = FLinearColor::FromSRGBColor(FColor(0xE4, 0xE4, 0xE4));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Undiscovered")
	FLinearColor UndiscoveredOutlineColor = FLinearColor::FromSRGBColor(FColor(0xB5, 0xB5, 0xB5));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Undiscovered")
	FLinearColor UndiscoveredTitleColor = FLinearColor::FromSRGBColor(FColor(0x50, 0x50, 0x50));
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Undiscovered")
	FLinearColor UndiscoveredBodyColor = FLinearColor::FromSRGBColor(FColor(0x64, 0x64, 0x64));
	/** Dims only the images with Slate's disabled draw effect. The card remains selectable. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance|Undiscovered")
	bool bDimUndiscoveredThumbnails = true;
private:
	void CaptureAuthoredAppearance();
	void RefreshDiscoveryAppearance();
	UPROPERTY(Transient) TObjectPtr<UFF_FacilityCodexItem> Item;
	// The original WBP did not expose Border_Card as a variable. Resolve its stable name without
	// changing/recompiling existing assets or adding a new required BindWidget contract.
	UPROPERTY(Transient) TObjectPtr<UBorder> CardBackground;
	UPROPERTY(Transient) TObjectPtr<UBorder> StateBackground;
	UPROPERTY(Transient) FSlateBrush AuthoredCardBrush;
	UPROPERTY(Transient) FSlateBrush AuthoredStateBrush;
	FLinearColor AuthoredCardBrushColor = FLinearColor::White;
	FLinearColor AuthoredStateBrushColor = FLinearColor::White;
	FSlateColor AuthoredNameColor;
	FSlateColor AuthoredRecipeColor;
	FSlateColor AuthoredBonusColor;
	FSlateColor AuthoredStateColor;
	bool bAuthoredTargetEnabled = true;
	bool bAuthoredPartnerEnabled = true;
	bool bCapturedAppearance = false;
	TSoftObjectPtr<UTexture2D> ShownTarget;
	TSoftObjectPtr<UTexture2D> ShownPartner;
};
