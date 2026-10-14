// Exact captured editor-world geometry, tested without loading or modifying a level.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "FFZAAutoWeld.h"
#include "FFZAGapAreaRepair.h"
#include "FFZAOverlapRepair.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FFZAWeldDiagnostics.h"

namespace FFZARealLayoutWeldTestsPrivate
{
    // Filled from the read-only capture; decimal literals retain the captured doubles.
    TArray<TArray<FVector>> CapturedWorldZones()
    {
        return {
            { // GZ_Tutorial
                FVector(74770, 27510, 447.24287578124495),
                FVector(86008.42826731487, 30816.791698044923, 363.47476211435924),
                FVector(88812.114176967792, 30742.076704237996, 980.87052359172083),
                FVector(89521.026570944654, 31645.762371317534, 980.69690769021156),
                FVector(91040, 40030, 438.01203746676765),
                FVector(76530, 40310, 447.24287578124495),
            },
            { // GZ_TemporaryParkingEast
                FVector(51880, 18810, 446.60888919089041),
                FVector(74770, 27510, 450.00107656249747),
                FVector(75410, 32330, 450.00107656249747),
                FVector(77550.372078306827, 47981.970513585191, 255.22899709472404),
                FVector(77734.887032803308, 50410.991851770988, 217.24676829732459),
                FVector(76026.855624271411, 47221.050069423727, 259.58955839426943),
                FVector(57491.237612898694, 33286.395884435668, 314.83065414963602),
                FVector(47600, 30560, 279.47566679570366),
            },
            { // GZ_FleaMarket
                FVector(31560, 11620, 70.276506152338698),
                FVector(51790, 18950, 454.67650386352034),
                FVector(47600, 30560, 190.2765061523387),
                FVector(38182.061874965424, 39364.339733875837, -611.74578647428041),
                FVector(18846.909541177854, 29200.984463193665, 374.8707444335887),
                FVector(19199.620195385538, 25777.389695861297, 367.64303373418409),
            },
            { // GZ_FountainPirates
                FVector(13910, 2390, 353.51708984375),
                FVector(31340, 11590, 70),
                FVector(19409.485996524032, 25537.012621894475, 70),
                FVector(-2630, 14860, 335.08869171142578),
                FVector(0.54947436708062014, 55.192192582397183, 341.43563863820253),
            },
            { // GZ_Beach01
                FVector(-2730, 14920, 345.87119374999747),
                FVector(19210, 25700, 70),
                FVector(17031.166363989352, 41944.937037474287, 128.44188771972404),
                FVector(7270, 34110, 124.82231984863029),
                FVector(4345.2603255596314, 32421.026358429019, 101.52358327636466),
                FVector(-1589.597566816168, 30122.165682239764, 95.317635400388099),
                FVector(-5120, 29980, 121.64457021484122),
                FVector(-5285.4758726744549, 29032.329446112664, 345.87119374999747),
            },
            { // GZ_Beach02
                FVector(18920.755515302699, 29308.119657222218, 381.1234130859375),
                FVector(38270, 39710, 70),
                FVector(35980, 56300, 70),
                FVector(29460.377878182775, 50614.426093453265, 70),
                FVector(23030.805783949047, 47324.31591383605, 70),
                FVector(17480, 42090, 70),
            },
            { // GZ_DadaYajang
                FVector(38148.900820745366, 39644.898792910928, 380.77418599853263),
                FVector(45580, 32780, 380.77418599853263),
                FVector(51378.623969908767, 34353.68643527801, 317.83102692902588),
                FVector(54098.804081198497, 36571.374944698407, 288.30389595258634),
                FVector(70654.691339928744, 50352.390207681376, 108.59232115987345),
                FVector(73547.150272413797, 45593.761446720324, 433.51819834283003),
                FVector(75940, 47363.302663420574, 70),
                FVector(70717.408658118395, 56060.218890671174, 260.47521833260612),
                FVector(59840.038197997739, 61618.648102697654, 428.64332537438133),
            },
            { // GZ_Operations
                FVector(70717.408658118395, 56060.218890671174, 207.61338124999747),
                FVector(72937.855328483871, 52722.451683521736, 207.61338124999747),
                FVector(75980, 58110, 69.999993188474036),
                FVector(81300, 66900, 207.61338124999747),
                FVector(89677.784579094819, 79366.005920090814, 70.000008447270375),
                FVector(86899.535263346042, 92775.830874717125, 69.999993188474036),
                FVector(59740, 61750, 69.999993188474036),
            },
            { // GZ_Beach03
                FVector(38251.410378640023, 39844.673283127522, 437.18214416503906),
                FVector(59602.918242934073, 61683.499095246589, 704.60288138916394),
                FVector(46409.091268233511, 68042.677251761008, 630),
                FVector(36010, 56650, 630),
            },
            { // GZ_Beach04
                FVector(59644.438804073056, 62086.593651841977, 914.13513341764315),
                FVector(71410, 75290, 520),
                FVector(59560, 82750, 520),
                FVector(52309.640600780192, 75854.978869920567, 617.18056438262863),
                FVector(46409.091268233511, 68042.677251761008, 727.41555835713461),
            },
            { // GZ_Beach05
                FVector(71332.808684663105, 75338.594701469468, 498.01936486803243),
                FVector(86460, 93720, 70),
                FVector(79210, 92040, 530),
                FVector(62169.727429959428, 88762.722283889118, 530),
                FVector(61805.857870042724, 86200.58662953967, 530),
                FVector(59590, 82810, 530),
            },
            { // GZ_DadaepoForest
                FVector(76530, 40310, 424.89254161376448),
                FVector(91040, 40030, 446.44124003905745),
                FVector(92370, 47720, 446.44124003905745),
                FVector(89228.505815879122, 48404.292053171121, 206.44124003905745),
                FVector(81207.903394079622, 53100.456940684613, 446.44124003905745),
                FVector(77860, 50310, 446.44124003905745),
            },
            { // GZ_PublicParkingWest
                FVector(80790, 53390, 70),
                FVector(89654.893106225078, 48311.414773362136, 69.9999925055115),
                FVector(91251.722434042516, 47963.587416297698, 306.49274253845215),
                FVector(91670, 51490, 70.000002980232239),
                FVector(96055.000735061883, 58572.54373618227, 256.99733885238174),
                FVector(95655.831241301785, 59806.903367662526, 254.56138850776046),
                FVector(95508.603086325078, 63234.698816575241, 253.32732590582938),
                FVector(94920.946444302725, 65847.767258847598, 224.83458559759993),
                FVector(92130, 67860, 196.56405389308929),
                FVector(91382.74540996681, 67780.100679217503, 197.87995641687937),
                FVector(90371.76779666607, 68127.390385310238, 198.82037903434789),
                FVector(89542.706381323413, 67866.170740234593, 199.59158118165669),
                FVector(88090, 66260, 217.42149978860482),
            },
        };
    }

    void ExtractCapturedGeometry(const TArray<TArray<FVector>>& WorldZones,
        TArray<TArray<FVector2D>>& OutPolygons, TArray<TArray<double>>& OutHeights)
    {
        OutPolygons.SetNum(WorldZones.Num());
        OutHeights.SetNum(WorldZones.Num());
        for (int32 Zone = 0; Zone < WorldZones.Num(); ++Zone)
        {
            for (const FVector& Point : WorldZones[Zone])
            {
                OutPolygons[Zone].Emplace(Point.X, Point.Y);
                OutHeights[Zone].Add(Point.Z);
            }
        }
    }

    void DumpRealLayoutPlan(FAutomationTestBase& Test, const FFZAAutoWeld::FResult& Result)
    {
        Test.AddInfo(FString::Printf(TEXT("Actual layout plan: writable=%d areaFallback=%d gaps=%d overlaps=%d rejected=%d failure=%s"),
            Result.bCanCommit, Result.bAreaFallbackUsed, Result.Diagnostics.GapPairCount,
            Result.Diagnostics.OverlapPairCount, Result.Stats.RejectedPolygonCount, *Result.FailureReason));
        for (int32 Zone = 0; Zone < Result.Polygons.Num(); ++Zone)
        {
            FString Coordinates;
            for (int32 Point = 0; Point < Result.Polygons[Zone].Num(); ++Point)
            {
                const FVector2D& XY = Result.Polygons[Zone][Point];
                const double Key = Result.Stats.SourcePointKeys.IsValidIndex(Zone) &&
                    Result.Stats.SourcePointKeys[Zone].IsValidIndex(Point)
                    ? Result.Stats.SourcePointKeys[Zone][Point] : -1.0;
                Coordinates += FString::Printf(TEXT("(%.17g,%.17g;key=%.17g),"), XY.X, XY.Y, Key);
            }
            Test.AddInfo(FString::Printf(TEXT("Actual layout output Zone %d [%d]: %s"),
                Zone, Result.Polygons[Zone].Num(), *Coordinates));
        }
        for (const FFZAWeldDiagnostics::FBoundaryIssue& Issue : Result.Diagnostics.Issues)
        {
            Test.AddInfo(FString::Printf(TEXT("Actual layout residual type=%d Zone=%d/%d A(%.17g,%.17g)-(%.17g,%.17g) B(%.17g,%.17g)-(%.17g,%.17g) gap=%.17g overlap=%.17g"),
                static_cast<int32>(Issue.Type), Issue.PolygonIndexA, Issue.PolygonIndexB,
                Issue.StartA.X, Issue.StartA.Y, Issue.EndA.X, Issue.EndA.Y,
                Issue.StartB.X, Issue.StartB.Y, Issue.EndB.X, Issue.EndB.Y,
                Issue.MaximumGapCm, Issue.OverlapAreaCm2));
        }
    }

    void DiagnoseCapturedAreaCandidate(FAutomationTestBase& Test,
        const TArray<TArray<FVector2D>>& Original, const FFZAWeldDiagnostics::FResult& Initial)
    {
        TArray<TArray<FVector2D>> AreaInput = Original;
        TArray<TArray<double>> InputKeys;
        InputKeys.SetNum(Original.Num());
        int32 LargestAreaOwner = 0;
        double LargestArea = -1.0;
        for (int32 Zone = 0; Zone < Original.Num(); ++Zone)
        {
            TArray<FVector2D> Local;
            for (int32 Point = 0; Point < Original[Zone].Num(); ++Point)
            {
                Local.Add(Original[Zone][Point] - Original[Zone][0]);
                InputKeys[Zone].Add(static_cast<double>(Point));
            }
            const double Area = FFZAPolygonMath::Area(Local);
            if (Area > LargestArea) { LargestArea = Area; LargestAreaOwner = Zone; }
        }
        if (Initial.OverlapPairCount > 0)
        {
            FString TrimError;
            const bool bTrimmed = FFZAOverlapRepair::Repair(AreaInput, LargestAreaOwner, {},
                InputKeys, Original, TNumericLimits<double>::Max(), TrimError);
            Test.AddInfo(FString::Printf(TEXT("Captured direct area source: original gaps=%d overlaps=%d trimOwner=%d trimmed=%d reason=%s"),
                Initial.GapPairCount, Initial.OverlapPairCount, LargestAreaOwner, bTrimmed, *TrimError));
            if (!bTrimmed) { return; }
        }
        FFZAGapAreaRepair::FResult Areas;
        const bool bBuilt = FFZAGapAreaRepair::Build(AreaInput, 100.0, Areas);
        FFZAWeldDiagnostics::FResult Residual;
        FFZAWeldDiagnostics::Analyze(Areas.Polygons, {}, 100.0, Residual, 131072);
        Test.AddInfo(FString::Printf(TEXT("Captured direct area candidate: built=%d complete=%d blocked=%d gaps=%d overlaps=%d reason=%s"),
            bBuilt, Areas.bComplete, Areas.bBlocked, Residual.GapPairCount, Residual.OverlapPairCount, *Areas.FailureReason));
        bool bHasInvalidStrictRing = false;
        for (int32 Zone = 0; Zone < Areas.Polygons.Num(); ++Zone)
        {
            const TArray<FVector2D>& Ring = Areas.Polygons[Zone];
            TArray<FVector2D> Local;
            if (Ring.IsEmpty()) { bHasInvalidStrictRing = true; continue; }
            for (const FVector2D& Point : Ring) { Local.Add(Point - Ring[0]); }
            if (FFZAPolygonMath::IsSimplePolygon(Local, 0.01)) { continue; }
            bHasInvalidStrictRing = true;
            Test.AddInfo(FString::Printf(TEXT("Captured direct area Zone %d fails strict 0.01cm topology; loose1e-4=%d points=%d"),
                Zone, FFZAPolygonMath::IsSimplePolygon(Local, 1.e-4), Ring.Num()));
            for (int32 Edge = 0; Edge < Ring.Num(); ++Edge)
            {
                const int32 Next = (Edge + 1) % Ring.Num();
                const double Length = FVector2D::Distance(Ring[Edge], Ring[Next]);
                if (Length > 0.0100001) { continue; }
                const double Key = Areas.SourcePointKeys.IsValidIndex(Zone) && Areas.SourcePointKeys[Zone].IsValidIndex(Edge)
                    ? Areas.SourcePointKeys[Zone][Edge] : -1.0;
                const double NextKey = Areas.SourcePointKeys.IsValidIndex(Zone) && Areas.SourcePointKeys[Zone].IsValidIndex(Next)
                    ? Areas.SourcePointKeys[Zone][Next] : -1.0;
                Test.AddInfo(FString::Printf(TEXT("Captured direct area Zone %d tiny edge %d length=%.17g A(%.17g,%.17g;key=%.17g) B(%.17g,%.17g;key=%.17g)"),
                    Zone, Edge, Length, Ring[Edge].X, Ring[Edge].Y, Key, Ring[Next].X, Ring[Next].Y, NextKey));
            }
        }
        if (bHasInvalidStrictRing)
        {
            FFZAAutoWeld::FResult Diagnostic;
            Diagnostic.Polygons = Areas.Polygons;
            Diagnostic.Stats.SourcePointKeys = Areas.SourcePointKeys;
            Diagnostic.Diagnostics = MoveTemp(Residual);
            Diagnostic.bAreaFallbackUsed = true;
            Diagnostic.FailureReason = TEXT("Direct area candidate before public provenance composition");
            DumpRealLayoutPlan(Test, Diagnostic);
        }
    }

    bool CheckRealLayoutPlan(FAutomationTestBase& Test, const TArray<TArray<FVector2D>>& Original,
        const TArray<TArray<double>>& OriginalHeights, FFZAAutoWeld::FResult& Result)
    {
        if (!Test.TestTrue(TEXT("Captured geometry produces a public automatic preview"),
            FFZAAutoWeld::BuildAutomaticResult(Original, 50.0, Result))) { return false; }
        const bool bApplicable = Test.TestTrue(*FString::Printf(TEXT("The complete captured layout is applicable: %s"),
            *Result.FailureReason), Result.bCanCommit);
        if (!bApplicable) { DumpRealLayoutPlan(Test, Result); }
        Test.TestEqual(TEXT("Default 50cm tolerance uses the 100cm repair radius"), Result.RepairRadiusCm, 100.0);
        Test.TestEqual(TEXT("Every captured Zone retains its output identity"), Result.Polygons.Num(), Original.Num());
        Test.TestEqual(TEXT("Every captured Zone retains a height-provenance row"), Result.Stats.SourcePointKeys.Num(), Original.Num());
        Test.TestEqual(TEXT("The public preview has no remaining gap"), Result.Diagnostics.GapPairCount, 0);
        Test.TestEqual(TEXT("The public preview has no remaining overlap"), Result.Diagnostics.OverlapPairCount, 0);
        Test.TestEqual(TEXT("No captured Zone is rejected"), Result.Stats.RejectedPolygonCount, 0);
        Test.TestEqual(TEXT("No captured Zone is classified as invalid"), Result.Stats.InvalidInputPolygonCount, 0);

        for (int32 Zone = 0; Zone < Original.Num(); ++Zone)
        {
            if (!Result.Polygons.IsValidIndex(Zone) || !Result.Stats.SourcePointKeys.IsValidIndex(Zone)) { continue; }
            const TArray<FVector2D>& Ring = Result.Polygons[Zone];
            const TArray<double>& Keys = Result.Stats.SourcePointKeys[Zone];
            const FString Prefix = FString::Printf(TEXT("Captured Zone %d"), Zone);
            if (!Test.TestTrue(*(Prefix + TEXT(" remains a nonempty boundary")), Ring.Num() >= 3)) { continue; }
            TArray<FVector2D> Local;
            for (const FVector2D& Point : Ring) { Local.Add(Point - Ring[0]); }
            Test.TestTrue(*(Prefix + TEXT(" remains one simple polygon at the editor's 0.01cm tolerance")),
                FFZAPolygonMath::IsSimplePolygon(Local, 0.01));
            Test.TestTrue(*(Prefix + TEXT(" retains its original winding and positive area")),
                FFZAPolygonMath::SignedArea(Local) * FFZAPolygonMath::SignedArea(Original[Zone]) > 0.0);
            if (!Test.TestEqual(*(Prefix + TEXT(" has a height key for every output point")), Keys.Num(), Ring.Num())) { continue; }
            bool bKeysValid = true;
            for (int32 Point = 0; Point < Keys.Num(); ++Point)
            {
                const double Key = Keys[Point];
                const bool bValid = FMath::IsFinite(Key) && Key >= 0.0 && Key < Original[Zone].Num();
                bKeysValid &= Test.TestTrue(*FString::Printf(TEXT("Captured Zone %d point %d source key %.17g lies in its own original closed ring"),
                    Zone, Point, Key), bValid);
            }
            if (!bKeysValid) { continue; }
            TArray<FVector> WorldPoints;
            if (!Test.TestTrue(*(Prefix + TEXT(" passes actual height-preserving writeback preflight")),
                FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(OriginalHeights[Zone], Ring, Keys, WorldPoints))) { continue; }
            if (!Test.TestEqual(*(Prefix + TEXT(" emits every output world point")), WorldPoints.Num(), Ring.Num())) { continue; }
            for (int32 Point = 0; Point < WorldPoints.Num(); ++Point)
            {
                const FVector& World = WorldPoints[Point];
                const int32 Edge = FMath::FloorToInt(Keys[Point]);
                const double ExpectedHeight = FMath::Lerp(OriginalHeights[Zone][Edge],
                    OriginalHeights[Zone][(Edge + 1) % OriginalHeights[Zone].Num()], Keys[Point] - Edge);
                Test.TestTrue(*(Prefix + TEXT(" writeback XYZ remains finite")),
                    FMath::IsFinite(World.X) && FMath::IsFinite(World.Y) && FMath::IsFinite(World.Z));
                Test.TestEqual(*(Prefix + TEXT(" writeback X matches the shared boundary")), World.X, Ring[Point].X);
                Test.TestEqual(*(Prefix + TEXT(" writeback Y matches the shared boundary")), World.Y, Ring[Point].Y);
                Test.TestEqual(*(Prefix + TEXT(" writeback Z is interpolated from its own original edge")), World.Z, ExpectedHeight, 1.e-8);
            }
        }

        FFZAWeldDiagnostics::FResult Independent;
        FFZAWeldDiagnostics::Analyze(Result.Polygons, {}, 100.0, Independent, 131072);
        Test.TestEqual(TEXT("An independent complete inspection finds no exposed gap"), Independent.GapPairCount, 0);
        Test.TestEqual(TEXT("An independent complete inspection finds no positive-area overlap"), Independent.OverlapPairCount, 0);
        return bApplicable;
    }

    bool SameRealLayoutShape(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZARealLayoutAutomaticApplyTest,
    "FF_ZoneAuthoring.Unit.RealLayoutWeld.CapturedThirteenZonesAreApplicableWithoutGapsOrOverlaps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZARealLayoutAutomaticApplyTest::RunTest(const FString& Parameters)
{
    using namespace FFZARealLayoutWeldTestsPrivate;
    const TArray<TArray<FVector>> WorldZones = CapturedWorldZones();
    if (!TestEqual(TEXT("The fixed regression contains all thirteen captured loaded Zones"), WorldZones.Num(), 13)) { return false; }
    TArray<TArray<FVector2D>> Original;
    TArray<TArray<double>> OriginalHeights;
    ExtractCapturedGeometry(WorldZones, Original, OriginalHeights);
    const auto Before = Original;
    const auto BeforeHeights = OriginalHeights;
    FFZAWeldDiagnostics::FResult Initial;
    FFZAWeldDiagnostics::Analyze(Original, {}, 100.0, Initial, 131072);
    TestTrue(TEXT("The actual source layout reproduces positive exposed gaps before repair"), Initial.GapPairCount > 0);
    AddInfo(FString::Printf(TEXT("Captured source layout has %d gap pairs and %d positive overlap pairs"),
        Initial.GapPairCount, Initial.OverlapPairCount));
    DiagnoseCapturedAreaCandidate(*this, Original, Initial);
    FFZAAutoWeld::FResult Result;
    CheckRealLayoutPlan(*this, Original, OriginalHeights, Result);
    TestTrue(TEXT("The public planner preserves every original captured XY coordinate"), Original == Before);
    TestTrue(TEXT("The public planner preserves every original captured world height"), OriginalHeights == BeforeHeights);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZARealLayoutIdempotenceTest,
    "FF_ZoneAuthoring.Unit.RealLayoutWeld.ReapplyingCapturedLayoutPreservesAllBoundaryPointsAndHeights",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZARealLayoutIdempotenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZARealLayoutWeldTestsPrivate;
    TArray<TArray<FVector2D>> Original;
    TArray<TArray<double>> OriginalHeights;
    ExtractCapturedGeometry(CapturedWorldZones(), Original, OriginalHeights);
    if (!TestEqual(TEXT("The repeat regression retains all thirteen Zones"), Original.Num(), 13)) { return false; }
    FFZAAutoWeld::FResult First;
    if (!CheckRealLayoutPlan(*this, Original, OriginalHeights, First)) { return false; }
    TArray<TArray<double>> RepairedHeights;
    RepairedHeights.SetNum(First.Polygons.Num());
    for (int32 Zone = 0; Zone < First.Polygons.Num(); ++Zone)
    {
        TArray<FVector> WorldPoints;
        if (!TestTrue(TEXT("First apply has a complete height-preserving writeback"),
            FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(OriginalHeights[Zone],
                First.Polygons[Zone], First.Stats.SourcePointKeys[Zone], WorldPoints))) { return false; }
        for (const FVector& WorldPoint : WorldPoints) { RepairedHeights[Zone].Add(WorldPoint.Z); }
    }
    const auto Before = First.Polygons;
    FFZAAutoWeld::FResult Second;
    if (!CheckRealLayoutPlan(*this, First.Polygons, RepairedHeights, Second)) { return false; }
    TestEqual(TEXT("A connected actual layout produces no additional Zone writes"), Second.Stats.ChangedPolygonCount, 0);
    TestEqual(TEXT("A connected actual layout moves no existing corner"), Second.Stats.MovedVertexCount, 0);
    TestEqual(TEXT("A connected actual layout inserts no additional point"), Second.Stats.InsertedVertexCount, 0);
    TestEqual(TEXT("A connected actual layout deletes no point"), Second.Stats.RemovedVertexCount, 0);
    TestTrue(TEXT("Repeat apply preserves all point counts, coordinates and order exactly"), Second.Polygons == Before);
    TestTrue(TEXT("Repeat preview never mutates its current snapshot"), First.Polygons == Before);
    for (int32 Zone = 0; Zone < Second.Polygons.Num(); ++Zone)
    {
        TArray<FVector> WorldPoints;
        if (!FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(RepairedHeights[Zone],
            Second.Polygons[Zone], Second.Stats.SourcePointKeys[Zone], WorldPoints)) { continue; }
        for (int32 Point = 0; Point < WorldPoints.Num(); ++Point)
        {
            TestEqual(TEXT("Repeat apply preserves each repaired point's own world height"),
                WorldPoints[Point].Z, RepairedHeights[Zone][Point], 1.e-8);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZARealLayoutPermutationTest,
    "FF_ZoneAuthoring.Unit.RealLayoutWeld.CapturedSharedSeamsIgnoreCollectionOrderSplineDirectionAndStart",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZARealLayoutPermutationTest::RunTest(const FString& Parameters)
{
    using namespace FFZARealLayoutWeldTestsPrivate;
    const TArray<TArray<FVector>> Identity = CapturedWorldZones();
    if (!TestEqual(TEXT("The permutation regression retains all thirteen Zone identities"), Identity.Num(), 13)) { return false; }
    TArray<TArray<FVector2D>> Original;
    TArray<TArray<double>> OriginalHeights;
    ExtractCapturedGeometry(Identity, Original, OriginalHeights);
    FFZAAutoWeld::FResult Baseline;
    if (!CheckRealLayoutPlan(*this, Original, OriginalHeights, Baseline)) { return false; }
    TArray<TArray<FVector>> PermutedWorldZones;
    TArray<int32> OriginalIndices;
    for (int32 Slot = 0; Slot < Identity.Num(); ++Slot)
    {
        const int32 OriginalIndex = Identity.Num() - 1 - Slot;
        TArray<FVector> Ring = Identity[OriginalIndex];
        Algo::Reverse(Ring);
        const int32 Start = Slot % Ring.Num();
        TArray<FVector> Rotated;
        for (int32 Point = 0; Point < Ring.Num(); ++Point) { Rotated.Add(Ring[(Start + Point) % Ring.Num()]); }
        PermutedWorldZones.Add(MoveTemp(Rotated));
        OriginalIndices.Add(OriginalIndex);
    }
    TArray<TArray<FVector2D>> Permuted;
    TArray<TArray<double>> PermutedHeights;
    ExtractCapturedGeometry(PermutedWorldZones, Permuted, PermutedHeights);
    const auto Before = Permuted;
    FFZAAutoWeld::FResult Result;
    if (!CheckRealLayoutPlan(*this, Permuted, PermutedHeights, Result)) { return false; }
    TestTrue(TEXT("Changing collection order or spline direction leaves the permuted source intact"), Permuted == Before);
    for (int32 Slot = 0; Slot < Result.Polygons.Num(); ++Slot)
    {
        const TArray<FVector2D>& BaselineRing = Baseline.Polygons[OriginalIndices[Slot]];
        const TArray<FVector2D>& PermutedRing = Result.Polygons[Slot];
        if (!TestTrue(*FString::Printf(TEXT("Captured Zone %d receives the same shared shape after collection and spline permutation"), OriginalIndices[Slot]),
            SameRealLayoutShape(BaselineRing, PermutedRing)))
        {
            FString BaselineCoordinates;
            FString PermutedCoordinates;
            for (const FVector2D& Point : BaselineRing)
            {
                BaselineCoordinates += FString::Printf(TEXT("(%.17g,%.17g),"), Point.X, Point.Y);
            }
            for (const FVector2D& Point : PermutedRing)
            {
                PermutedCoordinates += FString::Printf(TEXT("(%.17g,%.17g),"), Point.X, Point.Y);
            }
            AddInfo(FString::Printf(TEXT("Permutation mismatch original Zone %d baseline [%d]: %s"),
                OriginalIndices[Slot], BaselineRing.Num(), *BaselineCoordinates));
            AddInfo(FString::Printf(TEXT("Permutation mismatch original Zone %d permuted slot %d [%d]: %s"),
                OriginalIndices[Slot], Slot, PermutedRing.Num(), *PermutedCoordinates));
        }
    }
    return true;
}

#endif
