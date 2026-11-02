// FF_FFZSAudienceTests.cpp
// 역할: 관객 자리 생성, 관람 범위, 최대 수용 인원의 계산 계약을 자동 검증한다.
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSAudienceLibrary.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSGridLibrary.h"
#include "Tests/FF_FFZSTestFixtures.h"
#include <limits>

namespace
{
    FFFZSFacilitySpec MakeAudienceSpec()
    {
        FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
        Spec.Audience.bEnabled = true;
        // Deliberately separate from the fixture's ordinary queue and building.
        Spec.Audience.FirstSlotLocalCm = FVector(-760, 3000, 0);
        Spec.Audience.EntryLocalCm = FVector(-850, 4170, 0);
        Spec.Audience.ExitLocalCm = FVector(850, 4170, 0);
        Spec.Audience.FocusLocalCm = FVector::ZeroVector;
        return Spec;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceGeometryTest, "FF_ZoneSystem.Audience.GeometryAndRotation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceGeometryTest::RunTest(const FString& Parameters)
{
    const FFFZSFacilitySpec Spec = MakeAudienceSpec();
    TArray<FText> Errors;
    TestTrue(TEXT("Fixture and audience validate together"), Spec.IsValid(Errors));
    for (const FText& Error : Errors) AddInfo(Error.ToString());
    TestEqual(TEXT("Capacity counts individual visitors"), Spec.Audience.GetCapacity(), 120);
    FVector Left, Right, Row2, Last;
    Spec.Audience.TryGetSlotLocal(5, Left);
    Spec.Audience.TryGetSlotLocal(6, Right);
    Spec.Audience.TryGetSlotLocal(12, Row2);
    Spec.Audience.TryGetSlotLocal(119, Last);
    TestEqual(TEXT("Centre aisle adds 200cm to normal spacing"), Right.X - Left.X, 320.);
    TestEqual(TEXT("Row-major index advances one row"), Row2.Y, 3120.);
    TestTrue(TEXT("Last seat is front-to-back/right-most"), Last.Equals(FVector(760, 4080, 0)));
    const FVector Sentinel(9, 8, 7);
    FVector Unchanged = Sentinel;
    TestFalse(TEXT("Overflow index rejected"), Spec.Audience.TryGetSlotLocal(120, Unchanged));
    TestTrue(TEXT("Invalid lookup preserves output"), Unchanged.Equals(Sentinel));
    FFFZSAudienceSnapshot Snapshot;
    FText Error;
    const FTransform Placement(FRotator(0, 37, 0), FVector(12345, -600, 80));
    TestTrue(TEXT("Pure snapshot accepts arbitrary yaw"), UFFZSAudienceLibrary::BuildSnapshot(Spec.Audience, Placement, Snapshot, Error));
    FTransform Slot;
    TestTrue(TEXT("World slot resolves"), UFFZSAudienceLibrary::GetAudienceSlotTransform(Snapshot, 119, Slot));
    TestTrue(TEXT("World foot position follows translation and rotation"), Slot.GetLocation().Equals(Placement.TransformPosition(Last), .001));
    TestTrue(TEXT("NPC forward points to the stage"), Slot.GetRotation().GetForwardVector().Equals(
        (Snapshot.FocusWorldCm - Slot.GetLocation()).GetSafeNormal2D(), .001));
    FFFZSAudienceLayout Resized = Spec.Audience;
    Resized.Rows = 15;
    Resized.Columns = 9;
    TestTrue(TEXT("Changing capacity can automatically fit the portals"), Resized.TryCenterAndArrangePortals());
    TestEqual(TEXT("Arrangement retains requested visitor count"), Resized.GetCapacity(), 135);
    TestEqual(TEXT("Arrangement preserves front row distance"), Resized.FirstSlotLocalCm.Y, 3000.);
    TestTrue(TEXT("Rear entrance follows the final row"), Resized.EntryLocalCm.Equals(FVector(-670, 4770, 0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceValidationTest, "FF_ZoneSystem.Audience.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceValidationTest::RunTest(const FString& Parameters)
{
    FFFZSAudienceLayout A = MakeAudienceSpec().Audience;
    TArray<FText> Errors;
    A.Columns = MAX_int32;
    TestEqual(TEXT("Huge cardinality cannot overflow"), A.GetCapacity(), 0);
    TestFalse(TEXT("Huge cardinality is invalid"), A.IsValid(Errors));
    A = MakeAudienceSpec().Audience;
    A.RowSpacingCm = 50;
    TestFalse(TEXT("Body and arrival envelopes may not overlap"), A.IsValid(Errors));
    A = MakeAudienceSpec().Audience;
    A.EntryLocalCm = A.FirstSlotLocalCm;
    TestFalse(TEXT("Entry cannot occupy a spectator seat"), A.IsValid(Errors));
    A = MakeAudienceSpec().Audience;
    A.FirstSlotLocalCm.X = std::numeric_limits<double>::infinity();
    TestFalse(TEXT("Nonfinite positions fail closed"), A.IsValid(Errors));
    A.bEnabled = false;
    TestTrue(TEXT("Disabled legacy authoring does not allocate or reserve audience"), A.IsValid(Errors));
    TestEqual(TEXT("Disabled audience has no active slots"), A.GetCapacity(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceReservationTest, "FF_ZoneSystem.Audience.ReservationAndCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceReservationTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Spec = MakeAudienceSpec();
    const FFFZSGridPlacement Placement;
    FFFZSCellRect Rect;
    TestTrue(TEXT("Audience rectangle converts centre to footprint-corner coordinates"), UFFZSGridLibrary::TryGetAudienceReservedRect(Spec, Rect));
    const int64 AudienceArea = Rect.GetArea();
    const auto EnabledReserved = UFFZSGridLibrary::GetReservedRects(Spec, Placement);
    const auto EnabledQueue = UFFZSGridLibrary::GetQueueRects(Spec, Placement);
    Spec.Audience.bEnabled = false;
    const auto DisabledReserved = UFFZSGridLibrary::GetReservedRects(Spec, Placement);
    TestEqual(TEXT("Audience adds its complete reserved area"), UFFZSGridLibrary::GetCellRectArea(EnabledReserved)
        - UFFZSGridLibrary::GetCellRectArea(DisabledReserved), AudienceArea);
    TestEqual(TEXT("FIFO queue geometry stays unchanged"), UFFZSGridLibrary::GetCellRectArea(EnabledQueue),
        UFFZSGridLibrary::GetCellRectArea(UFFZSGridLibrary::GetQueueRects(Spec, Placement)));
    FFFZSCommand Base;
    Base.FacilitySpec = FFZSTest::MakeValidFacilitySpec();
    const FString LegacyFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Base);
    TestFalse(TEXT("Default audience adds no bytes to legacy receipts"), LegacyFingerprint.Contains(TEXT("spec.audience")));
    Base.FacilitySpec = MakeAudienceSpec();
    const FString EnabledFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Base);
    Base.FacilitySpec.Audience.FocusLocalCm.Z += 1.;
    TestNotEqual(TEXT("Audience changes affect idempotency payload"), UFFZSGridLibrary::BuildCommandFingerprint(Base), EnabledFingerprint);
    Spec = MakeAudienceSpec();
    const FVector Offset(0, -3000, 0);
    Spec.Audience.FirstSlotLocalCm += Offset;
    Spec.Audience.EntryLocalCm += Offset;
    Spec.Audience.ExitLocalCm += Offset;
    TArray<FText> Errors;
    TestFalse(TEXT("Audience may not overlap the stage footprint"), Spec.IsValid(Errors));
    Spec = MakeAudienceSpec();
    Spec.Audience.Columns = 512;
    Spec.Audience.Rows = 16;
    TestFalse(TEXT("Slot cap does not bypass the existing area budget"), UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    Spec.Audience.AreaLimitM2 = 15000;
    TestTrue(TEXT("Explicit audience allowance can exceed the project default"), UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    TestFalse(TEXT("Large audience retains compressed reservations"), UFFZSGridLibrary::GetReservedRects(Spec, Placement).IsEmpty());
    TestTrue(TEXT("Explicit allowance does not enable massive debug cell materialization"), UFFZSGridLibrary::GetReservedCells(Spec, Placement).IsEmpty());
    Spec.FootprintSizeCells = FIntPoint(10000, 10000);
    TestFalse(TEXT("Audience override never increases the building allowance"), UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    Spec = MakeAudienceSpec();
    Spec.Audience.AreaLimitM2 = FFFZSAudienceLayout::MaxAreaLimitM2 + 1;
    TestFalse(TEXT("Excessive audience budget is rejected"), Spec.IsValid(Errors));
    Spec.Audience.AreaLimitM2 = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN budget is rejected before area arithmetic"), UFFZSGridLibrary::CanEnumerateFacilityCells(Spec));
    Base.FacilitySpec = MakeAudienceSpec();
    const FString BeforeAreaOverride = UFFZSGridLibrary::BuildCommandFingerprint(Base);
    TestFalse(TEXT("Default area adds no bytes to earlier audience receipts"), BeforeAreaOverride.Contains(TEXT("area_limit_m2")));
    Base.FacilitySpec.Audience.AreaLimitM2 = 2000;
    TestNotEqual(TEXT("Explicit area participates in transaction identity"), UFFZSGridLibrary::BuildCommandFingerprint(Base), BeforeAreaOverride);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSAudienceUpgradeTest, "FF_ZoneSystem.Audience.UpgradeAndTaggedSerialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFFZSAudienceUpgradeTest::RunTest(const FString& Parameters)
{
    FFFZSFacilitySpec Base = MakeAudienceSpec();
    Base.Audience.AreaLimitM2 = 2000;
    const FFFZSFacilitySpec Larger = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 1.5f);
    TestTrue(TEXT("Audience positions scale once from the saved base"), Larger.Audience.FirstSlotLocalCm.Equals(Base.Audience.FirstSlotLocalCm * 1.5));
    TestEqual(TEXT("Facility upgrades do not enlarge NPC bodies"), Larger.Audience.AgentRadiusCm, Base.Audience.AgentRadiusCm);
    TestEqual(TEXT("Upgrade never silently increases user area allowance"), Larger.Audience.AreaLimitM2, Base.Audience.AreaLimitM2);
    TestEqual(TEXT("Size upgrades retain the authored visitor count"), Larger.Audience.GetCapacity(), Base.Audience.GetCapacity());
    TArray<FText> Errors;
    TestTrue(TEXT("Expanded audience remains valid"), Larger.IsValid(Errors));
    const FFFZSFacilitySpec Smaller = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, .5f);
    TestFalse(TEXT("Shrinking below safe person spacing is rejected"), Smaller.IsValid(Errors));
    FString Export;
    FFFZSFacilitySpec::StaticStruct()->ExportText(Export, &Base, nullptr, nullptr, PPF_None, nullptr);
    FFFZSFacilitySpec Restored;
    TestNotNull(TEXT("Spec tagged text roundtrip succeeds"), FFFZSFacilitySpec::StaticStruct()->ImportText(
        *Export, &Restored, nullptr, PPF_None, GWarn, TEXT("AudienceRoundTrip")));
    TestTrue(TEXT("Audience geometry survives serialization"), FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Base.Audience, &Restored.Audience, 0));
    return true;
}

#endif
