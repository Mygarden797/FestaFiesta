// Area-based gap repair regressions. Pure geometry arrays: no level or asset writes.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZAAutoWeld.h"
#include "FFZAGapAreaRepair.h"
#include "FFZAPolygonMath.h"
#include "FFZASeamConsensus.h"
#include "FFZAWeldDiagnostics.h"

namespace FFZAGapAreaRepairTestsPrivate
{
    TArray<FVector2D> GapAreaRectangle(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    TArray<TArray<FVector2D>> ThinTongueFixture()
    {
        // A 20cm tongue approaches to 5cm while its neighbouring long seams are 100cm apart.
        // A corner-movement solution must retain both ends of the tongue and their winding.
        return {
            GapAreaRectangle(0, 0, 1000, 1000),
            { FVector2D(1100, 0), FVector2D(2100, 0), FVector2D(2100, 1000), FVector2D(1100, 1000),
              FVector2D(1100, 520), FVector2D(1005, 520), FVector2D(1005, 500), FVector2D(1100, 500) }
        };
    }

    double StableGapArea(const TArray<FVector2D>& Ring)
    {
        if (Ring.Num() < 3) { return 0.0; }
        TArray<FVector2D> Local;
        for (const FVector2D& Point : Ring) { Local.Add(Point - Ring[0]); }
        return FFZAPolygonMath::Area(Local);
    }

    bool SameGapAreaShape(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
    {
        if (A.Num() != B.Num()) { return false; }
        for (int32 Start = 0; Start < B.Num(); ++Start)
        {
            for (const int32 Direction : { -1, 1 })
            {
                bool bSame = true;
                for (int32 Point = 0; Point < A.Num(); ++Point)
                {
                    const int32 Other = (Start + Direction * Point + B.Num()) % B.Num();
                    if (!A[Point].Equals(B[Other], 1.e-4)) { bSame = false; break; }
                }
                if (bSame) { return true; }
            }
        }
        return false;
    }

    void DumpGapAreaRings(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Rings, const TCHAR* Stage)
    {
        for (int32 Polygon = 0; Polygon < Rings.Num(); ++Polygon)
        {
            FString Coordinates;
            for (const FVector2D& Point : Rings[Polygon])
            {
                Coordinates += FString::Printf(TEXT("(%.17g,%.17g),"), Point.X, Point.Y);
            }
            Test.AddInfo(FString::Printf(TEXT("%s ring %d [%d]: %s"), Stage, Polygon, Rings[Polygon].Num(), *Coordinates));
        }
    }

    void CheckGapAreaResult(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Authored,
        const TArray<TArray<FVector2D>>& Repaired, const TArray<TArray<double>>& Keys, const double Radius)
    {
        if (!Test.TestEqual(TEXT("All Zone identities retain an output ring"), Repaired.Num(), Authored.Num()) ||
            !Test.TestEqual(TEXT("All Zone identities retain original height provenance"), Keys.Num(), Authored.Num())) { return; }
        for (int32 Polygon = 0; Polygon < Repaired.Num(); ++Polygon)
        {
            const TArray<FVector2D>& Ring = Repaired[Polygon];
            if (!Test.TestTrue(TEXT("Every output retains at least three boundary vertices"), Ring.Num() >= 3)) { continue; }
            TArray<FVector2D> Local;
            for (const FVector2D& Point : Ring) { Local.Add(Point - Ring[0]); }
            Test.TestTrue(TEXT("Area repair never deletes or collapses a Zone"), StableGapArea(Ring) > 1.e-4);
            if (!Test.TestTrue(TEXT("Every output remains one simple closed ring"), FFZAPolygonMath::IsSimplePolygon(Local)))
            {
                Test.AddInfo(FString::Printf(TEXT("Ring %d topology check: 0.01cm=%d 0.0001cm=%d"), Polygon,
                    FFZAPolygonMath::IsSimplePolygon(Local, 0.01), FFZAPolygonMath::IsSimplePolygon(Local, 1.e-4)));
                DumpGapAreaRings(Test, { Ring }, TEXT("Failed simple-ring output"));
            }
            if (!Test.TestEqual(TEXT("Every output vertex has a source height key"), Keys[Polygon].Num(), Ring.Num())) { continue; }
            for (int32 Point = 0; Point < Ring.Num(); ++Point)
            {
                const double Key = Keys[Polygon][Point];
                if (!Test.TestTrue(TEXT("Source keys remain finite and in the original cyclic ring"),
                    FMath::IsFinite(Key) && Key >= 0.0 && Key < Authored[Polygon].Num())) { continue; }
                const int32 Edge = FMath::FloorToInt(Key);
                const FVector2D Source = FMath::Lerp(Authored[Polygon][Edge],
                    Authored[Polygon][(Edge + 1) % Authored[Polygon].Num()], Key - Edge);
                Test.TestTrue(TEXT("Added gap boundaries remain inside the original repair radius"),
                    FVector2D::DistSquared(Source, Ring[Point]) <= FMath::Square(Radius + 1.e-3));
            }
        }
        FFZAWeldDiagnostics::FResult Final;
        FFZAWeldDiagnostics::Analyze(Repaired, {}, Radius, Final);
        Test.TestEqual(TEXT("Independent final inspection finds no exposed gap"), Final.GapPairCount, 0);
        Test.TestEqual(TEXT("Independent final inspection finds no positive overlap"), Final.OverlapPairCount, 0);
    }

    bool CheckDirectGapArea(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Authored,
        FFZAGapAreaRepair::FResult& Result)
    {
        if (!Test.TestTrue(TEXT("Valid gap-only geometry can be analyzed"), FFZAGapAreaRepair::Build(Authored, 100.0, Result))) { return false; }
        Test.TestTrue(*FString::Printf(TEXT("Area repair completes: %s"), *Result.FailureReason), Result.bComplete);
        Test.TestFalse(TEXT("Small authored fixtures never hit a processing budget"), Result.bBlocked);
        if (!Result.bComplete)
        {
            DumpGapAreaRings(Test, Result.Polygons, TEXT("Incomplete direct area output"));
            FFZAWeldDiagnostics::FResult Remaining;
            FFZAWeldDiagnostics::Analyze(Result.Polygons, {}, 100.0, Remaining);
            for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Remaining.Issues)
            {
                if (Issue.Type != FFZAWeldDiagnostics::EIssueType::Gap) { continue; }
                Test.AddInfo(FString::Printf(TEXT("Direct remaining gap %d/%d: A(%.9f,%.9f)-(%.9f,%.9f) B(%.9f,%.9f)-(%.9f,%.9f) max=%.9f"),
                    Issue.PolygonIndexA, Issue.PolygonIndexB, Issue.StartA.X, Issue.StartA.Y, Issue.EndA.X, Issue.EndA.Y,
                    Issue.StartB.X, Issue.StartB.Y, Issue.EndB.X, Issue.EndB.Y, Issue.MaximumGapCm));
            }
        }
        CheckGapAreaResult(Test, Authored, Result.Polygons, Result.SourcePointKeys, 100.0);
        for (int32 Polygon = 0; Polygon < Authored.Num() && Polygon < Result.Polygons.Num(); ++Polygon)
        {
            Test.TestTrue(TEXT("Gap-only area filling never removes authored Zone area"),
                StableGapArea(Result.Polygons[Polygon]) + 1.e-3 >= StableGapArea(Authored[Polygon]));
            for (const FVector2D& Corner : Authored[Polygon])
            {
                Test.TestTrue(TEXT("Area fallback preserves every authored corner inside its own Zone"),
                    FFZAPolygonMath::IsPointInside(Result.Polygons[Polygon], Corner, 1.e-3));
            }
        }
        return Result.bComplete;
    }

    void CheckIntegratedGapArea(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Authored)
    {
        FFZAAutoWeld::FResult Result;
        if (!Test.TestTrue(TEXT("Automatic repair produces a preview"), FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result))) { return; }
        Test.TestTrue(*FString::Printf(TEXT("Automatic apply is complete: %s"), *Result.FailureReason), Result.bCanCommit);
        CheckGapAreaResult(Test, Authored, Result.Polygons, Result.Stats.SourcePointKeys, 100.0);
        Test.TestEqual(TEXT("Completed area fallback has no rejected Zone"), Result.Stats.RejectedPolygonCount, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaThinTongueTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.ThinTongueKeepsItsCornersBetweenLongGaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaThinTongueTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = ThinTongueFixture();
    const auto Before = Authored;
    FFZASeamConsensus::FResult Consensus;
    if (FFZASeamConsensus::Build(Authored, 100.0, Consensus) && !Consensus.bComplete)
    {
        AddInfo(FString::Printf(TEXT("Corner consensus cannot finish this fixture: %s"), *Consensus.FailureReason));
    }
    FFZAGapAreaRepair::FResult Result;
    CheckDirectGapArea(*this, Authored, Result);
    CheckIntegratedGapArea(*this, Authored);
    TestTrue(TEXT("Preview calculations do not modify any authored input"), Authored == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaConcaveRecessTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.ConcaveRecessConnectsInnerNeighbourWithoutSelfIntersection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaConcaveRecessTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 400), FVector2D(900, 400),
          FVector2D(900, 600), FVector2D(1000, 600), FVector2D(1000, 1000), FVector2D(0, 1000) },
        GapAreaRectangle(1000, 450, 2000, 550)
    };
    FFZAWeldDiagnostics::FResult Before;
    FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Before);
    TestTrue(TEXT("The concave fixture begins with an actual exposed inner-boundary gap"), Before.GapPairCount > 0);
    TestEqual(TEXT("The recess neighbour never overlaps the authored C-shaped Zone"), Before.OverlapPairCount, 0);
    FFZAGapAreaRepair::FResult Result;
    CheckDirectGapArea(*this, Authored, Result);
    CheckIntegratedGapArea(*this, Authored);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaRepeatedEdgeTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.ThreeNeighboursAndTwoJunctionsOnOneLongEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaRepeatedEdgeTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        GapAreaRectangle(0, 0, 1000, 3000), GapAreaRectangle(1100, 0, 2100, 950),
        GapAreaRectangle(1100, 1050, 2100, 1950), GapAreaRectangle(1100, 2050, 2100, 3000)
    };
    FFZAGapAreaRepair::FResult Result;
    CheckDirectGapArea(*this, Authored, Result);
    CheckIntegratedGapArea(*this, Authored);
    if (Result.bComplete)
    {
        double Area = 0.0;
        for (const TArray<FVector2D>& Ring : Result.Polygons) { Area += StableGapArea(Ring); }
        TestEqual(TEXT("Both junction holes are filled together with all three vertical gaps"), Area, 6300000.0, 1.e-3);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaNarrowForkTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.NarrowMiddleZoneRetainsIdentityAtTwoSharedJunctions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaNarrowForkTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        GapAreaRectangle(0, 0, 1000, 2000), GapAreaRectangle(1100, 0, 2100, 950),
        GapAreaRectangle(1100, 970, 2100, 990), GapAreaRectangle(1100, 1010, 2100, 2000)
    };
    FFZAGapAreaRepair::FResult Result;
    CheckDirectGapArea(*this, Authored, Result);
    CheckIntegratedGapArea(*this, Authored);
    if (Result.Polygons.IsValidIndex(2))
    {
        TestTrue(TEXT("The original 20cm Zone is not absorbed by an adjacent owner"),
            FFZAPolygonMath::IsPointInside(Result.Polygons[2], FVector2D(1500, 980), 1.e-3));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaPermutationTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.ShapeOwnershipIgnoresOrderWindingAndCyclicStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaPermutationTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Identity = ThinTongueFixture();
    FFZAGapAreaRepair::FResult Baseline;
    if (!CheckDirectGapArea(*this, Identity, Baseline)) { return false; }
    TArray<TArray<FVector2D>> Permuted = { Identity[1], Identity[0] };
    for (TArray<FVector2D>& Ring : Permuted)
    {
        Algo::Reverse(Ring);
        const FVector2D First = Ring[0];
        Ring.RemoveAt(0);
        Ring.Add(First);
    }
    const auto Before = Permuted;
    FFZAGapAreaRepair::FResult Result;
    if (CheckDirectGapArea(*this, Permuted, Result))
    {
        TestTrue(TEXT("The first Zone's repaired shape is independent of collection order and spline direction"),
            SameGapAreaShape(Baseline.Polygons[0], Result.Polygons[1]));
        TestTrue(TEXT("The second Zone's repaired shape is independent of collection order and spline direction"),
            SameGapAreaShape(Baseline.Polygons[1], Result.Polygons[0]));
    }
    TestTrue(TEXT("Cyclic provenance calculations preserve original input order"), Permuted == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaLargeCoordinatesTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.TranslatedLargeWorldCoordinatesPreserveThinFeatures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaLargeCoordinatesTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Identity = ThinTongueFixture();
    FFZAGapAreaRepair::FResult Baseline;
    if (!CheckDirectGapArea(*this, Identity, Baseline)) { return false; }
    TArray<TArray<FVector2D>> Authored = Identity;
    const FVector2D Translation(1.e9, -1.e9);
    for (TArray<FVector2D>& Ring : Authored) { for (FVector2D& Point : Ring) { Point += Translation; } }
    FFZAGapAreaRepair::FResult Result;
    if (CheckDirectGapArea(*this, Authored, Result))
    {
        for (int32 Polygon = 0; Polygon < Result.Polygons.Num(); ++Polygon)
        {
            for (FVector2D& Point : Result.Polygons[Polygon]) { Point -= Translation; }
            TestTrue(TEXT("Large world translation does not change ownership or gap closure"),
                SameGapAreaShape(Baseline.Polygons[Polygon], Result.Polygons[Polygon]));
        }
    }
    CheckIntegratedGapArea(*this, Authored);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaIdempotenceTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.ReapplyingCompletedComplexSeamsCreatesNoAdditionalPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaIdempotenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = ThinTongueFixture();
    FFZAGapAreaRepair::FResult First;
    if (!CheckDirectGapArea(*this, Authored, First)) { return false; }
    FFZAGapAreaRepair::FResult Second;
    if (CheckDirectGapArea(*this, First.Polygons, Second))
    {
        TestTrue(TEXT("A second area fill preserves all point counts, order and coordinates exactly"), Second.Polygons == First.Polygons);
    }
    FFZAAutoWeld::FResult Applied;
    if (TestTrue(TEXT("A completed area-fill result can be passed to the public automatic planner"),
        FFZAAutoWeld::BuildAutomaticResult(First.Polygons, 50.0, Applied)))
    {
        TestTrue(TEXT("An already connected area-fill result remains applicable"), Applied.bCanCommit);
        TestEqual(TEXT("An already connected area-fill result produces zero writes"), Applied.Stats.ChangedPolygonCount, 0);
        TestTrue(TEXT("Automatic reapply preserves the area-fill geometry exactly"), Applied.Polygons == First.Polygons);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaInvalidAndUnsafeTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.InvalidInputAndUnrepresentableOverlapStayReadOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaInvalidAndUnsafeTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    TArray<TArray<FVector2D>> Invalid = ThinTongueFixture();
    Invalid[0] = { FVector2D(0, 0), FVector2D(1000, 1000), FVector2D(0, 1000), FVector2D(1000, 0) };
    const auto InvalidBefore = Invalid;
    FFZAGapAreaRepair::FResult InvalidResult;
    TestFalse(TEXT("A self-intersecting authored input is not silently repaired into a different Zone"),
        FFZAGapAreaRepair::Build(Invalid, 100.0, InvalidResult));
    TestFalse(TEXT("Invalid authored input never yields a completed writable plan"), InvalidResult.bComplete);
    TestTrue(TEXT("Invalid authored input is preserved for diagnosis"), Invalid == InvalidBefore);
    TestTrue(TEXT("Invalid authored input has an explicit explanation"), !InvalidResult.FailureReason.IsEmpty());

    const TArray<TArray<TArray<FVector2D>>> UnsafeCases = {
        // Difference would delete the inner Zone or introduce a hole in the outer Zone.
        { GapAreaRectangle(0, 0, 2000, 2000), GapAreaRectangle(500, 500, 1500, 1500) },
        // Subtracting the narrow crossing Zone splits the large Zone into two islands.
        { GapAreaRectangle(0, 0, 2000, 2000), GapAreaRectangle(950, -1000, 1050, 3000) }
    };
    for (const TArray<TArray<FVector2D>>& Authored : UnsafeCases)
    {
        const auto Before = Authored;
        FFZAAutoWeld::FResult Result;
        TestTrue(TEXT("Unrepresentable authored intersections still produce a read-only preview"),
            FFZAAutoWeld::BuildAutomaticResult(Authored, 50.0, Result));
        TestFalse(TEXT("Gap repair never bypasses one-ring-per-Zone identity for holes, splits or deleted Zones"), Result.bCanCommit);
        TestTrue(TEXT("Unsupported topology explains the refused write"), !Result.FailureReason.IsEmpty());
        TestTrue(TEXT("Refused planning cannot mutate any authored input"), Authored == Before);
        TestEqual(TEXT("Refused planning retains every Zone output slot"), Result.Polygons.Num(), Authored.Num());
        for (const TArray<FVector2D>& Ring : Result.Polygons)
        {
            TestTrue(TEXT("Refused planning never commits an empty Zone"), StableGapArea(Ring) > 1.e-4);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaFourWayAreaTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.FourSeparatedSquaresFillCentralJunctionArea",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaFourWayAreaTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        GapAreaRectangle(0, 0, 1000, 1000), GapAreaRectangle(1100, 0, 2100, 1000),
        GapAreaRectangle(0, 1100, 1000, 2100), GapAreaRectangle(1100, 1100, 2100, 2100)
    };
    FFZAGapAreaRepair::FResult Result;
    if (CheckDirectGapArea(*this, Authored, Result))
    {
        double Area = 0.0;
        for (const TArray<FVector2D>& Ring : Result.Polygons) { Area += StableGapArea(Ring); }
        TestEqual(TEXT("Zero diagnostic gaps must also mean the central 100cm square is actually filled"), Area, 4410000.0, 1.e-3);
        const FVector2D Center(1050, 1050);
        TestTrue(TEXT("The four-way center belongs to at least one actual Zone"), Result.Polygons.ContainsByPredicate(
            [&Center](const TArray<FVector2D>& Ring) { return FFZAPolygonMath::IsPointInside(Ring, Center, 1.e-3); }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaTouchingNotchTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.StaggeredTouchingNotchesHaveAValidAreaFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaTouchingNotchTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    // The lower half of the thin tongue faces the recess's inner edge across 100cm.
    // Its upper half already shares the outer edge. The two different source corners
    // on the concave Zone meet the same partner edge, so a single moved-point cluster
    // cannot safely erase that distinction.
    const TArray<TArray<FVector2D>> Authored = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 400), FVector2D(900, 400),
          FVector2D(900, 420), FVector2D(1000, 420), FVector2D(1000, 1000), FVector2D(0, 1000) },
        { FVector2D(1100, 0), FVector2D(2100, 0), FVector2D(2100, 1000), FVector2D(1100, 1000),
          FVector2D(1100, 430), FVector2D(1000, 430), FVector2D(1000, 410), FVector2D(1100, 410) }
    };
    FFZAWeldDiagnostics::FResult Before;
    FFZAWeldDiagnostics::Analyze(Authored, {}, 100.0, Before);
    TestTrue(TEXT("Staggered touching notches start with positive exposed gaps"), Before.GapPairCount > 0);
    TestEqual(TEXT("Staggered touching notches have no initial positive-area overlap"), Before.OverlapPairCount, 0);
    FFZASeamConsensus::FResult Consensus;
    const bool bBuilt = FFZASeamConsensus::Build(Authored, 100.0, Consensus);
    AddInfo(FString::Printf(TEXT("Touching-notch corner consensus: built=%d complete=%d blocked=%d reason=%s"),
        bBuilt, Consensus.bComplete, Consensus.bBlocked, *Consensus.FailureReason));
    FFZAGapAreaRepair::FResult Result;
    CheckDirectGapArea(*this, Authored, Result);
    CheckIntegratedGapArea(*this, Authored);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaRejectedCornerFallbackTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.RotatedThinNotchesRecoverFromRejectedCornerConsensus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaRejectedCornerFallbackTest::RunTest(const FString& Parameters)
{
    using namespace FFZAGapAreaRepairTestsPrivate;
    // A 30cm-deep, 1cm-tall paired notch, 100cm apart and rotated by 0.35 radians.
    // These full-precision coordinates reproduce the original solver's topology refusal.
    // Preserve the captured doubles: recomputing the transform can change its final bits
    // when a compiler folds trigonometry and arithmetic, hiding the exact regression.
    const TArray<TArray<FVector2D>> Reproducer = {
        { FVector2D(0, 0), FVector2D(939.37271284737903, 342.89780745545136),
          FVector2D(853.64826098351614, 577.74098566729617), FVector2D(825.46707959809487, 567.45405144363258),
          FVector2D(825.12418179063934, 568.39342415647991), FVector2D(853.30536317606072, 578.6803583801435),
          FVector2D(699.34424762856315, 1000.4587064486167), FVector2D(671.16306624314166, 990.1717722249532),
          FVector2D(670.82016843568624, 991.11114493780042), FVector2D(699.00134982110762, 1001.398079161464),
          FVector2D(596.47490539192768, 1282.2705203028304), FVector2D(-342.89780745545136, 939.37271284737903) },
        { FVector2D(1033.3099841321168, 377.18758820099646), FVector2D(1972.6826969794959, 720.08539565644787),
          FVector2D(1629.7848895240445, 1659.4581085038269), FVector2D(690.41217667666547, 1316.5603010483755),
          FVector2D(792.93862110584541, 1035.6878599070092), FVector2D(764.75743972042414, 1025.4009256833456),
          FVector2D(765.10033752787967, 1024.4615529704984), FVector2D(793.28151891330094, 1034.7484871941617),
          FVector2D(947.24263446079851, 612.97013912568855), FVector2D(919.06145307537724, 602.68320490202507),
          FVector2D(919.40435088283266, 601.74383218917774), FVector2D(947.58553226825393, 612.03076641284122) }
    };
    const auto Before = Reproducer;
    FFZAWeldDiagnostics::FResult Initial;
    FFZAWeldDiagnostics::Analyze(Reproducer, {}, 100.0, Initial);
    TestEqual(TEXT("Rejected-corner regression starts without positive-area overlap"), Initial.OverlapPairCount, 0);
    TestTrue(TEXT("Rejected-corner regression starts with exposed gaps"), Initial.GapPairCount > 0);
    FFZASeamConsensus::FResult Consensus;
    TestTrue(TEXT("The rejected-corner fixture is valid input to the original consensus solver"),
        FFZASeamConsensus::Build(Reproducer, 100.0, Consensus));
    TestFalse(TEXT("The original point-movement solution cannot finish this fixed regression"), Consensus.bComplete);
    TestFalse(TEXT("The regression is a topology refusal rather than a processing budget"), Consensus.bBlocked);
    TestTrue(TEXT("The original point-movement solver reproduces the collapse/self-intersection refusal"),
        Consensus.FailureReason.Contains(TEXT("붕괴")) || Consensus.FailureReason.Contains(TEXT("자기교차")));
    FFZAGapAreaRepair::FResult Direct;
    CheckDirectGapArea(*this, Reproducer, Direct);
    FFZAAutoWeld::FResult Integrated;
    if (TestTrue(TEXT("The public planner accepts valid geometry rejected by corner movement"),
        FFZAAutoWeld::BuildAutomaticResult(Reproducer, 50.0, Integrated)))
    {
        TestTrue(TEXT("The reproduced screenshot failure uses the area fallback branch"), Integrated.bAreaFallbackUsed);
        TestTrue(*FString::Printf(TEXT("Fallback completes the entire previously rejected plan: %s"), *Integrated.FailureReason), Integrated.bCanCommit);
        CheckGapAreaResult(*this, Reproducer, Integrated.Polygons, Integrated.Stats.SourcePointKeys, 100.0);
    }
    TestTrue(TEXT("Recovering a rejected corner plan preserves every authored source coordinate"), Reproducer == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZAGapAreaFinalBandTest,
    "FF_ZoneAuthoring.Unit.GapAreaRepair.FinalCleanupRechecksEntirePerimeterAgainstOriginalBand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZAGapAreaFinalBandTest::RunTest(const FString& Parameters)
{
    const TArray<TArray<FVector2D>> Concave = {{
        {0, 0}, {1000, 0}, {1000, 1000}, {700, 1000},
        {700, 300}, {300, 300}, {300, 1000}, {0, 1000}}};
    const TArray<TArray<FVector2D>> Chord = {{{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}}};
    FString Error;
    TestTrue(TEXT("An unchanged concave boundary remains within its original band"),
        FFZAGapAreaRepair::ValidateRepairBand(Concave, Concave, 100.0, Error));
    TestFalse(TEXT("Original endpoints do not authorize a distant chord across a concavity"),
        FFZAGapAreaRepair::ValidateRepairBand(Concave, Chord, 100.0, Error));
    TestFalse(TEXT("A rejected perimeter explains the failed distance check"), Error.IsEmpty());
    TestTrue(TEXT("A chord covered by the full original edge capsules is accepted"),
        FFZAGapAreaRepair::ValidateRepairBand(Concave, Chord, 200.0, Error));
    const TArray<TArray<FVector2D>> Outside = {{{0, 0}, {1100.001, 0}, {1100.001, 1000}, {0, 1000}}};
    TestFalse(TEXT("Final numerical cleanup cannot move beyond the original radius allowance"),
        FFZAGapAreaRepair::ValidateRepairBand(Chord, Outside, 100.0, Error));
    return true;
}

#endif
