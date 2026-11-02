// Pure geometry diagnostics: no editor world, actor, level, or asset mutations.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZAWeldDiagnostics.h"

#include <limits>

namespace FFZAWeldDiagnosticsTestsPrivate
{
    TArray<FVector2D> MakeWeldDiagnosticRectangle(const double X0, const double Y0, const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    double FirstOverlapArea(const FFZAWeldDiagnostics::FResult& Result)
    {
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Issues)
        {
            if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Overlap)
            {
                return Issue.OverlapAreaCm2;
            }
        }
        return -1.0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsPartialGapTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.PartialGapAndWinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsPartialGapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    for (const bool bReverse : { false, true })
    {
        TArray<TArray<FVector2D>> Polygons = { MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(120, 25, 220, 75) };
        if (bReverse)
        {
            Algo::Reverse(Polygons[1]);
        }
        const TArray<TArray<FVector2D>> Before = Polygons;
        FFZAWeldDiagnostics::FResult Result;
        FFZAWeldDiagnostics::Analyze(Polygons, {}, 50.0, Result);
        TestEqual(TEXT("One pair has a residual gap"), Result.GapPairCount, 1);
        TestEqual(TEXT("Disjoint rings have no overlap"), Result.OverlapPairCount, 0);
        if (TestEqual(TEXT("One facing partial span"), Result.Issues.Num(), 1))
        {
            const FFZAWeldDiagnostics::FBoundaryIssue& Issue = Result.Issues[0];
            TestEqual(TEXT("Gap length measures distance, not full edge length"), Issue.MaximumGapCm, 20.0);
            TestTrue(TEXT("Only the actual longitudinal overlap is displayed"),
                FMath::IsNearlyEqual(FMath::Min(Issue.StartA.Y, Issue.EndA.Y), 25.0) &&
                FMath::IsNearlyEqual(FMath::Max(Issue.StartA.Y, Issue.EndA.Y), 75.0));
            TestTrue(TEXT("Matched endpoints show actual boundary positions"),
                Issue.StartA.X == 100.0 && Issue.EndA.X == 100.0 &&
                Issue.StartB.X == 120.0 && Issue.EndB.X == 120.0);
        }
        TestTrue(TEXT("Diagnostics never mutates its input"), Before == Polygons);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsTouchingAndRangeTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.TouchingCornerAndRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsTouchingAndRangeTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    FFZAWeldDiagnostics::FResult Result;
    for (const TArray<FVector2D>& B : { MakeWeldDiagnosticRectangle(100, 0, 200, 100), MakeWeldDiagnosticRectangle(120, 120, 220, 220), MakeWeldDiagnosticRectangle(151, 0, 251, 100) })
    {
        FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), B }, {}, 50.0, Result);
        TestEqual(TEXT("Shared edges, corner-only proximity and beyond-range seams are not gaps"), Result.GapPairCount, 0);
        TestEqual(TEXT("No positive area overlap"), Result.OverlapPairCount, 0);
        TestEqual(TEXT("No erroneous spans"), Result.Issues.Num(), 0);
    }
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(150, 0, 250, 100) }, {}, 50.0, Result);
    TestEqual(TEXT("Inspection range includes its exact upper limit"), Result.GapPairCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsShallowOverlapTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.ShallowOverlapIsNotGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsShallowOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(99.999, 0, 200, 100) }, {}, 50.0, Result);
    TestEqual(TEXT("Shallow positive area is an overlap pair"), Result.OverlapPairCount, 1);
    TestEqual(TEXT("Overlap pair is never double-classified as gap"), Result.GapPairCount, 0);
    TestTrue(TEXT("Shallow area remains measurable"), FMath::IsNearlyEqual(FirstOverlapArea(Result), 0.1, 1e-6));
    for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Issues)
    {
        TestTrue(TEXT("Overlap perimeter uses the same segment on both sides for drawing once"),
            Issue.StartA == Issue.StartB && Issue.EndA == Issue.EndB);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsConcaveOverlapTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.ConcaveDisconnectedIntersection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsConcaveOverlapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    const TArray<FVector2D> UShape = { FVector2D(0, 0), FVector2D(100, 0), FVector2D(100, 100),
        FVector2D(70, 100), FVector2D(70, 30), FVector2D(30, 30), FVector2D(30, 100), FVector2D(0, 100) };
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ UShape, MakeWeldDiagnosticRectangle(-10, 50, 110, 90) }, {}, 50.0, Result);
    TestEqual(TEXT("Disconnected intersection still belongs to one polygon pair"), Result.OverlapPairCount, 1);
    TestTrue(TEXT("Both disjoint 30 x 40 overlap pieces are measured"),
        FMath::IsNearlyEqual(FirstOverlapArea(Result), 2400.0, 1e-6));
    for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Issues)
    {
        const FVector2D Middle = (Issue.StartA + Issue.EndA) * 0.5;
        TestTrue(TEXT("Perimeter never draws artificial bridges across the concave opening"),
            Middle.X <= 30.0 + 1e-4 || Middle.X >= 70.0 - 1e-4);
        TestTrue(TEXT("Reported perimeter lies in the actual overlap height"),
            Middle.Y >= 50.0 - 1e-4 && Middle.Y <= 90.0 + 1e-4);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsLargeOffsetTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.LargeWorldOffset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsLargeOffsetTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    constexpr double Offset = 1e12;
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(Offset, Offset, Offset + 100, Offset + 100),
        MakeWeldDiagnosticRectangle(Offset + 80, Offset + 20, Offset + 140, Offset + 80) }, {}, 50.0, Result);
    TestEqual(TEXT("Large-offset simple polygons remain valid"), Result.OverlapPairCount, 1);
    TestTrue(TEXT("Translated clipping preserves 20 x 60 overlap area"),
        FMath::IsNearlyEqual(FirstOverlapArea(Result), 1200.0, 1e-6));
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(Offset, Offset, Offset + 100, Offset + 100),
        MakeWeldDiagnosticRectangle(Offset + 120, Offset + 25, Offset + 220, Offset + 75) }, {}, 50.0, Result);
    TestEqual(TEXT("Large-offset facing seam is found"), Result.GapPairCount, 1);
    if (!Result.Issues.IsEmpty())
    {
        TestEqual(TEXT("Large-offset gap remains exact"), Result.Issues[0].MaximumGapCm, 20.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsInvalidAndRejectedTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.InvalidGeometryAndRejectionContext",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsInvalidAndRejectedTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    const double NaN = std::numeric_limits<double>::quiet_NaN();
    const TArray<TArray<FVector2D>> Polygons = { MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(120, 0, 220, 100),
        { FVector2D(10, 10), FVector2D(90, 90), FVector2D(10, 90), FVector2D(90, 10) },
        { FVector2D(NaN, 0), FVector2D(10, 0), FVector2D(10, 10) }, {} };
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze(Polygons, { 1, 2, 3, 4, 4, -1, 99 }, 50.0, Result);
    TestEqual(TEXT("Rejected indices are unique and in bounds"), Result.RejectedPolygonCount, 4);
    TestEqual(TEXT("Invalid rings are safely excluded from pair proof"), Result.OverlapPairCount, 0);
    TestEqual(TEXT("Valid gap pair is still diagnosed"), Result.GapPairCount, 1);
    bool bSawRejectedGap = false;
    bool bSawInvalidRingContext = false;
    for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Issues)
    {
        TestTrue(TEXT("Every drawable span is finite"), FMath::IsFinite(Issue.StartA.X) &&
            FMath::IsFinite(Issue.StartA.Y) && FMath::IsFinite(Issue.EndA.X) && FMath::IsFinite(Issue.EndA.Y));
        if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Gap)
        {
            bSawRejectedGap |= Issue.bRelatedToRejection;
        }
        if (Issue.Type == FFZAWeldDiagnostics::EIssueType::Rejected)
        {
            TestEqual(TEXT("Rejected ring has no partner index"), Issue.PolygonIndexB, INDEX_NONE);
            bSawInvalidRingContext |= Issue.PolygonIndexA == 2;
        }
    }
    TestTrue(TEXT("Residual linked to a rejected proposal is tagged"), bSawRejectedGap);
    TestTrue(TEXT("Finite invalid-ring edges can be displayed as rejection context"), bSawInvalidRingContext);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsDisplayCapTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.DisplayCapKeepsCompleteCounts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsDisplayCapTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    const TArray<TArray<FVector2D>> Polygons = { MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(120, 0, 220, 100),
        MakeWeldDiagnosticRectangle(1000, 0, 1100, 100), MakeWeldDiagnosticRectangle(1120, 0, 1220, 100) };
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze(Polygons, {}, 50.0, Result, 1);
    TestEqual(TEXT("Only requested number of spans is retained"), Result.Issues.Num(), 1);
    TestTrue(TEXT("Display truncation is explicit"), Result.bTruncated);
    TestEqual(TEXT("All gap pairs remain counted after the cap"), Result.GapPairCount, 2);
    FFZAWeldDiagnostics::Analyze(Polygons, {}, 50.0, Result, 0);
    TestEqual(TEXT("Zero display budget still performs pair inspection"), Result.GapPairCount, 2);
    TestEqual(TEXT("Zero budget retains no spans"), Result.Issues.Num(), 0);
    TestTrue(TEXT("Zero budget signals hidden spans"), Result.bTruncated);
    FFZAWeldDiagnostics::Analyze({}, {}, 50.0, Result, 0);
    TestFalse(TEXT("Zero budget without findings is not truncated"), Result.bTruncated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsSlopedRangeTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.NearParallelSpanClipsToRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsSlopedRangeTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    const TArray<FVector2D> Sloped = { FVector2D(110, 0), FVector2D(210, 0), FVector2D(230, 100), FVector2D(130, 100) };
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), Sloped }, {}, 20.0, Result);
    TestEqual(TEXT("Near-parallel boundary qualifies for inspection"), Result.GapPairCount, 1);
    if (TestEqual(TEXT("One clipped sloped gap span"), Result.Issues.Num(), 1))
    {
        TestTrue(TEXT("Only the first half within 20cm is displayed"),
            FMath::IsNearlyEqual(Result.Issues[0].EndA.Y, 50.0, 1e-6));
        TestTrue(TEXT("Maximum gap is the inspection radius"),
            FMath::IsNearlyEqual(Result.Issues[0].MaximumGapCm, 20.0, 1e-6));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsInterveningZoneTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.InterveningZoneRemovesFalseGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsInterveningZoneTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(100, 0, 140, 100), MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result);
    TestEqual(TEXT("A middle Zone completely fills the apparent outer pair gap"), Result.GapPairCount, 0);
    TestEqual(TEXT("An exactly tiled layout has no positive overlap"), Result.OverlapPairCount, 0);
    TestEqual(TEXT("No misleading diagnostic is drawn through the middle Zone"), Result.Issues.Num(), 0);
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(110, 0, 130, 100), MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result);
    TestEqual(TEXT("A narrower intervening Zone leaves its two real neighbouring gaps"), Result.GapPairCount, 2);
    for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Issues)
    {
        TestEqual(TEXT("Narrower voids are measured against the middle Zone"), Issue.MaximumGapCm, 10.0);
        TestFalse(TEXT("Outer gap is not drawn through a selected Zone"), Issue.PolygonIndexA == 0 && Issue.PolygonIndexB == 2);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsPartialCoverageTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.PartialCoverageKeepsExposedSpans",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsPartialCoverageTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(100, 25, 140, 75), MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result);
    TestEqual(TEXT("Exposed parts remain one distinct gap pair"), Result.GapPairCount, 1);
    if (TestEqual(TEXT("A middle covered interval leaves two exposed spans"), Result.Issues.Num(), 2))
    {
        TestTrue(TEXT("First exposed interval ends at coverage"),
            FMath::IsNearlyEqual(Result.Issues[0].StartA.Y, 0.0) && FMath::IsNearlyEqual(Result.Issues[0].EndA.Y, 25.0));
        TestTrue(TEXT("Second exposed interval starts after coverage"),
            FMath::IsNearlyEqual(Result.Issues[1].StartA.Y, 75.0) && FMath::IsNearlyEqual(Result.Issues[1].EndA.Y, 100.0));
        TestEqual(TEXT("First interval retains original pair width"), Result.Issues[0].MaximumGapCm, 40.0);
        TestEqual(TEXT("Second interval retains original pair width"), Result.Issues[1].MaximumGapCm, 40.0);
    }
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(100, 25, 140, 75), MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result, 1);
    TestEqual(TEXT("Coverage subtraction still keeps full pair count under a display cap"), Result.GapPairCount, 1);
    TestTrue(TEXT("Second exposed span sets truncation"), Result.bTruncated);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsMergedCoverageTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.MergesCoverageFromMultipleZones",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsMergedCoverageTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ MakeWeldDiagnosticRectangle(0, 0, 100, 100), MakeWeldDiagnosticRectangle(100, 0, 140, 50), MakeWeldDiagnosticRectangle(100, 50, 140, 100),
        MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result);
    TestEqual(TEXT("Several intervening rings cover the complete apparent gap"), Result.GapPairCount, 0);
    TestEqual(TEXT("Shared coverage endpoints do not become microscopic gap spans"), Result.Issues.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZADiagnosticsConcaveSelfCoverageTest,
    "FF_ZoneAuthoring.Unit.WeldDiagnostics.ConcaveArmRemovesHiddenGap",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZADiagnosticsConcaveSelfCoverageTest::RunTest(const FString& Parameters)
{
    using namespace FFZAWeldDiagnosticsTestsPrivate;
    const TArray<FVector2D> Concave = { FVector2D(0, 0), FVector2D(100, 0), FVector2D(100, 80),
        FVector2D(120, 80), FVector2D(120, 0), FVector2D(130, 0), FVector2D(130, 100), FVector2D(0, 100) };
    FFZAWeldDiagnostics::FResult Result;
    FFZAWeldDiagnostics::Analyze({ Concave, MakeWeldDiagnosticRectangle(140, 0, 240, 100) }, {}, 100.0, Result);
    TestEqual(TEXT("Concave neighbour has one exposed gap pair"), Result.GapPairCount, 1);
    if (TestEqual(TEXT("Hidden wall is removed from the displayed spans"), Result.Issues.Num(), 1))
    {
        TestEqual(TEXT("Exposed wall retains its 10cm gap"), Result.Issues[0].MaximumGapCm, 10.0);
        TestEqual(TEXT("Actual exposed wall is the outer concave arm"), Result.Issues[0].StartA.X, 130.0);
        TestEqual(TEXT("Actual exposed wall remains the outer concave arm"), Result.Issues[0].EndA.X, 130.0);
    }
    return true;
}

#endif
