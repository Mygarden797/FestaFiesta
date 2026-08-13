#if WITH_DEV_AUTOMATION_TESTS

#include "FF_FFZSFacilityEditorHelpers.h"
#include "FF_FFZSGridLibrary.h"
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

    // 600 x 300 시설의 앞쪽(+Y)에 대기열이 붙어 있고 Entry 앵커가 앞 변에 놓인 표준 배치다.
    FFFZSFacilitySpec ResizeSpec;
    ResizeSpec.FootprintSizeCells = FIntPoint(600, 300);
    ResizeSpec.QueueReservedRects = { FFFZSCellRect(FIntPoint(0, 300), FIntPoint(600, 600)) };
    ResizeSpec.QueueSplinePointsCm = { FVector(0.0, 200.0, 0.0), FVector(0.0, 300.0, 0.0) };
    FFFZSAnchorDefinition& FrontAnchor = ResizeSpec.Anchors.AddDefaulted_GetRef();
    FrontAnchor.LocalTransform.SetTranslation(FVector(0.0, 150.0, 0.0));
    FFFZSAnchorDefinition& BackAnchor = ResizeSpec.Anchors.AddDefaulted_GetRef();
    BackAnchor.LocalTransform.SetTranslation(FVector(0.0, -150.0, 0.0));

    FFFZSFacilitySpec ResizedSpec;
    TestTrue(
        TEXT("Footprint growth keeps the surrounding layout"),
        FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
            ResizeSpec, FIntPoint(600, 400), ResizedSpec));
    TestEqual(TEXT("New footprint depth is applied"), ResizedSpec.FootprintSizeCells.Y, 400);
    TestEqual(
        TEXT("Reserved rectangle bound above the centre takes the full size change"),
        ResizedSpec.QueueReservedRects[0].Min.Y,
        400);
    TestEqual(
        TEXT("Reserved rectangle keeps its depth after the full-delta shift"),
        ResizedSpec.QueueReservedRects[0].Max.Y,
        700);
    TestEqual(
        TEXT("Reserved rectangle bound below the centre stays on the grid origin"),
        ResizedSpec.QueueReservedRects[0].Min.X,
        0);
    TestEqual(
        TEXT("Reserved rectangle width is unchanged when width is unchanged"),
        ResizedSpec.QueueReservedRects[0].Max.X,
        600);
    TestTrue(
        TEXT("Queue points move by half the size change"),
        ResizedSpec.QueueSplinePointsCm[1].Equals(FVector(0.0, 350.0, 0.0)));
    TestTrue(
        TEXT("Front anchor stays on the front edge"),
        ResizedSpec.Anchors[0].LocalTransform.GetTranslation().Equals(FVector(0.0, 200.0, 0.0)));
    TestTrue(
        TEXT("Back anchor stays on the back edge"),
        ResizedSpec.Anchors[1].LocalTransform.GetTranslation().Equals(FVector(0.0, -200.0, 0.0)));

    FFFZSFacilitySpec ShrunkSpec;
    TestTrue(
        TEXT("Footprint shrink is the exact inverse"),
        FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
            ResizedSpec, FIntPoint(600, 300), ShrunkSpec));
    TestTrue(
        TEXT("Shrinking restores the original reserved rectangle"),
        ShrunkSpec.QueueReservedRects[0].Min == ResizeSpec.QueueReservedRects[0].Min &&
        ShrunkSpec.QueueReservedRects[0].Max == ResizeSpec.QueueReservedRects[0].Max);
    TestTrue(
        TEXT("Shrinking restores the original front anchor"),
        ShrunkSpec.Anchors[0].LocalTransform.GetTranslation().Equals(
            ResizeSpec.Anchors[0].LocalTransform.GetTranslation()));

    FFFZSFacilitySpec LegacySpec = ResizeSpec;
    LegacySpec.QueueReservedCells = { FIntPoint(0, 300) };
    FFFZSFacilitySpec RejectedSpec;
    TestFalse(
        TEXT("Legacy 1 cm cell lists are refused instead of shifted incorrectly"),
        FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
            LegacySpec, FIntPoint(600, 400), RejectedSpec));
    TestFalse(
        TEXT("Non-positive target sizes are refused"),
        FFFZSFacilityEditorHelpers::TryResizeFootprintKeepingLayout(
            ResizeSpec, FIntPoint(0, 400), RejectedSpec));

    // 600 x 300 시설의 앞(+Y)으로 나갔다가 오른쪽(+X)으로 꺾이는 L자 대기열이다.
    // 경로 길이 300 + 400 = 700cm이고 8명 x 100cm 간격이 정확히 그 길이를 사용한다.
    FFFZSFacilitySpec CorridorSpec;
    CorridorSpec.FootprintSizeCells = FIntPoint(600, 300);
    CorridorSpec.QueueRules.MaxWaitingAgents = 8;
    CorridorSpec.QueueRules.SlotSpacingCm = 100.0f;
    CorridorSpec.QueueSplinePointsCm = {
        FVector(0.0, 200.0, 0.0),
        FVector(0.0, 500.0, 0.0),
        FVector(400.0, 500.0, 0.0)
    };

    TArray<FFFZSCellRect> CorridorRects;
    TestTrue(
        TEXT("A bent queue path produces reserved rectangles"),
        FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(CorridorSpec, 50, CorridorRects));
    TestTrue(
        TEXT("A bend is reserved as a corridor rather than one rectangle"),
        CorridorRects.Num() >= 2);
    TestEqual(
        TEXT("The corridor reserves only the walked area, not the bounding box"),
        UFFZSGridLibrary::GetCellRectArea(CorridorRects),
        static_cast<int64>(80'000));
    TestFalse(
        TEXT("The corridor never overlaps the facility footprint"),
        UFFZSGridLibrary::DoCellRectSetsOverlap(
            CorridorRects,
            { FFFZSCellRect(FIntPoint::ZeroValue, CorridorSpec.FootprintSizeCells) }));

    TArray<FVector> CorridorSlots;
    TestTrue(
        TEXT("Slot points can be sampled along the bent path"),
        FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
            CorridorSpec.QueueSplinePointsCm,
            CorridorSpec.QueueRules.MaxWaitingAgents,
            CorridorSpec.QueueRules.SlotSpacingCm,
            CorridorSlots));
    bool bAllCorridorSlotsReserved = true;
    for (const FVector& SlotPoint : CorridorSlots)
    {
        bAllCorridorSlotsReserved &= FFFZSQueueGeometry::IsLocalPointInsideReservation(
            SlotPoint, CorridorSpec.FootprintSizeCells, CorridorRects);
    }
    TestTrue(
        TEXT("Every slot of the full capacity stays inside the generated corridor"),
        bAllCorridorSlotsReserved);

    // 저작 경로가 정원이 쓰는 길이보다 길면 남는 꼬리는 예약하지 않는다.
    FFFZSFacilitySpec OverlongSpec = CorridorSpec;
    OverlongSpec.QueueRules.MaxWaitingAgents = 4;
    OverlongSpec.QueueSplinePointsCm = {
        FVector(0.0, 200.0, 0.0),
        FVector(0.0, 1000.0, 0.0)
    };
    TArray<FFFZSCellRect> TrimmedRects;
    TestTrue(
        TEXT("An overlong authored path still produces a reservation"),
        FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(OverlongSpec, 50, TrimmedRects));
    TestEqual(TEXT("A straight path needs a single rectangle"), TrimmedRects.Num(), 1);
    TestEqual(
        TEXT("The unused tail beyond the last slot is not reserved"),
        TrimmedRects[0].Max.Y,
        700);
    TestEqual(
        TEXT("The reservation starts at the footprint edge, not inside the body"),
        TrimmedRects[0].Min.Y,
        300);

    TArray<FFFZSCellRect> RefusedRects;
    TestFalse(
        TEXT("A zero margin would reserve a zero-width corridor and is refused"),
        FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(CorridorSpec, 0, RefusedRects));
    FFFZSFacilitySpec LegacyCorridorSpec = CorridorSpec;
    LegacyCorridorSpec.QueueReservedCells = { FIntPoint(0, 300) };
    TestFalse(
        TEXT("Legacy 1 cm cell lists are refused instead of half-replaced"),
        FFFZSFacilityEditorHelpers::TryBuildQueueRectsFromPath(LegacyCorridorSpec, 50, RefusedRects));

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
