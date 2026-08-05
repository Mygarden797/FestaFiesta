/**
 * @file FF_FFZSFacilityAnchorComponent.cpp
 * @brief 데이터 전용 시설 앵커 컴포넌트의 링크 구현 단위다.
 * 흐름: 생성과 갱신은 시설 액터가 담당하고 이 컴포넌트는 Type·Index·Transform만 제공한다.
 * 불변식: 이동 요청이나 권한 상태를 소유하지 않는다.
 * 실패/성능: Tick과 별도 런타임 로직이 없다.
 */

#include "FF_FFZSFacilityAnchorComponent.h"

// 컴포넌트 생성·부착·Transform 갱신은 AFFZSFacilityActor::RebuildAnchors가 한 곳에서 담당한다.
// 여기에는 별도 Tick이나 등록 부작용을 추가하지 않는다.

