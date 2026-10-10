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
    // Project Settings의 허용 목록을 실제로 통과시키는 합성 월드용 ID다.
    // 지면·시설 모양은 각 테스트가 만들며, 운영 맵의 Zone Definition은 로드하지 않는다.
    // 구역 개편 시 이 두 계약만 갱신한다. 테스트를 위해 ID 제한을 해제하지 않는다.
    inline FName ProjectZoneId() { return FName(TEXT("GZ_Beach01")); }
    inline FName SecondaryProjectZoneId() { return FName(TEXT("GZ_Beach02")); }

    /** 다른 테스트의 변경이 섞이지 않도록 새로운 정상 시설 데이터를 만들어 반환한다. */
    FF_ZONESYSTEM_API FFFZSFacilitySpec MakeValidFacilitySpec(
        EFFZSTestFacilityVariant Variant = EFFZSTestFacilityVariant::FoodTruck);
}

#endif // WITH_DEV_AUTOMATION_TESTS
