// Selected-scope regressions use only arrays/transient components, never level assets.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "FFZALayoutOps.h"
#include "FFZAPolygonMath.h"
#include "FFZASplineAuthoring.h"
#include "FF_ZoneAuthoringModule.h"
#include "ToolMenus.h"
#include "Weld/FFZASelectedWeldSession.h"
#include <limits>

namespace FFZASelectedWeldTestsPrivate
{
    TArray<FVector2D> MakeSelectedWeldRectangle(double X0, double Y0, double X1, double Y1)
    {
        return { FVector2D(X0, Y0), FVector2D(X1, Y0), FVector2D(X1, Y1), FVector2D(X0, Y1) };
    }

    bool HasEdge(const TArray<FVector2D>& Ring, const FVector2D& A, const FVector2D& B)
    {
        for (int32 I = 0; I < Ring.Num(); ++I)
        {
            const FVector2D& P = Ring[I];
            const FVector2D& Q = Ring[(I + 1) % Ring.Num()];
            if ((P == A && Q == B) || (P == B && Q == A)) { return true; }
        }
        return false;
    }

    void CheckRepeat(FAutomationTestBase& Test, TArray<TArray<FVector2D>>& Polygons, int32 Reference)
    {
        const TArray<TArray<FVector2D>> Before = Polygons;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        Test.TestEqual(TEXT("Repeat moves no points"),
            FFZALayoutOps::WeldSelectedPolygons(Polygons, Reference, 50.0, Changed, &Stats), 0);
        Test.TestTrue(TEXT("Repeat preserves all coordinates and counts"), Polygons == Before);
        Test.TestEqual(TEXT("Repeat inserts no points"), Stats.InsertedVertexCount, 0);
        Test.TestEqual(TEXT("Repeat reports no changes"), Stats.ChangedPolygonCount, 0);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldManualReferenceTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.ManualReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldManualReferenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Input = { MakeSelectedWeldRectangle(0, 0, 1000, 1000), MakeSelectedWeldRectangle(1020, 0, 2000, 1000) };
    for (int32 Reference = 0; Reference < 2; ++Reference)
    {
        TArray<TArray<FVector2D>> Polygons = Input;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        TestEqual(TEXT("Explicit reference welds the two facing points"),
            FFZALayoutOps::WeldSelectedPolygons(Polygons, Reference, 50.0, Changed, &Stats), 2);
        TestTrue(TEXT("Explicit reference retains its entire authored ring"), Polygons[Reference] == Input[Reference]);
        const double X = Reference == 0 ? 1000.0 : 1020.0;
        TestTrue(TEXT("Both rings use the manually selected reference edge"),
            HasEdge(Polygons[0], FVector2D(X, 0), FVector2D(X, 1000)) &&
            HasEdge(Polygons[1], FVector2D(X, 0), FVector2D(X, 1000)));
        TestEqual(TEXT("Maximum movement is reported for preview"), Stats.MaximumMoveCm, 20.0, 1e-6);
        CheckRepeat(*this, Polygons, Reference);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldOrderTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.SelectionOrderIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldOrderTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Authored = {
        MakeSelectedWeldRectangle(0, 0, 1000, 1000), MakeSelectedWeldRectangle(1020, 0, 2000, 1000), MakeSelectedWeldRectangle(2020, 0, 3000, 1000)
    };
    const TArray<TArray<int32>> Orders = { {0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0} };
    TArray<TArray<FVector2D>> Expected;
    for (const TArray<int32>& Order : Orders)
    {
        TArray<TArray<FVector2D>> Polygons;
        for (int32 Identity : Order) { Polygons.Add(Authored[Identity]); }
        const int32 Reference = Order.Find(1);
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        FFZALayoutOps::WeldSelectedPolygons(Polygons, Reference, 50.0, Changed, &Stats);
        TArray<TArray<FVector2D>> ByIdentity;
        ByIdentity.SetNum(Authored.Num());
        for (int32 I = 0; I < Order.Num(); ++I)
        {
            ByIdentity[Order[I]] = Polygons[I];
            TestTrue(TEXT("Output provenance remains aligned with caller indices"),
                Stats.SourcePointKeys[I].Num() == Polygons[I].Num());
        }
        if (Expected.IsEmpty()) { Expected = ByIdentity; }
        TestTrue(TEXT("All six selection orders produce identical actor geometry"), ByIdentity == Expected);
        TestTrue(TEXT("Manual middle reference is unchanged"), Polygons[Reference] == Authored[1]);
        CheckRepeat(*this, Polygons, Reference);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldDisconnectedTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.DisconnectedScope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldDisconnectedTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeSelectedWeldRectangle(0, 0, 1000, 1000), MakeSelectedWeldRectangle(1020, 0, 2000, 1000),
        MakeSelectedWeldRectangle(5000, 0, 6000, 1000), MakeSelectedWeldRectangle(6020, 0, 7000, 1000)
    };
    const TArray<TArray<FVector2D>> Original = Polygons;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygons(Polygons, 0, 50.0, Changed, &Stats);
    TestTrue(TEXT("Disconnected selected pair does not acquire an automatic reference"),
        Polygons[2] == Original[2] && Polygons[3] == Original[3]);
    TestTrue(TEXT("Disconnected indices identify caller scope"),
        Stats.DisconnectedPolygonIndices.Contains(2) && Stats.DisconnectedPolygonIndices.Contains(3));
    TestFalse(TEXT("Disconnected zone is not changed"), Changed[2]);
    TestFalse(TEXT("Other disconnected zone is not changed"), Changed[3]);
    CheckRepeat(*this, Polygons, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldConnectedChainTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.ReferenceConnectedChain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldConnectedChainTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    // Input order places the farthest target first; only the middle target touches the manual root.
    TArray<TArray<FVector2D>> Polygons = { MakeSelectedWeldRectangle(2020, 0, 3000, 1000), MakeSelectedWeldRectangle(1020, 0, 2000, 1000), MakeSelectedWeldRectangle(0, 0, 1000, 1000) };
    const TArray<FVector2D> Reference = Polygons[2];
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygons(Polygons, 2, 50.0, Changed, &Stats);
    TestTrue(TEXT("Direct root contact is welded first"), HasEdge(Polygons[1], FVector2D(1000, 0), FVector2D(1000, 1000)));
    TestTrue(TEXT("Connected target inherits the frozen nearer anchor"), HasEdge(Polygons[0], FVector2D(2000, 0), FVector2D(2000, 1000)));
    TestTrue(TEXT("Root retains authored shape"), Polygons[2] == Reference);
    TestEqual(TEXT("Connected chain has no disconnected zones"), Stats.DisconnectedPolygonIndices.Num(), 0);
    CheckRepeat(*this, Polygons, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldStepTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.FinalizedStepAnchors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldStepTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    TArray<TArray<FVector2D>> Polygons = {
        MakeSelectedWeldRectangle(1020, 0, 2000, 1000), MakeSelectedWeldRectangle(0, 0, 1000, 1000), MakeSelectedWeldRectangle(1005, 400, 1015, 600)
    };
    const double ReferenceArea = FFZAPolygonMath::Area(Polygons[1]);
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygons(Polygons, 1, 50.0, Changed, &Stats);
    TestTrue(TEXT("Narrow neighbour retains its finalized root contact"),
        HasEdge(Polygons[2], FVector2D(1000, 400), FVector2D(1000, 600)));
    TestTrue(TEXT("Exposed secondary anchor owns the step span"),
        HasEdge(Polygons[0], FVector2D(1015, 400), FVector2D(1015, 600)));
    TestTrue(TEXT("Both step shoulders are retained"),
        HasEdge(Polygons[0], FVector2D(1000, 400), FVector2D(1015, 400)) &&
        HasEdge(Polygons[0], FVector2D(1000, 600), FVector2D(1015, 600)));
    TestEqual(TEXT("Reference shape is fixed despite added nodes"), FFZAPolygonMath::Area(Polygons[1]), ReferenceArea, 1e-6);
    TestEqual(TEXT("Valid anchors are not rejected"), Stats.RejectedPolygonCount, 0);
    for (const TArray<FVector2D>& Ring : Polygons) { TestTrue(TEXT("Selected output is simple"), FFZAPolygonMath::IsSimplePolygon(Ring)); }
    CheckRepeat(*this, Polygons, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldInvalidReferenceTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.InvalidReferenceAndTolerance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldInvalidReferenceTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    const TArray<TArray<FVector2D>> Original = { MakeSelectedWeldRectangle(0, 0, 1000, 1000), MakeSelectedWeldRectangle(1020, 0, 2000, 1000) };
    for (int32 Reference : TArray<int32>{ INDEX_NONE, 2 })
    {
        TArray<TArray<FVector2D>> Polygons = Original;
        TArray<bool> Changed;
        FFZALayoutOps::FWeldStats Stats;
        TestEqual(TEXT("Missing or out-of-scope reference does not mutate geometry"),
            FFZALayoutOps::WeldSelectedPolygons(Polygons, Reference, 50.0, Changed, &Stats), 0);
        TestTrue(TEXT("Invalid reference is explicit"), Stats.bInvalidReference);
        TestTrue(TEXT("All inputs remain untouched"), Polygons == Original);
        TestEqual(TEXT("No changed polygons"), Stats.ChangedPolygonCount, 0);
    }
    for (double Tolerance : { 0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
    {
        TArray<TArray<FVector2D>> Polygons = Original;
        TArray<bool> Changed;
        TestEqual(TEXT("Invalid selected tolerance is a no-op"), FFZALayoutOps::WeldSelectedPolygons(Polygons, 0, Tolerance, Changed), 0);
        TestTrue(TEXT("Bad tolerance does not mutate source"), Polygons == Original);
    }
    TArray<TArray<FVector2D>> Invalid = { { FVector2D(0, 0), FVector2D(1000, 1000), FVector2D(0, 1000), FVector2D(1000, 0) }, Original[1] };
    const auto Before = Invalid;
    TArray<bool> Changed;
    FFZALayoutOps::FWeldStats Stats;
    FFZALayoutOps::WeldSelectedPolygons(Invalid, 0, 50.0, Changed, &Stats);
    TestTrue(TEXT("Invalid root aborts all selected welding"), Stats.bInvalidReference && Invalid == Before);
    TestTrue(TEXT("Invalid input diagnostics retain caller index"), Stats.InvalidInputPolygonIndices.Contains(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldWritebackPreflightTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.WritebackPreflight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldWritebackPreflightTest::RunTest(const FString& Parameters)
{
    using namespace FFZASelectedWeldTestsPrivate;
    const TArray<double> Heights = { 10.0, 30.0, 70.0, 90.0 };
    const TArray<FVector2D> Ring = MakeSelectedWeldRectangle(0, 0, 1000, 1000);
    TArray<FVector> Output = { FVector(123, 456, 789) };
    const TArray<FVector> Before = Output;
    TestFalse(TEXT("All writeback points must pass preflight before any output is changed"),
        FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights, Ring, { 0.0, 1.0, 2.0, 4.0 }, Output));
    TestTrue(TEXT("Preflight failure is atomic"), Output == Before);
    TestTrue(TEXT("Preview can compute world points with no actors or level"),
        FFZASplineAuthoring::BuildWorldPolygonPreservingHeights(Heights, Ring, { 0.0, 0.5, 3.5, 3.5 }, Output));
    TestEqual(TEXT("Duplicate provenance at shoulder points preserves matching heights"), Output[2].Z, Output[3].Z);
    TestEqual(TEXT("Closing-edge inserted height"), Output[2].Z, 50.0, 1e-6);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldStaleSnapshotTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.StalePreviewGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldStaleSnapshotTest::RunTest(const FString& Parameters)
{
    FFFZASelectedWeldGeometrySnapshot Before;
    Before.Points = { FVector(0, 0, 10), FVector(1000, 0, 20), FVector(1000, 1000, 30), FVector(0, 1000, 40) };
    Before.PointTypes = { 0, 0, 0, 0 };
    Before.bClosedLoop = true;
    TestTrue(TEXT("Unchanged geometry passes the actual apply preflight predicate"), Before.Matches(Before));

    TArray<FFFZASelectedWeldGeometrySnapshot> Changed;
    Changed.Add(Before); Changed.Last().Points[1].X += 0.001;
    Changed.Add(Before); Changed.Last().Points[1].Z += 1.0;
    Changed.Add(Before); Changed.Last().Points.RemoveAt(1);
    Changed.Add(Before); Changed.Last().PointTypes[1] = 1;
    Changed.Add(Before); Changed.Last().bClosedLoop = false;
    Changed.Add(Before); Changed.Last().ActorTransform.SetLocation(FVector(10, 0, 0));
    Changed.Add(Before); Changed.Last().SplineTransform.SetScale3D(FVector(2, 1, 1));
    for (const FFFZASelectedWeldGeometrySnapshot& After : Changed)
    {
        TestFalse(TEXT("Coordinate, Z, topology, type, closure and transform edits invalidate preview"), Before.Matches(After));
        TestFalse(TEXT("Snapshot comparison is symmetric"), After.Matches(Before));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZASelectedWeldEditorEntryPointsTest,
    "FF_ZoneAuthoring.Unit.SelectedWeld.EditorEntryPoints",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZASelectedWeldEditorEntryPointsTest::RunTest(const FString& Parameters)
{
    // Registration is inspected without opening a window, changing selection, or applying a weld.
    TestTrue(TEXT("Manager request is bound to the editor tool"), FFZAGetSelectedWeldRequest().IsBound());
    UToolMenus* Menus = UToolMenus::TryGet();
    if (!TestNotNull(TEXT("Editor menus are available"), Menus)) { return false; }
    for (const FName Name : { FName(TEXT("LevelEditor.ActorContextMenu")), FName(TEXT("LevelEditor.SceneOutlinerContextMenu")) })
    {
        UToolMenu* Menu = Menus->FindMenu(Name);
        if (TestNotNull(TEXT("Selected actor context menu exists"), Menu))
        {
            FToolMenuSection* Section = Menu->FindSection(TEXT("FFZoneAuthoring"));
            if (TestNotNull(TEXT("Zone Authoring menu section is registered"), Section))
            {
                TestNotNull(TEXT("Selected boundary weld entry is registered"), Section->FindEntry(TEXT("FFZASelectedBoundaryWeld")));
            }
        }
    }
    return true;
}
#endif
