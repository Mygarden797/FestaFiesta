// FF_FFZSQueueGeometryTests.cpp
// 역할: 꺾인 대기 경로에서도 자리 간격과 위치가 올바르게 계산되는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometryZeroLengthSegmentTest,
    "FF_ZoneSystem.Unit.QueueGeometry.ZeroLengthSegment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometryZeroLengthSegmentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FVector> Points = {
        FVector(10.0, 20.0, 0.0),
        FVector(10.0, 20.0, 0.0),
        FVector(110.0, 20.0, 0.0)};

    TestEqual(TEXT("A zero-length segment does not add to the path length"),
        FFFZSQueueGeometry::GetLinearPolylineLengthCm(Points), 100.0);

    FVector Sample = FVector::ZeroVector;
    TestTrue(TEXT("Sampling succeeds after a zero-length segment"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(Points, 25.0, Sample));
    TestTrue(TEXT("The zero-length segment is skipped without shifting distance"),
        Sample.Equals(FVector(35.0, 20.0, 0.0), UE_KINDA_SMALL_NUMBER));

    TArray<FVector> Slots;
    TestTrue(TEXT("Bulk slot generation also skips a zero-length segment"),
        FFFZSQueueGeometry::BuildLinearQueueSlotPoints(Points, 3, 50.0, Slots));
    TestEqual(TEXT("Bulk sampling preserves the requested slot count"), Slots.Num(), 3);
    if (Slots.Num() == 3)
    {
        TestTrue(TEXT("The bulk sampler reaches the exact tail after the degenerate segment"),
            Slots.Last().Equals(Points.Last(), UE_KINDA_SMALL_NUMBER));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometryExactTailTest,
    "FF_ZoneSystem.Unit.QueueGeometry.ExactTail",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometryExactTailTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FVector> Points = {
        FVector::ZeroVector,
        FVector(100.0, 0.0, 0.0)};

    FVector ExactTail = FVector::ZeroVector;
    TestTrue(TEXT("Sampling exactly at the total length succeeds"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(Points, 100.0, ExactTail));
    TestTrue(TEXT("The exact total length resolves to the tail point"),
        ExactTail.Equals(Points.Last(), UE_KINDA_SMALL_NUMBER));

    FVector BeyondTail = FVector::ZeroVector;
    TestTrue(TEXT("Sampling beyond the total length succeeds"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(Points, 150.0, BeyondTail));
    TestTrue(TEXT("A distance beyond the path clamps to the tail"),
        BeyondTail.Equals(Points.Last(), UE_KINDA_SMALL_NUMBER));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometryMultiSegmentTest,
    "FF_ZoneSystem.Unit.QueueGeometry.MultiSegmentSlots",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometryMultiSegmentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FVector> Points = {
        FVector::ZeroVector,
        FVector(100.0, 0.0, 0.0),
        FVector(100.0, 200.0, 0.0)};

    TArray<FVector> Slots;
    TestTrue(TEXT("Slot generation succeeds for a multi-segment path"),
        FFFZSQueueGeometry::BuildLinearQueueSlotPoints(Points, 7, 50.0, Slots));
    TestEqual(TEXT("The requested number of slots is generated"), Slots.Num(), 7);

    const TArray<FVector> Expected = {
        FVector(0.0, 0.0, 0.0),
        FVector(50.0, 0.0, 0.0),
        FVector(100.0, 0.0, 0.0),
        FVector(100.0, 50.0, 0.0),
        FVector(100.0, 100.0, 0.0),
        FVector(100.0, 150.0, 0.0),
        FVector(100.0, 200.0, 0.0)};
    for (int32 Index = 0; Index < FMath::Min(Slots.Num(), Expected.Num()); ++Index)
    {
        TestTrue(*FString::Printf(TEXT("Slot %d follows cumulative polyline distance"), Index),
            Slots[Index].Equals(Expected[Index], UE_KINDA_SMALL_NUMBER));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometrySegmentTangentTest,
    "FF_ZoneSystem.Unit.QueueGeometry.SegmentTangentAtBends",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometrySegmentTangentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FVector> Points = {
        FVector::ZeroVector,
        FVector(100.0, 0.0, 0.0),
        FVector(100.0, 200.0, 0.0)};

    FVector Sample = FVector(999.0);
    FVector Tangent = FVector(999.0);
    TestTrue(TEXT("Sampling the exact bend returns a position and tangent"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
            Points, 100.0, Sample, Tangent));
    TestTrue(TEXT("The exact bend keeps the authored corner position"),
        Sample.Equals(FVector(100.0, 0.0, 0.0), UE_KINDA_SMALL_NUMBER));
    TestTrue(TEXT("The incoming segment owns the tangent at an exact bend"),
        Tangent.Equals(FVector(1.0, 0.0, 0.0), UE_KINDA_SMALL_NUMBER));

    TestTrue(TEXT("Sampling beyond the bend succeeds"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
            Points, 125.0, Sample, Tangent));
    TestTrue(TEXT("A point beyond the bend follows the second segment"),
        Sample.Equals(FVector(100.0, 25.0, 0.0), UE_KINDA_SMALL_NUMBER));
    TestTrue(TEXT("A point beyond the bend uses the outgoing tangent"),
        Tangent.Equals(FVector(0.0, 1.0, 0.0), UE_KINDA_SMALL_NUMBER));

    TestTrue(TEXT("Sampling beyond the tail clamps successfully"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
            Points, 1000.0, Sample, Tangent));
    TestTrue(TEXT("The clamped tail keeps the final usable segment tangent"),
        Sample.Equals(Points.Last(), UE_KINDA_SMALL_NUMBER) &&
        Tangent.Equals(FVector(0.0, 1.0, 0.0), UE_KINDA_SMALL_NUMBER));

    const TArray<FVector> Degenerate = {
        FVector::ZeroVector,
        FVector::ZeroVector};
    Sample = FVector(999.0);
    Tangent = FVector(999.0);
    TestFalse(TEXT("A path with no usable segment has no tangent"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
            Degenerate, 0.0, Sample, Tangent));
    TestTrue(TEXT("A failed tangent sample resets both outputs"),
        Sample == FVector::ZeroVector && Tangent == FVector::ZeroVector);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometryReservationCoordinatesTest,
    "FF_ZoneSystem.Unit.QueueGeometry.ReservationCoordinates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometryReservationCoordinatesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FIntPoint Footprint(600, 300);
    const FVector LocalPoint(250.0, 200.0, 0.0);
    TestTrue(TEXT("actor 중심 local cm가 footprint 좌하단 예약 좌표로 변환된다"),
        FFFZSQueueGeometry::LocalPointCmToReservationGrid(LocalPoint, Footprint)
            .Equals(FVector2D(550.0, 350.0), UE_KINDA_SMALL_NUMBER));

    const TArray<FFFZSCellRect> ReservedRects = {
        FFFZSCellRect(FIntPoint(500, 300), FIntPoint(600, 1700))};
    TestTrue(TEXT("변환된 queue slot은 반열림 예약 rect 내부로 판정된다"),
        FFFZSQueueGeometry::IsLocalPointInsideReservation(
            LocalPoint, Footprint, ReservedRects));
    TestFalse(TEXT("footprint 반대편 local point는 예약 rect 밖이다"),
        FFFZSQueueGeometry::IsLocalPointInsideReservation(
            FVector(-250.0, 200.0, 0.0), Footprint, ReservedRects));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSQueueGeometryNearDegenerateSegmentsTest,
    "FF_ZoneSystem.Unit.QueueGeometry.NearDegenerateSegmentsAreConsistent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueGeometryNearDegenerateSegmentsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const double Tiny = UE_DOUBLE_KINDA_SMALL_NUMBER * 0.5;
    const TArray<FVector> Points = {
        FVector::ZeroVector,
        FVector(Tiny, 0.0, 0.0),
        FVector(Tiny * 2.0, 0.0, 0.0),
        FVector(100.0, 0.0, 0.0)
    };

    TestEqual(TEXT("Length ignores the same near-degenerate segments as sampling"),
        FFFZSQueueGeometry::GetLinearPolylineLengthCm(Points), 100.0 - Tiny * 2.0,
        UE_DOUBLE_KINDA_SMALL_NUMBER);
    FVector Tail = FVector::ZeroVector;
    TestTrue(TEXT("Sampling the reported length reaches the authored tail"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(
            Points,
            FFFZSQueueGeometry::GetLinearPolylineLengthCm(Points),
            Tail));
    TestTrue(TEXT("The sampled tail is not collapsed by discarded micro segments"),
        Tail.Equals(Points.Last(), UE_KINDA_SMALL_NUMBER));

    FVector Head = FVector::ZeroVector;
    TestTrue(TEXT("Sampling zero distance succeeds for a near-degenerate prefix"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(Points, 0.0, Head));
    TestTrue(TEXT("Zero distance always resolves to the authored first point"),
        Head == Points[0]);

    TArray<FVector> Slots;
    TestTrue(TEXT("Bulk sampling preserves the authored head with a near-degenerate prefix"),
        FFFZSQueueGeometry::BuildLinearQueueSlotPoints(Points, 2, 50.0, Slots));
    TestTrue(TEXT("Slot zero is exactly the authored first point"),
        Slots.Num() == 2 && Slots[0] == Points[0]);

    const TArray<FVector> AllDegeneratePoints = {
        FVector::ZeroVector,
        FVector(Tiny, 0.0, 0.0),
        FVector(Tiny * 2.0, 0.0, 0.0)
    };
    TestTrue(TEXT("An all-degenerate path still produces a single head slot"),
        FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
            AllDegeneratePoints, 1, 115.0, Slots));
    TestTrue(TEXT("The all-degenerate single slot is the authored first point"),
        Slots.Num() == 1 && Slots[0] == AllDegeneratePoints[0]);
    return true;
}

#endif
