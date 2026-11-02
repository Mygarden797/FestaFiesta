// Grouped welding regressions use pure arrays and never create or edit level assets.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "FFZALayoutOps.h"
#include "FFZAPolygonMath.h"
#include <limits>

namespace FFZAGroupedWeldTestsPrivate
{
    TArray<FVector2D> MakeGroupedWeldRectangle(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    bool HasGroupedWeldEdge(const TArray<FVector2D>& Ring, const FVector2D& A, const FVector2D& B)
    {
        for (int32 I = 0; I < Ring.Num(); ++I)
        {
            const FVector2D& P = Ring[I];
            const FVector2D& Q = Ring[(I + 1) % Ring.Num()];
            if ((P == A && Q == B) || (P == B && Q == A)) { return true; }
        }
        return false;
    }

    void CheckGroupedWeldPointKey(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Polygons,
        const FFZALayoutOps::FWeldStats& Stats, const int32 PolygonIndex, const FVector2D& Point, const double ExpectedKey)
    {
        const int32 PointIndex = Polygons[PolygonIndex].Find(Point);
        if (Test.TestTrue(TEXT("Expected seam point is present in its original caller slot"), PointIndex != INDEX_NONE) &&
            Test.TestTrue(TEXT("Seam provenance belongs to the same caller slot"),
                Stats.SourcePointKeys.IsValidIndex(PolygonIndex) && Stats.SourcePointKeys[PolygonIndex].IsValidIndex(PointIndex)))
        {
            Test.TestEqual(TEXT("Inserted point uses its own original ring edge parameter"),
                Stats.SourcePointKeys[PolygonIndex][PointIndex], ExpectedKey, 1.e-6);
        }
    }

    void CheckGroupedWeldRepeat(FAutomationTestBase& Test, TArray<TArray<FVector2D>>& Polygons, const int32 Reference)
    {
        const TArray<TArray<FVector2D>> Before = Polygons;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        Test.TestEqual(TEXT("Repeated grouped execution moves no existing points"),
            FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, Reference, 50.0, Changed, &Stats), 0);
        Test.TestTrue(TEXT("Repeated grouped execution preserves every coordinate and point count"), Polygons == Before);
        Test.TestEqual(TEXT("Repeated grouped execution adds no split points"), Stats.InsertedVertexCount, 0);
        Test.TestEqual(TEXT("Repeated grouped execution has no changed polygons"), Stats.ChangedPolygonCount, 0);
        Test.TestEqual(TEXT("Repeated grouped execution has zero maximum displacement"), Stats.MaximumMoveCm, 0.0);
        for (const bool bChanged : Changed) { Test.TestFalse(TEXT("Repeated grouped execution clears every changed flag"), bChanged); }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldAllComponentsTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.AllComponentsAndPreferredGroup",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldAllComponentsTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(1020, 0, 2000, 1000),
        MakeGroupedWeldRectangle(6000, 0, 7000, 1000), MakeGroupedWeldRectangle(7020, 0, 8000, 1000),
        MakeGroupedWeldRectangle(20000, 0, 21000, 1000)
    };
    const TArray<TArray<FVector2D>> Before = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    TestEqual(TEXT("Both disconnected pairs weld their two near vertices"),
        FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 3, 50.0, Changed, &Stats), 4);
    if (!TestEqual(TEXT("Both pairs and the singleton are reported"), Stats.Groups.Num(), 3)) { return false; }
    TestEqual(TEXT("Manually chosen component is processed first despite higher world X"), Stats.Groups[0].ReferencePolygonIndex, 3);
    TestTrue(TEXT("First group marks the explicit manual reference"), Stats.Groups[0].bUsesManualReference);
    TestTrue(TEXT("First group retains original input member indices"),
        Stats.Groups[0].PolygonIndices.Contains(2) && Stats.Groups[0].PolygonIndices.Contains(3) && Stats.Groups[0].PolygonIndices.Num() == 2);
    TestEqual(TEXT("Independent pair receives its automatic root"), Stats.Groups[1].ReferencePolygonIndex, 0);
    TestFalse(TEXT("Independent pair is marked automatic"), Stats.Groups[1].bUsesManualReference);
    TestEqual(TEXT("Singleton receives its own root"), Stats.Groups[2].ReferencePolygonIndex, 4);
    TestEqual(TEXT("Singleton has one reported member"), Stats.Groups[2].PolygonIndices.Num(), 1);
    TestTrue(TEXT("Manual root is fixed even though its neighbour has larger area"), Polygons[3] == Before[3]);
    TestTrue(TEXT("Automatic root and singleton retain authored geometry"), Polygons[0] == Before[0] && Polygons[4] == Before[4]);
    TestTrue(TEXT("Manual seam belongs to both selected rings"),
        HasGroupedWeldEdge(Polygons[2], FVector2D(7020, 0), FVector2D(7020, 1000)) &&
        HasGroupedWeldEdge(Polygons[3], FVector2D(7020, 0), FVector2D(7020, 1000)));
    TestTrue(TEXT("Automatic seam belongs to both independent rings"),
        HasGroupedWeldEdge(Polygons[0], FVector2D(1000, 0), FVector2D(1000, 1000)) &&
        HasGroupedWeldEdge(Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
    TestEqual(TEXT("Only the two targets changed"), Stats.ChangedPolygonCount, 2);
    TestTrue(TEXT("Changed flags refer to original array indices"), !Changed[0] && Changed[1] && Changed[2] && !Changed[3] && !Changed[4]);
    TestEqual(TEXT("Independent completed groups are not misreported as disconnected"), Stats.DisconnectedPolygonIndices.Num(), 0);
    CheckGroupedWeldRepeat(*this, Polygons, 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldLargestAreaTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.AutomaticLargestAreaReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldLargestAreaTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(-10000, 0, -9000, 1000),
        MakeGroupedWeldRectangle(0, 0, 1000, 600), MakeGroupedWeldRectangle(1020, 0, 2520, 1000)
    };
    const TArray<FVector2D> OriginalAutomaticRoot = Polygons[2];
    const double OriginalRootArea = FFZAPolygonMath::Area(Polygons[2]);
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 0, 50.0, Changed, &Stats);
    if (!TestEqual(TEXT("Manual singleton and automatic pair are reported"), Stats.Groups.Num(), 2)) { return false; }
    TestEqual(TEXT("Larger-area ring wins over the geometrically earlier smaller ring"), Stats.Groups[1].ReferencePolygonIndex, 2);
    TestFalse(TEXT("Largest-area reference is automatic"), Stats.Groups[1].bUsesManualReference);
    for (const FVector2D& Corner : OriginalAutomaticRoot)
    {
        TestTrue(TEXT("Automatic reference preserves every authored corner while gaining shared nodes"), Polygons[2].Contains(Corner));
    }
    TestEqual(TEXT("Automatic reference preserves area"), FFZAPolygonMath::Area(Polygons[2]), OriginalRootArea, 1.e-6);
    TestTrue(TEXT("Small ring follows the automatic reference boundary exactly"),
        HasGroupedWeldEdge(Polygons[1], FVector2D(1020, 0), FVector2D(1020, 600)) &&
        HasGroupedWeldEdge(Polygons[2], FVector2D(1020, 0), FVector2D(1020, 600)));
    TestEqual(TEXT("Partial shared span adds one reference node"), Stats.InsertedVertexCount, 1);
    CheckGroupedWeldRepeat(*this, Polygons, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldInvalidAndSingletonTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.SingletonsInvalidInputsAndActualConnectivity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldInvalidAndSingletonTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(1020, 0, 2000, 1000),
        { FVector2D(5000, 0), FVector2D(6000, 1000), FVector2D(5000, 1000), FVector2D(6000, 0) },
        MakeGroupedWeldRectangle(20, 20, 300, 300), MakeGroupedWeldRectangle(20000, 0, 21000, 1000)
    };
    const auto Before = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 0, 50.0, Changed, &Stats);
    TestEqual(TEXT("Invalid ring is counted once for the entire selected set"), Stats.InvalidInputPolygonCount, 1);
    TestTrue(TEXT("Invalid input retains its global index"), Stats.InvalidInputPolygonIndices == TArray<int32>{2});
    TestTrue(TEXT("Invalid input remains unchanged"), Polygons[2] == Before[2] && !Changed[2]);
    TestEqual(TEXT("Valid pair and two valid singletons are reported"), Stats.Groups.Num(), 3);
    int32 ReportedMemberCount = 0;
    bool bNestedSingletonFound = false;
    for (const FFZALayoutOps::FWeldGroup& Group : Stats.Groups)
    {
        ReportedMemberCount += Group.PolygonIndices.Num();
        TestFalse(TEXT("Invalid ring never enters a component"), Group.PolygonIndices.Contains(2));
        if (Group.PolygonIndices.Contains(3))
        {
            bNestedSingletonFound = Group.PolygonIndices.Num() == 1 && Group.ReferencePolygonIndex == 3;
        }
    }
    TestEqual(TEXT("Every valid ring is reported exactly once"), ReportedMemberCount, 4);
    TestTrue(TEXT("Overlapping bounds with same-side edges do not create a weld connection"), bNestedSingletonFound);
    TestTrue(TEXT("Nested and remote singletons stay untouched"), Polygons[3] == Before[3] && Polygons[4] == Before[4] && !Changed[3] && !Changed[4]);
    TestEqual(TEXT("Reported singletons are not treated as unresolved members"), Stats.DisconnectedPolygonIndices.Num(), 0);
    TestEqual(TEXT("Valid manual reference does not abort because another input is invalid"), Stats.bInvalidReference, false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldSelectionOrderTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.SelectionOrderIndependentReferencesAndResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldSelectionOrderTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeGroupedWeldRectangle(-10000, 0, -9000, 1000),
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(1020, 0, 2520, 1000),
        MakeGroupedWeldRectangle(5000, 0, 6000, 1000), MakeGroupedWeldRectangle(6020, 0, 7020, 1000),
        MakeGroupedWeldRectangle(20000, 0, 21000, 1000)
    };
    const TArray<TArray<int32>> Orders = {
        {0, 1, 2, 3, 4, 5}, {5, 4, 3, 2, 1, 0}, {2, 5, 0, 4, 1, 3}, {4, 1, 3, 0, 5, 2}, {3, 2, 5, 1, 0, 4}
    };
    TArray<TArray<FVector2D>> Expected;
    TArray<bool> ExpectedChanged;
    for (const TArray<int32>& Order : Orders)
    {
        TArray<TArray<FVector2D>> Polygons;
        for (const int32 Identity : Order) { Polygons.Add(Authored[Identity]); }
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, Order.Find(0), 50.0, Changed, &Stats);
        TArray<TArray<FVector2D>> ByIdentity;
        TArray<bool> ChangedByIdentity;
        ByIdentity.SetNum(Authored.Num()); ChangedByIdentity.Init(false, Authored.Num());
        for (int32 I = 0; I < Order.Num(); ++I)
        {
            ByIdentity[Order[I]] = Polygons[I]; ChangedByIdentity[Order[I]] = Changed[I];
        }
        TArray<int32> RootIdentities;
        for (const FFZALayoutOps::FWeldGroup& Group : Stats.Groups)
        {
            RootIdentities.Add(Order[Group.ReferencePolygonIndex]);
        }
        TestTrue(TEXT("Manual, largest-area, equal-area geometric tie and singleton roots are stable across selection order"),
            RootIdentities == TArray<int32>({0, 2, 3, 5}));
        if (Expected.IsEmpty()) { Expected = ByIdentity; ExpectedChanged = ChangedByIdentity; }
        TestTrue(TEXT("Shuffling selection preserves actor geometry exactly"), ByIdentity == Expected);
        TestTrue(TEXT("Shuffling selection preserves changed actor identities"), ChangedByIdentity == ExpectedChanged);
        TestEqual(TEXT("All shuffled components are resolved"), Stats.DisconnectedPolygonIndices.Num(), 0);
        CheckGroupedWeldRepeat(*this, Polygons, Order.Find(0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldRejectionIsolationTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.RejectedComponentDoesNotBlockIndependentSuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldRejectionIsolationTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(980, 300, 990, 700),
        MakeGroupedWeldRectangle(5000, 0, 6000, 1000), MakeGroupedWeldRectangle(6020, 0, 7000, 1000)
    };
    const auto Before = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    TestEqual(TEXT("Independent safe pair still moves its two near vertices"),
        FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 0, 50.0, Changed, &Stats), 2);
    TestTrue(TEXT("Unsafe narrow target rejection retains the caller index"), Stats.RejectedPolygonIndices == TArray<int32>{1});
    TestEqual(TEXT("Unsafe target is rejected only once"), Stats.RejectedPolygonCount, 1);
    TestEqual(TEXT("Unsafe target remains a valid authored input"), Stats.InvalidInputPolygonCount, 0);
    TestTrue(TEXT("Rejected proposal leaks no coordinate changes or nodes into either side"), Polygons[0] == Before[0] && Polygons[1] == Before[1]);
    TestTrue(TEXT("Independent safe pair completes its shared seam"),
        HasGroupedWeldEdge(Polygons[2], FVector2D(6000, 0), FVector2D(6000, 1000)) &&
        HasGroupedWeldEdge(Polygons[3], FVector2D(6000, 0), FVector2D(6000, 1000)));
    TestTrue(TEXT("Only the independent target is reported changed"), !Changed[0] && !Changed[1] && !Changed[2] && Changed[3]);
    TestEqual(TEXT("Independent components remain separately reported"), Stats.Groups.Num(), 2);
    TestEqual(TEXT("Rejected partial seam produces no leaked split nodes"), Stats.InsertedVertexCount, 0);
    CheckGroupedWeldRepeat(*this, Polygons, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldGlobalProvenanceTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.GlobalSourcePointKeyMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldGlobalProvenanceTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(6020, 300, 7000, 700), MakeGroupedWeldRectangle(1020, 200, 2000, 800),
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(5000, 0, 6000, 1000),
        MakeGroupedWeldRectangle(20000, 0, 21000, 1000)
    };
    const auto Original = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 2, 50.0, Changed, &Stats);
    TestEqual(TEXT("Both partial seams move their target corners"), Stats.MovedVertexCount, 4);
    TestEqual(TEXT("Both global reference slots gain two nodes"), Stats.InsertedVertexCount, 4);
    if (!TestEqual(TEXT("Provenance retains the entire original caller array"), Stats.SourcePointKeys.Num(), Original.Num())) { return false; }
    for (int32 I = 0; I < Polygons.Num(); ++I)
    {
        if (!TestEqual(TEXT("Every output point has provenance in the original polygon slot"), Stats.SourcePointKeys[I].Num(), Polygons[I].Num())) { continue; }
        for (const double Key : Stats.SourcePointKeys[I])
        {
            TestTrue(TEXT("Every point key refers to its original closed ring"), FMath::IsFinite(Key) && Key >= 0.0 && Key < Original[I].Num());
        }
        for (int32 Point = 0; Point < Original[I].Num(); ++Point)
        {
            TestTrue(TEXT("Retained points keep their original integer key"), Stats.SourcePointKeys[I].Contains(static_cast<double>(Point)));
        }
    }
    CheckGroupedWeldPointKey(*this, Polygons, Stats, 2, FVector2D(1000, 200), 1.2);
    CheckGroupedWeldPointKey(*this, Polygons, Stats, 2, FVector2D(1000, 800), 1.8);
    CheckGroupedWeldPointKey(*this, Polygons, Stats, 3, FVector2D(6000, 300), 1.3);
    CheckGroupedWeldPointKey(*this, Polygons, Stats, 3, FVector2D(6000, 700), 1.7);
    TestTrue(TEXT("Moved targets retain their own integer provenance instead of reference keys"),
        Stats.SourcePointKeys[0] == TArray<double>({0.0, 1.0, 2.0, 3.0}) &&
        Stats.SourcePointKeys[1] == TArray<double>({0.0, 1.0, 2.0, 3.0}));
    TestEqual(TEXT("Global movement statistic covers both components"), Stats.MaximumMoveCm, 20.0, 1.e-6);
    CheckGroupedWeldRepeat(*this, Polygons, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldNodedRepeatTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.NodedStepAndChainRepeatStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldNodedRepeatTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeGroupedWeldRectangle(1020, 0, 2000, 1000), MakeGroupedWeldRectangle(1005, 400, 1015, 600),
        MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(5000, 0, 6000, 1000),
        MakeGroupedWeldRectangle(6020, 300, 7000, 700), MakeGroupedWeldRectangle(7020, 300, 8000, 700)
    };
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 2, 50.0, Changed, &Stats);
    TestTrue(TEXT("Fixture performs real snapping and shared-node insertion"), Stats.MovedVertexCount > 0 && Stats.InsertedVertexCount > 0);
    TestEqual(TEXT("Both the stepped component and the independent chain are reported"), Stats.Groups.Num(), 2);
    TestEqual(TEXT("Valid stepped and chained proposals are accepted"), Stats.RejectedPolygonCount, 0);
    TestEqual(TEXT("All connected chain targets reach accepted anchors"), Stats.DisconnectedPolygonIndices.Num(), 0);
    TestTrue(TEXT("Step uses the frozen narrow neighbour's exposed boundary"),
        HasGroupedWeldEdge(Polygons[0], FVector2D(1015, 400), FVector2D(1015, 600)));
    TestTrue(TEXT("Chain follows the second accepted anchor rather than the distant group root"),
        HasGroupedWeldEdge(Polygons[5], FVector2D(7000, 300), FVector2D(7000, 700)));
    for (const TArray<FVector2D>& Ring : Polygons)
    {
        TestTrue(TEXT("Grouped stepped and chain output is simple and nondegenerate"), FFZAPolygonMath::IsSimplePolygon(Ring));
    }
    CheckGroupedWeldRepeat(*this, Polygons, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGroupedWeldInvalidReferenceTest,
    "FF_ZoneAuthoring.Unit.GroupedWeld.InvalidPreferredReferenceAndToleranceAbort",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGroupedWeldInvalidReferenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGroupedWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        {}, MakeGroupedWeldRectangle(0, 0, 1000, 1000), MakeGroupedWeldRectangle(1020, 0, 2000, 1000)
    };
    for (const int32 Reference : TArray<int32>{INDEX_NONE, 0, 3})
    {
        TArray<TArray<FVector2D>> Polygons = Original;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        TestEqual(TEXT("Invalid explicit reference aborts before automatic groups can mutate"),
            FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, Reference, 50.0, Changed, &Stats), 0);
        TestTrue(TEXT("Invalid preferred reference is reported"), Stats.bInvalidReference);
        TestTrue(TEXT("Invalid preferred reference leaves every ring unchanged"), Polygons == Original);
        TestEqual(TEXT("Invalid preferred reference creates no processing groups"), Stats.Groups.Num(), 0);
        TestEqual(TEXT("Invalid input is still reported exactly once on abort"), Stats.InvalidInputPolygonCount, 1);
        for (const bool bChanged : Changed) { TestFalse(TEXT("Aborted grouped weld clears changed flags"), bChanged); }
    }
    for (const double Tolerance : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        TArray<TArray<FVector2D>> Polygons = Original;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        TestEqual(TEXT("Invalid tolerance cannot trigger automatic welding"),
            FFZALayoutOps::WeldSelectedPolygonGroups(Polygons, 1, Tolerance, Changed, &Stats), 0);
        TestTrue(TEXT("Invalid tolerance preserves all inputs"), Polygons == Original);
        TestFalse(TEXT("Invalid tolerance is distinguished from an invalid manual reference"), Stats.bInvalidReference);
        TestEqual(TEXT("Invalid tolerance creates no processing groups"), Stats.Groups.Num(), 0);
    }
    return true;
}
#endif
