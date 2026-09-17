#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Styling/SlateBrush.h"

namespace FFBuildCategoryIcons
{
	/** Existing project artwork; the category choice owns the displayed size and selected tint. */
	FESTAFIESTA_API FSlateBrush Resolve(bool bAll, EFFZSFacilityCategory Category);
}
