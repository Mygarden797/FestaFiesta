#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
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

#endif
