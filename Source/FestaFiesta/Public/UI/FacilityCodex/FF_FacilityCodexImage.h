#pragma once

#include "CoreMinimal.h"
#include "Components/Image.h"
#include "FF_FacilityCodexImage.generated.h"

class UTextBlock;
class UTexture2D;

/** Async thumbnail with a designer-owned placeholder. Pooled rows cancel stale requests. */
UCLASS(meta=(DisplayName="FF Facility Codex Image"))
class FESTAFIESTA_API UFF_FacilityCodexImage final : public UImage
{
	GENERATED_BODY()
public:
	void ShowThumbnail(TSoftObjectPtr<UTexture2D> Texture, UTextBlock* Fallback);
};
