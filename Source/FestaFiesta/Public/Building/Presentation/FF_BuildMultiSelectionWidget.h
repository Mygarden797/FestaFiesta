// FF_BuildMultiSelectionWidget.h
// The build session's selected facilities, presented as a compact horizontal strip.

#pragma once

#include "Blueprint/UserWidget.h"
#include "FF_BuildMultiSelectionWidget.generated.h"

class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UFF_BuildCatalogSubsystem;
class UFF_BuildModeSubsystem;
class AFFZSFacilityActor;

/** Read-only facility cards with clear and exit actions. Selection remains owned by BuildMode. */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Multi Selection", DisableNativeTick))
class FESTAFIESTA_API UFF_BuildMultiSelectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFF_BuildMultiSelectionWidget(const FObjectInitializer& ObjectInitializer);

	/** Rebuilds the strip from BuildMode's current selection; safe to call after widget creation. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Multi Selection")
	void RefreshSelection();

	/** Viewport pixel coordinates from the build input path; drawing is presentation-only. */
	void SetMarquee(bool bVisible, FVector2D StartViewportPx, FVector2D EndViewportPx);

	/** Raw world mouse input must exclude this panel before Slate dispatches button clicks. */
	bool IsPointerOverStrip() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> Border_MultiSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Text_MultiSelectionCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UHorizontalBox> Row_SelectedFacilities;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_InspectSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ClearSelection;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Button_ExitMultiSelection;

private:
	void BuildNativeTree();
	void ResolveBindings();
	void AddFacilityCard(const AFFZSFacilityActor& Facility, int32 DisplayIndex);
	void AddEmptyHint();

	UFUNCTION() void HandleSelectionChanged();
	UFUNCTION() void HandleCatalogChanged();
	UFUNCTION() void HandleInspectClicked();
	UFUNCTION() void HandleClearClicked();
	UFUNCTION() void HandleExitClicked();

	TWeakObjectPtr<UFF_BuildModeSubsystem> BuildMode;
	TWeakObjectPtr<UFF_BuildCatalogSubsystem> Catalog;
	bool bMarqueeVisible = false;
	FVector2D MarqueeStartViewportPx = FVector2D::ZeroVector;
	FVector2D MarqueeEndViewportPx = FVector2D::ZeroVector;
};
