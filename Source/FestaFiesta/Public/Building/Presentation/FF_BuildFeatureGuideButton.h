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

	/** Required designer-owned controls; names must match in the derived WBP. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Guide;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GuideIcon;

private:
	UFUNCTION()
	void HandleGuideClicked();
};
