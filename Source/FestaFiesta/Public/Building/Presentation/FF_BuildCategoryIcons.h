#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Styling/SlateBrush.h"

namespace FFBuildCategoryIcons
{
	/** Original SDF schematic pictograms; the existing choice owns selected tint and input. */
	FESTAFIESTA_API FSlateBrush Resolve(bool bAll, EFFZSFacilityCategory Category);
}
