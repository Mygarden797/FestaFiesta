/**
 * @file FF_FFZSPhase0BridgeTests.cpp
 * @brief 노션 Phase 0의 항상 성공·고정 슬롯 3개 계약을 회귀 검증한다.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSPhase0Bridge.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSPhase0WorldChangeStubContractTest,
    "FF_ZoneSystem.Unit.Contract.Phase0WorldChangeStub",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPhase0WorldChangeStubContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    // Arrange: 실제 ExecuteCommand라면 거절될 비어 있는 명령도 Phase 0 스텁에는 유효하다.
    const FFFZSCommand InvalidRequest;

    // Act: 같은 요청을 두 번 보내 응답의 결정성과 비권위 성격도 확인한다.
    const FFFZSPhase0WorldChangeResponse First =
        UFFZSPhase0Bridge::RequestWorldChangePhase0(InvalidRequest);
    const FFFZSPhase0WorldChangeResponse Second =
        UFFZSPhase0Bridge::RequestWorldChangePhase0(InvalidRequest);

    // Assert: 성공, 정확히 세 좌표, 고정 순서와 값, 실제 월드 미변경이 Phase 0 계약이다.
    TestTrue(TEXT("Phase 0 스텁은 항상 요청을 허용한다"), First.bSucceeded);
    TestFalse(TEXT("Phase 0 스텁은 권위 월드 상태를 절대 변경하지 않는다"), First.bWorldStateChanged);
    TestEqual(TEXT("응답이 정확히 3개의 프로토타입 슬롯을 제공한다"), First.SlotCoordinatesCm.Num(), 3);

    const FVector ExpectedSlots[] = {
        FVector(1000.0, 2000.0, 0.0),
        FVector(3000.0, 2000.0, 0.0),
        FVector(5000.0, 2000.0, 0.0)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedSlots); ++Index)
    {
        TestTrue(
            *FString::Printf(TEXT("슬롯 %d가 승인된 Phase 0 좌표를 유지한다"), Index),
            First.SlotCoordinatesCm[Index].Equals(ExpectedSlots[Index], KINDA_SMALL_NUMBER));
    }

    TestEqual(TEXT("반복 호출에서도 슬롯 개수가 유지된다"),
        Second.SlotCoordinatesCm.Num(), First.SlotCoordinatesCm.Num());
    for (int32 Index = 0; Index < First.SlotCoordinatesCm.Num(); ++Index)
    {
        TestTrue(TEXT("반복 호출에서도 결정적인 슬롯 순서가 유지된다"),
            Second.SlotCoordinatesCm[Index].Equals(First.SlotCoordinatesCm[Index], KINDA_SMALL_NUMBER));
    }
    TestFalse(TEXT("스텁 응답이 제한 사항을 명확히 설명한다"), First.Message.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
