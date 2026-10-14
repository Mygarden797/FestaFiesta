// Reference-free shared-seam regressions. Pure arrays only: no editor world or level assets.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZAAutoWeld.h"
#include "FFZAPolygonMath.h"
#include "FFZASeamConsensus.h"
#include "FFZASplineAuthoring.h"

#include <limits>

namespace FFZASeamConsensusTestsPrivate
{
    TArray<FVector2D> MakeConsensusRectangle(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    bool HasConsensusPoint(const TArray<FVector2D>& Ring, const FVector2D& Point)
    {
        return Ring.ContainsByPredicate([&Point](const FVector2D& Candidate) { return Candidate.Equals(Point, 1.e-6); });
    }

    bool HasConsensusEdge(const TArray<FVector2D>& Ring, const FVector2D& A, const FVector2D& B)
    {
        for (int32 Index = 0; Index < Ring.Num(); ++Index)
        {
            const FVector2D& P = Ring[Index];
            const FVector2D& Q = Ring[(Index + 1) % Ring.Num()];
            if ((P.Equals(A, 1.e-6) && Q.Equals(B, 1.e-6)) || (P.Equals(B, 1.e-6) && Q.Equals(A, 1.e-6))) { return true; }
        }
        return false;
    }

    bool SameConsensusRing(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
    {
        if (A.Num() != B.Num()) { return false; }
        for (int32 Start = 0; Start < B.Num(); ++Start)
        {
            for (const int32 Direction : { -1, 1 })
            {
                bool bSame = true;
                for (int32 Index = 0; Index < A.Num(); ++Index)
                {
                    const int32 Other = (Start + Direction * Index + B.Num()) % B.Num();
                    if (!A[Index].Equals(B[Other], 1.e-6)) { bSame = false; break; }
                }
                if (bSame) { return true; }
            }
        }
        return false;
    }

    void CheckConsensusComplete(FAutomationTestBase& Test, const FFZAAutoWeld::FResult& Result, const int32 Count)
    {
        Test.TestTrue(TEXT("The whole reference-free result is writable"), Result.bCanCommit);
        Test.TestEqual(TEXT("Every selected input retains an output slot"), Result.Polygons.Num(), Count);
        Test.TestEqual(TEXT("Every selected input retains a changed flag"), Result.ChangedPolygons.Num(), Count);
        Test.TestEqual(TEXT("No exposed inspected gaps remain"), Result.Diagnostics.GapPairCount, 0);
        Test.TestEqual(TEXT("No positive-area overlaps remain"), Result.Diagnostics.OverlapPairCount, 0);
        Test.TestEqual(TEXT("No rejected zones are hidden by completion"), Result.Diagnostics.RejectedPolygonCount, 0);
        for (const TArray<FVector2D>& Ring : Result.Polygons)
        {
            Test.TestTrue(TEXT("Consensus keeps a simple closed polygon per zone"), FFZAPolygonMath::IsSimplePolygon(Ring));
            Test.TestTrue(TEXT("Consensus never collapses a zone"), FFZAPolygonMath::Area(Ring) > 1.e-6);
        }
    }

    void CheckConsensusOriginalBound(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Authored,
        const TArray<TArray<FVector2D>>& Repaired, const TArray<TArray<double>>& Keys, const double Radius)
    {
        if (!Test.TestEqual(TEXT("Provenance stays aligned with the original selection"), Keys.Num(), Authored.Num()) ||
            !Test.TestEqual(TEXT("Geometry stays aligned with the original selection"), Repaired.Num(), Authored.Num())) { return; }
        const double BoundSquared = FMath::Square(Radius + 1.e-4);
        for (int32 PolygonIndex = 0; PolygonIndex < Authored.Num(); ++PolygonIndex)
        {
            if (!Test.TestEqual(TEXT("Every result point has original-ring provenance"), Keys[PolygonIndex].Num(), Repaired[PolygonIndex].Num())) { continue; }
            for (int32 PointIndex = 0; PointIndex < Repaired[PolygonIndex].Num(); ++PointIndex)
            {
                const double Key = Keys[PolygonIndex][PointIndex];
                if (!Test.TestTrue(TEXT("Every original-ring key is finite and in range"),
                    FMath::IsFinite(Key) && Key >= 0.0 && Key < Authored[PolygonIndex].Num())) { continue; }
                const int32 Edge = FMath::FloorToInt(Key);
                const FVector2D Source = FMath::Lerp(Authored[PolygonIndex][Edge],
                    Authored[PolygonIndex][(Edge + 1) % Authored[PolygonIndex].Num()], Key - Edge);
                Test.TestTrue(TEXT("Repeated consensus passes never exceed the original displacement budget"),
                    FVector2D::DistSquared(Source, Repaired[PolygonIndex][PointIndex]) <= BoundSquared);
            }
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusMidpointTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.BothSidesUseSharedMidpointWithoutReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusMidpointTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1100, 0, 2100, 1000) };
    const auto Before = Authored;
    FFZASeamConsensus::FResult Consensus;
    if (!TestTrue(TEXT("Valid reference-free geometry is accepted"), FFZASeamConsensus::Build(Authored, 100.0, Consensus))) { return false; }
    TestTrue(TEXT("The shared-seam solver completes without a reference zone"), Consensus.bComplete);
    if (!TestEqual(TEXT("Both midpoint rings are returned"), Consensus.Polygons.Num(), 2)) { return false; }
    TestTrue(TEXT("Left zone moves its boundary 50cm to the midpoint"),
        HasConsensusEdge(Consensus.Polygons[0], FVector2D(1050, 0), FVector2D(1050, 1000)));
    TestTrue(TEXT("Right zone moves its boundary 50cm to the same midpoint"),
        HasConsensusEdge(Consensus.Polygons[1], FVector2D(1050, 0), FVector2D(1050, 1000)));
    for (const FFZALayoutOps::FWeldGroup& Group : Consensus.Groups)
    {
        TestEqual(TEXT("Consensus group has no fixed reference anchor"), Group.ReferencePolygonIndex, INDEX_NONE);
        TestFalse(TEXT("Consensus group is never a manual-reference group"), Group.bUsesManualReference);
    }
    CheckConsensusOriginalBound(*this, Authored, Consensus.Polygons, Consensus.SourcePointKeys, 100.0);
    FFZAAutoWeld::FResult Integrated;
    if (!TestTrue(TEXT("Reference-free automatic plan requires no reference argument"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Integrated))) { return false; }
    CheckConsensusComplete(*this, Integrated, 2);
    TestTrue(TEXT("Both sides become actual writable changes"), Integrated.ChangedPolygons[0] && Integrated.ChangedPolygons[1]);
    TestEqual(TEXT("Largest bilateral movement is 50cm"), Integrated.Stats.MaximumMoveCm, 50.0, 1.e-6);
    TestTrue(TEXT("Preview planning preserves all authored input"), Authored == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusTinyAndRangeTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.ClosesSubCentimeterGapAndHonorsFixedRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusTinyAndRangeTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Tiny = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1000.1, 0, 2000.1, 1000) };
    FFZAAutoWeld::FResult Result;
    if (TestTrue(TEXT("A 0.1cm gap is valid repair input"), FFZAAutoWeld::BuildAutomaticResult(Tiny, 50.0, Result)))
    {
        CheckConsensusComplete(*this, Result, 2);
        TestTrue(TEXT("Even a 0.1cm gap uses one exact shared boundary"),
            HasConsensusEdge(Result.Polygons[0], FVector2D(1000.05, 0), FVector2D(1000.05, 1000)) &&
            HasConsensusEdge(Result.Polygons[1], FVector2D(1000.05, 0), FVector2D(1000.05, 1000)));
    }
    const TArray<TArray<FVector2D>> Distant = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1101, 0, 2101, 1000) };
    TestTrue(TEXT("Beyond-range selected zones can still be inspected"), FFZAAutoWeld::BuildAutomaticResult(Distant, 50.0, Result));
    TestTrue(TEXT("Automatic consensus never bridges an authored 101cm seam using a 100cm radius"), Result.Polygons == Distant);
    TestEqual(TEXT("No out-of-range change is planned"), Result.Stats.ChangedPolygonCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusAllComponentsTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.ThirteenZonesSevenGapsAcrossFiveGroups",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusAllComponentsTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<double>> GapsByGroup = { { 100.0, 100.0, 0.0 }, { 100.0, 100.0 }, { 100.0 }, { 100.0 }, { 66.73 } };
    TArray<TArray<FVector2D>> Authored;
    for (int32 GroupIndex = 0; GroupIndex < GapsByGroup.Num(); ++GroupIndex)
    {
        double X = 0.0;
        const double Y = GroupIndex * 5000.0;
        Authored.Add(MakeConsensusRectangle(X, Y, X + 1000, Y + 1000));
        X += 1000;
        for (const double Gap : GapsByGroup[GroupIndex])
        {
            X += Gap;
            Authored.Add(MakeConsensusRectangle(X, Y, X + 1000, Y + 1000));
            X += 1000;
        }
    }
    FFZAWeldDiagnostics::FResult Initial;
    FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Initial);
    TestEqual(TEXT("Fixture reproduces all seven reported gaps"), Initial.GapPairCount, 7);
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Whole selection is planned without manually choosing any root"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return false; }
    CheckConsensusComplete(*this, Result, 13);
    CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
    TestEqual(TEXT("All five connected components are retained, including an already shared seam"), Result.Stats.Groups.Num(), 5);
    TestTrue(TEXT("An arbitrary first zone is not preserved as a hidden reference"), Result.ChangedPolygons[0]);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusTJunctionTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.LongEdgeAndTwoNeighboursShareOneTJunction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusTJunctionTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeConsensusRectangle(0, 0, 1000, 2000), MakeConsensusRectangle(1100, 0, 2100, 950), MakeConsensusRectangle(1100, 1050, 2100, 2000)
    };
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Long/short neighbouring boundaries can be planned"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return false; }
    CheckConsensusComplete(*this, Result, 3);
    CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
    for (const TArray<FVector2D>& Ring : Result.Polygons)
    {
        TestTrue(TEXT("All three rings use exactly the same T junction"), HasConsensusPoint(Ring, FVector2D(1050, 1000)));
    }
    TestTrue(TEXT("Short neighbours also share their formerly separated horizontal seam"),
        HasConsensusEdge(Result.Polygons[1], FVector2D(1050, 1000), FVector2D(2100, 1000)) &&
        HasConsensusEdge(Result.Polygons[2], FVector2D(1050, 1000), FVector2D(2100, 1000)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusFourWayTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.FourWayCornerClosesCentralHole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusFourWayTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1100, 0, 2100, 1000),
        MakeConsensusRectangle(0, 1100, 1000, 2100), MakeConsensusRectangle(1100, 1100, 2100, 2100)
    };
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("Four-way separated corner can be planned"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return false; }
    CheckConsensusComplete(*this, Result, 4);
    CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
    double FilledArea = 0.0;
    for (const TArray<FVector2D>& Ring : Result.Polygons)
    {
        TestTrue(TEXT("Every zone shares the single 1050,1050 junction coordinate"), HasConsensusPoint(Ring, FVector2D(1050, 1050)));
        FilledArea += FFZAPolygonMath::Area(Ring);
    }
    TestEqual(TEXT("The four rings fill the entire 2100cm square without a central hole"), FilledArea, 4410000.0, 1.e-6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusOccludedPairTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.NarrowMiddleZoneBlocksFalseOuterPair",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusOccludedPairTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1020, 0, 1040, 1000), MakeConsensusRectangle(1060, 0, 2060, 1000)
    };
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("A narrow intervening zone is valid automatic input"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return false; }
    CheckConsensusComplete(*this, Result, 3);
    CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
    TestTrue(TEXT("Left exposed 20cm gap uses its own midpoint"),
        HasConsensusEdge(Result.Polygons[0], FVector2D(1010, 0), FVector2D(1010, 1000)) &&
        HasConsensusEdge(Result.Polygons[1], FVector2D(1010, 0), FVector2D(1010, 1000)));
    TestTrue(TEXT("Right exposed 20cm gap uses its own midpoint"),
        HasConsensusEdge(Result.Polygons[1], FVector2D(1050, 0), FVector2D(1050, 1000)) &&
        HasConsensusEdge(Result.Polygons[2], FVector2D(1050, 0), FVector2D(1050, 1000)));
    TestEqual(TEXT("The 20cm middle zone survives with its legitimate two-sided extension"), FFZAPolygonMath::Area(Result.Polygons[1]), 40000.0, 1.e-6);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusBentSeamTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.BentBoundariesConvergeAcrossExpandedRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusBentSeamTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    for (const double Gap : { 55.0, 100.0 })
    {
        const TArray<TArray<FVector2D>> Authored = {
            { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 600), FVector2D(1200, 800), FVector2D(1200, 1600), FVector2D(0, 1600) },
            { FVector2D(1000 + Gap, 0), FVector2D(2200, 0), FVector2D(2200, 1600), FVector2D(1200 + Gap, 1600),
                FVector2D(1200 + Gap, 800), FVector2D(1000 + Gap, 600) }
        };
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Paired bent boundaries are valid automatic input"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { continue; }
        CheckConsensusComplete(*this, Result, 2);
        CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
        int32 SharedPointCount = 0;
        for (const FVector2D& Point : Result.Polygons[0]) { SharedPointCount += HasConsensusPoint(Result.Polygons[1], Point) ? 1 : 0; }
        TestTrue(TEXT("Both sides share endpoint and bend nodes along the entire seam"), SharedPointCount >= 4);
        TestTrue(TEXT("Exposed outer authored corners remain unchanged"),
            HasConsensusPoint(Result.Polygons[0], FVector2D(0, 0)) && HasConsensusPoint(Result.Polygons[0], FVector2D(0, 1600)) &&
            HasConsensusPoint(Result.Polygons[1], FVector2D(2200, 0)) && HasConsensusPoint(Result.Polygons[1], FVector2D(2200, 1600)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusOrderHeightsTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.SelectionOrderWindingClosingKeysAndHeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusOrderHeightsTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Identity = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(-1100, 250, -100, 750) };
    const TArray<TArray<int32>> Orders = { { 0, 1 }, { 1, 0 } };
    TArray<TArray<FVector2D>> Expected;
    for (const bool bReverse : { false, true })
    {
        for (const TArray<int32>& Order : Orders)
        {
            TArray<TArray<FVector2D>> Authored;
            for (const int32 PolygonIdentity : Order)
            {
                Authored.Add(Identity[PolygonIdentity]);
                if (bReverse) { Algo::Reverse(Authored.Last()); }
            }
            FFZAAutoWeld::FResult Result;
            if (!TestTrue(TEXT("Any input order and winding produces a valid plan"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { continue; }
            CheckConsensusComplete(*this, Result, 2);
            CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
            TArray<TArray<FVector2D>> ByIdentity;
            ByIdentity.SetNum(2);
            for (int32 Index = 0; Index < Order.Num(); ++Index) { ByIdentity[Order[Index]] = Result.Polygons[Index]; }
            if (Expected.IsEmpty()) { Expected = ByIdentity; }
            else
            {
                TestTrue(TEXT("Geometry is independent of input index and authored winding"),
                    SameConsensusRing(ByIdentity[0], Expected[0]) && SameConsensusRing(ByIdentity[1], Expected[1]));
            }
            const int32 LongRingIndex = Order.Find(0);
            const TArray<FVector2D>& LongRing = Result.Polygons[LongRingIndex];
            const TArray<double>& Keys = Result.Stats.SourcePointKeys[LongRingIndex];
            for (const double Y : { 250.0, 750.0 })
            {
                const int32 PointIndex = LongRing.IndexOfByPredicate([Y](const FVector2D& Point) { return Point.Equals(FVector2D(-50, Y), 1.e-6); });
                if (TestTrue(TEXT("Partial shared midpoint includes its closing-edge endpoint"), PointIndex != INDEX_NONE))
                {
                    const double ExpectedKey = bReverse ? (Y == 250.0 ? 3.25 : 3.75) : (Y == 250.0 ? 3.75 : 3.25);
                    TestEqual(TEXT("Closing-edge provenance follows the actual original winding"), Keys[PointIndex], ExpectedKey, 1.e-6);
                }
            }
            TArray<double> Heights;
            for (const FVector2D& Point : Authored[LongRingIndex]) { Heights.Add(10.0 + 40.0 * Point.Y / 1000.0); }
            TArray<FVector> WorldPoints;
            if (TestTrue(TEXT("Reference-free inserted nodes support pure original height reconstruction"),
                FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights, LongRing, Keys, WorldPoints)))
            {
                for (const FVector& Point : WorldPoints)
                {
                    if (FVector2D(Point.X, Point.Y).Equals(FVector2D(-50, 250), 1.e-6)) { TestEqual(TEXT("Lower closing-edge node preserves original height interpolation"), Point.Z, 20.0, 1.e-6); }
                    if (FVector2D(Point.X, Point.Y).Equals(FVector2D(-50, 750), 1.e-6)) { TestEqual(TEXT("Upper closing-edge node preserves original height interpolation"), Point.Z, 40.0, 1.e-6); }
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusInvalidGateTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.InvalidInputAndSettingsBlockWholePlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusInvalidGateTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeConsensusRectangle(0, 0, 1000, 1000),
        { FVector2D(5000, 0), FVector2D(6000, 1000), FVector2D(5000, 1000), FVector2D(6000, 0) },
        MakeConsensusRectangle(1100, 0, 2100, 1000)
    };
    const auto Before = Authored;
    FFZASeamConsensus::FResult Consensus;
    TestFalse(TEXT("Invalid selected ring is not silently excluded from consensus completion"), FFZASeamConsensus::Build(Authored, 100.0, Consensus));
    TestFalse(TEXT("Invalid input cannot report complete consensus"), Consensus.bComplete);
    TestTrue(TEXT("Invalid original caller index is retained"), Consensus.RejectedPolygonIndices.Contains(1));
    FFZAAutoWeld::FResult Result;
    FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result);
    TestFalse(TEXT("One invalid selected zone blocks all automatic writes"), Result.bCanCommit);
    TestTrue(TEXT("Invalid selection is explained"), !Result.FailureReason.IsEmpty());
    TestTrue(TEXT("An invalid analysis never mutates any authored input"), Authored == Before);
    for (const double BadTolerance : { 0.0, -50.0 })
    {
        FFZAAutoWeld::BuildAutomaticResult({ MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1100, 0, 2100, 1000) }, BadTolerance, Result);
        TestFalse(TEXT("Invalid automatic radius can never authorize a write"), Result.bCanCommit);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusOverlapTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.DeepOverlapRepairsWithoutManualRoot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Identity = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(500, 100, 1500, 900) };
    TArray<TArray<FVector2D>> Expected;
    for (const TArray<int32>& Order : TArray<TArray<int32>>{ { 0, 1 }, { 1, 0 } })
    {
        TArray<TArray<FVector2D>> Authored;
        for (const int32 PolygonIdentity : Order) { Authored.Add(Identity[PolygonIdentity]); }
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Deep overlap needs no manually selected reference"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { continue; }
        CheckConsensusComplete(*this, Result, 2);
        TArray<TArray<FVector2D>> ByIdentity;
        ByIdentity.SetNum(2);
        for (int32 Index = 0; Index < Order.Num(); ++Index) { ByIdentity[Order[Index]] = Result.Polygons[Index]; }
        TestEqual(TEXT("Automatic ownership keeps the larger zone area"), FFZAPolygonMath::Area(ByIdentity[0]), 1000000.0, 1.e-6);
        TestEqual(TEXT("Automatic ownership removes only the 500cm-deep covered target area"), FFZAPolygonMath::Area(ByIdentity[1]), 400000.0, 1.e-6);
        if (Expected.IsEmpty()) { Expected = ByIdentity; }
        else { TestTrue(TEXT("Automatic overlap ownership is independent of input ordering"), SameConsensusRing(ByIdentity[0], Expected[0]) && SameConsensusRing(ByIdentity[1], Expected[1])); }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusAtomicTopologyTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.UnsupportedOverlapBlocksIndependentSuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusAtomicTopologyTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(400, 400, 600, 600),
        MakeConsensusRectangle(5000, 0, 6000, 1000), MakeConsensusRectangle(6100, 0, 7100, 1000)
    };
    const auto Before = Authored;
    FFZAAutoWeld::FResult Result;
    FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result);
    TestFalse(TEXT("Contained-zone deletion cannot authorize a partial world-zone apply"), Result.bCanCommit);
    TestTrue(TEXT("Whole-plan refusal explains the unsupported topology"), !Result.FailureReason.IsEmpty());
    TestEqual(TEXT("Every input zone remains represented for read-only review"), Result.Polygons.Num(), Authored.Num());
    for (const TArray<FVector2D>& Ring : Result.Polygons) { TestTrue(TEXT("Refused automatic plan never drops a selected zone ring"), Ring.Num() >= 3); }
    TestTrue(TEXT("An independent solvable gap does not mutate original input on global refusal"), Authored == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusIdempotenceTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.RepeatedApplyDoesNotGrowSeamPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusIdempotenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<TArray<FVector2D>>> Cases = {
        { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1100, 0, 2100, 1000) },
        { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1100, 0, 2100, 1000),
          MakeConsensusRectangle(0, 1100, 1000, 2100), MakeConsensusRectangle(1100, 1100, 2100, 2100) }
    };
    for (const TArray<TArray<FVector2D>>& Authored : Cases)
    {
        FFZAAutoWeld::FResult First;
        if (!TestTrue(TEXT("Initial seam plan is valid"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, First))) { continue; }
        CheckConsensusComplete(*this, First, Authored.Num());
        FFZAAutoWeld::FResult Second;
        if (!TestTrue(TEXT("Completed seams can be analyzed a second time"), FFZAAutoWeld::BuildAutomaticResult(First.Polygons, 50.0, Second))) { continue; }
        CheckConsensusComplete(*this, Second, Authored.Num());
        TestTrue(TEXT("Second apply preserves all coordinates, point order and point counts"), Second.Polygons == First.Polygons);
        TestEqual(TEXT("Completed geometry has no pending writes"), Second.Stats.ChangedPolygonCount, 0);
        TestEqual(TEXT("Completed geometry inserts no additional seam points"), Second.Stats.InsertedVertexCount, 0);
        TestEqual(TEXT("Completed geometry has no additional movement"), Second.Stats.MaximumMoveCm, 0.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusDisplayBudgetTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.ProcessesAllPairsBeyondIssueDisplayBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusDisplayBudgetTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    constexpr int32 PairCount = 600;
    TArray<TArray<FVector2D>> Authored;
    Authored.Reserve(PairCount * 2);
    for (int32 PairIndex = 0; PairIndex < PairCount; ++PairIndex)
    {
        const double Y = PairIndex * 3000.0;
        Authored.Add(MakeConsensusRectangle(0, Y, 1000, Y + 1000));
        Authored.Add(MakeConsensusRectangle(1100, Y, 2100, Y + 1000));
    }
    FFZAWeldDiagnostics::FResult Initial;
    FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Initial, 512);
    TestEqual(TEXT("Initial diagnostic counts every gap even beyond displayed spans"), Initial.GapPairCount, PairCount);
    TestTrue(TEXT("Fixture exceeds the actual UI issue display budget"), Initial.bTruncated);
    FFZAAutoWeld::FResult Result;
    if (!TestTrue(TEXT("All loaded-style zone pairs can be analyzed beyond the display budget"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return false; }
    CheckConsensusComplete(*this, Result, PairCount * 2);
    CheckConsensusOriginalBound(*this, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
    TestEqual(TEXT("Every pair is processed, including the last pair after 512 spans"), Result.Stats.ChangedPolygonCount, PairCount * 2);
    TestTrue(TEXT("Last pair also shares its automatic midpoint"),
        HasConsensusEdge(Result.Polygons[PairCount * 2 - 2], FVector2D(1050, (PairCount - 1) * 3000.0), FVector2D(1050, (PairCount - 1) * 3000.0 + 1000)) &&
        HasConsensusEdge(Result.Polygons[PairCount * 2 - 1], FVector2D(1050, (PairCount - 1) * 3000.0), FVector2D(1050, (PairCount - 1) * 3000.0 + 1000)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusCumulativeOriginsTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.PreviousStageOriginsPreventAccumulatedMovement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusCumulativeOriginsTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Origins = {
        MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1300, 0, 2300, 1000)
    };
    // A previous stage has already used each side's 100cm displacement allowance.
    // The remaining current gap is only 100cm, but the original movement disks do not intersect.
    const TArray<TArray<FVector2D>> Current = {
        MakeConsensusRectangle(0, 0, 1100, 1000), MakeConsensusRectangle(1200, 0, 2300, 1000)
    };
    const auto Before = Current;
    FFZASeamConsensus::FResult Unconstrained;
    if (TestTrue(TEXT("Current-stage rings alone form valid consensus input"), FFZASeamConsensus::Build(Current, 100.0, Unconstrained)))
    {
        TestTrue(TEXT("A fresh current-stage budget alone could close the remaining gap"), Unconstrained.bComplete);
        if (TestEqual(TEXT("Fresh-stage plan preserves both zones"), Unconstrained.Polygons.Num(), 2))
        {
            TestTrue(TEXT("The fresh-stage midpoint would move beyond the original allowance"),
                HasConsensusEdge(Unconstrained.Polygons[0], FVector2D(1150, 0), FVector2D(1150, 1000)));
        }
    }
    FFZASeamConsensus::FResult Constrained;
    TestTrue(TEXT("Valid parallel movement origins can be analyzed"), FFZASeamConsensus::Build(Current, 100.0, Constrained, &Origins));
    TestFalse(TEXT("A later stage cannot silently reset an already used displacement budget"), Constrained.bComplete);
    TestTrue(TEXT("Unreachable original movement disks explain the read-only refusal"), !Constrained.FailureReason.IsEmpty());
    CheckConsensusOriginalBound(*this, Origins, Constrained.Polygons, Constrained.SourcePointKeys, 100.0);
    TestTrue(TEXT("Constrained planning preserves every current-stage authored coordinate"), Current == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusInvalidOriginsTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.ValidatesParallelFiniteMovementOrigins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusInvalidOriginsTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    const TArray<TArray<FVector2D>> Current = { MakeConsensusRectangle(0, 0, 1100, 1000), MakeConsensusRectangle(1200, 0, 2300, 1000) };
    const TArray<TArray<FVector2D>> ValidOrigins = { MakeConsensusRectangle(0, 0, 1000, 1000), MakeConsensusRectangle(1300, 0, 2300, 1000) };
    TArray<TArray<TArray<FVector2D>>> InvalidCases;
    TArray<TArray<FVector2D>> MissingPolygon = ValidOrigins;
    MissingPolygon.Pop();
    InvalidCases.Add(MoveTemp(MissingPolygon));
    TArray<TArray<FVector2D>> MissingPoint = ValidOrigins;
    MissingPoint[1].Pop();
    InvalidCases.Add(MoveTemp(MissingPoint));
    TArray<TArray<FVector2D>> Nonfinite = ValidOrigins;
    Nonfinite[0][0].X = std::numeric_limits<double>::infinity();
    InvalidCases.Add(MoveTemp(Nonfinite));
    TArray<TArray<FVector2D>> NonfiniteY = ValidOrigins;
    NonfiniteY[1][2].Y = std::numeric_limits<double>::quiet_NaN();
    InvalidCases.Add(MoveTemp(NonfiniteY));
    TArray<TArray<FVector2D>> AlreadyOutside = ValidOrigins;
    AlreadyOutside[0][1].X = 990.0;
    InvalidCases.Add(MoveTemp(AlreadyOutside));
    for (const TArray<TArray<FVector2D>>& Origins : InvalidCases)
    {
        FFZASeamConsensus::FResult Result;
        TestFalse(TEXT("Malformed, nonfinite or already exceeded movement origins cannot be ignored"),
            FFZASeamConsensus::Build(Current, 100.0, Result, &Origins));
        TestFalse(TEXT("Invalid movement constraints never produce a completed plan"), Result.bComplete);
        TestTrue(TEXT("Invalid movement constraints provide a failure explanation"), !Result.FailureReason.IsEmpty());
        TestTrue(TEXT("Constraint preflight refuses before producing any mutated output ring"), Result.Polygons == Current);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAConsensusEqualMinXPermutationTest,
    "FF_ZoneAuthoring.Unit.SeamConsensus.AngledEqualMinXDiagnosticsAndPlanAreOrderIndependent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAConsensusEqualMinXPermutationTest::RunTest(const FString& Parameters)
{
    using namespace FFZASeamConsensusTestsPrivate;
    // Both bounds begin at X=0. Using caller indices to break that sweep tie reverses the
    // projection direction of these angled spans and changes which part is considered in range.
    const TArray<TArray<FVector2D>> Identity = {
        MakeConsensusRectangle(0, 0, 1000, 1000),
        { FVector2D(0, 1080), FVector2D(1000, 1660), FVector2D(1000, 2660), FVector2D(0, 2080) }
    };
    const TArray<TArray<int32>> Orders = { { 0, 1 }, { 1, 0 } };
    TArray<FFZAWeldDiagnostics::FBoundaryIssue> ExpectedSpans;
    TArray<TArray<FVector2D>> ExpectedGeometry;
    TArray<bool> ExpectedChanged;
    bool bExpectedCommit = false;
    for (const TArray<int32>& Order : Orders)
    {
        TArray<TArray<FVector2D>> Authored;
        for (const int32 PolygonIdentity : Order) { Authored.Add(Identity[PolygonIdentity]); }
        FFZAWeldDiagnostics::FResult Before;
        FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Before);
        TestEqual(TEXT("Both input orders detect the same positive angled gap pair"), Before.GapPairCount, 1);
        TArray<FFZAWeldDiagnostics::FBoundaryIssue> Spans;
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Before.Issues)
        {
            if (Issue.Type != FFZAWeldDiagnostics::EIssueType::Gap) { continue; }
            TestEqual(TEXT("Equal-X sweep tie chooses the same geometric owner instead of caller order"), Order[Issue.PolygonIndexA], 0);
            TestEqual(TEXT("Partner identity also remains stable across sweep ties"), Order[Issue.PolygonIndexB], 1);
            Spans.Add(Issue);
        }
        TestTrue(TEXT("A finite angled partial span is available for comparison"), !Spans.IsEmpty());
        if (ExpectedSpans.IsEmpty()) { ExpectedSpans = Spans; }
        else
        {
            TestEqual(TEXT("Sweep tie permutation keeps the same number of exposed spans"), Spans.Num(), ExpectedSpans.Num());
            for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Spans)
            {
                const bool bSameSpan = ExpectedSpans.ContainsByPredicate([&Issue](const FFZAWeldDiagnostics::FBoundaryIssue& Expected)
                {
                    const bool bForward = Issue.StartA.Equals(Expected.StartA, 1.e-6) && Issue.EndA.Equals(Expected.EndA, 1.e-6) &&
                        Issue.StartB.Equals(Expected.StartB, 1.e-6) && Issue.EndB.Equals(Expected.EndB, 1.e-6);
                    const bool bReverse = Issue.StartA.Equals(Expected.EndA, 1.e-6) && Issue.EndA.Equals(Expected.StartA, 1.e-6) &&
                        Issue.StartB.Equals(Expected.EndB, 1.e-6) && Issue.EndB.Equals(Expected.StartB, 1.e-6);
                    return (bForward || bReverse) && FMath::IsNearlyEqual(Issue.MaximumGapCm, Expected.MaximumGapCm, 1.e-6);
                });
                TestTrue(TEXT("The inspected boundary endpoints do not change with selection order"), bSameSpan);
            }
        }
        FFZAAutoWeld::FResult Result;
        if (!TestTrue(TEXT("Equal-X angled inputs produce a valid automatic analysis"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { continue; }
        if (!TestEqual(TEXT("Automatic result preserves both input slots"), Result.Polygons.Num(), 2) ||
            !TestEqual(TEXT("Automatic result preserves both changed flags"), Result.ChangedPolygons.Num(), 2)) { continue; }
        TArray<TArray<FVector2D>> ByIdentity;
        TArray<bool> ChangedByIdentity;
        ByIdentity.SetNum(2);
        ChangedByIdentity.Init(false, 2);
        for (int32 Index = 0; Index < Order.Num(); ++Index)
        {
            ByIdentity[Order[Index]] = Result.Polygons[Index];
            ChangedByIdentity[Order[Index]] = Result.ChangedPolygons[Index];
        }
        if (ExpectedGeometry.IsEmpty())
        {
            ExpectedGeometry = ByIdentity;
            ExpectedChanged = ChangedByIdentity;
            bExpectedCommit = Result.bCanCommit;
        }
        else
        {
            TestEqual(TEXT("Input order cannot change the automatic commit decision"), Result.bCanCommit, bExpectedCommit);
            TestTrue(TEXT("Input order cannot change the automatic seam proposal"),
                SameConsensusRing(ByIdentity[0], ExpectedGeometry[0]) && SameConsensusRing(ByIdentity[1], ExpectedGeometry[1]));
            TestTrue(TEXT("Input order cannot change which zones would be written"), ChangedByIdentity == ExpectedChanged);
        }
        if (Result.bCanCommit) { CheckConsensusComplete(*this, Result, 2); }
        else { TestTrue(TEXT("An incomplete angled proposal stays read-only with an explanation"), !Result.FailureReason.IsEmpty()); }
    }
    return true;
}

#endif
