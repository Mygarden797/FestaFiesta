// FF_BuildCategoryIcons.h
// 역할: 시설 분류의 새 아이콘과 예비 아이콘을 선택한다.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Styling/SlateBrush.h"

namespace FFBuildCategoryIcons
{
	/** Original SDF schematic pictograms; the existing choice owns selected tint and input. */
	FESTAFIESTA_API FSlateBrush Resolve(bool bAll, EFFZSFacilityCategory Category);
}
