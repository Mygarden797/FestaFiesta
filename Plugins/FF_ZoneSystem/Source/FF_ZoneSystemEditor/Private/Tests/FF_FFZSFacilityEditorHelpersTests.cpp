#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSQueueGeometry.h"

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSFacilityEditorHelpersTest,
    "FF_ZoneSystem.Editor.FacilityAuthoringHelpers",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSFacilityEditorHelpersTest::RunTest(const FString& Parameters)
{
    FIntPoint Footprint = FIntPoint::ZeroValue;
    TestTrue(
        TEXT("Mesh bounds produce conservative footprint cells"),
        FFFZSFacilityEditorHelpers::TryCalculateFootprintCells(FVector(600.01, 299.1, 250.0), Footprint));
    TestEqual(TEXT("Footprint X rounds up"), Footprint.X, 601);
    TestEqual(TEXT("Footprint Y rounds up"), Footprint.Y, 300);

    TestTrue(
        TEXT("Source bounds account for authored rotation and offset"),
        FFFZSFacilityEditorHelpers::TryCalculateFootprintCellsFromSourceBounds(
            FBox(FVector(-100.0, -50.0, 0.0), FVector(100.0, 50.0, 100.0)),
            FRotator(0.0, 90.0, 0.0),
            FVector(25.0, 0.0, 0.0),
            Footprint));
    TestEqual(TEXT("Rotated width and offset are contained around the origin"), Footprint.X, 150);
    TestEqual(TEXT("Rotated depth is contained around the origin"), Footprint.Y, 200);

    const TArray<FVector> BentQueue = {
        FVector(0.0, 0.0, 0.0),
        FVector(100.0, 0.0, 0.0),
        FVector(100.0, 100.0, 0.0)
    };

    int32 Capacity = 0;
    TestTrue(
        TEXT("Capacity can be derived from polyline length"),
        FFFZSFacilityEditorHelpers::TryCalculateMaxWaitingAgents(BentQueue, 50.0, Capacity));
    TestEqual(TEXT("Head slot is included in capacity"), Capacity, 5);

    TArray<FVector> ShortenedQueue;
    TestTrue(
        TEXT("Queue tail can be shortened through a bend"),
        FFFZSFacilityEditorHelpers::TryResizeQueueTail(BentQueue, 150.0, ShortenedQueue));
    TestEqual(TEXT("Shortened queue keeps the bend"), ShortenedQueue.Num(), 3);
    TestTrue(
        TEXT("Shortened tail is sampled on the final segment"),
        ShortenedQueue.Last().Equals(FVector(100.0, 50.0, 0.0)));

    TArray<FVector> ExtendedQueue;
    TestTrue(
        TEXT("Queue tail can be extended"),
        FFFZSFacilityEditorHelpers::TryResizeQueueTail(BentQueue, 250.0, ExtendedQueue));
    TestTrue(
        TEXT("Extension follows the last non-degenerate segment"),
        ExtendedQueue.Last().Equals(FVector(100.0, 150.0, 0.0)));

    const double MicroSegmentCm = 0.0005;
    const TArray<FVector> QueueWithMicroPrefix = {
        FVector::ZeroVector,
        FVector(MicroSegmentCm, 0.0, 0.0),
        FVector(10.0, 0.0, 0.0)
    };
    TArray<FVector> MicroPrefixResize;
    TestTrue(
        TEXT("Tail resize uses the runtime geometry threshold for micro segments"),
        FFFZSFacilityEditorHelpers::TryResizeQueueTail(
            QueueWithMicroPrefix, 5.0, MicroPrefixResize));
    TestEqual(
        TEXT("Resized micro-prefix path has the exact shared runtime length"),
        FFFZSQueueGeometry::GetLinearPolylineLengthCm(MicroPrefixResize),
        5.0,
        UE_DOUBLE_KINDA_SMALL_NUMBER);
    FVector RuntimeTail = FVector::ZeroVector;
    TestTrue(
        TEXT("Shared sampler reaches the resized editor tail"),
        FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(
            MicroPrefixResize, 5.0, RuntimeTail));
    TestTrue(
        TEXT("Editor and runtime agree on the micro-prefix tail position"),
        RuntimeTail == MicroPrefixResize.Last());

    double RequiredLengthCm = 0.0;
    TestTrue(
        TEXT("Required length can be derived from capacity"),
        FFFZSFacilityEditorHelpers::TryCalculateRequiredQueueLengthCm(12, 115.0, RequiredLengthCm));
    TestEqual(TEXT("Slot zero needs no leading spacing"), RequiredLengthCm, 1265.0);

    return true;
}

#endif
