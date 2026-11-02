// Designer-authored icon button that opens a building feature explanation.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "FF_BuildFeatureGuideButton.generated.h"

class UButton;
class UImage;

/**
 * Place a WBP derived from this class inside WBP_FF_BuildTools. The WBP owns
 * its button style, focus behavior and layout; the topic ID and image brush may
 * be customized on each instance. Opening a guide does not execute its action.
 */
UCLASS(Blueprintable, meta = (DisplayName = "FF Build Feature Guide Button"))
class FESTAFIESTA_API UFF_BuildFeatureGuideButton : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Stable ID from FFBuildFeatureGuide::Topics(). Empty or unknown IDs do nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide", meta = (ExposeOnSpawn = "true"))
	FName FeatureTopicId;

	/** The icon shown by Image_GuideIcon; configure per instance in the UMG Designer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide", meta = (ExposeOnSpawn = "true"))
	FSlateBrush IconBrush;

	/** Sample one cell of IconBrush's texture atlas. Leave disabled for standalone icons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Icon Atlas")
	bool bUseIconAtlas = false;

	/** Atlas dimensions and zero-based, row-major cell. Visible in each UMG instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Icon Atlas", meta = (EditCondition = "bUseIconAtlas", ClampMin = "1", ClampMax = "32", UIMin = "1", UIMax = "32"))
	int32 IconAtlasColumns = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Icon Atlas", meta = (EditCondition = "bUseIconAtlas", ClampMin = "1", ClampMax = "32", UIMin = "1", UIMax = "32"))
	int32 IconAtlasRows = 5;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Icon Atlas", meta = (EditCondition = "bUseIconAtlas", ClampMin = "0", UIMin = "0"))
	int32 IconAtlasCell = 0;

	/** Subtle, frame-rate independent motion. Only the icon moves, never its hit target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Motion")
	bool bEnableIconMotion = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Motion", meta = (ClampMin = "1.0", ClampMax = "1.15"))
	float IconHoverScale = 1.075f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Motion", meta = (ClampMin = "0.85", ClampMax = "1.0"))
	float IconPressedScale = .94f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feature Guide|Motion", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float IconHoverLift = 1.5f;

	/** Returns the authored brush with the chosen normalized atlas UVs applied. */
	FSlateBrush ResolveIconBrush() const;

	/** Opens only an existing topic on the enclosing, same-player build tools widget. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Guide")
	bool TryOpenGuide();

	/** Refreshes the icon and accessible label after changing instance data at runtime. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|Building|Guide")
	void RefreshGuidePresentation();

	/** Returns focus to the internal button for its owning player, if it is usable. */
	bool RestoreGuideFocus();

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Required designer-owned controls; names must match in the derived WBP. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Guide;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GuideIcon;

private:
	FWidgetTransform AuthoredIconTransform;
	FVector2D AuthoredIconPivot = FVector2D(.5f, .5f);
	float MotionScale = 1.f;
	float MotionLift = 0.f;
	bool bCapturedIconTransform = false;
	void ResetIconMotion();

	UFUNCTION()
	void HandleGuideClicked();
};
