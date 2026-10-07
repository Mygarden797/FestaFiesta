#pragma once

#include "CoreMinimal.h"
#include "FF_StaffPromotionTypes.generated.h"

/** 원본 승급 판정과 명단에서 읽은 표시 상태. 비용이나 별도 승급 규칙을 소유하지 않는다. */
UENUM(BlueprintType)
enum class EFF_StaffPromotionState : uint8
{
	Ready,
	NotStarted,
	NotFound,
	MaxLevel,
	HiredToday,
	AlreadyPromotedToday,
	Unavailable
};
