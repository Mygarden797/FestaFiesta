// A designer-editable UMG image displaying one cell of a shared UI texture.
#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "FF_BuildAtlasImage.generated.h"

class UTexture2D;

namespace FFBuildAtlas
{
	/** Apply a 0-based row-major cell. Invalid inputs leave the brush unchanged. */
	FESTAFIESTA_API bool TryApply(FSlateBrush& Brush, UTexture2D* Texture,
		int32 Columns, int32 Rows, int32 Index);
}

/**
 * Displays an unmodified cell of a regularly divided image atlas.
 * Cells use left-to-right, top-to-bottom order. All layout, opacity and tint
 * remain authored on the image widget. No per-frame updates or extra materials
 * are needed; multiple instances can share the same UI texture.
 */
UCLASS(meta = (DisplayName = "FF Build Atlas Image"))
class FESTAFIESTA_API UFF_BuildAtlasImage : public UImage
{
	GENERATED_BODY()

public:
	/** Original atlas texture; keep the texture in UI group with mipmaps disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Atlas")
	TObjectPtr<UTexture2D> AtlasTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Atlas", meta = (ClampMin = "1", ClampMax = "128", UIMin = "1", UIMax = "128"))
	int32 AtlasColumns = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Atlas", meta = (ClampMin = "1", ClampMax = "128", UIMin = "1", UIMax = "128"))
	int32 AtlasRows = 1;

	/** Zero-based, left-to-right then top-to-bottom cell index. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance|Atlas", meta = (ClampMin = "0", UIMin = "0"))
	int32 AtlasIndex = 0;

	/** Change only the cell, retaining the authored texture, brush size and tint. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Atlas")
	bool SetAtlasIndex(int32 NewIndex);

	/** Set source and grid together; useful for runtime-created or pooled widgets. */
	UFUNCTION(BlueprintCallable, Category = "FestaFiesta|UI|Atlas")
	bool SetAtlasSource(UTexture2D* NewTexture, int32 NewColumns, int32 NewRows, int32 NewIndex);

	virtual void SynchronizeProperties() override;

private:
	void UpdateAtlasBrush();
};
