// FF_FFZSCollision.h

// FF_ZoneSystem과 이를 소비하는 게임 모듈이 공유하는 충돌 채널 계약이다.

#pragma once

#include "Engine/EngineTypes.h"

namespace FFZSCollision
{
    // 시설 선택 전용 Trace Channel. DefaultEngine.ini의 FacilitySelection 채널과 같은 슬롯을 사용한다.
    inline constexpr ECollisionChannel FacilitySelectionTraceChannel = ECC_GameTraceChannel1;

    // 기존 시설 메시의 플레이어 이동 충돌용 Object Channel. 기본 응답은 Ignore이며
    // 게임의 플레이어 캡슐만 Block으로 참여한다. NPC·선택 Trace에는 영향을 주지 않는다.
    inline constexpr ECollisionChannel FacilityPlayerObjectChannel = ECC_GameTraceChannel2;
}
