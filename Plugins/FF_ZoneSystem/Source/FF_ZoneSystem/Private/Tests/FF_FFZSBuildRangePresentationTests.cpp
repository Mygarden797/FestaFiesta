#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangePresentation.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSOrientedGeometry.h"
#include "SceneManagement.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRangeFailurePresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.FailureRegionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRangeFailurePresentationTest::RunTest(const FString& Parameters)
{
    using namespace FFZSBuildRangePresentation;
    FFFZSPlacementValidation Validation;
    Validation.Reasons = { EFFZSPlacementFailure::InsufficientFunds, EFFZSPlacementFailure::WrongStage,
        EFFZSPlacementFailure::RequiredRouteDisconnected, EFFZSPlacementFailure::OutsideZone };
    TestFalse(TEXT("A nonlocalized failure does not invent a footprint collision"), HasRegionFailure(Validation, ERegion::Footprint));
    TestFalse(TEXT("A nonlocalized failure does not invent a clearance collision"), HasRegionFailure(Validation, ERegion::Clearance));
    TestFalse(TEXT("A nonlocalized failure does not invent a queue collision"), HasRegionFailure(Validation, ERegion::Queue));
    Validation.Reasons.Add(EFFZSPlacementFailure::InsufficientClearance);
    TestTrue(TEXT("Clearance reason highlights the actual clearance group"), HasRegionFailure(Validation, ERegion::Clearance));
    TestFalse(TEXT("Clearance failure does not spill into footprint"), HasRegionFailure(Validation, ERegion::Footprint));
    TestFalse(TEXT("Clearance failure does not spill into queue"), HasRegionFailure(Validation, ERegion::Queue));
    Validation.Reasons.Add(EFFZSPlacementFailure::QueueFootprintBlocked);
    TestTrue(TEXT("Queue reason highlights its own group"), HasRegionFailure(Validation, ERegion::Queue));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSlotPresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.QueueSlotsUseFacilityTransformOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueSlotPresentationTest::RunTest(const FString& Parameters)
{
    TGuardValue<bool> ShowSlots(GetMutableDefault<UFFZSBuildPresentationSettings>()->bShowQueueSlots, true);
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    FFFZSFacilitySpec Spec;
    Spec.QueueSplinePointsCm = { FVector(0, 0, 0), FVector(100, 0, 0), FVector(100, 100, 0) };
    Spec.QueueRules.MaxWaitingAgents = 3;
    Spec.QueueRules.SlotSpacingCm = 75.0f;
    const FTransform Transform(FRotator(0, 90, 0), FVector(1000, 2000, 30));
    TArray<FBatchedLine> Lines;
    FFZSBuildRangePresentation::AppendQueueSlots(Lines, Spec, Transform, Style.QueueColor, Style);
    if (!TestEqual(TEXT("Three real slots produce three four-edge markers"), Lines.Num(), 12)) return false;
    const double Z = 30.0 + FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
    const FVector Expected[] = { FVector(1000, 2000, Z), FVector(1000, 2075, Z), FVector(950, 2100, Z) };
    for (int32 Slot = 0; Slot < 3; ++Slot)
    {
        FVector Center = FVector::ZeroVector;
        for (int32 Edge = 0; Edge < 4; ++Edge) Center += Lines[Slot * 4 + Edge].Start;
        TestTrue(TEXT("Bent queue marker uses the correct distance and one world transform"),
            (Center / 4.0).Equals(Expected[Slot], 0.001));
    }
    TestEqual(TEXT("Drawing leaves queue capacity unchanged"), Spec.QueueRules.MaxWaitingAgents, 3);
    TestEqual(TEXT("Drawing leaves queue spacing unchanged"), Spec.QueueRules.SlotSpacingCm, 75.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSRangeOutlinePresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.ExactRotatedOutlineAndBoundedDashes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSRangeOutlinePresentationTest::RunTest(const FString& Parameters)
{
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    FFZSBuildRangePresentation::FWorldCorners Corners;
    Corners[0] = FVector(0, 50, 4);
    Corners[1] = FVector(50, 100, 4);
    Corners[2] = FVector(100, 50, 4);
    Corners[3] = FVector(50, 0, 4);
    TArray<FBatchedLine> Lines;
    FFZSBuildRangePresentation::AppendOutline(Lines, Corners, Style.FootprintColor, false, false, Style);
    if (!TestEqual(TEXT("Rotated footprint stays four planar edges"), Lines.Num(), 4)) return false;
    for (int32 Edge = 0; Edge < 4; ++Edge)
    {
        TestTrue(TEXT("Outline preserves each exact rotated corner"), Lines[Edge].Start.Equals(Corners[Edge]));
        TestTrue(TEXT("Outline never substitutes an enlarged AABB"), Lines[Edge].End.Equals(Corners[(Edge + 1) % 4]));
    }
    for (FVector& Corner : Corners) Corner *= 100000.0;
    Lines.Reset();
    FFZSBuildRangePresentation::AppendOutline(Lines, Corners, Style.ClearanceColor, true, false, Style);
    TestTrue(TEXT("Huge display outline has a bounded dash budget"), Lines.Num() <= 260);
    TestEqual(TEXT("Readable projected spacing retains all minor lines"),
        FFZSBuildRangePresentation::ResolveMinorLineStride(20.0f, 9.0f), 1);
    TestEqual(TEXT("Distant grid keeps major and boundary lines only"),
        FFZSBuildRangePresentation::ResolveMinorLineStride(1.0f, 9.0f), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSPlacementObstructionBudgetTest,
    "FF_ZoneSystem.Unit.BuildPresentation.ObstructionBudgetAndAuthorityIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPlacementObstructionBudgetTest::RunTest(const FString& Parameters)
{
    FFFZSPlacementValidation Validation;
    Validation.bIsValid = true;
    FFFZSPlacementObstruction Obstruction;
    Obstruction.Reason = EFFZSPlacementFailure::PlacementCollision;
    Obstruction.WorldCenter = FVector(200, 300, 40);
    Obstruction.WorldHalfExtent = FVector(100, 20, 40);
    Obstruction.WorldRotation = FQuat(FVector::UpVector, PI / 4.0);
    Validation.AddObstruction(Obstruction);
    Validation.AddObstruction(Obstruction);
    TestEqual(TEXT("Identical obstruction is not duplicated"), Validation.Obstructions.Num(), 1);
    TestTrue(TEXT("Diagnostics never authorize or reject a command"), Validation.bIsValid && Validation.Reasons.IsEmpty());
    if (!Validation.Obstructions.IsEmpty())
        TestTrue(TEXT("World bounds contain the rotated box, not an unrotated extent"),
            Validation.Obstructions[0].WorldBounds.GetExtent().Y > 80.0);
    for (int32 Index = 0; Index < 1000; ++Index)
    {
        Obstruction.WorldCenter.X += 10;
        Validation.AddObstruction(Obstruction);
    }
    TestEqual(TEXT("Dense scenes keep a fixed diagnostic memory budget"),
        Validation.Obstructions.Num(), FFFZSPlacementValidation::MaxObstructions);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSPlacementFeedbackGeometryTest,
    "FF_ZoneSystem.Unit.BuildPresentation.PlacementFeedbackGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSPlacementFeedbackGeometryTest::RunTest(const FString& Parameters)
{
    using namespace FFZSBuildRangePresentation;
    const FFFZSOrientedRect Region(FVector2D(1700, 2400), FVector2D(400, 70), 37.0f);
    FWorldCorners Corners;
    const auto XY = FFZSOrientedGeometry::GetCorners(Region);
    for (int32 Index = 0; Index < 4; ++Index) Corners[Index] = FVector(XY[Index], 8);
    TArray<FBatchedLine> Lines;
    AppendFailureHatching(Lines, Corners, FLinearColor::Red);
    TestEqual(TEXT("Hatching cost is independent of world area"), Lines.Num(), 15);
    for (const FBatchedLine& Line : Lines)
    {
        TestEqual(TEXT("Feedback remains visible behind a building"), Line.DepthPriority, uint8(SDPG_Foreground));
        TestEqual(TEXT("Persistent actor owns lifetime, not one debug frame"), Line.RemainingLifeTime, 0.0f);
        for (const FVector& Point : {Line.Start, Line.End})
        {
            const FVector2D Local = (FVector2D(Point) - Region.CenterCell).GetRotated(-Region.YawDegrees);
            TestTrue(TEXT("Every hatch endpoint is inside the exact rotated reservation"),
                FMath::Abs(Local.X) <= Region.HalfExtentCells.X + 0.01 &&
                FMath::Abs(Local.Y) <= Region.HalfExtentCells.Y + 0.01);
        }
    }
    FFFZSOrientedRect FoundA, FoundB;
    const TArray<FFFZSOrientedRect> A = {Region};
    const TArray<FFFZSOrientedRect> B = {FFFZSOrientedRect(Region.CenterCell, FVector2D(20, 20), 0)};
    TestTrue(TEXT("Existing SAT returns its actual first collision pair"),
        UFFZSGridLibrary::DoOrientedRectSetsOverlap(A, B, &FoundA, &FoundB));
    TestTrue(TEXT("Diagnostic retains the authoritative yaw"), FMath::IsNearlyEqual(FoundA.YawDegrees, 37.0f));
    FFFZSPlacementValidation Validation;
    FFFZSPlacementObstruction Obstruction;
    Obstruction.Reason = EFFZSPlacementFailure::PlacementCollision;
    Obstruction.WorldCenter = FVector(100, 200, 50);
    Obstruction.WorldHalfExtent = FVector(40, 30, 50);
    Validation.AddObstruction(Obstruction);
    Lines.Reset();
    AppendObstructionOutline(Lines, Validation.Obstructions[0], FLinearColor::Yellow);
    TestEqual(TEXT("Blocker brackets have a fixed 24 segment budget"), Lines.Num(), 24);
    return true;
}

#endif
