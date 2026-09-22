// FF_FFZSTestFixtures.h
// 역할: 자동화 테스트에서 공유하는 정상 시설·지역 데이터를 만든다. 게임 실행용 콘텐츠는 아니다.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * Test-only facility shapes used to exercise category and geometry branches.
 *
 * These values are fixtures, not shipped facility definitions. Runtime facility
 * data must come from UFFZSFacilityDefinition assets.
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
    /** Returns an independently owned, valid facility spec for automation tests. */
    FF_ZONESYSTEM_API FFFZSFacilitySpec MakeValidFacilitySpec(
        EFFZSTestFacilityVariant Variant = EFFZSTestFacilityVariant::FoodTruck);
}

#endif // WITH_DEV_AUTOMATION_TESTS
