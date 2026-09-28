// FF_FFZSOrientedGeometryTests.cpp
// 역할: 회전 사각형의 교차·포함·경계 접촉 판정을 자동 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"

namespace
{
    FFFZSCellRect TranslateRect(const FFFZSCellRect& Rect, const FIntPoint Offset)
    {
        return FFFZSCellRect(Rect.Min + Offset, Rect.Max + Offset);
    }

    FFFZSOrientedRect MakeTestRect(
        const FVector2D Center,
        const FVector2D HalfExtent,
        const float YawDegrees)
    {
        FFFZSOrientedRect Result;
        Result.CenterCell = Center;
        Result.HalfExtentCells = HalfExtent;
        Result.YawDegrees = YawDegrees;
        return Result;
    }

    bool ContainsPointInclusive(const FFFZSCellRect& Rect, const FVector2D Point)
    {
        return Point.X >= Rect.Min.X && Point.X <= Rect.Max.X &&
            Point.Y >= Rect.Min.Y && Point.Y <= Rect.Max.Y;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOrientedGeometryLegacyCardinalTest,
    "FF_ZoneSystem.Unit.OrientedGeometry.LegacyCardinalLatticeEquivalence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrientedGeometryLegacyCardinalTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FFFZSCellRect LocalRect(FIntPoint(-2, 1), FIntPoint(4, 6));
    const FIntPoint Origin(17, -9);
    for (int32 QuarterTurns = 0; QuarterTurns < 4; ++QuarterTurns)
    {
        const FFFZSOrientedRect Oriented = FFZSOrientedGeometry::MakeFromLocalRect(
            LocalRect,
            Origin,
            static_cast<float>(QuarterTurns * 90));
        const FFFZSCellRect Expected = TranslateRect(
            UFFZSGridLibrary::RotateRect(LocalRect, QuarterTurns),
            Origin);
        const FFFZSCellRect Actual = FFZSOrientedGeometry::ToConservativeCellRect(Oriented);
        TestTrue(
            *FString::Printf(TEXT("%d quarter turns preserve the legacy half-open lattice"), QuarterTurns),
            Actual == Expected);

        const FBox2D Aabb = FFZSOrientedGeometry::GetAabb(Oriented);
        TestTrue(
            *FString::Printf(TEXT("%d quarter turns preserve the exact continuous AABB"), QuarterTurns),
            Aabb.bIsValid &&
            Aabb.Min.Equals(FVector2D(Expected.Min), UE_DOUBLE_KINDA_SMALL_NUMBER) &&
            Aabb.Max.Equals(FVector2D(Expected.Max), UE_DOUBLE_KINDA_SMALL_NUMBER));
    }

    const FFFZSCellRect UnitCell(FIntPoint::ZeroValue, FIntPoint(1, 1));
    const FFFZSCellRect RotatedUnit = FFZSOrientedGeometry::ToConservativeCellRect(
        FFZSOrientedGeometry::MakeFromLocalRect(UnitCell, Origin, 90.0f));
    TestTrue(TEXT("The origin cell remains fixed when rotated around its centre"),
        RotatedUnit == TranslateRect(UnitCell, Origin));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOrientedGeometryIntersectionTest,
    "FF_ZoneSystem.Unit.OrientedGeometry.IntersectionTouchingAndFortyFiveDegrees",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrientedGeometryIntersectionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FFFZSOrientedRect AxisAligned = MakeTestRect(
        FVector2D::ZeroVector,
        FVector2D(2.0, 1.0),
        0.0f);
    const FFFZSOrientedRect ObliqueOverlap = MakeTestRect(
        FVector2D(1.5, 0.25),
        FVector2D(2.0, 0.75),
        45.0f);
    const FFFZSOrientedRect ObliqueFar = MakeTestRect(
        FVector2D(20.0, 20.0),
        FVector2D(2.0, 0.75),
        45.0f);
    TestTrue(TEXT("A 45-degree rectangle with positive shared area overlaps"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, ObliqueOverlap));
    TestFalse(TEXT("A separated 45-degree rectangle does not overlap"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, ObliqueFar));

    const FFFZSOrientedRect CardinalTouch = MakeTestRect(
        FVector2D(4.0, 0.0),
        FVector2D(2.0, 1.0),
        0.0f);
    const FFFZSOrientedRect CardinalInside = MakeTestRect(
        FVector2D(4.0 - 1.0e-4, 0.0),
        FVector2D(2.0, 1.0),
        0.0f);
    TestFalse(TEXT("Cardinal edge contact is not positive-area overlap"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, CardinalTouch));
    TestTrue(TEXT("Moving inward from cardinal contact creates positive-area overlap"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, CardinalInside));

    const double ObliqueSupportX = FMath::Sqrt(2.0);
    const FFFZSOrientedRect ObliqueTouch = MakeTestRect(
        FVector2D(2.0 + ObliqueSupportX, 0.0),
        FVector2D(1.0, 1.0),
        45.0f);
    const FFFZSOrientedRect ObliqueInside = MakeTestRect(
        FVector2D(2.0 + ObliqueSupportX - 1.0e-4, 0.0),
        FVector2D(1.0, 1.0),
        45.0f);
    TestFalse(TEXT("A 45-degree edge touching a cardinal edge is not overlap"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, ObliqueTouch));
    TestTrue(TEXT("A 45-degree rectangle moved just inside the edge overlaps"),
        FFZSOrientedGeometry::IntersectsPositiveArea(AxisAligned, ObliqueInside));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FFFZSOrientedGeometrySymmetryTest,
    "FF_ZoneSystem.Unit.OrientedGeometry.SymmetryAndConservativeBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOrientedGeometrySymmetryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const TArray<FFFZSOrientedRect> Rects = {
        MakeTestRect(FVector2D(0.0, 0.0), FVector2D(3.0, 1.0), 15.0f),
        MakeTestRect(FVector2D(1.0, 0.5), FVector2D(1.5, 2.0), 45.0f),
        MakeTestRect(FVector2D(9.0, -3.0), FVector2D(2.0, 2.0), -37.0f),
        MakeTestRect(FVector2D(-4.0, 2.0), FVector2D(0.5, 4.0), 123.0f)
    };
    for (int32 AIndex = 0; AIndex < Rects.Num(); ++AIndex)
    {
        for (int32 BIndex = 0; BIndex < Rects.Num(); ++BIndex)
        {
            const bool AB = FFZSOrientedGeometry::IntersectsPositiveArea(
                Rects[AIndex], Rects[BIndex]);
            const bool BA = FFZSOrientedGeometry::IntersectsPositiveArea(
                Rects[BIndex], Rects[AIndex]);
            TestEqual(
                *FString::Printf(TEXT("SAT is symmetric for pair %d/%d"), AIndex, BIndex),
                AB,
                BA);
        }
    }

    const FFFZSOrientedRect FortyFive = FFZSOrientedGeometry::MakeFromLocalRect(
        FFFZSCellRect(FIntPoint(-3, 2), FIntPoint(5, 7)),
        FIntPoint(100, -40),
        45.0f);
    const FFFZSCellRect Conservative =
        FFZSOrientedGeometry::ToConservativeCellRect(FortyFive);
    TestTrue(TEXT("A valid 45-degree rectangle produces a valid conservative cell rect"),
        Conservative.IsValid());
    for (const FVector2D& Corner : FFZSOrientedGeometry::GetCorners(FortyFive))
    {
        TestTrue(TEXT("Every oriented corner lies inside the conservative cell rect"),
            ContainsPointInclusive(Conservative, Corner));
    }

    const FFFZSOrientedRect Invalid = MakeTestRect(
        FVector2D::ZeroVector,
        FVector2D(0.0, 1.0),
        0.0f);
    TestFalse(TEXT("A degenerate rectangle never overlaps a valid rectangle"),
        FFZSOrientedGeometry::IntersectsPositiveArea(Invalid, Rects[0]));
    TestFalse(TEXT("A degenerate rectangle has no conservative cell rect"),
        FFZSOrientedGeometry::ToConservativeCellRect(Invalid).IsValid());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
