#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSZoneActor.h"
#include "Tests/FF_FFZSTestFixtures.h"

namespace FFZSTest
{
    // 설정 변경이 본문 검증 실패로 연쇄 전파되지 않게 준비 단계에서 원인을 기록한다.
    inline bool ApplyZonePreset(FAutomationTestBase& Test, AFFZSGameplayZone* Zone,
        const FName ZoneId = ProjectZoneId())
    {
        return Test.TestNotNull(TEXT("Synthetic test zone exists"), Zone) &&
            Test.TestTrue(FString::Printf(
                TEXT("Synthetic zone '%s' is accepted by current ProjectZoneIds; update the shared fixture when zone IDs change"),
                *ZoneId.ToString()), Zone->ApplyAcceptedZonePreset(ZoneId));
    }
}

#endif
