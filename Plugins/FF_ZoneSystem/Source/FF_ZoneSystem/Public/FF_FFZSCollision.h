// FF_FFZSCollision.h

// FF_ZoneSystem과 이를 소비하는 게임 모듈이 공유하는 충돌 채널 계약이다.

#pragma once

#include "Engine/EngineTypes.h"

namespace FFZSCollision
{
    // 시설 선택 전용 Trace Channel. DefaultEngine.ini의 FacilitySelection 채널과 같은 슬롯을 사용한다.
    inline constexpr ECollisionChannel FacilitySelectionTraceChannel = ECC_GameTraceChannel1;
}
