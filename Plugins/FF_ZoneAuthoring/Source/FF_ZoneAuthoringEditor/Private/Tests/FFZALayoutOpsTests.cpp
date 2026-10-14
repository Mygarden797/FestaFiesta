// Pure geometry regressions: these tests never create a world or edit level assets.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZALayoutOps.h"
#include "FFZAPolygonMath.h"

#include <limits>

namespace FFZALayoutOpsTestsPrivate
{
    TArray<FVector2D> MakeRect(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    bool HasVertex(const TArray<FVector2D>& Polygon, const FVector2D& Expected)
    {
        return Polygon.Contains(Expected);
    }

    bool HasEdge(const TArray<FVector2D>& Polygon, const FVector2D& A, const FVector2D& B)
    {
        for (int32 I = 0; I < Polygon.Num(); ++I)
        {
            const FVector2D& Start = Polygon[I];
            const FVector2D& End = Polygon[(I + 1) % Polygon.Num()];
            if ((Start == A && End == B) || (Start == B && End == A))
            {
                return true;
            }
        }
        return false;
    }

    // Compare saved inputs even when the invalid-input fixture contains NaN.
    bool SameCoordinate(const double A, const double B)
    {
        return A == B || (FMath::IsNaN(A) && FMath::IsNaN(B));
    }

    bool SamePolygons(const TArray<TArray<FVector2D>>& A, const TArray<TArray<FVector2D>>& B)
    {
        if (A.Num() != B.Num())
        {
            return false;
        }
        for (int32 PolygonIndex = 0; PolygonIndex < A.Num(); ++PolygonIndex)
        {
            if (A[PolygonIndex].Num() != B[PolygonIndex].Num())
            {
                return false;
            }
            for (int32 PointIndex = 0; PointIndex < A[PolygonIndex].Num(); ++PointIndex)
            {
                const FVector2D& First = A[PolygonIndex][PointIndex];
                const FVector2D& Second = B[PolygonIndex][PointIndex];
                if (!SameCoordinate(First.X, Second.X) || !SameCoordinate(First.Y, Second.Y))
                {
                    return false;
                }
            }
        }
        return true;
    }

    void TestValidOutput(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Polygons)
    {
        for (int32 I = 0; I < Polygons.Num(); ++I)
        {
            Test.TestTrue(FString::Printf(TEXT("Output %d is simple and nondegenerate"), I),
                FFZAPolygonMath::IsSimplePolygon(Polygons[I]));
            for (const FVector2D& Point : Polygons[I])
            {
                Test.TestTrue(TEXT("Output coordinates are finite"), FMath::IsFinite(Point.X) && FMath::IsFinite(Point.Y));
            }
        }
    }

    // Triangulate the clip polygon, so the overlap assertion also works for a bent/concave seam.
    double ComputeIntersectionArea(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
    {
        TArray<int32> Triangles;
        if (!FFZAPolygonMath::Triangulate(B, Triangles))
        {
            return -1.0;
        }
        double OverlapArea = 0.0;
        for (int32 I = 0; I < Triangles.Num(); I += 3)
        {
            const TArray<FVector2D> Triangle = { B[Triangles[I]], B[Triangles[I + 1]], B[Triangles[I + 2]] };
            OverlapArea += FFZAPolygonMath::IntersectionAreaWithConvex(A, Triangle);
        }
        return OverlapArea;
    }

    void TestNoAreaOverlap(FAutomationTestBase& Test, const TArray<FVector2D>& A, const TArray<FVector2D>& B)
    {
        const double OverlapArea = ComputeIntersectionArea(A, B);
        Test.TestTrue(TEXT("Overlap fixture triangulates"), OverlapArea >= 0.0);
        Test.TestTrue(FString::Printf(TEXT("Shared seam has no positive-area overlap (%.9f cm2)"), OverlapArea),
            OverlapArea >= 0.0 && OverlapArea < 1e-4);
    }

    void TestIdempotent(FAutomationTestBase& Test, TArray<TArray<FVector2D>>& Polygons, const double Tolerance)
    {
        const TArray<TArray<FVector2D>> Once = Polygons;
        TArray<bool> Changed;
        Test.TestEqual(TEXT("Second weld moves no original vertices"), FFZALayoutOps::WeldPolygons(Polygons, Tolerance, Changed), 0);
        Test.TestTrue(TEXT("Second weld preserves every coordinate and point count"), SamePolygons(Polygons, Once));
        Test.TestEqual(TEXT("Changed flags match polygon count"), Changed.Num(), Polygons.Num());
        for (const bool bChanged : Changed)
        {
            Test.TestFalse(TEXT("Second weld reports no polygon changes"), bChanged);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldStraightSeamsTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.StraightGapAndOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldStraightSeamsTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    for (const double Offset : { 20.0, -20.0, 50.0 })
    {
        TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(1000 + Offset, 0, 2000, 1000) };
        const TArray<FVector2D> OriginalReference = Polygons[0];
        TArray<bool> Changed;
        TestEqual(TEXT("Two original seam vertices move"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed), 2);
        TestTrue(TEXT("Reference geometry is preserved"), Polygons[0] == OriginalReference);
        TestTrue(TEXT("Both zones contain the same complete shared edge"),
            HasEdge(Polygons[0], FVector2D(1000, 0), FVector2D(1000, 1000)) &&
            HasEdge(Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
        if (TestEqual(TEXT("Changed flag count"), Changed.Num(), 2))
        {
            TestFalse(TEXT("Unsplit reference is unchanged"), Changed[0]);
            TestTrue(TEXT("Target change is reported"), Changed[1]);
        }
        TestValidOutput(*this, Polygons);
        TestNoAreaOverlap(*this, Polygons[0], Polygons[1]);
        TestIdempotent(*this, Polygons, 50.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldBentReferenceTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.BentReferenceChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldBentReferenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    const TArray<FVector2D> Seam = { FVector2D(1000, 0), FVector2D(1000, 200), FVector2D(1030, 500),
        FVector2D(1000, 800), FVector2D(1000, 1000) };
    TArray<TArray<FVector2D>> Polygons = {
        { FVector2D(0, 0), Seam[0], Seam[1], Seam[2], Seam[3], Seam[4], FVector2D(0, 1000) },
        MakeRect(1020, 0, 2000, 1000)
    };
    const double ReferenceArea = FFZAPolygonMath::Area(Polygons[0]);
    TArray<bool> Changed;
    TestTrue(TEXT("Original target vertices are moved"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed) > 0);
    for (int32 I = 0; I < Seam.Num(); ++I)
    {
        TestTrue(TEXT("Target contains the exact reference bend coordinates"), HasVertex(Polygons[1], Seam[I]));
        if (I + 1 < Seam.Num())
        {
            TestTrue(TEXT("Both sides use the same canonical seam segment"),
                HasEdge(Polygons[0], Seam[I], Seam[I + 1]) && HasEdge(Polygons[1], Seam[I], Seam[I + 1]));
        }
    }
    TestTrue(TEXT("Reference shape area is preserved"), FMath::IsNearlyEqual(FFZAPolygonMath::Area(Polygons[0]), ReferenceArea, 1e-6));
    TestValidOutput(*this, Polygons);
    TestNoAreaOverlap(*this, Polygons[0], Polygons[1]);
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldTJunctionTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.TJunctionSubdivision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldTJunctionTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    for (const double Offset : { 20.0, 0.0 })
    {
        TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(1000 + Offset, 300, 2000, 700) };
        TArray<bool> Changed;
        TestEqual(TEXT("Return count only includes moved original vertices"),
            FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed), Offset > 0.0 ? 2 : 0);
        const FVector2D Lower(1000, 300);
        const FVector2D Upper(1000, 700);
        TestTrue(TEXT("Reference receives lower T junction node"), HasVertex(Polygons[0], Lower));
        TestTrue(TEXT("Reference receives upper T junction node"), HasVertex(Polygons[0], Upper));
        TestTrue(TEXT("Both sides contain the identical junction edge"), HasEdge(Polygons[0], Lower, Upper) && HasEdge(Polygons[1], Lower, Upper));
        if (TestEqual(TEXT("Changed flag count"), Changed.Num(), 2))
        {
            TestTrue(TEXT("Reference-only subdivision is reported"), Changed[0]);
            TestEqual(TEXT("Unmoved target flag"), Changed[1], Offset > 0.0);
        }
        TestTrue(TEXT("Subdivision preserves reference area"), FMath::IsNearlyEqual(FFZAPolygonMath::Area(Polygons[0]), 1000000.0, 1e-6));
        TestValidOutput(*this, Polygons);
        TestNoAreaOverlap(*this, Polygons[0], Polygons[1]);
        TestIdempotent(*this, Polygons, 50.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldPartialLongEdgeTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.PartialLongEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldPartialLongEdgeTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(1020, -500, 2000, 1500) };
    TArray<bool> Changed;
    FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed);
    TestTrue(TEXT("Long edge is welded despite both original endpoints being out of range"),
        HasEdge(Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
    TestTrue(TEXT("Unshared lower endpoint is retained"), HasVertex(Polygons[1], FVector2D(1020, -500)));
    TestTrue(TEXT("Unshared upper endpoint is retained"), HasVertex(Polygons[1], FVector2D(1020, 1500)));
    if (TestEqual(TEXT("Changed flag count"), Changed.Num(), 2))
    {
        TestTrue(TEXT("Inserted target points mark the target changed"), Changed[1]);
    }
    TestValidOutput(*this, Polygons);
    TestNoAreaOverlap(*this, Polygons[0], Polygons[1]);
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldNearestEdgeTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.NearestEdgeAndRepeatStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldNearestEdgeTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(1040, 100, 2000, 960) };
    TArray<bool> Changed;
    TestEqual(TEXT("Two near vertices move"), FFZALayoutOps::WeldPolygons(Polygons, 60.0, Changed), 2);
    TestTrue(TEXT("Closer segment wins over the reference corner within tolerance"), HasVertex(Polygons[1], FVector2D(1000, 960)));
    TestFalse(TEXT("Near-corner vertex does not jump to the farther corner"), HasVertex(Polygons[1], FVector2D(1000, 1000)));
    TestTrue(TEXT("Both polygons receive the identical partial edge"),
        HasEdge(Polygons[0], FVector2D(1000, 100), FVector2D(1000, 960)) &&
        HasEdge(Polygons[1], FVector2D(1000, 100), FVector2D(1000, 960)));
    TestValidOutput(*this, Polygons);
    TestIdempotent(*this, Polygons, 60.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldThreeZoneTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.ThreeZoneJunction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldThreeZoneTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeRect(0, 0, 1000, 1000), MakeRect(1020, 0, 2000, 1000), MakeRect(0, 1020, 2000, 2000)
    };
    TArray<bool> Changed;
    FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed);
    const FVector2D Junction(1000, 1000);
    for (const TArray<FVector2D>& Polygon : Polygons)
    {
        TestTrue(TEXT("All three zones share the exact junction coordinate"), HasVertex(Polygon, Junction));
    }
    TestTrue(TEXT("Upper target carries both reference top edges"),
        HasEdge(Polygons[2], FVector2D(0, 1000), Junction) &&
        HasEdge(Polygons[2], Junction, FVector2D(2000, 1000)));
    TestValidOutput(*this, Polygons);
    for (int32 I = 0; I < Polygons.Num(); ++I)
    {
        for (int32 J = I + 1; J < Polygons.Num(); ++J)
        {
            TestNoAreaOverlap(*this, Polygons[I], Polygons[J]);
        }
    }
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldWindingTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.WindingIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldWindingTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    for (int32 Winding = 0; Winding < 4; ++Winding)
    {
        TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(980, 300, 2000, 700) };
        if ((Winding & 1) != 0)
        {
            Algo::Reverse(Polygons[0]);
        }
        if ((Winding & 2) != 0)
        {
            Algo::Reverse(Polygons[1]);
        }
        const double ReferenceSignedArea = FFZAPolygonMath::SignedArea(Polygons[0]);
        const double TargetSignedArea = FFZAPolygonMath::SignedArea(Polygons[1]);
        TArray<bool> Changed;
        TestEqual(TEXT("Both winding directions weld the shallow overlap"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed), 2);
        TestTrue(TEXT("Shared edge is independent of winding"),
            HasEdge(Polygons[0], FVector2D(1000, 300), FVector2D(1000, 700)) &&
            HasEdge(Polygons[1], FVector2D(1000, 300), FVector2D(1000, 700)));
        TestTrue(TEXT("Reference winding is preserved"), FFZAPolygonMath::SignedArea(Polygons[0]) * ReferenceSignedArea > 0.0);
        TestTrue(TEXT("Target winding is preserved"), FFZAPolygonMath::SignedArea(Polygons[1]) * TargetSignedArea > 0.0);
        TestValidOutput(*this, Polygons);
        TestNoAreaOverlap(*this, Polygons[0], Polygons[1]);
        TestIdempotent(*this, Polygons, 50.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldUnrelatedGeometryTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.UnrelatedGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldUnrelatedGeometryTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        MakeRect(0, 0, 1000, 1000), MakeRect(20, 20, 980, 980),
        MakeRect(1050.01, 300, 2000, 700), MakeRect(5000, 5000, 6000, 6000)
    };
    TArray<TArray<FVector2D>> Polygons = Original;
    TArray<bool> Changed;
    TestEqual(TEXT("Nested same-side boundaries and out-of-range zones do not weld"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed), 0);
    TestTrue(TEXT("Unrelated geometry is preserved exactly"), SamePolygons(Polygons, Original));
    for (const bool bChanged : Changed)
    {
        TestFalse(TEXT("Unrelated zone is not reported changed"), bChanged);
    }
    TestValidOutput(*this, Polygons);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldInvalidInputsTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.InvalidInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldInvalidInputsTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const double Infinity = std::numeric_limits<double>::infinity();
    const TArray<TArray<FVector2D>> ValidInput = { MakeRect(0, 0, 1000, 1000), MakeRect(1020, 300, 2000, 700) };
    for (const double Tolerance : { 0.0, -50.0, NaN, Infinity })
    {
        TArray<TArray<FVector2D>> Polygons = ValidInput;
        TArray<bool> Changed = { true, true, true };
        TestEqual(TEXT("Invalid tolerance performs no weld"), FFZALayoutOps::WeldPolygons(Polygons, Tolerance, Changed), 0);
        TestTrue(TEXT("Invalid tolerance preserves inputs"), SamePolygons(Polygons, ValidInput));
        TestEqual(TEXT("Invalid tolerance resets changed flags to the input size"), Changed.Num(), Polygons.Num());
        for (const bool bChanged : Changed)
        {
            TestFalse(TEXT("Invalid tolerance clears changed flags"), bChanged);
        }
    }

    const TArray<TArray<FVector2D>> InvalidShapes = {
        {}, { FVector2D(1020, 300), FVector2D(1020, 700) },
        { FVector2D(1020, 300), FVector2D(2000, 700), FVector2D(1020, 700), FVector2D(2000, 300) },
        { FVector2D(1020, 300), FVector2D(1020, 300), FVector2D(2000, 700), FVector2D(1020, 700) },
        { FVector2D(NaN, 300), FVector2D(2000, 300), FVector2D(2000, 700), FVector2D(1020, 700) },
        { FVector2D(Infinity, 300), FVector2D(2000, 300), FVector2D(2000, 700), FVector2D(1020, 700) }
    };
    for (const TArray<FVector2D>& Invalid : InvalidShapes)
    {
        for (const bool bInvalidIsReference : { false, true })
        {
            TArray<TArray<FVector2D>> Polygons = bInvalidIsReference
                ? TArray<TArray<FVector2D>>{ Invalid, MakeRect(0, 0, 1000, 1000) }
                : TArray<TArray<FVector2D>>{ MakeRect(0, 0, 1000, 1000), Invalid };
            const TArray<TArray<FVector2D>> Original = Polygons;
            TArray<bool> Changed;
            TestEqual(TEXT("Invalid reference or target is skipped"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed), 0);
            TestTrue(TEXT("Invalid geometry and its neighbor remain unchanged"), SamePolygons(Polygons, Original));
            for (const bool bChanged : Changed)
            {
                TestFalse(TEXT("Invalid geometry produces no changed flag"), bChanged);
            }
        }
    }

    TArray<TArray<FVector2D>> Empty;
    TArray<bool> EmptyChanged = { true };
    TestEqual(TEXT("Empty polygon set is safe"), FFZALayoutOps::WeldPolygons(Empty, 50.0, EmptyChanged), 0);
    TestEqual(TEXT("Empty polygon set clears changed flags"), EmptyChanged.Num(), 0);
    TArray<TArray<FVector2D>> Single = { MakeRect(0, 0, 1000, 1000) };
    TestEqual(TEXT("One zone needs no weld"), FFZALayoutOps::WeldPolygons(Single, 50.0, EmptyChanged), 0);
    TestEqual(TEXT("One zone has one unchanged flag"), EmptyChanged.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldNearbyNodesTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.NearbyNodesStayDistinct",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldNearbyNodesTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> NarrowZones = {
        MakeRect(0, 0, 1000, 1000), MakeRect(1040, 0, 1080, 1000), MakeRect(1120, 0, 2000, 1000)
    };
    TArray<bool> Changed;
    FFZALayoutOps::WeldPolygons(NarrowZones, 50.0, Changed);
    TestTrue(TEXT("Near endpoints do not transitively merge across a narrow zone"),
        HasEdge(NarrowZones[1], FVector2D(1000, 0), FVector2D(1000, 1000)) &&
        HasEdge(NarrowZones[1], FVector2D(1080, 0), FVector2D(1080, 1000)));
    TestTrue(TEXT("The next seam keeps its own reference coordinates"),
        HasEdge(NarrowZones[2], FVector2D(1080, 0), FVector2D(1080, 1000)));
    TestValidOutput(*this, NarrowZones);
    TestNoAreaOverlap(*this, NarrowZones[0], NarrowZones[1]);
    TestNoAreaOverlap(*this, NarrowZones[1], NarrowZones[2]);
    TestIdempotent(*this, NarrowZones, 50.0);

    TArray<FVector2D> DenseTarget = MakeRect(1020, 0, 2000, 1000);
    for (int32 Y = 980; Y > 0; Y -= 20)
    {
        DenseTarget.Add(FVector2D(1020, Y));
    }
    TArray<TArray<FVector2D>> DenseSeam = { MakeRect(0, 0, 1000, 1000), DenseTarget };
    TestEqual(TEXT("Every authored seam point moves without clustering neighboring nodes"),
        FFZALayoutOps::WeldPolygons(DenseSeam, 50.0, Changed), 51);
    for (int32 Y = 0; Y <= 1000; Y += 20)
    {
        const FVector2D Node(1000, Y);
        TestTrue(TEXT("Dense authored target point is retained exactly"), HasVertex(DenseSeam[1], Node));
        TestTrue(TEXT("Sparse reference receives each identical seam node"), HasVertex(DenseSeam[0], Node));
    }
    TestValidOutput(*this, DenseSeam);
    TestNoAreaOverlap(*this, DenseSeam[0], DenseSeam[1]);
    TestIdempotent(*this, DenseSeam, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldRejectUnsafeProposalTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.RejectUnsafeProposal",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldRejectUnsafeProposalTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = { MakeRect(0, 0, 1000, 1000), MakeRect(980, 300, 990, 700) };
    TestValidOutput(*this, Polygons);
    const TArray<TArray<FVector2D>> Original = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    TestEqual(TEXT("A proposed seam that would flip a narrow polygon is rejected"),
        FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed, &Stats), 0);
    TestTrue(TEXT("Rejected proposal restores target and reference without leaked nodes"), SamePolygons(Polygons, Original));
    TestTrue(TEXT("Unsafe proposal is reported for review"), Stats.RejectedPolygonCount > 0);
    TestEqual(TEXT("Rejected proposal moves no original points"), Stats.MovedVertexCount, 0);
    TestEqual(TEXT("Rejected proposal inserts no points"), Stats.InsertedVertexCount, 0);
    TestEqual(TEXT("Rejected proposal changes no polygons"), Stats.ChangedPolygonCount, 0);
    TestEqual(TEXT("Valid input is distinguished from a rejected proposal"), Stats.InvalidInputPolygonCount, 0);
    for (const bool bChanged : Changed)
    {
        TestFalse(TEXT("Rejected proposal leaves changed flags false"), bChanged);
    }
    TestValidOutput(*this, Polygons);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldProvenanceTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.PointProvenanceAndStats",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldProvenanceTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    const TArray<TArray<FVector2D>> Original = { MakeRect(0, 0, 1000, 1000), MakeRect(1020, 300, 2000, 700) };
    TArray<TArray<FVector2D>> Polygons = Original;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    TestEqual(TEXT("Stats keep the legacy moved-vertex count"), FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed, &Stats), 2);
    TestEqual(TEXT("Stats report original points moved"), Stats.MovedVertexCount, 2);
    TestEqual(TEXT("Stats report the two new reference nodes separately"), Stats.InsertedVertexCount, 2);
    TestEqual(TEXT("Stats include reference-only point subdivision"), Stats.ChangedPolygonCount, 2);
    TestEqual(TEXT("Valid proposal is not rejected"), Stats.RejectedPolygonCount, 0);
    if (!TestEqual(TEXT("Provenance has one array per polygon"), Stats.SourcePointKeys.Num(), Polygons.Num()))
    {
        return false;
    }
    for (int32 PolygonIndex = 0; PolygonIndex < Polygons.Num(); ++PolygonIndex)
    {
        const TArray<double>& Keys = Stats.SourcePointKeys[PolygonIndex];
        if (!TestEqual(TEXT("Every output point has provenance"), Keys.Num(), Polygons[PolygonIndex].Num()))
        {
            continue;
        }
        for (const double Key : Keys)
        {
            TestTrue(TEXT("Point keys are finite positions in the original closed ring"),
                FMath::IsFinite(Key) && Key >= 0.0 && Key < Original[PolygonIndex].Num());
        }
        for (int32 OriginalIndex = 0; OriginalIndex < Original[PolygonIndex].Num(); ++OriginalIndex)
        {
            TestTrue(TEXT("Retained original points keep their integer source keys"), Keys.Contains(static_cast<double>(OriginalIndex)));
        }
    }
    const int32 LowerIndex = Polygons[0].Find(FVector2D(1000, 300));
    const int32 UpperIndex = Polygons[0].Find(FVector2D(1000, 700));
    if (TestTrue(TEXT("Reference contains the lower inserted point"), LowerIndex != INDEX_NONE) &&
        TestTrue(TEXT("Lower insertion has provenance"), Stats.SourcePointKeys[0].IsValidIndex(LowerIndex)))
    {
        TestTrue(TEXT("Lower insertion records its position along original edge 1"),
            FMath::IsNearlyEqual(Stats.SourcePointKeys[0][LowerIndex], 1.3, 1e-6));
    }
    if (TestTrue(TEXT("Reference contains the upper inserted point"), UpperIndex != INDEX_NONE) &&
        TestTrue(TEXT("Upper insertion has provenance"), Stats.SourcePointKeys[0].IsValidIndex(UpperIndex)))
    {
        TestTrue(TEXT("Upper insertion records its position along original edge 1"),
            FMath::IsNearlyEqual(Stats.SourcePointKeys[0][UpperIndex], 1.7, 1e-6));
    }
    TestValidOutput(*this, Polygons);
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldMultiReferenceStepTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.MultiReferenceStepSeam",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldMultiReferenceStepTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeRect(0, 0, 1000, 1000), MakeRect(1005, 400, 1015, 600), MakeRect(1020, 0, 2000, 1000)
    };
    TArray<bool> Changed;
    FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed);
    const TArray<FVector2D> Seam = {
        FVector2D(1000, 0), FVector2D(1000, 400), FVector2D(1015, 400),
        FVector2D(1015, 600), FVector2D(1000, 600), FVector2D(1000, 1000)
    };
    for (int32 I = 0; I + 1 < Seam.Num(); ++I)
    {
        TestTrue(TEXT("Target follows exact stepped reference segments including both shoulder endpoints"),
            HasEdge(Polygons[2], Seam[I], Seam[I + 1]));
    }
    TestTrue(TEXT("Target shares the lower exposed edge with zone A"), HasEdge(Polygons[0], Seam[0], Seam[1]));
    TestTrue(TEXT("Target shares the upper exposed edge with zone A"), HasEdge(Polygons[0], Seam[4], Seam[5]));
    TestTrue(TEXT("Target shares the narrow zone right edge"), HasEdge(Polygons[1], Seam[2], Seam[3]));
    TestTrue(TEXT("Lower shoulder leaves no triangular gap beside zone A"),
        FFZAPolygonMath::IsPointInside(Polygons[2], FVector2D(1001, 200), 0.0));
    TestTrue(TEXT("Upper shoulder leaves no triangular gap beside zone A"),
        FFZAPolygonMath::IsPointInside(Polygons[2], FVector2D(1001, 800), 0.0));
    TestValidOutput(*this, Polygons);
    for (int32 I = 0; I < Polygons.Num(); ++I)
    {
        TestTrue(TEXT("Stepped seam preserves counter-clockwise input winding"), FFZAPolygonMath::SignedArea(Polygons[I]) > 0.0);
        for (int32 J = I + 1; J < Polygons.Num(); ++J)
        {
            TestNoAreaOverlap(*this, Polygons[I], Polygons[J]);
        }
    }
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAWeldUnmatchedShelfTest,
    "FF_ZoneAuthoring.Unit.LayoutWeld.UnmatchedShelfPreservesOverlap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAWeldUnmatchedShelfTest::RunTest(const FString& Parameters)
{
    using namespace FFZALayoutOpsTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 400), FVector2D(1300, 400),
            FVector2D(1300, 600), FVector2D(1000, 600), FVector2D(1000, 1000), FVector2D(0, 1000) },
        MakeRect(1020, 0, 2000, 1000)
    };
    const double OriginalOverlap = ComputeIntersectionArea(Polygons[0], Polygons[1]);
    TestTrue(TEXT("Shelf fixture starts with the expected existing overlap"), FMath::IsNearlyEqual(OriginalOverlap, 56000.0, 1e-4));
    TArray<bool> Changed;
    FFZALayoutOps::WeldPolygons(Polygons, 50.0, Changed);
    TestTrue(TEXT("Lower in-range span is welded exactly"),
        HasEdge(Polygons[1], FVector2D(1000, 0), FVector2D(1000, 400)));
    TestTrue(TEXT("Upper in-range span is welded exactly"),
        HasEdge(Polygons[1], FVector2D(1000, 600), FVector2D(1000, 1000)));
    TestTrue(TEXT("Out-of-tolerance shelf span keeps the original target boundary"),
        HasEdge(Polygons[1], FVector2D(1020, 400), FVector2D(1020, 600)));
    const double WeldedOverlap = ComputeIntersectionArea(Polygons[0], Polygons[1]);
    TestTrue(TEXT("Welding nearby spans does not increase existing overlap beneath the shelf"),
        FMath::IsNearlyEqual(WeldedOverlap, OriginalOverlap, 1e-4));
    TestValidOutput(*this, Polygons);
    TestIdempotent(*this, Polygons, 50.0);
    return true;
}

#endif
