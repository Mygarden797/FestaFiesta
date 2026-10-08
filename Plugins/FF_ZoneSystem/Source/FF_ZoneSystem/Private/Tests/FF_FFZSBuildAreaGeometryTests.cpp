// FF_FFZSBuildAreaGeometryTests.cpp
// 역할: 건설 영역 합집합과 경계 포함 판정의 기하학적 예외를 자동 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSBuildAreaGeometry.h"

namespace
{
    FFFZSBuildAreaSourcePolygon MakeSource(
        const double MinX, const double MinY, const double MaxX, const double MaxY,
        const double MinZ = -100.0, const double MaxZ = 100.0)
    {
        FFFZSBuildAreaSourcePolygon Result;
        Result.WorldVertices = {
            FVector2D(MinX, MinY), FVector2D(MaxX, MinY),
            FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY)};
        Result.MinZ = MinZ;
        Result.MaxZ = MaxZ;
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildAreaSharedSeamTest,
    "FF_ZoneSystem.Unit.BuildArea.SharedSeamOverlapAndRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildAreaSharedSeamTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSBuildAreaGeometry Geometry;
    TestTrue(TEXT("Adjacent regions build an exact union"), Geometry.Rebuild({
        MakeSource(-100.0, -100.0, 0.0, 100.0), MakeSource(0.0, -100.0, 100.0, 100.0)}));
    TestTrue(TEXT("A footprint may cross a former region seam"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(50, 50), 0), 0));
    TestTrue(TEXT("An arbitrary yaw footprint may cross the seam"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(50, 20), 37), 0));
    TestTrue(TEXT("Outer boundary contact remains allowed"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(100, 100), 0), 0));
    TestFalse(TEXT("Rotating the full-size footprint outside the union is rejected"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(100, 100), 45), 0));
    TestFalse(TEXT("An outside edge cannot pass the inclusive tolerance"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(0.2, 0), FVector2D(100, 100), 0), 0));
    TestTrue(TEXT("Overlapping regions can be merged"), Geometry.Rebuild({
        MakeSource(-100.0, -100.0, 30.0, 100.0), MakeSource(-30.0, -100.0, 100.0, 100.0)}));
    TestTrue(TEXT("Overlapping area is counted once"), FMath::IsNearlyEqual(Geometry.GetAreaSquareMeters(), 4.0, 1.e-8));
    TestTrue(TEXT("Overlapping internal edges disappear"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(90, 90), 0), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildAreaHoleAndGapTest,
    "FF_ZoneSystem.Unit.BuildArea.HolesConcavityAndDisconnectedGaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildAreaHoleAndGapTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSBuildAreaGeometry Geometry;
    TestTrue(TEXT("Four strips form a ring"), Geometry.Rebuild({
        MakeSource(0, 0, 100, 40), MakeSource(0, 60, 100, 100),
        MakeSource(0, 40, 40, 60), MakeSource(60, 40, 100, 60)}));
    TestFalse(TEXT("An enclosed hole never becomes buildable"), Geometry.ContainsWorldPoint(FVector(50, 50, 0)));
    TestTrue(TEXT("A ring strip is buildable"), Geometry.ContainsWorldPoint(FVector(20, 50, 0)));
    TestFalse(TEXT("Four valid corners do not permit a footprint to cover an enclosed hole"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(50, 50), FVector2D(40, 40), 0), 0));
    TestTrue(TEXT("A footprint may touch the hole's boundary without entering it"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(20, 50), FVector2D(20, 20), 0), 0));
    TestTrue(TEXT("A retraced union connector cannot reject a rectangle on a filled source seam"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(62, 40), FVector2D(1, 1), 0), 0));
    TestTrue(TEXT("The opposite filled source seam also permits crossing"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(20, 40), FVector2D(1, 1), 0), 0));
    TestTrue(TEXT("Disconnected islands retain their gap"), Geometry.Rebuild({
        MakeSource(0, 0, 40, 100), MakeSource(60, 0, 100, 100)}));
    TestFalse(TEXT("A footprint spanning islands is rejected"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(50, 50), FVector2D(40, 40), 0), 0));
    TestFalse(TEXT("A point in the gap is rejected"), Geometry.ContainsWorldPoint(FVector(50, 50, 0)));

    FFFZSBuildAreaSourcePolygon Concave = MakeSource(0, 0, 100, 100);
    Concave.WorldVertices = {FVector2D(0, 0), FVector2D(100, 0), FVector2D(100, 100),
        FVector2D(60, 100), FVector2D(60, 40), FVector2D(40, 40), FVector2D(40, 100), FVector2D(0, 100)};
    TestTrue(TEXT("Concave source polygon builds"), Geometry.Rebuild({Concave}));
    TestFalse(TEXT("A concave notch inside four valid corners is rejected"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(50, 50), FVector2D(40, 40), 0), 0));
    TestTrue(TEXT("The base of a concave polygon remains buildable"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(FVector2D(50, 20), FVector2D(40, 15), 0), 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildAreaHeightAndInvalidationTest,
    "FF_ZoneSystem.Unit.BuildArea.VerticalBoundsAndFailedRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildAreaHeightAndInvalidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSBuildAreaGeometry Geometry;
    TestTrue(TEXT("Sources may have different vertical ranges"), Geometry.Rebuild({
        MakeSource(-100, -100, 0, 100, -100, 0), MakeSource(0, -100, 100, 100, 0, 100)}));
    const FFFZSOrientedRect Crossing(FVector2D::ZeroVector, FVector2D(20, 20), 0);
    TestTrue(TEXT("Original inclusive vertical boundaries can share a horizontal plane"),
        Geometry.ContainsWorldOrientedRect(Crossing, 0));
    TestFalse(TEXT("A lower region cannot be extended into the upper region's height"),
        Geometry.ContainsWorldOrientedRect(Crossing, 1));
    TestFalse(TEXT("An upper region cannot be extended into the lower region's height"),
        Geometry.ContainsWorldOrientedRect(Crossing, -1));
    TestFalse(TEXT("A point above its own source region is rejected"),
        Geometry.ContainsWorldPoint(FVector(-50, 0, 50)));
    TestTrue(TEXT("A point on its original bottom boundary remains valid"),
        Geometry.ContainsWorldPoint(FVector(-50, 0, -100)));
    TestFalse(TEXT("An empty rebuild fails"), Geometry.Rebuild({}));
    TestFalse(TEXT("A failed rebuild clears old authority"), Geometry.IsReady());
    TestFalse(TEXT("An old boundary is not retained after failure"), Geometry.ContainsWorldPoint(FVector(-50, 0, -50)));
    FFFZSBuildAreaSourcePolygon Invalid = MakeSource(0, 0, 100, 100);
    Invalid.WorldVertices.SetNum(2);
    TestFalse(TEXT("Invalid source polygons fail closed"), Geometry.Rebuild({Invalid}));
    TestFalse(TEXT("Invalid rectangles are never accepted"),
        Geometry.ContainsWorldOrientedRect(FFFZSOrientedRect(), 0));
    TestTrue(TEXT("A later valid rebuild recovers"), Geometry.Rebuild({MakeSource(1000, 1000, 2000, 2000)}));
    TestFalse(TEXT("A rebuild does not retain an old location"), Geometry.ContainsWorldPoint(FVector(-50, 0, -50)));
    TestTrue(TEXT("The new location takes effect"), Geometry.ContainsWorldPoint(FVector(1500, 1500, 0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildAreaBoundaryPresentationTest,
    "FF_ZoneSystem.Unit.BuildArea.VisibleBoundaryMatchesUnion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildAreaBoundaryPresentationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSBuildAreaGeometry Geometry;
    const FBox2D View(FVector2D(-200), FVector2D(200));
    TArray<FFFZSBuildAreaLineSegment> Lines;
    TestTrue(TEXT("Adjacent regions with different heights build"), Geometry.Rebuild({
        MakeSource(-100, -100, 0, 100, -100, 0), MakeSource(0, -100, 100, 100, 0, 100)}));
    Geometry.GetBoundarySegments(0, View, Lines);
    double Length = 0;
    for (const FFFZSBuildAreaLineSegment& Line : Lines)
    {
        Length += FVector2D::Distance(Line.Start, Line.End);
        TestFalse(TEXT("The shared height plane does not display the internal region seam"),
            FMath::IsNearlyZero(Line.Start.X) && FMath::IsNearlyZero(Line.End.X));
    }
    TestTrue(TEXT("Only the union perimeter is drawn once"), FMath::IsNearlyEqual(Length, 800.0));
    Geometry.GetBoundarySegments(1, View, Lines);
    TestTrue(TEXT("The height-dependent outside edge remains visible"), Lines.ContainsByPredicate(
        [](const FFFZSBuildAreaLineSegment& Line)
        { return FMath::IsNearlyZero(Line.Start.X) && FMath::IsNearlyZero(Line.End.X); }));
    Geometry.GetBoundarySegments(200, View, Lines);
    TestTrue(TEXT("No boundary is invented beyond source heights"), Lines.IsEmpty());

    TestTrue(TEXT("A union with a hole builds"), Geometry.Rebuild({
        MakeSource(0, 0, 100, 40), MakeSource(0, 60, 100, 100),
        MakeSource(0, 40, 40, 60), MakeSource(60, 40, 100, 60)}));
    Geometry.GetBoundarySegments(0, FBox2D(FVector2D(35), FVector2D(65)), Lines);
    Length = 0;
    // Polygon booleans pack/unpack integer coordinates. A 0.001cm comparison still checks
    // presentation 100 times more precisely than the authority's 0.1cm boundary tolerance.
    constexpr double PresentationToleranceCm = 0.001;
    bool HoleSides[4] = {false, false, false, false};
    for (const FFFZSBuildAreaLineSegment& Line : Lines)
    {
        Length += FVector2D::Distance(Line.Start, Line.End);
        for (const FVector2D& Point : {Line.Start, Line.End})
        {
            TestTrue(TEXT("Clipped hole vertices stay within the expected 40-60cm square"),
                Point.X >= 40.0 - PresentationToleranceCm && Point.X <= 60.0 + PresentationToleranceCm &&
                Point.Y >= 40.0 - PresentationToleranceCm && Point.Y <= 60.0 + PresentationToleranceCm);
        }
        HoleSides[0] |= FMath::IsNearlyEqual(Line.Start.X, 40.0, PresentationToleranceCm) &&
            FMath::IsNearlyEqual(Line.End.X, 40.0, PresentationToleranceCm);
        HoleSides[1] |= FMath::IsNearlyEqual(Line.Start.X, 60.0, PresentationToleranceCm) &&
            FMath::IsNearlyEqual(Line.End.X, 60.0, PresentationToleranceCm);
        HoleSides[2] |= FMath::IsNearlyEqual(Line.Start.Y, 40.0, PresentationToleranceCm) &&
            FMath::IsNearlyEqual(Line.End.Y, 40.0, PresentationToleranceCm);
        HoleSides[3] |= FMath::IsNearlyEqual(Line.Start.Y, 60.0, PresentationToleranceCm) &&
            FMath::IsNearlyEqual(Line.End.Y, 60.0, PresentationToleranceCm);
    }
    TestTrue(TEXT("All four expected hole sides remain visible"),
        HoleSides[0] && HoleSides[1] && HoleSides[2] && HoleSides[3]);
    TestEqual(TEXT("The visible hole boundary is retained while distant outside edges are clipped"),
        Length, 80.0, PresentationToleranceCm);
    if (!FMath::IsNearlyEqual(Length, 80.0, PresentationToleranceCm))
    {
        TArray<FFFZSBuildAreaLineSegment> FullBoundary;
        Geometry.GetBoundarySegments(0, View, FullBoundary);
        for (const FFFZSBuildAreaLineSegment& Line : FullBoundary)
            AddInfo(FString::Printf(TEXT("Union boundary: (%.12f, %.12f) -> (%.12f, %.12f)"),
                Line.Start.X, Line.Start.Y, Line.End.X, Line.End.Y));
    }
    Geometry.GetBoundarySegments(0, FBox2D(FVector2D(-10, -10), FVector2D(10, 10)), Lines);
    Length = 0;
    for (const FFFZSBuildAreaLineSegment& Line : Lines)
    {
        Length += FVector2D::Distance(Line.Start, Line.End);
        TestTrue(TEXT("Both boundary endpoints remain within the requested view"),
            Line.Start.X <= 10 && Line.Start.Y <= 10 && Line.End.X <= 10 && Line.End.Y <= 10);
    }
    TestTrue(TEXT("Long boundary lines are clipped instead of discarded"), FMath::IsNearlyEqual(Length, 20.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildAreaOutsidePresentationTest,
    "FF_ZoneSystem.Unit.BuildArea.OutsideHatchingMatchesAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildAreaOutsidePresentationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FFFZSBuildAreaGeometry Geometry;
    TestTrue(TEXT("A union with a hole builds"), Geometry.Rebuild({
        MakeSource(0, 0, 100, 40), MakeSource(0, 60, 100, 100),
        MakeSource(0, 40, 40, 60), MakeSource(60, 40, 100, 60)}));
    const FFFZSOrientedRect Reservation(FVector2D(50, 50), FVector2D(70, 70), 0);
    const TArray<FFFZSBuildAreaLineSegment> Input{{FVector2D(-100, 50), FVector2D(200, 50)}};
    TArray<FFFZSBuildAreaLineSegment> Outside;
    Geometry.GetOutsideSegments(Reservation, 0, Input, Outside);
    TestEqual(TEXT("A line across a hole has two outside ends and one hole segment"), Outside.Num(), 3);
    if (Outside.Num() == 3)
    {
        TestTrue(TEXT("Left line clips at the reservation and build boundary"),
            Outside[0].Start.Equals(FVector2D(-20, 50)) && Outside[0].End.Equals(FVector2D(0, 50)));
        TestTrue(TEXT("Only the unbuildable hole is hatched inside the union"),
            Outside[1].Start.Equals(FVector2D(40, 50)) && Outside[1].End.Equals(FVector2D(60, 50)));
        TestTrue(TEXT("Right line clips at the build boundary and reservation"),
            Outside[2].Start.Equals(FVector2D(100, 50)) && Outside[2].End.Equals(FVector2D(120, 50)));
    }
    for (const FFFZSBuildAreaLineSegment& Line : Outside)
    {
        TestFalse(TEXT("Hatch segment midpoint is rejected by placement authority"),
            Geometry.ContainsWorldPoint(FVector((Line.Start + Line.End) * 0.5, 0)));
    }
    Geometry.GetOutsideSegments(Reservation, 0, {{FVector2D(0, 0), FVector2D(100, 0)}}, Outside);
    TestTrue(TEXT("An allowed boundary contact is never hatched"), Outside.IsEmpty());
    Geometry.GetOutsideSegments(Reservation, 0, {{FVector2D(1, -0.05), FVector2D(99, -0.05)}}, Outside);
    TestTrue(TEXT("The same inclusive 0.1cm tolerance applies to boundary feedback"), Outside.IsEmpty());

    TestTrue(TEXT("Disconnected regions build"), Geometry.Rebuild({
        MakeSource(0, 0, 40, 100), MakeSource(60, 0, 100, 100)}));
    Geometry.GetOutsideSegments(FFFZSOrientedRect(FVector2D(50), FVector2D(50), 0), 0, Input, Outside);
    TestEqual(TEXT("Disconnected regions hatch only their intervening gap"), Outside.Num(), 1);
    if (Outside.Num() == 1) TestTrue(TEXT("The gap endpoints match authority"),
        Outside[0].Start.Equals(FVector2D(40, 50)) && Outside[0].End.Equals(FVector2D(60, 50)));

    const FFFZSOrientedRect Rotated(FVector2D(300, 300), FVector2D(10), 45);
    Geometry.GetOutsideSegments(Rotated, 0, {{FVector2D(250, 305), FVector2D(350, 305)}}, Outside);
    TestEqual(TEXT("An outside rotated reservation retains one clipped hatch"), Outside.Num(), 1);
    if (Outside.Num() == 1) TestTrue(TEXT("Hatch clips to the rotated rectangle rather than its AABB"),
        FMath::IsNearlyEqual(FVector2D::Distance(Outside[0].Start, Outside[0].End), 20.0 * FMath::Sqrt(2.0) - 10.0, 0.001));
    Geometry.GetOutsideSegments(FFFZSOrientedRect(FVector2D(20, 50), FVector2D(10), 0), 0, Input, Outside);
    TestTrue(TEXT("A contained reservation produces no false warning"), Outside.IsEmpty());
    return true;
}

#endif
