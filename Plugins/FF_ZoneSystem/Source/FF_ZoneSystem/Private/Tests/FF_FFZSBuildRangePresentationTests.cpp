#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildRangePresentation.h"

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

#endif
