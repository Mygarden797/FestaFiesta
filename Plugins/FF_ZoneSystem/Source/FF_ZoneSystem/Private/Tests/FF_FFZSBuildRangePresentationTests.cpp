// FF_FFZSBuildRangePresentationTests.cpp
// 역할: 표시 영역이 실제 예약 모양과 일치하고 표시가 설치 권한을 바꾸지 않는지 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangePresentation.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSBuildAreaGeometry.h"
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
    TestFalse(TEXT("Legacy clearance no longer highlights a retired safety region"), HasRegionFailure(Validation, ERegion::Clearance));
    TestTrue(TEXT("The preserved reservation failure highlights audience space"), HasRegionFailure(Validation, ERegion::Audience));
    TestFalse(TEXT("Clearance failure does not spill into footprint"), HasRegionFailure(Validation, ERegion::Footprint));
    TestFalse(TEXT("Clearance failure does not spill into queue"), HasRegionFailure(Validation, ERegion::Queue));
    Validation.Reasons.Add(EFFZSPlacementFailure::QueueFootprintBlocked);
    TestTrue(TEXT("Queue reason highlights its own group"), HasRegionFailure(Validation, ERegion::Queue));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSSelectedFailurePresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.SelectedFailureMatchesExplanation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSelectedFailurePresentationTest::RunTest(const FString& Parameters)
{
    using namespace FFZSBuildRangePresentation;
    FFFZSPlacementValidation Validation;
    Validation.Reasons = { EFFZSPlacementFailure::OutsideZone, EFFZSPlacementFailure::PlacementCollision,
        EFFZSPlacementFailure::InsufficientClearance, EFFZSPlacementFailure::QueueFootprintBlocked };
    const auto OriginalReasons = Validation.Reasons;
    const EFFZSPlacementFailure Primary = ResolveDisplayedFailureReason(Validation, EFFZSPlacementFailure::None);
    TestEqual(TEXT("Reset selects the same primary problem as the text panel"), Primary, EFFZSPlacementFailure::OutsideZone);
    TestFalse(TEXT("Boundary explanation cannot tint the whole building red despite another collision"),
        IsFailureForRegion(Primary, ERegion::Footprint));
    const EFFZSPlacementFailure Audience = ResolveDisplayedFailureReason(Validation, EFFZSPlacementFailure::InsufficientClearance);
    TestTrue(TEXT("Selecting audience collision identifies its own reservation"), IsFailureForRegion(Audience, ERegion::Audience));
    TestFalse(TEXT("Audience explanation cannot leak into the simultaneous building collision"), IsFailureForRegion(Audience, ERegion::Footprint));
    TestFalse(TEXT("Audience explanation cannot leak into the simultaneous queue collision"), IsFailureForRegion(Audience, ERegion::Queue));
    const EFFZSPlacementFailure Footprint = ResolveDisplayedFailureReason(Validation, EFFZSPlacementFailure::PlacementCollision);
    TestTrue(TEXT("Selecting building collision restores its own red ghost and region"), IsFailureForRegion(Footprint, ERegion::Footprint));
    TestFalse(TEXT("Building collision does not highlight audience space"), IsFailureForRegion(Footprint, ERegion::Audience));
    TestTrue(TEXT("Display selection preserves all authoritative failures"), Validation.Reasons == OriginalReasons);
    Validation.Reasons.Remove(EFFZSPlacementFailure::PlacementCollision);
    TestEqual(TEXT("A resolved selection falls back to the current primary reason"),
        ResolveDisplayedFailureReason(Validation, Footprint), EFFZSPlacementFailure::OutsideZone);
    TestEqual(TEXT("A still-present selection survives other problem changes"),
        ResolveDisplayedFailureReason(Validation, Audience), Audience);
    Validation.bIsValid = true;
    TestEqual(TEXT("Valid placement never resurrects stale diagnostics"),
        ResolveDisplayedFailureReason(Validation, Audience), EFFZSPlacementFailure::None);
    Validation.bIsValid = false;
    Validation.Reasons.Reset();
    TestEqual(TEXT("No diagnosis produces no problem overlay"),
        ResolveDisplayedFailureReason(Validation, Audience), EFFZSPlacementFailure::None);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueuePathPresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.QueuePathKeepsBendsAndHeadDirection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueuePathPresentationTest::RunTest(const FString& Parameters)
{
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    FFFZSFacilitySpec Spec;
    // Duplicate head is legal; spacing deliberately skips the corner between actual slots.
    Spec.QueueSplinePointsCm = { FVector(0, 0, 0), FVector(0, 0, 0), FVector(100, 0, 0), FVector(100, 100, 0) };
    Spec.QueueRules.MaxWaitingAgents = 3;
    Spec.QueueRules.SlotSpacingCm = 75.0f;
    const FTransform Transform(FRotator(0, 90, 0), FVector(1000, 2000, 30));
    TArray<FBatchedLine> Lines;
    FFZSBuildRangePresentation::AppendQueuePath(Lines, Spec, Transform, Style.QueueColor, Style, true);
    if (!TestEqual(TEXT("Bent path has two true segments and one two-arm head arrow"), Lines.Num(), 4)) return false;
    const double Z = 30.0 + FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
    const FVector Head(1000, 2000, Z), Bend(1000, 2100, Z), Tail(900, 2100, Z);
    TestTrue(TEXT("Path uses the facility transform once and retains the actual bend"),
        Lines[0].Start.Equals(Head, 0.001) && Lines[0].End.Equals(Bend, 0.001) &&
        Lines[3].Start.Equals(Bend, 0.001) && Lines[3].End.Equals(Tail, 0.001));
    TestTrue(TEXT("The arrow points toward the queue head, not its tail"),
        Lines[1].End.Equals(Head, 0.001) && Lines[2].End.Equals(Head, 0.001));
    for (const FBatchedLine& Line : Lines)
        TestEqual(TEXT("Active queue stays visible behind nearby meshes"), Line.DepthPriority, uint8(SDPG_Foreground));
    Lines.Reset();
    FFZSBuildRangePresentation::AppendQueuePath(Lines, Spec, Transform, Style.QueueColor, Style);
    for (const FBatchedLine& Line : Lines)
        TestEqual(TEXT("Installed queue remains depth tested to avoid background clutter"), Line.DepthPriority, uint8(SDPG_World));
    Spec.QueueSplinePointsCm.Reset();
    for (int32 Index = 0; Index < 1000; ++Index) Spec.QueueSplinePointsCm.Add(FVector(Index * 100.0, 0, 0));
    Lines.Reset();
    FFZSBuildRangePresentation::AppendQueuePath(Lines, Spec, Transform, Style.QueueColor, Style);
    TestTrue(TEXT("Malformed long paths keep a fixed display budget"), Lines.Num() <= 130);
    Spec.QueueRules.MaxWaitingAgents = 0;
    Lines.Reset();
    FFZSBuildRangePresentation::AppendQueuePath(Lines, Spec, Transform, Style.QueueColor, Style);
    TestTrue(TEXT("No waiting capacity invents no queue path"), Lines.IsEmpty());
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
    Lines.Reset();
    FFZSBuildRangePresentation::AppendOutline(Lines, Corners, Style.FootprintColor, false, false, Style, true);
    for (const FBatchedLine& Line : Lines)
        TestEqual(TEXT("Selected reservation stays readable through the preview mesh"), Line.DepthPriority, uint8(SDPG_Foreground));
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
    Obstruction.AffectedWorldRegion = FFFZSOrientedRect(FVector2D(200, 300), FVector2D(40, 40), 0);
    Validation.AddObstruction(Obstruction);
    TestEqual(TEXT("The same blocker retains each distinct affected reservation"), Validation.Obstructions.Num(), 2);
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
    Obstruction.AffectedWorldRegion = FFFZSOrientedRect(FVector2D(100, 200), FVector2D(200, 200), 0);
    Validation.AddObstruction(Obstruction);
    Lines.Reset();
    AppendObstructionFeedback(Lines, Validation.Obstructions[0], 8.0, FLinearColor::Yellow, FLinearColor::Red);
    TestEqual(TEXT("A rectangular blocker has eleven hatch strokes and two continuous outlines"), Lines.Num(), 19);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSLocalizedObstructionPresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.LocalizedObstructionRemainsVisible",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSLocalizedObstructionPresentationTest::RunTest(const FString& Parameters)
{
    using namespace FFZSBuildRangePresentation;
    FFFZSPlacementValidation Validation;
    FFFZSPlacementObstruction Obstruction;
    Obstruction.Reason = EFFZSPlacementFailure::PlacementCollision;
    Obstruction.WorldCenter = FVector(90, 0, -1000);
    Obstruction.WorldHalfExtent = FVector(30, 20, 2000);
    Obstruction.AffectedWorldRegion = FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(100, 100), 0);
    Validation.AddObstruction(Obstruction);
    TArray<FBatchedLine> Lines;
    FFFZSOrientedRect Surface;
    TestTrue(TEXT("A partly intersecting buried obstacle has visible projected feedback"),
        AppendObstructionFeedback(Lines, Validation.Obstructions[0], 9.0,
            FLinearColor::Yellow, FLinearColor::Red, &Surface));
    TestTrue(TEXT("The translucent plane covers only the rectangular overlap"),
        Surface.IsValid() && Surface.CenterCell.Equals(FVector2D(80, 0), 0.01) &&
        FMath::IsNearlyEqual(Surface.HalfExtentCells.X * Surface.HalfExtentCells.Y, 400.0, 0.01));
    for (const FBatchedLine& Line : Lines)
    {
        TestEqual(TEXT("Obstacle feedback remains visible through terrain and the ghost"),
            Line.DepthPriority, uint8(SDPG_Foreground));
        for (const FVector& Point : {Line.Start, Line.End})
        {
            TestTrue(TEXT("Every line is projected at the preview ground, not the obstacle's hidden height"),
                Point.Z >= 9.0 && Point.Z <= 9.5);
            if (Line.Color.Equals(FLinearColor::Red))
                TestTrue(TEXT("Red strokes cover only the intersecting part, never the whole facility"),
                    Point.X >= 60.0 - 0.01 && Point.X <= 100.0 + 0.01 && FMath::Abs(Point.Y) <= 20.01);
        }
    }

    Obstruction.WorldCenter = FVector::ZeroVector;
    Obstruction.WorldHalfExtent = FVector(1000000, 1000000, 1000000);
    Obstruction.AffectedWorldRegion = FFFZSOrientedRect(FVector2D(2000, 3000), FVector2D(800, 400), 37);
    Validation.Obstructions.Reset();
    Validation.AddObstruction(Obstruction);
    TArray<FVector2D> Bounds, Overlap;
    TestTrue(TEXT("A map-sized collider retains a local diagnostic footprint"),
        BuildObstructionFootprints(Validation.Obstructions[0], Bounds, Overlap));
    for (const FVector2D& Point : Bounds)
    {
        const FVector2D Local = (Point - Obstruction.AffectedWorldRegion.CenterCell).GetRotated(-37.0);
        TestTrue(TEXT("Map-sized collider edges stay within two meters of the active reservation"),
            FMath::Abs(Local.X) <= 1000.01 && FMath::Abs(Local.Y) <= 600.01);
    }
    Lines.Reset();
    AppendObstructionFeedback(Lines, Validation.Obstructions[0], 9,
        FLinearColor::Yellow, FLinearColor::Red);
    TestTrue(TEXT("Projected feedback has a fixed geometry budget"), Lines.Num() <= 35);
    TestTrue(TEXT("Long connected edges replace tiny corner brackets on large bounds"),
        Lines.ContainsByPredicate([](const FBatchedLine& Line)
            { return Line.Color.Equals(FLinearColor::Yellow) && FVector::Distance(Line.Start, Line.End) > 1000.0; }));

    Obstruction.WorldCenter = FVector(50, 30, 0);
    Obstruction.WorldHalfExtent = FVector(80, 30, 500);
    Obstruction.WorldRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(55.0));
    Obstruction.AffectedWorldRegion = FFFZSOrientedRect(FVector2D::ZeroVector, FVector2D(100, 60), -15);
    Validation.Obstructions.Reset();
    Validation.AddObstruction(Obstruction);
    TestTrue(TEXT("Differently rotated bounds clip to the true reservation axes"),
        BuildObstructionFootprints(Validation.Obstructions[0], Bounds, Overlap));
    for (const FVector2D& Point : Overlap)
    {
        const FVector2D FacilityLocal = Point.GetRotated(15.0);
        const FVector2D BlockerLocal = (Point - FVector2D(50, 30)).GetRotated(-55.0);
        TestTrue(TEXT("Every red polygon vertex belongs to both projected regions"),
            FMath::Abs(FacilityLocal.X) <= 100.01 && FMath::Abs(FacilityLocal.Y) <= 60.01 &&
            FMath::Abs(BlockerLocal.X) <= 80.01 && FMath::Abs(BlockerLocal.Y) <= 30.01);
    }
    Obstruction.Kind = EFFZSPlacementObstructionKind::GroundSupport;
    Obstruction.Reason = EFFZSPlacementFailure::InsufficientClearance;
    Validation.Obstructions.Reset();
    Validation.AddObstruction(Obstruction);
    Lines.Reset();
    TestFalse(TEXT("A terrain limit never draws an orange Landscape collision hull"),
        AppendObstructionFeedback(Lines, Validation.Obstructions[0], 9,
            FLinearColor::Yellow, FLinearColor::Red));
    TestTrue(TEXT("Terrain diagnostics leave object feedback empty for the affected-region renderer"), Lines.IsEmpty());
    TestFalse(TEXT("Audience terrain failures do not turn the footprint ghost red"),
        IsFailureForRegion(Obstruction.Reason, ERegion::Footprint));
    TestTrue(TEXT("Audience terrain failures retain their own region highlight"),
        IsFailureForRegion(Obstruction.Reason, ERegion::Audience));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceRangePresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.AudienceKeepsAuthoredReservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSAudienceRangePresentationTest::RunTest(const FString& Parameters)
{
    const UFFZSBuildPresentationSettings& Style = *GetDefault<UFFZSBuildPresentationSettings>();
    FFFZSFacilitySpec Spec;
    Spec.FootprintSizeCells = FIntPoint(200, 100);
    Spec.QueueRules.MaxWaitingAgents = 0;
    Spec.Audience.bEnabled = true;
    Spec.Audience.FirstSlotLocalCm = FVector(-120, 500, 0);
    Spec.Audience.Rows = 2;
    Spec.Audience.Columns = 3;
    Spec.Audience.CenterAisleWidthCm = 0;
    FFFZSGridPlacement Placement;
    FFFZSPlacementValidation Validation;
    UFFZSGridLibrary::BuildPlacementOrientedRegions(Spec, Placement,
        Validation.FootprintOrientedRects, Validation.ClearanceOrientedRects,
        Validation.QueueOrientedRects, Validation.ReservedOrientedRects);
    // The validator's named audience view is the collision-compatible clearance reservation.
    Validation.AudienceOrientedRects = Validation.ClearanceOrientedRects;
    if (!TestFalse(TEXT("Authored audience creates its actual reserved space"),
        Validation.AudienceOrientedRects.IsEmpty())) return false;
    const FTransform ZoneTransform(FRotator(0, 90, 0), FVector(1000, 2000, 100));
    TArray<FBatchedLine> Lines;
    FFZSBuildRangePresentation::AppendPlacementRanges(Lines, Validation, ZoneTransform, 307.0, Style);
    TArray<FBatchedLine> AudienceLines = Lines.FilterByPredicate([&Style](const FBatchedLine& Line)
        { return Line.Color.Equals(Style.AudienceColor); });
    TestTrue(TEXT("Each actual audience rectangle has a visible purple outline"),
        AudienceLines.Num() >= Validation.AudienceOrientedRects.Num() * 4);
    TestFalse(TEXT("Audience reservation is never presented as safety clearance"),
        Lines.ContainsByPredicate([&Style](const FBatchedLine& Line) { return Line.Color.Equals(Style.ClearanceColor); }));
    for (int32 RegionIndex = 0; RegionIndex < Validation.AudienceOrientedRects.Num(); ++RegionIndex)
    {
        const auto Corners = FFZSOrientedGeometry::GetCorners(Validation.AudienceOrientedRects[RegionIndex]);
        for (int32 Edge = 0; Edge < 4; ++Edge)
        {
            FVector Expected = ZoneTransform.TransformPosition(FVector(Corners[Edge], 0));
            Expected.Z = 307.0;
            TestTrue(TEXT("Audience matches authored geometry after exactly one zone transform"),
                AudienceLines.ContainsByPredicate([&Expected](const FBatchedLine& Line)
                    { return Line.Start.Equals(Expected, 0.001); }));
        }
    }
    for (const FBatchedLine& Line : AudienceLines)
    {
        TestTrue(TEXT("Every solid or dashed stroke stays on an actual authored audience edge"),
            Validation.AudienceOrientedRects.ContainsByPredicate([&Line, &ZoneTransform](const FFFZSOrientedRect& Rect)
            {
                const FVector2D A = (FVector2D(ZoneTransform.InverseTransformPosition(Line.Start)) -
                    Rect.CenterCell).GetRotated(-Rect.YawDegrees);
                const FVector2D B = (FVector2D(ZoneTransform.InverseTransformPosition(Line.End)) -
                    Rect.CenterCell).GetRotated(-Rect.YawDegrees);
                const bool bVertical = FMath::IsNearlyEqual(A.X, B.X, 0.001) &&
                    FMath::IsNearlyEqual(FMath::Abs(A.X), Rect.HalfExtentCells.X, 0.001) &&
                    FMath::Abs(A.Y) <= Rect.HalfExtentCells.Y + 0.001 && FMath::Abs(B.Y) <= Rect.HalfExtentCells.Y + 0.001;
                const bool bHorizontal = FMath::IsNearlyEqual(A.Y, B.Y, 0.001) &&
                    FMath::IsNearlyEqual(FMath::Abs(A.Y), Rect.HalfExtentCells.Y, 0.001) &&
                    FMath::Abs(A.X) <= Rect.HalfExtentCells.X + 0.001 && FMath::Abs(B.X) <= Rect.HalfExtentCells.X + 0.001;
                return bVertical || bHorizontal;
            }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSOutsideHatchPresentationTest,
    "FF_ZoneSystem.Unit.BuildPresentation.OutsideHatchExcludesBuildableLand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSOutsideHatchPresentationTest::RunTest(const FString& Parameters)
{
    FFFZSBuildAreaSourcePolygon Source;
    Source.WorldVertices = {FVector2D(0, 0), FVector2D(100, 0), FVector2D(100, 100), FVector2D(0, 100)};
    Source.MinZ = -10;
    Source.MaxZ = 10;
    FFFZSBuildAreaGeometry Geometry;
    if (!TestTrue(TEXT("Buildable land is available"), Geometry.Rebuild({Source}))) return false;
    const FFFZSOrientedRect Region(FVector2D(90, 50), FVector2D(30, 20), 23.0f);
    TArray<FFFZSBuildAreaLineSegment> CandidateStrokes, OutsideStrokes;
    FFZSBuildRangePresentation::BuildFailureHatchSegments(Region, CandidateStrokes);
    Geometry.GetOutsideSegments(Region, 0.0, CandidateStrokes, OutsideStrokes);
    if (!TestFalse(TEXT("Overhang produces visible clipped hatching"), OutsideStrokes.IsEmpty())) return false;
    TArray<FBatchedLine> Lines;
    // Rendering height may be well outside the volume's vertical range; authority stays at zone Z.
    FFZSBuildRangePresentation::AppendPlanarSegments(Lines, OutsideStrokes, 900.0, FLinearColor::Red, 2.0f);
    for (const FBatchedLine& Line : Lines)
    {
        TestEqual(TEXT("Drawing height cannot change boundary membership"), Line.Start.Z, 900.0);
        TestTrue(TEXT("No red boundary hatch enters the buildable side"),
            Line.Start.X >= 100.0 - 0.01 && Line.End.X >= 100.0 - 0.01);
        TestFalse(TEXT("Each red segment explains an actual outside portion"),
            Geometry.ContainsWorldPoint(FVector((FVector2D(Line.Start) + FVector2D(Line.End)) * 0.5, 0)));
        TestEqual(TEXT("Overhang is visible through the preview"), Line.DepthPriority, uint8(SDPG_Foreground));
    }
    Geometry.GetOutsideSegments(FFFZSOrientedRect(FVector2D(50, 50), FVector2D(10, 10), 0),
        0, CandidateStrokes, OutsideStrokes);
    TestTrue(TEXT("A fully buildable region never receives red boundary hatching"), OutsideStrokes.IsEmpty());

    const auto MakeRectangle = [](double MinX, double MinY, double MaxX, double MaxY)
    {
        FFFZSBuildAreaSourcePolygon Rectangle;
        Rectangle.WorldVertices = {FVector2D(MinX, MinY), FVector2D(MaxX, MinY),
            FVector2D(MaxX, MaxY), FVector2D(MinX, MaxY)};
        Rectangle.MinZ = -10;
        Rectangle.MaxZ = 10;
        return Rectangle;
    };
    if (!TestTrue(TEXT("A small excluded hole remains in the exact union"), Geometry.Rebuild({
        MakeRectangle(-100, -100, 100, 5), MakeRectangle(-100, 10, 100, 100),
        MakeRectangle(-100, 5, 5, 10), MakeRectangle(10, 5, 100, 10)}))) return false;
    const FFFZSOrientedRect EnclosingRegion(FVector2D::ZeroVector, FVector2D(100, 100), 0);
    FFZSBuildRangePresentation::BuildFailureHatchSegments(EnclosingRegion, CandidateStrokes);
    Geometry.GetOutsideSegments(EnclosingRegion, 0, CandidateStrokes, OutsideStrokes);
    TestTrue(TEXT("The regression hole lies between all regular hatch strokes"), OutsideStrokes.IsEmpty());
    TArray<FFFZSBuildAreaLineSegment> HoleBoundary;
    Geometry.GetBoundarySegments(0, FBox2D(FVector2D(-100, -100), FVector2D(100, 100)), HoleBoundary);
    FFZSBuildRangePresentation::AppendBoundaryHatchSeeds(EnclosingRegion, HoleBoundary, CandidateStrokes);
    Geometry.GetOutsideSegments(EnclosingRegion, 0, CandidateStrokes, OutsideStrokes);
    TestFalse(TEXT("Boundary-seeded strokes expose even an enclosed 5 cm hole"), OutsideStrokes.IsEmpty());
    TestTrue(TEXT("Small-hole explanation keeps a fixed candidate stroke budget"), CandidateStrokes.Num() <= 143);
    for (const FFFZSBuildAreaLineSegment& Stroke : OutsideStrokes)
    {
        const FVector2D Midpoint = (Stroke.Start + Stroke.End) * 0.5;
        TestTrue(TEXT("Extra red strokes remain entirely in the actual hole"),
            Midpoint.X > 5 && Midpoint.X < 10 && Midpoint.Y > 5 && Midpoint.Y < 10);
        TestFalse(TEXT("A boundary seed does not invent unbuildable ground"),
            Geometry.ContainsWorldPoint(FVector(Midpoint, 0)));
    }
    return true;
}

#endif
