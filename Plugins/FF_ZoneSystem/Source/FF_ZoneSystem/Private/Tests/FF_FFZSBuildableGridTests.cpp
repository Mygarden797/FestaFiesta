#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSBuildableGrid.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridMergedEdgesTest,
    "FF_ZoneSystem.Unit.BuildableGrid.MergedEdgesAndEmptyMask",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridMergedEdgesTest::RunTest(const FString& Parameters)
{
    FFFZSBuildableGridMask Mask;
    TestTrue(TEXT("Initialize a 2 x 2 grid"), Mask.Initialize(FVector2D(-100.0), FVector2D(100.0), 100));
    TArray<FFFZSBuildableGridSegment> Segments;
    FFZSBuildableGrid::BuildSegments(Mask, Segments);
    TestEqual(TEXT("Unbuildable land has no grid at all"), Segments.Num(), 0);
    for (int32 Y = 0; Y < 2; ++Y)
    {
        for (int32 X = 0; X < 2; ++X)
        {
            Mask.SetBuildable(X, Y, true);
            Mask.SetBuildable(X, Y, true);
        }
    }
    TestEqual(TEXT("Idempotent writes preserve population"), Mask.GetBuildableCellCount(), 4);
    FFZSBuildableGrid::BuildSegments(Mask, Segments);
    TestEqual(TEXT("Shared edges are unique and collinear edges merge"), Segments.Num(), 6);
    int32 BoundaryCount = 0;
    for (const FFFZSBuildableGridSegment& Segment : Segments)
    {
        BoundaryCount += Segment.bBoundary ? 1 : 0;
        TestEqual(TEXT("Every merged line spans two cells"),
            FVector2D::Distance(Segment.StartLocalCm, Segment.EndLocalCm), 200.0);
    }
    TestEqual(TEXT("The four outside contours have boundary styling"), BoundaryCount, 4);
    TestFalse(TEXT("Out-of-range lookup is blocked"), Mask.IsBuildable(-1, 0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridHoleTest,
    "FF_ZoneSystem.Unit.BuildableGrid.HolesNeverReceiveInteriorLines",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridHoleTest::RunTest(const FString& Parameters)
{
    FFFZSBuildableGridMask Mask;
    Mask.Initialize(FVector2D::ZeroVector, FVector2D(300.0), 100);
    for (int32 Y = 0; Y < 3; ++Y)
    {
        for (int32 X = 0; X < 3; ++X)
        {
            Mask.SetBuildable(X, Y, X != 1 || Y != 1);
        }
    }
    TArray<FFFZSBuildableGridSegment> Segments;
    FFZSBuildableGrid::BuildSegments(Mask, Segments);
    int32 InnerBoundaryCount = 0;
    for (const FFFZSBuildableGridSegment& Segment : Segments)
    {
        const FVector2D Midpoint = (Segment.StartLocalCm + Segment.EndLocalCm) * 0.5;
        TestFalse(TEXT("No segment midpoint enters the blocked cell"),
            Midpoint.X > 100.0 && Midpoint.X < 200.0 && Midpoint.Y > 100.0 && Midpoint.Y < 200.0);
        if (Segment.bBoundary && Midpoint.X >= 100.0 && Midpoint.X <= 200.0 &&
            Midpoint.Y >= 100.0 && Midpoint.Y <= 200.0)
        {
            ++InnerBoundaryCount;
            TestEqual(TEXT("An obstacle contour cannot extend through neighbouring free edges"),
                FVector2D::Distance(Segment.StartLocalCm, Segment.EndLocalCm), 100.0);
        }
    }
    TestEqual(TEXT("The hole has exactly four visible boundary edges"), InnerBoundaryCount, 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSBuildableGridBudgetTest,
    "FF_ZoneSystem.Unit.BuildableGrid.BudgetAlignmentAndInvalidInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSBuildableGridBudgetTest::RunTest(const FString& Parameters)
{
    FFFZSBuildableGridMask Mask;
    TestTrue(TEXT("Large requests adapt instead of allocating physical cells"),
        Mask.Initialize(FVector2D(-500000.0), FVector2D(500000.0), 25));
    TestTrue(TEXT("Cell allocation stays within its strict budget"),
        Mask.GetCellCount() > 0 && Mask.GetCellCount() <= FFFZSBuildableGridMask::MaximumCellCount);
    TestTrue(TEXT("Large extents use a coarser multiple of the requested lattice"),
        Mask.GetStepCm() > 25.0 && FMath::Fmod(Mask.GetStepCm(), 25.0) == 0.0);
    TestTrue(TEXT("Negative origin still aligns to the zone lattice"),
        FMath::Fmod(Mask.GetMinLocalCm().X, Mask.GetStepCm()) == 0.0);
    TestFalse(TEXT("Zero extent is rejected"), Mask.Initialize(FVector2D::ZeroVector, FVector2D::ZeroVector, 100));
    TestEqual(TEXT("Rejected requests clear stale visibility"), Mask.GetCellCount(), 0);
    TestFalse(TEXT("A physical 1 cm request cannot bypass the visual budget"),
        Mask.Initialize(FVector2D::ZeroVector, FVector2D(100.0), 1));
    TestFalse(TEXT("Reversed bounds are rejected"),
        Mask.Initialize(FVector2D(100.0), FVector2D::ZeroVector, 100));
    TestFalse(TEXT("Coordinates outside the authority lattice are rejected"),
        Mask.Initialize(FVector2D::ZeroVector, FVector2D(static_cast<double>(MAX_int32) + 1.0), 100));
    return true;
}

#endif
