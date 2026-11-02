// Automatic repair regressions use pure geometry; no editor world or level assets are touched.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZAAutoWeld.h"
#include "FFZAOverlapRepair.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"

#include <limits>

namespace FFZAAutoWeldTestsPrivate
{
    TArray<FVector2D> MakeAutoWeldRectangle(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    bool HasAutoWeldEdge(const TArray<FVector2D>& Ring, const FVector2D& A, const FVector2D& B)
    {
        for (int32 Index = 0; Index < Ring.Num(); ++Index)
        {
            const FVector2D& P = Ring[Index];
            const FVector2D& Q = Ring[(Index + 1) % Ring.Num()];
            if ((P.Equals(A, 1.e-6) && Q.Equals(B, 1.e-6)) || (P.Equals(B, 1.e-6) && Q.Equals(A, 1.e-6)))
            {
                return true;
            }
        }
        return false;
    }

    void CheckAutoWeldComplete(FAutomationTestBase& Test, const FFZAAutoWeld::FResult& Result, const int32 ExpectedCount)
    {
        Test.TestTrue(TEXT("A completed repair can be committed atomically"), Result.bCanCommit);
        Test.TestEqual(TEXT("No selected zone is lost"), Result.Polygons.Num(), ExpectedCount);
        Test.TestEqual(TEXT("Changed flags retain selected input slots"), Result.ChangedPolygons.Num(), ExpectedCount);
        Test.TestEqual(TEXT("Final inspection finds no in-range gap pairs"), Result.Diagnostics.GapPairCount, 0);
        Test.TestEqual(TEXT("Final inspection finds no positive-area overlaps"), Result.Diagnostics.OverlapPairCount, 0);
        Test.TestEqual(TEXT("Final inspection has no rejected zones"), Result.Diagnostics.RejectedPolygonCount, 0);
        Test.TestEqual(TEXT("Completed repair contains no invalid input"), Result.Stats.InvalidInputPolygonCount, 0);
        for (int32 PolygonIndex = 0; PolygonIndex < Result.Polygons.Num(); ++PolygonIndex)
        {
            const TArray<FVector2D>& Polygon = Result.Polygons[PolygonIndex];
            Test.TestTrue(TEXT("Every resulting ring is still a simple polygon"), FFZAPolygonMath::IsSimplePolygon(Polygon));
            Test.TestTrue(TEXT("Every resulting zone retains positive area"), FFZAPolygonMath::Area(Polygon) > 1.e-6);
            if (Test.TestTrue(TEXT("Provenance retains the original zone slot"), Result.Stats.SourcePointKeys.IsValidIndex(PolygonIndex)))
            {
                Test.TestEqual(TEXT("Every output point has a source key"), Result.Stats.SourcePointKeys[PolygonIndex].Num(), Polygon.Num());
            }
        }
    }

    void CheckAutoWeldSourceKey(FAutomationTestBase& Test, const FFZAAutoWeld::FResult& Result,
        const int32 PolygonIndex, const FVector2D& Point, const double ExpectedKey)
    {
        const int32 PointIndex = Result.Polygons[PolygonIndex].IndexOfByPredicate([&Point](const FVector2D& Candidate)
        {
            return Candidate.Equals(Point, 1.e-6);
        });
        if (Test.TestTrue(TEXT("Expected intersection or inserted point is present"), PointIndex != INDEX_NONE) &&
            Test.TestTrue(TEXT("Expected point has source provenance"),
                Result.Stats.SourcePointKeys.IsValidIndex(PolygonIndex) && Result.Stats.SourcePointKeys[PolygonIndex].IsValidIndex(PointIndex)))
        {
            Test.TestEqual(TEXT("Source key belongs to its own original edge"),
                Result.Stats.SourcePointKeys[PolygonIndex][PointIndex], ExpectedKey, 1.e-6);
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldExpandedGapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.DefaultExpandedRepairRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldExpandedGapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    for (const double GapCm : { 66.73, 100.0 })
    {
        const TArray<TArray<FVector2D>> Authored = {
            MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(1000 + GapCm, 0, 2000 + GapCm, 1000)
        };
        const auto Before = Authored;
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Valid selected rings can be analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { continue; }
        CheckAutoWeldComplete(*this, Result, 2);
        TestTrue(TEXT("Residual gap triggers the bounded automatic fallback"), Result.bExpandedRadiusUsed);
        TestEqual(TEXT("50cm weld setting permits exactly 100cm automatic repair"), Result.RepairRadiusCm, 100.0);
        TestTrue(TEXT("Manual reference stays at its authored boundary"), Result.Polygons[0] == Authored[0]);
        TestTrue(TEXT("Both selected rings share the entire repaired seam"),
            HasAutoWeldEdge(Result.Polygons[0], FVector2D(1000, 0), FVector2D(1000, 1000)) &&
            HasAutoWeldEdge(Result.Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
        TestTrue(TEXT("Preview computation preserves authored input"), Authored == Before);
        TestEqual(TEXT("Measured displacement includes the expanded movement"), Result.Stats.MaximumMoveCm, GapCm, 1.e-6);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldOutsideRangeTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.PreservesBeyondRangeZones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldOutsideRangeTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(1101, 0, 2101, 1000) };
    FFZAAutoWeld::FResult Result;
    TestTrue(TEXT("Distant valid zones still produce a valid inspection"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result));
    TestTrue(TEXT("101cm seam is never silently bridged by a 100cm repair"), Result.Polygons == Authored);
    TestEqual(TEXT("No out-of-range point is moved"), Result.Stats.MovedVertexCount, 0);
    TestEqual(TEXT("Out-of-range zones produce no pending write"), Result.Stats.ChangedPolygonCount, 0);
    for (const bool bChanged : Result.ChangedPolygons) { TestFalse(TEXT("Out-of-range changed flags remain clear"), bChanged); }
    FFZAWeldDiagnostics::FResult WiderInspection;
    FFZAWeldDiagnostics::Analyze(Result.Polygons, {}, 101.0, WiderInspection);
    TestEqual(TEXT("The out-of-range gap remains visible when explicitly inspected at its distance"), WiderInspection.GapPairCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldScreenshotFixtureTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.ThirteenZonesSevenScreenshotGaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldScreenshotFixtureTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    // Five groups, thirteen rings and seven gaps reproduce the reported 50cm preview failure.
    // One exact seam is already connected; six remaining seams are 100cm and one is 66.73cm.
    const TArray<TArray<double>> GapsByGroup = { { 100.0, 100.0, 0.0 }, { 100.0, 100.0 }, { 100.0 }, { 100.0 }, { 66.73 } };
    TArray<TArray<FVector2D>> Authored;
    for (int32 GroupIndex = 0; GroupIndex < GapsByGroup.Num(); ++GroupIndex)
    {
        double NextX = 0.0;
        const double Y = GroupIndex * 5000.0;
        Authored.Add(MakeAutoWeldRectangle(NextX, Y, NextX + 1000.0, Y + 1000.0));
        NextX += 1000.0;
        for (const double GapCm : GapsByGroup[GroupIndex])
        {
            NextX += GapCm;
            Authored.Add(MakeAutoWeldRectangle(NextX, Y, NextX + 1000.0, Y + 1000.0));
            NextX += 1000.0;
        }
    }
    TestEqual(TEXT("Fixture includes every selected zone from the report"), Authored.Num(), 13);
    TArray<TArray<FVector2D>> NarrowResult = Authored;
    TArray<bool> NarrowChanged;
    FFZALayoutOps::FWeldStats NarrowStats;
    FFZALayoutOps::WeldSelectedPolygonGroups(NarrowResult, 0, 50.0, NarrowChanged, &NarrowStats);
    TestEqual(TEXT("Original 50cm preview has no writable changes"), NarrowStats.ChangedPolygonCount, 0);
    FFZAWeldDiagnostics::FResult InitialDiagnostics;
    FFZAWeldDiagnostics::Analyze(NarrowResult, {}, 100.0, InitialDiagnostics);
    TestEqual(TEXT("Original preview reports the seven remaining gaps"), InitialDiagnostics.GapPairCount, 7);
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Automatic application can analyze the whole fixed selection"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { return false; }
    CheckAutoWeldComplete(*this, Result, 13);
    TestTrue(TEXT("Automatic pass produces actual writable changes"), Result.Stats.ChangedPolygonCount > 0);
    TestTrue(TEXT("Screenshot gaps use the automatic 100cm pass"), Result.bExpandedRadiusUsed);
    TestEqual(TEXT("Every repaired component is reported"), Result.Stats.Groups.Num(), 5);
    TestTrue(TEXT("Preferred reference remains fixed"), Result.Polygons[0] == Authored[0]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldReferenceOrderTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.ManualAndAutomaticReferenceOrder",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldReferenceOrderTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeAutoWeldRectangle(-10000, 0, -9000, 1000),
        MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(1100, 0, 2600, 1000),
        MakeAutoWeldRectangle(5000, 0, 6000, 1000), MakeAutoWeldRectangle(6100, 0, 8100, 1000)
    };
    const TArray<TArray<int32>> Orders = { { 0, 1, 2, 3, 4 }, { 4, 2, 0, 3, 1 }, { 3, 1, 4, 2, 0 } };
    TArray<TArray<FVector2D>> Expected;
    for (const TArray<int32>& Order : Orders)
    {
        TArray<TArray<FVector2D>> Reordered;
        for (const int32 Identity : Order) { Reordered.Add(Authored[Identity]); }
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Order-independent input is valid"), FFZAAutoWeld::BuildResult(Reordered, Order.Find(0), 50.0, Result))) { continue; }
        CheckAutoWeldComplete(*this, Result, Authored.Num());
        TArray<TArray<FVector2D>> ByIdentity;
        ByIdentity.SetNum(Authored.Num());
        for (int32 Index = 0; Index < Order.Num(); ++Index) { ByIdentity[Order[Index]] = Result.Polygons[Index]; }
        TestTrue(TEXT("Manual singleton stays fixed"), ByIdentity[0] == Authored[0]);
        TestTrue(TEXT("Largest automatic references keep their authored full spans"), ByIdentity[2] == Authored[2] && ByIdentity[4] == Authored[4]);
        if (Expected.IsEmpty()) { Expected = ByIdentity; }
        else { TestTrue(TEXT("Input selection order does not change repaired geometry"), ByIdentity == Expected); }
        TArray<int32> AutomaticRootIdentities;
        for (const FFZALayoutOps::FWeldGroup& Group : Result.Stats.Groups)
        {
            if (Group.bUsesManualReference) { TestEqual(TEXT("Preferred identity remains the explicit root"), Order[Group.ReferencePolygonIndex], 0); }
            else { AutomaticRootIdentities.Add(Order[Group.ReferencePolygonIndex]); }
        }
        TestTrue(TEXT("Independent automatic groups select the largest-area zones"),
            AutomaticRootIdentities.Num() == 2 && AutomaticRootIdentities.Contains(2) && AutomaticRootIdentities.Contains(4));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldCrossingOverlapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.CrossingOverlapPreservesExposedGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldCrossingOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeAutoWeldRectangle(0, 0, 1000, 1000),
        { FVector2D(980, 980), FVector2D(1400, 900), FVector2D(1400, 1400) }
    };
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Crossing overlap is analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { return false; }
    CheckAutoWeldComplete(*this, Result, 2);
    for (const FVector2D& Corner : Authored[0]) { TestTrue(TEXT("Every original manual reference corner remains fixed"), Result.Polygons[0].Contains(Corner)); }
    TestEqual(TEXT("Manual reference area is preserved"), FFZAPolygonMath::Area(Result.Polygons[0]), FFZAPolygonMath::Area(Authored[0]), 1.e-6);
    TestTrue(TEXT("Exposed target corners are preserved instead of shrinking the whole target"),
        Result.Polygons[1].Contains(FVector2D(1400, 900)) && Result.Polygons[1].Contains(FVector2D(1400, 1400)));
    TestTrue(TEXT("Only positive overlap is removed from the target"), FFZAPolygonMath::Area(Result.Polygons[1]) < FFZAPolygonMath::Area(Authored[1]));
    TestTrue(TEXT("Crossing repair respects the original displacement budget"), Result.Stats.MaximumMoveCm <= 100.0 + 1.e-6);
    CheckAutoWeldSourceKey(*this, Result, 1, FVector2D(1000, 1000), 2.0 + 400.0 / 420.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldShallowOverlapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.ShallowOverlapAndWinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldShallowOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    for (const bool bReverse : { false, true })
    {
        TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(980, 0, 2000, 1000) };
        if (bReverse) { Algo::Reverse(Authored[1]); }
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Shallow overlap input is valid in either winding"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { continue; }
        CheckAutoWeldComplete(*this, Result, 2);
        TestTrue(TEXT("Reference boundary is unchanged"), Result.Polygons[0] == Authored[0]);
        TestTrue(TEXT("Target adopts the exact reference seam"), HasAutoWeldEdge(Result.Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
        TestEqual(TEXT("Shallow overlap repair preserves the target's exposed extent"), FFZAPolygonMath::Area(Result.Polygons[1]), 1000000.0, 1.e-6);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldDeepOverlapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.DeepOverlapSafelyTrimsOnlyCoveredArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldDeepOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(500, 100, 1500, 900),
        MakeAutoWeldRectangle(5000, 0, 6000, 1000), MakeAutoWeldRectangle(6100, 0, 7100, 1000)
    };
    const auto Before = Authored;
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Deep positive-area overlap can be analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { return false; }
    CheckAutoWeldComplete(*this, Result, 4);
    TestEqual(TEXT("Deep trim preserves the entire manual reference area"), FFZAPolygonMath::Area(Result.Polygons[0]), 1000000.0, 1.e-6);
    TestEqual(TEXT("500cm overlap is subtracted without reducing the target's exposed area"), FFZAPolygonMath::Area(Result.Polygons[1]), 400000.0, 1.e-6);
    TestTrue(TEXT("Deep trim preserves both exposed target corners"),
        Result.Polygons[1].Contains(FVector2D(1500, 100)) && Result.Polygons[1].Contains(FVector2D(1500, 900)));
    TestTrue(TEXT("Independent 100cm gap is repaired in the same atomic result"),
        HasAutoWeldEdge(Result.Polygons[3], FVector2D(6000, 0), FVector2D(6000, 1000)));
    TestTrue(TEXT("Preview computation never changes authored rings"), Authored == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldUnsupportedTopologyTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.RejectsEmptyHoleAndSplitWithoutLosingZone",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldUnsupportedTopologyTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<TArray<FVector2D>>> Cases = {
        { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(400, 400, 600, 600) },
        { MakeAutoWeldRectangle(200, 200, 300, 300), MakeAutoWeldRectangle(150, 150, 350, 350) },
        { MakeAutoWeldRectangle(450, 0, 550, 1000), MakeAutoWeldRectangle(0, 400, 1000, 600) }
    };
    for (const TArray<TArray<FVector2D>>& Authored : Cases)
    {
        const auto Before = Authored;
        FFZAAutoWeld::FResult Result;
        TestTrue(TEXT("Unsupported repair topology still has a valid inspection"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result));
        TestFalse(TEXT("Empty, holed or split zone cannot be silently committed as one spline"), Result.bCanCommit);
        TestTrue(TEXT("Unsupported topology is explained"), !Result.FailureReason.IsEmpty());
        TestEqual(TEXT("No selected actor slot is deleted"), Result.Polygons.Num(), Authored.Num());
        for (const TArray<FVector2D>& Polygon : Result.Polygons)
        {
            TestTrue(TEXT("Rejected repair retains a nonempty original ring"), Polygon.Num() >= 3 && FFZAPolygonMath::Area(Polygon) > 1.e-6);
        }
        TestTrue(TEXT("Topology rejection leaves authored input untouched"), Authored == Before);

        TArray<TArray<FVector2D>> HelperPolygons = Authored;
        TArray<TArray<double>> HelperKeys;
        HelperKeys.SetNum(Authored.Num());
        for (int32 PolygonIndex = 0; PolygonIndex < Authored.Num(); ++PolygonIndex)
        {
            for (int32 PointIndex = 0; PointIndex < Authored[PolygonIndex].Num(); ++PointIndex)
            {
                HelperKeys[PolygonIndex].Add(static_cast<double>(PointIndex));
            }
        }
        const auto OriginalKeys = HelperKeys;
        FFZALayoutOps::FWeldGroup HelperGroup;
        HelperGroup.PolygonIndices = { 0, 1 };
        HelperGroup.ReferencePolygonIndex = 0;
        HelperGroup.bUsesManualReference = true;
        FString HelperFailure;
        TestFalse(TEXT("Overlap helper refuses unsupported empty, holed or multipart cuts"),
            FFZAOverlapRepair::Repair(HelperPolygons, 0, { HelperGroup }, HelperKeys,
                Authored, TNumericLimits<double>::Max(), HelperFailure));
        TestTrue(TEXT("Overlap helper failure is atomic for all polygon rings"), HelperPolygons == Authored);
        TestTrue(TEXT("Overlap helper failure is atomic for all source provenance"), HelperKeys == OriginalKeys);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldChainRepeatTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.MergedChainAndIdempotence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldChainRepeatTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(1040, 0, 2040, 1000), MakeAutoWeldRectangle(2140, 0, 3140, 1000)
    };
    FFZAAutoWeld::FResult First;
    if (!TestTrue(TEXT("Mixed narrow and expanded chain is analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, First))) { return false; }
    CheckAutoWeldComplete(*this, First, 3);
    TestEqual(TEXT("Expanded adjacency merges every connected chain member"), First.Stats.Groups.Num(), 1);
    TestTrue(TEXT("First target follows manual reference"), HasAutoWeldEdge(First.Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
    TestTrue(TEXT("Far target follows the accepted exposed edge of its immediate neighbour"), HasAutoWeldEdge(First.Polygons[2], FVector2D(2040, 0), FVector2D(2040, 1000)));
    TestTrue(TEXT("Expanded pass restarts from authored geometry and stays within budget"), First.Stats.MaximumMoveCm <= 100.0 + 1.e-6);
    FFZAAutoWeld::FResult Second;
    if (!TestTrue(TEXT("Completed chain can be inspected again"), FFZAAutoWeld::BuildResult(First.Polygons, 0, 50.0, Second))) { return false; }
    CheckAutoWeldComplete(*this, Second, 3);
    TestTrue(TEXT("Second execution preserves every coordinate and point count"), Second.Polygons == First.Polygons);
    TestEqual(TEXT("Second execution produces no pending changes"), Second.Stats.ChangedPolygonCount, 0);
    TestEqual(TEXT("Second execution inserts no further seam nodes"), Second.Stats.InsertedVertexCount, 0);
    TestEqual(TEXT("Second execution has no displacement"), Second.Stats.MaximumMoveCm, 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldClosingHeightTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.OriginalClosingEdgeProvenanceAndHeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldClosingHeightTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(-1100, 250, -100, 750) };
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Partial seam near the closing edge is analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { return false; }
    CheckAutoWeldComplete(*this, Result, 2);
    CheckAutoWeldSourceKey(*this, Result, 0, FVector2D(0, 750), 3.25);
    CheckAutoWeldSourceKey(*this, Result, 0, FVector2D(0, 250), 3.75);
    TArray<FVector> WorldPoints;
    if (!TestTrue(TEXT("Automatic geometry can be written with pure height interpolation"),
        FFZASplineAuthoring::BuildWorldPolygonPreservingHeights({ 10.0, 20.0, 30.0, 50.0 },
            Result.Polygons[0], Result.Stats.SourcePointKeys[0], WorldPoints))) { return false; }
    for (const FVector& Point : WorldPoints)
    {
        if (FVector2D(Point.X, Point.Y).Equals(FVector2D(0, 750), 1.e-6)) { TestEqual(TEXT("First closing edge node keeps original interpolation"), Point.Z, 40.0, 1.e-6); }
        if (FVector2D(Point.X, Point.Y).Equals(FVector2D(0, 250), 1.e-6)) { TestEqual(TEXT("Second closing edge node wraps to original point zero"), Point.Z, 20.0, 1.e-6); }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldInvalidInputTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.InvalidReferenceAndUnsupportedRowsBlockWrites",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldInvalidInputTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<FVector2D> InvalidRing = { FVector2D(5000, 0), FVector2D(6000, 1000), FVector2D(5000, 1000), FVector2D(6000, 0) };
    const TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), InvalidRing, MakeAutoWeldRectangle(1100, 0, 2100, 1000) };
    for (const int32 InvalidReference : TArray<int32>{ INDEX_NONE, 1, 3 })
    {
        FFZAAutoWeld::FResult Result;
        TestFalse(TEXT("Invalid reference aborts instead of silently choosing another zone"), FFZAAutoWeld::BuildResult(Authored, InvalidReference, 50.0, Result));
        TestFalse(TEXT("Invalid reference never yields writable output"), Result.bCanCommit);
        TestTrue(TEXT("Invalid reference is explicitly reported"), Result.Stats.bInvalidReference);
    }
    FFZAAutoWeld::FResult Result;
    TestTrue(TEXT("A valid reference can still provide diagnostic analysis of invalid neighbours"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result));
    TestFalse(TEXT("Skipped invalid selected rows prevent whole-selection commit"), Result.bCanCommit);
    TestTrue(TEXT("Invalid row identity is preserved in original input order"), Result.Stats.InvalidInputPolygonIndices.Contains(1));
    TestTrue(TEXT("Invalid original ring is not silently replaced"), Result.Polygons.IsValidIndex(1) && Result.Polygons[1] == InvalidRing);
    for (const double InvalidTolerance : { 0.0, -50.0, std::numeric_limits<double>::infinity() })
    {
        FFZAAutoWeld::FResult InvalidSettings;
        TestFalse(TEXT("Invalid repair distance cannot authorize geometry computation"), FFZAAutoWeld::BuildResult(Authored, 0, InvalidTolerance, InvalidSettings));
        TestFalse(TEXT("Invalid distance never yields writable output"), InvalidSettings.bCanCommit);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldReversedProvenanceTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.OverlapNodingComposesReversedAndClosingKeys",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldReversedProvenanceTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(-20, 250, 20, 750) };
    TArray<TArray<FVector2D>> Working = Authored;
    for (TArray<FVector2D>& Ring : Working) { Algo::Reverse(Ring); }
    TArray<TArray<double>> Keys = { { 3.0, 2.0, 1.0, 0.0 }, { 3.0, 2.0, 1.0, 0.0 } };
    FFZALayoutOps::FWeldGroup Group;
    Group.PolygonIndices = { 0, 1 };
    Group.ReferencePolygonIndex = 0;
    Group.bUsesManualReference = true;
    FString FailureReason;
    if (!TestTrue(TEXT("Overlap cleanup accepts reversed working rings with original authored keys"),
        FFZAOverlapRepair::Repair(Working, 0, { Group }, Keys, Authored, TNumericLimits<double>::Max(), FailureReason))) { return false; }
    FFZAAutoWeld::FResult Result;
    Result.Polygons = Working;
    Result.Stats.SourcePointKeys = Keys;
    CheckAutoWeldSourceKey(*this, Result, 0, FVector2D(0, 750), 3.25);
    CheckAutoWeldSourceKey(*this, Result, 0, FVector2D(0, 250), 3.75);
    CheckAutoWeldSourceKey(*this, Result, 1, FVector2D(0, 250), 0.5);
    CheckAutoWeldSourceKey(*this, Result, 1, FVector2D(0, 750), 2.5);
    FFZAWeldDiagnostics::FResult Diagnostics;
    FFZAWeldDiagnostics::Analyze(Working, {}, 100.0, Diagnostics);
    TestEqual(TEXT("Reversed provenance cleanup retains no overlap"), Diagnostics.OverlapPairCount, 0);
    TestEqual(TEXT("Reversed provenance cleanup creates no seam gap"), Diagnostics.GapPairCount, 0);
    const TArray<TArray<double>> Heights = { { 10.0, 20.0, 30.0, 50.0 }, { 100.0, 200.0, 300.0, 400.0 } };
    for (int32 PolygonIndex = 0; PolygonIndex < Working.Num(); ++PolygonIndex)
    {
        TArray<FVector> WorldPoints;
        if (TestTrue(TEXT("Composed reversed keys support original height reconstruction"),
            FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights[PolygonIndex], Working[PolygonIndex], Keys[PolygonIndex], WorldPoints)))
        {
            for (const FVector& Point : WorldPoints)
            {
                if (FVector2D(Point.X, Point.Y).Equals(FVector2D(0, 750), 1.e-6))
                {
                    TestEqual(TEXT("Upper cut preserves source height in either reversed ring"), Point.Z, PolygonIndex == 0 ? 40.0 : 350.0, 1.e-6);
                }
                if (FVector2D(Point.X, Point.Y).Equals(FVector2D(0, 250), 1.e-6))
                {
                    TestEqual(TEXT("Lower cut preserves source height across the closing key wrap"), Point.Z, PolygonIndex == 0 ? 20.0 : 150.0, 1.e-6);
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldDiagnosticCapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.CappedDisplayNeverAuthorizesIncompleteRepair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldDiagnosticCapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    TArray<TArray<FVector2D>> Authored;
    constexpr int32 PairCount = 130;
    Authored.Reserve(PairCount * 2);
    for (int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex)
    {
        const double Y = PairIndex * 3000.0;
        Authored.Add(MakeAutoWeldRectangle(0, Y, 1000, Y + 1000));
        // Complete containment cannot become one nonempty target spline, so it must remain
        // blocked even though ordinary deep partial intersections are safe to trim.
        Authored.Add(MakeAutoWeldRectangle(400, Y + 400, 600, Y + 600));
    }
    FFZAAutoWeld::FResult Result;
    TestTrue(TEXT("Large selected set can still be analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result));
    TestFalse(TEXT("Capped issue display cannot make unresolved geometry committable"), Result.bCanCommit);
    TestTrue(TEXT("Issue display stays within its documented bound"), Result.Diagnostics.Issues.Num() <= 512);
    TestTrue(TEXT("Truncation is explicit when unresolved perimeter spans exceed the display cap"), Result.Diagnostics.bTruncated);
    TestEqual(TEXT("Overlap pair count remains complete after span display truncation"), Result.Diagnostics.OverlapPairCount, PairCount);
    TestEqual(TEXT("All selected zone slots remain available for review"), Result.Polygons.Num(), PairCount * 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldAngledGateTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.AngledPartialGapsCannotBypassCompletionGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldAngledGateTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    for (const double AngleDegrees : { 20.0, 30.0, 40.0 })
    {
        // The 30 degree case uses the reported 580cm skew fixture exactly.
        const double SideOffset = AngleDegrees == 30.0 ? 580.0 : FMath::Tan(FMath::DegreesToRadians(AngleDegrees)) * 1000.0;
        const TArray<TArray<FVector2D>> Authored = {
            MakeAutoWeldRectangle(0, 0, 1000, 1000),
            { FVector2D(1080, 0), FVector2D(2080, 0), FVector2D(2080 + SideOffset, 1000), FVector2D(1080 + SideOffset, 1000) }
        };
        FFZAWeldDiagnostics::FResult Before;
        FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Before);
        TestEqual(TEXT("20-40 degree facing seams remain visible to the weld-compatible gap inspector"), Before.GapPairCount, 1);
        TestEqual(TEXT("Diverging selected rings do not overlap"), Before.OverlapPairCount, 0);
        int32 GapSpanCount = 0;
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Before.Issues)
        {
            if (Issue.Type != FFZAWeldDiagnostics::EIssueType::Gap) { continue; }
            ++GapSpanCount;
            TestTrue(TEXT("Angled diagnostic reports a positive gap within the configured radius"),
                Issue.MaximumGapCm > 0.0 && Issue.MaximumGapCm <= 100.0 + 1.e-4);
            const double HighestY = FMath::Max(FMath::Max(Issue.StartA.Y, Issue.EndA.Y), FMath::Max(Issue.StartB.Y, Issue.EndB.Y));
            TestTrue(TEXT("Inspection clips the diverging seam to its actual in-range portion"),
                HighestY > 0.0 && HighestY < 100.0);
        }
        TestTrue(TEXT("At least one finite positive-length angled gap span is displayed"), GapSpanCount > 0);
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Angled authored rings are valid analysis input"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { continue; }
        FFZAWeldDiagnostics::FResult After;
        FFZAWeldDiagnostics::Analyze(Result.Polygons, Result.Stats.RejectedPolygonIndices, 100.0, After);
        if (Result.bCanCommit)
        {
            TestEqual(TEXT("A writable proposal has zero angled gaps under the same 45 degree policy"), After.GapPairCount, 0);
            TestEqual(TEXT("A writable angled proposal has zero overlaps"), After.OverlapPairCount, 0);
            TestEqual(TEXT("A writable angled proposal has zero rejected zones"), After.RejectedPolygonCount, 0);
        }
        else
        {
            TestTrue(TEXT("An incomplete angled repair explains why the entire selection is blocked"), !Result.FailureReason.IsEmpty());
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAAutoWeldRotatedOffsetOverlapTest,
    "FF_ZoneAuthoring.Unit.AutoWeld.RotatedDeepOverlapAndLargeWorldOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAAutoWeldRotatedOffsetOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAAutoWeldTestsPrivate;
    const double Angle = FMath::DegreesToRadians(37.0);
    const double Cos = FMath::Cos(Angle);
    const double Sin = FMath::Sin(Angle);
    for (const double Offset : { 0.0, 1.e9 })
    {
        const FVector2D Origin(Offset, -Offset);
        TArray<TArray<FVector2D>> Authored = { MakeAutoWeldRectangle(0, 0, 1000, 1000), MakeAutoWeldRectangle(500, 100, 1500, 900) };
        for (TArray<FVector2D>& Ring : Authored)
        {
            for (FVector2D& Point : Ring)
            {
                Point = Origin + FVector2D(Point.X * Cos - Point.Y * Sin, Point.X * Sin + Point.Y * Cos);
            }
        }
        const TArray<TArray<FVector2D>> Before = Authored;
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Rotated large-offset overlap can be analyzed"), FFZAAutoWeld::BuildResult(Authored, 0, 50.0, Result))) { continue; }
        TestTrue(TEXT("Rotated overlap planning preserves every authored input coordinate"), Authored == Before);
        if (Offset == 0.0) { TestTrue(TEXT("Safe rotated deep difference is writable at the ordinary world origin"), Result.bCanCommit); }
        if (Result.bCanCommit)
        {
            TestEqual(TEXT("Writable rotated Boolean result reports no overlap pairs"), Result.Diagnostics.OverlapPairCount, 0);
            TestEqual(TEXT("Writable rotated Boolean result reports no seam gaps"), Result.Diagnostics.GapPairCount, 0);
        }
        else
        {
            TestTrue(TEXT("A conservative large-offset refusal stays read-only and explains its failed validation"), !Result.FailureReason.IsEmpty());
        }
        if (!TestEqual(TEXT("Rotated repair preserves both selected rings"), Result.Polygons.Num(), 2)) { continue; }
        for (const FVector2D& Corner : Authored[0])
        {
            TestTrue(TEXT("Every manual root corner retains its exact authored coordinate"), Result.Polygons[0].Contains(Corner));
        }
        for (int32 PolygonIndex = 0; PolygonIndex < Result.Polygons.Num(); ++PolygonIndex)
        {
            TArray<FVector2D> Local;
            for (const FVector2D& Point : Result.Polygons[PolygonIndex]) { Local.Add(Point - Origin); }
            TestTrue(TEXT("Rotation and offset leave each resulting ring simple"), FFZAPolygonMath::IsSimplePolygon(Local));
            TestEqual(TEXT("Deep difference removes only the area owned by the fixed reference"),
                FFZAPolygonMath::Area(Local), PolygonIndex == 0 ? 1000000.0 : 400000.0, 1.e-3);
        }
        int32 ExactSharedCutNodes = 0;
        for (const FVector2D& Point : Result.Polygons[1])
        {
            if (FFZAPolygonMath::IsPointOnBoundary(Result.Polygons[0], Point, 1.e-4))
            {
                ++ExactSharedCutNodes;
                TestTrue(TEXT("Boolean seam nodes share identical world coordinates on both sides"), Result.Polygons[0].Contains(Point));
            }
        }
        TestEqual(TEXT("Both endpoints of the rotated cut are noded on the reference"), ExactSharedCutNodes, 2);
        FFZAWeldDiagnostics::FResult IndependentInspection;
        FFZAWeldDiagnostics::Analyze(Result.Polygons, {}, 100.0, IndependentInspection);
        if (Result.bCanCommit)
        {
            TestEqual(TEXT("Independent translated inspection confirms zero positive-area intersection for every writable result"), IndependentInspection.OverlapPairCount, 0);
            TestEqual(TEXT("Independent translated inspection confirms an exact shared seam for every writable result"), IndependentInspection.GapPairCount, 0);
        }
        if (IndependentInspection.OverlapPairCount > 0 || IndependentInspection.GapPairCount > 0)
        {
            TestFalse(TEXT("Large-offset residuals can never authorize a partial geometry write"), Result.bCanCommit);
        }
    }
    return true;
}

#endif
