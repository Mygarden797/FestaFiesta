// FF_FFZSTestFixtures.h
// 역할: 자동화 테스트에서 공유하는 정상 시설·지역 데이터를 만든다. 게임 실행용 콘텐츠는 아니다.

#pragma once

// 자동화 테스트를 제외한 빌드에서는 시설 데이터 헤더까지 읽지 않도록 선언과 의존성을 함께 감싼다.
#if WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"

/**
 * 시설 종류와 모양에 따른 처리 분기를 검증하기 위한 테스트 전용 예시다.
 * 게임에서 설치할 실제 시설은 UFFZSFacilityDefinition 에셋에서 읽어야 한다.
 */
enum class EFFZSTestFacilityVariant : uint8
{
    FoodTruck,
    Toilet,
    Info,
    Rest,
    Performance,
    Safety,
    PhotoSpot,
    Count
};

namespace FFZSTest
{
    /** 다른 테스트의 변경이 섞이지 않도록 새로운 정상 시설 데이터를 만들어 반환한다. */
    FF_ZONESYSTEM_API FFFZSFacilitySpec MakeValidFacilitySpec(
        EFFZSTestFacilityVariant Variant = EFFZSTestFacilityVariant::FoodTruck);
}

#endif // WITH_DEV_AUTOMATION_TESTS
