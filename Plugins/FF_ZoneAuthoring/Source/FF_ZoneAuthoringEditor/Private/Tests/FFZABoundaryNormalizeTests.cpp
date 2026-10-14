// Numerical boundary cleanup regressions. All inputs are arrays; no actors or levels.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZABoundaryNormalize.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FFZAWeldDiagnostics.h"

namespace FFZABoundaryNormalizeTestsPrivate
{
    TArray<FVector2D> NormalizeTestRectangle(const double X0, const double Y0,
        const double X1, const double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    TArray<TArray<double>> NormalizeTestIdentityKeys(const TArray<TArray<FVector2D>>& Rings)
    {
        TArray<TArray<double>> Keys;
        Keys.SetNum(Rings.Num());
        for (int32 Zone = 0; Zone < Rings.Num(); ++Zone)
        {
            for (int32 Point = 0; Point < Rings[Zone].Num(); ++Point) { Keys[Zone].Add(static_cast<double>(Point)); }
        }
        return Keys;
    }

    int32 NormalizeTestFindPoint(const TArray<FVector2D>& Ring, const FVector2D& Point)
    {
        return Ring.IndexOfByPredicate([&Point](const FVector2D& Candidate) { return Candidate.Equals(Point, 1.e-8); });
    }

    bool NormalizeTestOwnHeights(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Rings,
        const TArray<TArray<double>>& Keys, const TArray<TArray<double>>& OriginalHeights)
    {
        bool bAllValid = true;
        if (!Test.TestEqual(TEXT("Numerical cleanup preserves one key row per original Zone"), Keys.Num(), Rings.Num())) { return false; }
        for (int32 Zone = 0; Zone < Rings.Num(); ++Zone)
        {
            if (!Test.TestEqual(TEXT("Every normalized point retains its own Zone's height key"), Keys[Zone].Num(), Rings[Zone].Num()))
            {
                bAllValid = false; continue;
            }
            bool bKeysValid = true;
            for (const double Key : Keys[Zone])
            {
                bKeysValid &= Test.TestTrue(TEXT("Normalized source height keys remain finite and local to their original closed ring"),
                    FMath::IsFinite(Key) && Key >= 0.0 && Key < OriginalHeights[Zone].Num());
            }
            if (!bKeysValid) { bAllValid = false; continue; }
            TArray<FVector> WorldPoints;
            if (!Test.TestTrue(TEXT("Normalized geometry passes real height-preserving writeback preflight"),
                FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(OriginalHeights[Zone], Rings[Zone], Keys[Zone], WorldPoints)))
            {
                bAllValid = false; continue;
            }
            for (int32 Point = 0; Point < WorldPoints.Num(); ++Point)
            {
                const int32 Edge = FMath::FloorToInt(Keys[Zone][Point]);
                const double Expected = FMath::Lerp(OriginalHeights[Zone][Edge],
                    OriginalHeights[Zone][(Edge + 1) % OriginalHeights[Zone].Num()], Keys[Zone][Point] - Edge);
                bAllValid &= Test.TestEqual(TEXT("A common XY retains each Zone's separate original height interpolation"),
                    WorldPoints[Point].Z, Expected, 1.e-8);
            }
            TArray<FVector2D> Local;
            for (const FVector2D& Point : Rings[Zone]) { Local.Add(Point - Rings[Zone][0]); }
            bAllValid &= Test.TestTrue(TEXT("Every normalized result passes the editor's 0.01cm single-ring topology check"),
                FFZAPolygonMath::IsSimplePolygon(Local, 0.01));
        }
        return bAllValid;
    }

    void NormalizeTestHeightAtPoint(FAutomationTestBase& Test, const TArray<FVector2D>& Ring,
        const TArray<double>& Keys, const TArray<double>& OriginalHeights,
        const FVector2D& XY, const double ExpectedHeight, const double ExpectedKey)
    {
        const int32 Point = NormalizeTestFindPoint(Ring, XY);
        if (!Test.TestTrue(TEXT("The exact common boundary point exists in this Zone"), Point != INDEX_NONE)) { return; }
        Test.TestEqual(TEXT("The common point retains its own exact original height key"), Keys[Point], ExpectedKey, 1.e-8);
        TArray<FVector> WorldPoints;
        if (FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(OriginalHeights, Ring, Keys, WorldPoints))
        {
            Test.TestEqual(TEXT("The common point's height comes from this Zone's original edge"),
                WorldPoints[Point].Z, ExpectedHeight, 1.e-8);
        }
    }

    void NormalizeTestNoResidual(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Rings)
    {
        FFZAWeldDiagnostics::FResult Diagnostics;
        FFZAWeldDiagnostics::Analyze(Rings, {}, 0.1, Diagnostics);
        Test.TestEqual(TEXT("Normalized nearby boundaries have no exposed microscopic gap"), Diagnostics.GapPairCount, 0);
        Test.TestEqual(TEXT("Normalized nearby boundaries have no positive-area overlap"), Diagnostics.OverlapPairCount, 0);
    }

    bool NormalizeTestSameShape(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
    {
        if (A.Num() != B.Num()) { return false; }
        for (int32 Start = 0; Start < B.Num(); ++Start)
        {
            for (const int32 Direction : { -1, 1 })
            {
                bool bSame = true;
                for (int32 Point = 0; Point < A.Num(); ++Point)
                {
                    if (!A[Point].Equals(B[(Start + Direction * Point + B.Num()) % B.Num()], 1.e-8))
                    {
                        bSame = false; break;
                    }
                }
                if (bSame) { return true; }
            }
        }
        return false;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeAuthoredCornerAnchorTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.AuthoredCornersAnchorNearLongSharedEdgesAndKeepSeparateHeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeAuthoredCornerAnchorTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 1000, 1000), NormalizeTestRectangle(1000.0001, 250, 2000, 750)
    };
    const auto Before = Original;
    const TArray<TArray<double>> Heights = { { 10, 30, 70, 90 }, { 100, 200, 300, 400 } };
    TArray<TArray<FVector2D>> Rings = Original;
    auto Keys = NormalizeTestIdentityKeys(Original);
    FString Error;
    if (!TestTrue(*FString::Printf(TEXT("An authored corner near its neighbour's long edge can be noded: %s"), *Error),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error))) { return false; }
    TestEqual(TEXT("The long neighbouring edge receives the two exact authored corner nodes"), Rings[0].Num(), 6);
    TestTrue(TEXT("The anchored Zone retains every original coordinate and original point order"), Rings[1] == Original[1]);
    for (int32 Corner = 0; Corner < Original[0].Num(); ++Corner)
    {
        const int32 Point = NormalizeTestFindPoint(Rings[0], Original[0][Corner]);
        if (TestTrue(TEXT("Numerical noding preserves every original anchor corner"), Point != INDEX_NONE))
        {
            TestEqual(TEXT("Original anchor corners retain their integer height key"), Keys[0][Point], static_cast<double>(Corner));
        }
    }
    NormalizeTestHeightAtPoint(*this, Rings[0], Keys[0], Heights[0], FVector2D(1000.0001, 250), 40.0, 1.25);
    NormalizeTestHeightAtPoint(*this, Rings[1], Keys[1], Heights[1], FVector2D(1000.0001, 250), 100.0, 0.0);
    NormalizeTestHeightAtPoint(*this, Rings[0], Keys[0], Heights[0], FVector2D(1000.0001, 750), 60.0, 1.75);
    NormalizeTestHeightAtPoint(*this, Rings[1], Keys[1], Heights[1], FVector2D(1000.0001, 750), 400.0, 3.0);
    NormalizeTestOwnHeights(*this, Rings, Keys, Heights);
    NormalizeTestNoResidual(*this, Rings);
    TestTrue(TEXT("Read-only original geometry is untouched by joint numerical cleanup"), Original == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeGeneratedProjectionTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.GeneratedCornersProjectExactlyOntoAnOriginalLongNeighbourEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeGeneratedProjectionTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 1000, 1000), NormalizeTestRectangle(1100, 250, 2000, 750)
    };
    const TArray<TArray<double>> Heights = { { 10, 30, 70, 90 }, { 100, 200, 300, 400 } };
    TArray<TArray<FVector2D>> Rings = {
        Original[0], NormalizeTestRectangle(1000.0001, 250, 2000, 750)
    };
    auto Keys = NormalizeTestIdentityKeys(Original);
    FString Error;
    if (!TestTrue(TEXT("A generated corner one micron from a long original edge can be normalized"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    NormalizeTestHeightAtPoint(*this, Rings[0], Keys[0], Heights[0], FVector2D(1000, 250), 40.0, 1.25);
    NormalizeTestHeightAtPoint(*this, Rings[1], Keys[1], Heights[1], FVector2D(1000, 250), 100.0, 0.0);
    NormalizeTestHeightAtPoint(*this, Rings[0], Keys[0], Heights[0], FVector2D(1000, 750), 60.0, 1.75);
    NormalizeTestHeightAtPoint(*this, Rings[1], Keys[1], Heights[1], FVector2D(1000, 750), 400.0, 3.0);
    TestEqual(TEXT("Projecting shared corners does not change the partner's point count"), Rings[1].Num(), 4);
    NormalizeTestOwnHeights(*this, Rings, Keys, Heights);
    NormalizeTestNoResidual(*this, Rings);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeTinyGeneratedEdgeTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.RemovesGeneratedTinyAdjacentEdgesWithoutDiscardingOriginalCornerHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeTinyGeneratedEdgeTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 1000, 1000), NormalizeTestRectangle(1100, 0, 2000, 1000)
    };
    const TArray<TArray<double>> Heights = { { 10, 30, 70, 90 }, { 100, 200, 300, 400 } };
    TArray<TArray<FVector2D>> Rings = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 0.0001), FVector2D(1000, 1000), FVector2D(0, 1000) },
        NormalizeTestRectangle(1000.0001, 0, 2000, 1000)
    };
    TArray<TArray<double>> Keys = { { 0.0, 1.0, 1.0000001, 2.0, 3.0 }, { 0.0, 1.0, 2.0, 3.0 } };
    FString Error;
    if (!TestTrue(TEXT("The microscopic generated edge has a valid coordinated cleanup"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    TestTrue(TEXT("Removing the tiny generated edge retains the original rectangle exactly"), Rings[0] == Original[0]);
    TestTrue(TEXT("Removing the tiny generated edge retains every exact original height key"), Keys[0] == TArray<double>({ 0, 1, 2, 3 }));
    NormalizeTestHeightAtPoint(*this, Rings[0], Keys[0], Heights[0], FVector2D(1000, 0), 30.0, 1.0);
    NormalizeTestHeightAtPoint(*this, Rings[1], Keys[1], Heights[1], FVector2D(1000, 0), 100.0, 0.0);
    NormalizeTestOwnHeights(*this, Rings, Keys, Heights);
    NormalizeTestNoResidual(*this, Rings);
    const auto Before = Rings;
    const auto BeforeKeys = Keys;
    TestTrue(TEXT("Numerical normalization can be repeated on its complete result"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error));
    TestTrue(TEXT("Repeat cleanup preserves exact coordinates and point counts"), Rings == Before);
    TestTrue(TEXT("Repeat cleanup preserves exact original height provenance"), Keys == BeforeKeys);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeAtomicCrossingTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.ARealSelfIntersectionRejectsEveryGeometryAndKeyMutationAtomically",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeAtomicCrossingTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 1000, 1000), NormalizeTestRectangle(2000, 0, 3000, 1000)
    };
    TArray<TArray<FVector2D>> Rings = {
        { Original[0][0], Original[0][2], Original[0][1], Original[0][3] },
        { FVector2D(2000, 0), FVector2D(3000, 0), FVector2D(3000, 0.0001), FVector2D(3000, 1000), FVector2D(2000, 1000) }
    };
    TArray<TArray<double>> Keys = { { 0, 2, 1, 3 }, { 0, 1, 1.0000001, 2, 3 } };
    const auto Before = Rings;
    const auto BeforeKeys = Keys;
    FString Error;
    TestFalse(TEXT("A genuine crossing cannot be hidden by microscopic-edge cleanup"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error));
    TestTrue(TEXT("Refused cleanup explains the invalid ring"), !Error.IsEmpty());
    TestTrue(TEXT("A refused ring leaves every Zone's proposal unchanged, including the cleanable neighbour"), Rings == Before);
    TestTrue(TEXT("A refused ring leaves all original height keys unchanged"), Keys == BeforeKeys);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeClosingPermutationTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.ClosingEdgeHeightKeysSurviveCollectionWindingAndStartPermutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeClosingPermutationTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 1000, 1000), NormalizeTestRectangle(-1000, 250, -0.0001, 750)
    };
    const TArray<TArray<double>> Heights = { { 10, 30, 70, 90 }, { 100, 200, 300, 400 } };
    auto Baseline = Original;
    auto BaselineKeys = NormalizeTestIdentityKeys(Original);
    FString Error;
    if (!TestTrue(TEXT("The baseline closing edge accepts both neighbour corner nodes"),
        FFZABoundaryNormalize::Normalize(Baseline, BaselineKeys, Original, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    NormalizeTestHeightAtPoint(*this, Baseline[0], BaselineKeys[0], Heights[0], FVector2D(-0.0001, 750), 70.0, 3.25);
    NormalizeTestHeightAtPoint(*this, Baseline[0], BaselineKeys[0], Heights[0], FVector2D(-0.0001, 250), 30.0, 3.75);
    NormalizeTestOwnHeights(*this, Baseline, BaselineKeys, Heights);
    TArray<TArray<FVector2D>> PermutedOriginal;
    TArray<TArray<double>> PermutedHeights;
    for (const int32 OriginalIndex : { 1, 0 })
    {
        TArray<FVector2D> Reversed = Original[OriginalIndex];
        TArray<double> ReversedHeights = Heights[OriginalIndex];
        Algo::Reverse(Reversed); Algo::Reverse(ReversedHeights);
        TArray<FVector2D> Rotated;
        TArray<double> RotatedHeights;
        for (int32 Point = 0; Point < Reversed.Num(); ++Point)
        {
            Rotated.Add(Reversed[(Point + 1) % Reversed.Num()]);
            RotatedHeights.Add(ReversedHeights[(Point + 1) % ReversedHeights.Num()]);
        }
        PermutedOriginal.Add(MoveTemp(Rotated)); PermutedHeights.Add(MoveTemp(RotatedHeights));
    }
    const auto Before = PermutedOriginal;
    auto Permuted = PermutedOriginal;
    auto PermutedKeys = NormalizeTestIdentityKeys(PermutedOriginal);
    if (!TestTrue(TEXT("Reversed winding, collection order and cyclic start can be normalized together"),
        FFZABoundaryNormalize::Normalize(Permuted, PermutedKeys, PermutedOriginal, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    TestTrue(TEXT("The first Zone's exact boundary shape is independent of collection and winding"),
        NormalizeTestSameShape(Baseline[0], Permuted[1]));
    TestTrue(TEXT("The partner's exact boundary shape is independent of collection and winding"),
        NormalizeTestSameShape(Baseline[1], Permuted[0]));
    NormalizeTestHeightAtPoint(*this, Permuted[1], PermutedKeys[1], PermutedHeights[1], FVector2D(-0.0001, 750), 70.0, 2.75);
    NormalizeTestHeightAtPoint(*this, Permuted[1], PermutedKeys[1], PermutedHeights[1], FVector2D(-0.0001, 250), 30.0, 2.25);
    NormalizeTestOwnHeights(*this, Permuted, PermutedKeys, PermutedHeights);
    NormalizeTestNoResidual(*this, Permuted);
    TestTrue(TEXT("Height provenance never reorders the caller's original rings"), PermutedOriginal == Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeBoundedClusterTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.NearNodeChainsCannotCollapseDistantEndpointsOrDistinctOriginalCorners",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeBoundedClusterTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = { NormalizeTestRectangle(0, 0, 1000, 1000) };
    TArray<TArray<FVector2D>> Rings = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 0.008), FVector2D(1000, 0.016),
          FVector2D(1000, 0.024), FVector2D(1000, 1000), FVector2D(0, 1000) }
    };
    TArray<TArray<double>> Keys = { { 0, 1, 1.000008, 1.000016, 1.000024, 2, 3 } };
    FString Error;
    if (!TestTrue(TEXT("A short chain of synthetic nodes can be cleaned within one fixed tolerance"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    TestTrue(TEXT("The exact original corner remains fixed at the start of a near-node chain"),
        NormalizeTestFindPoint(Rings[0], FVector2D(1000, 0)) != INDEX_NONE);
    TestTrue(TEXT("A transitive .008cm chain never merges its .024cm endpoint into the original corner"),
        Rings[0].ContainsByPredicate([](const FVector2D& Point) { return Point.X == 1000.0 && Point.Y > 0.01 && Point.Y < 0.025; }));
    NormalizeTestOwnHeights(*this, Rings, Keys, { { 10, 30, 70, 90 } });

    TArray<TArray<FVector2D>> CollapsedProposal = {
        { FVector2D(0, 0), FVector2D(1000, 0), FVector2D(1000, 0.005), FVector2D(0, 1000) }
    };
    auto CornerKeys = NormalizeTestIdentityKeys(Original);
    const auto Before = CollapsedProposal;
    const auto BeforeKeys = CornerKeys;
    TestFalse(TEXT("Two original corners 1000cm apart cannot be merged because a rejected proposal moved them .005cm apart"),
        FFZABoundaryNormalize::Normalize(CollapsedProposal, CornerKeys, Original, 0.01, Error));
    TestTrue(TEXT("Refusing to merge distinct original corners preserves the proposal atomically"), CollapsedProposal == Before);
    TestTrue(TEXT("Refusing to merge distinct original corners preserves both integer Z keys"), CornerKeys == BeforeKeys);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZANormalizeProjectionCreatesShortEdgeTest,
    "FF_ZoneAuthoring.Unit.BoundaryNormalize.NeighbourEdgeProjectionDoesNotLeaveANewMicroscopicAdjacentEdge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZANormalizeProjectionCreatesShortEdgeTest::RunTest(const FString& Parameters)
{
    using namespace FFZABoundaryNormalizeTestsPrivate;
    const TArray<TArray<FVector2D>> Original = {
        NormalizeTestRectangle(0, 0, 2000, 2000), NormalizeTestRectangle(990, 1000, 1010, 2000)
    };
    // The two convex bevel corners start 0.012728cm apart, outside the cleanup cluster.
    // Projecting (1000,1000.009) onto the neighbour's original y=1000 edge then puts
    // them only .009cm apart. The final cleanup must recompute shared nodes and keys.
    TArray<TArray<FVector2D>> Rings = {
        { FVector2D(0, 0), FVector2D(1000.009, 0), FVector2D(1000.009, 1000),
          FVector2D(1000, 1000.009), FVector2D(0, 1000.009) }, Original[1]
    };
    TArray<TArray<double>> Keys = { { 0, 0.5000045, 1.5, 1.5000045, 3.4999955 }, { 0, 1, 2, 3 } };
    const TArray<TArray<double>> Heights = { { 10, 30, 70, 90 }, { 100, 200, 300, 400 } };
    TestTrue(TEXT("The bevel proposal initially passes the strict 0.01cm topology check"),
        FFZAPolygonMath::IsSimplePolygon(Rings[0], 0.01));
    TestTrue(TEXT("Its bevel starts outside the initial numerical cluster tolerance"),
        FVector2D::Distance(Rings[0][2], Rings[0][3]) > 0.01);
    FString Error;
    if (!TestTrue(TEXT("Projecting two separate clusters onto a neighbour produces a valid final ring"),
        FFZABoundaryNormalize::Normalize(Rings, Keys, Original, 0.01, Error)))
    {
        AddInfo(Error); return false;
    }
    NormalizeTestOwnHeights(*this, Rings, Keys, Heights);
    int32 JunctionCount = 0;
    FVector2D CommonPoint;
    for (const FVector2D& Point : Rings[0])
    {
        if (FVector2D::Distance(Point, FVector2D(1000, 1000)) < 0.02)
        {
            ++JunctionCount; CommonPoint = Point;
        }
    }
    TestEqual(TEXT("The projected numerical bevel becomes one final shared junction"), JunctionCount, 1);
    TestTrue(TEXT("The neighbour receives the identical final junction coordinate"),
        NormalizeTestFindPoint(Rings[1], CommonPoint) != INDEX_NONE);
    for (const TArray<FVector2D>& Ring : Rings)
    {
        for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
        {
            TestTrue(TEXT("Late projection cannot leave a new adjacent edge at or below the cleanup tolerance"),
                FVector2D::Distance(Ring[Edge], Ring[(Edge + 1) % Ring.Num()]) > 0.01);
        }
    }
    return true;
}

#endif
