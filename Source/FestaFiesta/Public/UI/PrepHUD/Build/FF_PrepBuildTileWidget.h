// PrepBuild TileView and its virtualized entry presentation.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/TileView.h"
#include "Styling/SlateTypes.h"
#include "FF_PrepBuildTileWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class USizeBox;
class UTextBlock;
class UFF_PrepBuildTileData;

namespace FFPrepBuildTileLayout
{
	constexpr float CardWidth = 170.0f;
	constexpr float CardHeight = 164.0f;
	constexpr float ThumbnailWidth = 150.0f;
	constexpr float ThumbnailHeight = 70.0f;
	constexpr float HorizontalSpacing = 10.0f;
	FESTAFIESTA_API FVector2D FitThumbnailSize(int32 TextureWidth, int32 TextureHeight,
		FVector2D Bounds = FVector2D(ThumbnailWidth, ThumbnailHeight));
}

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

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
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

public:
	// Rebind visible data after a price/thumbnail change without releasing the virtualized row.
	void RefreshItemPresentation();

	/** 선택한 기존 시설과 관련된 설치 후보를 강조합니다. 스타일은 WBP가 소유합니다. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build|Synergy")
	void SetSynergyHighlighted(bool bHighlighted);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Synergy")
	bool IsSynergyHighlighted() const { return bSynergyHighlighted; }

	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Prep|Build|Required")
	void SetRequiredFacilityStatus(bool bRequired, bool bSatisfied, int32 CurrentCount, int32 MinimumCount);

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	bool IsRequiredFacility() const { return bRequiredFacility; }

	UFUNCTION(BlueprintPure, Category = "FestaFiesta|UI|Prep|Build|Required")
	bool IsRequiredFacilitySatisfied() const { return bRequiredFacilitySatisfied; }

protected:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnItemSelectionChanged(bool bIsSelected) override;
	virtual void NativeOnEntryReleased() override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	/** 추가 애니메이션 등을 연결합니다. 재활용 해제 시에도 false 상태를 받습니다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep|Build|Synergy")
	void BP_OnSynergyHighlightChanged(bool bHighlighted);

	UFUNCTION(BlueprintImplementableEvent, Category = "FestaFiesta|UI|Prep|Build|Required")
	void BP_OnRequiredFacilityStatusChanged(bool bRequired, bool bSatisfied, int32 CurrentCount, int32 MinimumCount);

	/** 부족·충족 각각의 문구/색상/폰트/배치는 WBP에서 편집합니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Required")
	TObjectPtr<UBorder> Border_RequiredMissing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Required")
	TObjectPtr<UBorder> Border_RequiredSatisfied;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Required")
	TObjectPtr<UTextBlock> Text_RequiredMissing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Required")
	TObjectPtr<UTextBlock> Text_RequiredSatisfied;

	/** 표시만 제어하는 독립 레이어라 기존 선택·호버 색상과 충돌하지 않습니다. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Synergy")
	TObjectPtr<UBorder> Border_SynergyHighlight;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build|Synergy")
	TObjectPtr<UTextBlock> Text_SynergyBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UBorder> Border_Card;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UImage> Image_Thumbnail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Name;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_CategoryPath;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_BuildLimit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_ThumbnailPlaceholder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Cost;

	/** Explicit cause text keeps disabled states distinguishable without relying on color. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_State;

	/** A sibling hit target: starring never activates the facility's TileView row. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UButton> Button_Favorite;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "FestaFiesta|UI|Prep|Build")
	TObjectPtr<UTextBlock> Text_Favorite;

	/** Normal brush and typography come from Designer; these are interaction-state accents. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|States")
	FLinearColor HoverTint = FLinearColor::FromSRGBColor(FColor(0xEE, 0xF2, 0xEF));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|States")
	FLinearColor SelectedTint = FLinearColor::FromSRGBColor(FColor(0xE4, 0xEE, 0xE0));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|States")
	FLinearColor UnavailableColor = FLinearColor::FromSRGBColor(FColor(0xD7, 0x4E, 0x43));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|States")
	FLinearColor FavoriteColor = FLinearColor::FromSRGBColor(FColor(0x99, 0x73, 0x21));

	/** Used only when a custom WBP omits SizeBox_Thumbnail or clears one of its size overrides. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Appearance|Thumbnail", meta = (ClampMin = "1.0"))
	FVector2D ThumbnailFallbackBounds = FVector2D(150.0, 70.0);

private:
	UFUNCTION()
	void HandleFavoriteClicked();
	void RefreshFavorite();
	void RefreshFromItem();
	void RefreshVisualState(bool bIsSelected);
	void ApplySynergyHighlight();
	void ApplyRequiredFacilityStatus();
	FVector2D GetAuthoredThumbnailBounds() const;
	TWeakObjectPtr<USizeBox> ThumbnailFrame;

	UPROPERTY(Transient)
	TObjectPtr<UFF_PrepBuildTileData> Item;
	bool bCardHovered = false;
	bool bSynergyHighlighted = false;
	bool bRequiredFacility = false;
	bool bRequiredFacilitySatisfied = false;
	int32 RequiredFacilityCurrentCount = 0;
	int32 RequiredFacilityMinimumCount = 0;
	FSlateBrush AuthoredCardBrush;
	FSlateColor AuthoredCostColor;
	FSlateColor AuthoredStateColor;
	FSlateColor AuthoredFavoriteColor;
	bool bCapturedAppearance = false;
};
