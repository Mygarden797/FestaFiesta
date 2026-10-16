#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSQueueGeometry.h"
#include "Tests/FF_FFZSTestFixtures.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueLayoutGeometryTest,
    "FF_ZoneSystem.Unit.QueueLayout.GeometryAndCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueLayoutGeometryTest::RunTest(const FString& Parameters)
{
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 10'000'000);
    for (uint8 Variant = 0; Variant < static_cast<uint8>(EFFZSTestFacilityVariant::Count); ++Variant)
    {
        const FFFZSFacilitySpec Authored = FFZSTest::MakeValidFacilitySpec(static_cast<EFFZSTestFacilityVariant>(Variant));
        for (const EFFZSQueueLayout Layout : {EFFZSQueueLayout::Left, EFFZSQueueLayout::Right, EFFZSQueueLayout::UShape})
        {
            FFFZSFacilitySpec Candidate = Authored;
            Candidate.QueueLayout = Layout;
            FText Error;
            const FString Label = FString::Printf(TEXT("Variant %d / layout %d"), Variant, static_cast<uint8>(Layout));
            if (!TestTrue(*Label, FFFZSQueueGeometry::ApplySelectedLayout(Candidate, Error)))
            {
                AddError(Error.ToString());
                continue;
            }
            FFFZSFacilitySpec WithoutClearance = Authored;
            WithoutClearance.ClearanceCells = 0;
            WithoutClearance.QueueLayout = Layout;
            TestTrue(TEXT("The same queue layout is valid without legacy clearance"),
                FFFZSQueueGeometry::ApplySelectedLayout(WithoutClearance, Error));
            TestTrue(TEXT("Legacy clearance does not push generated queue paths away from the facility"),
                Candidate.QueueSplinePointsCm == WithoutClearance.QueueSplinePointsCm);
            TestTrue(TEXT("Legacy clearance does not enlarge generated queue reservations"),
                Candidate.QueueReservedRects == WithoutClearance.QueueReservedRects);
            TArray<FText> Errors;
            TestTrue(TEXT("Generated slots, anchors, footprint and reservations satisfy facility invariants"), Candidate.IsValid(Errors));
            TestEqual(TEXT("Queue capacity is unchanged"), Candidate.QueueRules.MaxWaitingAgents, Authored.QueueRules.MaxWaitingAgents);
            for (int32 Index = 0; Index < Candidate.Anchors.Num(); ++Index)
            {
                if (Candidate.Anchors[Index].Type != EFFZSAnchorType::Entry || Candidate.Anchors[Index].Index != 0)
                {
                    TestTrue(TEXT("Service, exit and extra authored anchors remain fixed"),
                        Candidate.Anchors[Index].LocalTransform.Equals(Authored.Anchors[Index].LocalTransform));
                }
            }
            TArray<FVector> Slots;
            TestTrue(TEXT("Every requested waiting slot can be sampled"), FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
                Candidate.QueueSplinePointsCm, Candidate.QueueRules.MaxWaitingAgents, Candidate.QueueRules.SlotSpacingCm, Slots));
            for (const FVector& Slot : Slots)
            {
                TestTrue(TEXT("Every queue slot is reserved"), FFFZSQueueGeometry::IsLocalPointInsideReservation(
                    Slot, Candidate.FootprintSizeCells, Candidate.QueueReservedRects));
            }
            for (int32 Segment = 1; Segment < Candidate.QueueSplinePointsCm.Num(); ++Segment)
            {
                for (int32 Step = 0; Step <= 10; ++Step)
                {
                    const FVector Sample = FMath::Lerp(Candidate.QueueSplinePointsCm[Segment - 1],
                        Candidate.QueueSplinePointsCm[Segment], Step / 10.0);
                    TestTrue(TEXT("The complete corridor, including each bend, remains reserved"),
                        FFFZSQueueGeometry::IsLocalPointInsideReservation(Sample,
                            Candidate.FootprintSizeCells, Candidate.QueueReservedRects));
                    TestFalse(TEXT("No corridor sample crosses the solid building footprint"),
                        FMath::Abs(Sample.X) < Candidate.FootprintSizeCells.X * 0.5 &&
                        FMath::Abs(Sample.Y) < Candidate.FootprintSizeCells.Y * 0.5);
                }
            }
            const FFFZSAnchorDefinition* Entry = Candidate.Anchors.FindByPredicate([](const FFFZSAnchorDefinition& Anchor)
            {
                return Anchor.Type == EFFZSAnchorType::Entry && Anchor.Index == 0;
            });
            TestTrue(TEXT("Approach entry is included in the new reserved lane"), Entry &&
                FFFZSQueueGeometry::IsLocalPointInsideReservation(Entry->LocalTransform.GetLocation(),
                    Candidate.FootprintSizeCells, Candidate.QueueReservedRects));
        }
        const FFFZSFacilitySpec Identity = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Authored, 1, 1.0f);
        FFFZSFacilitySpec NormalizedAuthored = Authored;
        NormalizedAuthored.ClearanceCells = 0;
        TestTrue(TEXT("Authored geometry is preserved while retired clearance is removed"),
            FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&NormalizedAuthored, &Identity, 0));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueLayoutPersistenceGeometryTest,
    "FF_ZoneSystem.Unit.QueueLayout.ScaleRestoreAndRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueLayoutPersistenceGeometryTest::RunTest(const FString& Parameters)
{
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 10'000'000);
    const FFFZSFacilitySpec Original = FFZSTest::MakeValidFacilitySpec();
    FFFZSFacilitySpec Base = Original;
    Base.QueueLayout = EFFZSQueueLayout::UShape;
    const FFFZSFacilitySpec Enlarged = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 1.5f);
    TArray<FText> Errors;
    TestTrue(TEXT("Selected queue remains valid after facility size upgrade"), Enlarged.IsValid(Errors));
    TestTrue(TEXT("The instance base retains the authored points"), Base.QueueSplinePointsCm == Original.QueueSplinePointsCm);
    Base.QueueLayout = EFFZSQueueLayout::Authored;
    const FFFZSFacilitySpec Reverted = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 1, 1.0f);
    FFFZSFacilitySpec NormalizedOriginal = Original;
    NormalizedOriginal.ClearanceCells = 0;
    TestTrue(TEXT("Choosing Authored restores original geometry without legacy clearance"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&NormalizedOriginal, &Reverted, 0));

    FFFZSCommand Command;
    Command.FacilitySpec = Original;
    const FString Legacy = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestFalse(TEXT("Legacy receipt has no additive shape field"), Legacy.Contains(TEXT("spec.queue_layout")));
    Command.FacilitySpec.QueueLayout = EFFZSQueueLayout::Left;
    const FString LeftFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    Command.FacilitySpec.QueueLayout = EFFZSQueueLayout::Right;
    TestNotEqual(TEXT("Different layout choices cannot alias a command receipt"), LeftFingerprint,
        UFFZSGridLibrary::BuildCommandFingerprint(Command));

    FFFZSFacilitySpec TooSmall = Original;
    TooSmall.QueueLayout = EFFZSQueueLayout::UShape;
    TooSmall.QueueRules.MaxWaitingAgents = 3;
    const FFFZSFacilitySpec Before = TooSmall;
    FText Error;
    TestFalse(TEXT("U shape requires enough slots to form its corners"), FFFZSQueueGeometry::ApplySelectedLayout(TooSmall, Error));
    TestTrue(TEXT("Rejected geometry leaves the input unchanged"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Before, &TooSmall, 0));
    TestFalse(TEXT("A rejected shape explains the problem"), Error.IsEmpty());
    TooSmall.QueueLayout = static_cast<EFFZSQueueLayout>(255);
    TestFalse(TEXT("Unknown saved enum fails closed"), FFFZSQueueGeometry::ApplySelectedLayout(TooSmall, Error));

    // Production movement-target profiles deliberately push Service a little past a
    // front corner. The original queue head, not that sideways correction, defines frontage.
    FFFZSFacilitySpec Asymmetric = Original;
    for (FFFZSAnchorDefinition& Anchor : Asymmetric.Anchors)
    {
        if (Anchor.Type == EFFZSAnchorType::Service)
            Anchor.LocalTransform.SetLocation(FVector(325, 150, 0));
        if (Anchor.Type == EFFZSAnchorType::Exit)
            Anchor.LocalTransform.SetLocation(FVector(-115, 225, 0));
    }
    Asymmetric.QueueLayout = EFFZSQueueLayout::Left;
    if (!TestTrue(TEXT("Corner-offset service supports a generated shape"), FFFZSQueueGeometry::ApplySelectedLayout(Asymmetric, Error)))
    {
        AddError(Error.ToString());
        return false;
    }
    TestTrue(TEXT("Authored +Y frontage is preserved despite the +X service offset"),
        FMath::IsNearlyEqual(Asymmetric.QueueSplinePointsCm[0].X, 325.0) && Asymmetric.QueueSplinePointsCm[0].Y > 225.0);
    TestTrue(TEXT("Fixed exit retains room for the project's 34cm Crowd body"),
        FFFZSQueueGeometry::IsLocalPointInsideReservation(FVector(-115, 259, 0),
            Asymmetric.FootprintSizeCells, Asymmetric.QueueReservedRects));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSettingsGeometryTest,
    "FF_ZoneSystem.Unit.QueueSettings.GeneratedGeometryAndCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueSettingsGeometryTest::RunTest(const FString& Parameters)
{
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 20'000'000);
    for (uint8 Variant = 0; Variant < static_cast<uint8>(EFFZSTestFacilityVariant::Count); ++Variant)
    {
        const FFFZSFacilitySpec Original = FFZSTest::MakeValidFacilitySpec(static_cast<EFFZSTestFacilityVariant>(Variant));
        for (const EFFZSQueueLayout Layout : {EFFZSQueueLayout::Authored, EFFZSQueueLayout::Left, EFFZSQueueLayout::Right, EFFZSQueueLayout::UShape})
        {
            FFFZSFacilitySpec Base = Original;
            Base.QueueSettings.bEnabled = true;
            Base.QueueLayout = Layout;
            const FFFZSFacilitySpec Effective = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 1, 1.0f);
            TArray<FText> Errors;
            if (!TestTrue(*FString::Printf(TEXT("Unified variant %d layout %d validates"), Variant, static_cast<uint8>(Layout)), Effective.IsValid(Errors)))
            {
                for (const FText& Error : Errors) AddError(Error.ToString());
                continue;
            }
            TestEqual(TEXT("Every service facility uses six team slots"), Effective.QueueRules.MaxWaitingAgents, 6);
            TestEqual(TEXT("Every service facility uses the same spacing"), Effective.QueueRules.SlotSpacingCm, 115.0f);
            TestTrue(TEXT("The authored path is retained in the base spec"), Base.QueueSplinePointsCm == Original.QueueSplinePointsCm);
            const FFFZSFacilitySpec Scaled = FFZSFacilityUpgradeRules::BuildEffectiveSpec(Base, 2, 1.5f);
            TestTrue(TEXT("Unified layout remains valid after facility scale"), Scaled.IsValid(Errors));
            TestEqual(TEXT("Facility scale keeps user spacing in world centimetres"), Scaled.QueueRules.SlotSpacingCm, 115.0f);
            TestEqual(TEXT("Facility scale keeps user lane width in world centimetres"), Scaled.QueueSettings.LaneWidthCm, 300.0f);
            if (Layout == EFFZSQueueLayout::UShape)
            {
                TestTrue(TEXT("Parallel U lanes have at least a full lane width between them"),
                    FVector::Dist(Effective.QueueSplinePointsCm[1], Effective.QueueSplinePointsCm[2]) >= 300.0);
            }
        }
    }
    FFFZSCommand Command;
    Command.FacilitySpec = FFZSTest::MakeValidFacilitySpec();
    const FString Legacy = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestFalse(TEXT("Disabled defaults add no spec receipt fields"), Legacy.Contains(TEXT("queue_settings")));
    Command.FacilitySpec.QueueSettings.bEnabled = true;
    const FString DefaultFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    TestNotEqual(TEXT("Enabled settings differ from legacy receipts"), Legacy, DefaultFingerprint);
    Command.FacilitySpec.QueueSettings.LaneWidthCm = 800.f;
    TestNotEqual(TEXT("Lane width participates in spec receipt identity"), DefaultFingerprint, UFFZSGridLibrary::BuildCommandFingerprint(Command));
    Command.FacilitySpec.QueueSettings = FFFZSQueueSettings();
    Command.bChangeQueueSettings = true;
    Command.QueueSettings.bEnabled = true;
    const FString MoveFingerprint = UFFZSGridLibrary::BuildCommandFingerprint(Command);
    Command.QueueSettings.MaxWaitingTeams = 13;
    TestNotEqual(TEXT("Move settings participate in command receipt identity"), MoveFingerprint, UFFZSGridLibrary::BuildCommandFingerprint(Command));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSettingsRejectTest,
    "FF_ZoneSystem.Unit.QueueSettings.RejectionAndPassiveNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueSettingsRejectTest::RunTest(const FString& Parameters)
{
    FFFZSQueueSettings Settings;
    Settings.bEnabled = true;
    TArray<FText> Errors;
    TestTrue(TEXT("Common settings are valid"), Settings.IsValid(Errors));
    for (const int32 Capacity : {0, 513})
    {
        Settings.MaxWaitingTeams = Capacity;
        TestFalse(TEXT("Out of range team capacity rejected"), Settings.IsValid(Errors));
    }
    Settings.MaxWaitingTeams = 12;
    Settings.SlotSpacingCm = 89.f;
    TestFalse(TEXT("Party member placement clearance minimum respected"), Settings.IsValid(Errors));
    Settings.SlotSpacingCm = 90.f;
    TestTrue(TEXT("Minimum safe party slot spacing is accepted"), Settings.IsValid(Errors));
    Settings.SlotSpacingCm = 1001.f;
    TestFalse(TEXT("Unbounded path spacing rejected"), Settings.IsValid(Errors));
    Settings.SlotSpacingCm = 115.f;
    Settings.LaneWidthCm = 99.f;
    TestFalse(TEXT("Minimum lane width respected"), Settings.IsValid(Errors));
    Settings.LaneWidthCm = 1001.f;
    TestFalse(TEXT("Unbounded lane width rejected"), Settings.IsValid(Errors));
    FFFZSFacilitySpec Spec = FFZSTest::MakeValidFacilitySpec();
    Spec.QueueSettings.bEnabled = true;
    Spec.QueueSettings.MaxWaitingTeams = 4;
    Spec.QueueSettings.LaneWidthCm = 600.f;
    Spec.QueueLayout = EFFZSQueueLayout::UShape;
    const FFFZSFacilitySpec Before = Spec;
    FText Error;
    TestFalse(TEXT("Too few slots cannot create overlapping wide U lanes"), FFFZSQueueGeometry::ApplySelectedLayout(Spec, Error));
    TestFalse(TEXT("Rejected U width explains the problem"), Error.IsEmpty());
    TestTrue(TEXT("Rejected settings preserve the complete input spec"), FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Spec, &Before, 0));
    Spec.QueueLayout = EFFZSQueueLayout::Authored;
    Spec.QueueSettings.SlotSpacingCm = 90.f;
    Spec.QueueRules.SlotArrivalToleranceCm = 45.f;
    TestFalse(TEXT("Slot spacing must exceed the arrival diameter"), FFFZSQueueGeometry::ApplySelectedLayout(Spec, Error));
    Spec = FFZSTest::MakeValidFacilitySpec();
    Spec.Category = EFFZSFacilityCategory::Lighting;
    Spec.QueueSettings.bEnabled = true;
    Spec.NormalizeNonServiceGeometry();
    TestFalse(TEXT("Passive facilities never retain player queue settings"), Spec.QueueSettings.bEnabled);
    TestEqual(TEXT("Passive queue capacity is zero"), Spec.QueueRules.MaxWaitingAgents, 0);
    TestTrue(TEXT("Passive queue geometry is empty"), Spec.QueueSplinePointsCm.IsEmpty() && Spec.QueueReservedRects.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSQueueSettingsAudienceAccessTest,
    "FF_ZoneSystem.Unit.QueueSettings.FixedAccessPreservesAudienceAndWaitingWidth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSQueueSettingsAudienceAccessTest::RunTest(const FString& Parameters)
{
    // MainStage's service and exit sit at opposite front corners. Its independent
    // audience starts nearby; widening the fixed exit to the full waiting-lane width
    // used to intrude into that otherwise valid audience rectangle.
    TGuardValue<int32> Budget(GetMutableDefault<UFFZSDeveloperSettings>()->MaxEnumeratedCellsPerFacility, 20'000'000);
    FFFZSFacilitySpec Base = FFZSTest::MakeValidFacilitySpec();
    Base.FootprintSizeCells = FIntPoint(2535, 1625);
    Base.QueueRules.MaxWaitingAgents = 1;
    Base.QueueRules.SlotSpacingCm = 100.f;
    Base.QueueSplinePointsCm = {FVector(1217.5, 862.5, 0), FVector(1217.5, 862.5, 0)};
    Base.QueueReservedRects = {
        FFFZSCellRect(FIntPoint(2435, 1625), FIntPoint(2535, 1725)),
        FFFZSCellRect(FIntPoint(2450, 1640), FIntPoint(2720, 1710))};
    for (FFFZSAnchorDefinition& Anchor : Base.Anchors)
    {
        Anchor.LocalTransform.SetLocation(Anchor.Type == EFFZSAnchorType::Entry ? FVector(1417.5, 862.5, 0)
            : Anchor.Type == EFFZSAnchorType::Service ? FVector(1292.5, 812.5, 0) : FVector(-1217.5, 862.5, 0));
    }
    Base.Audience.bEnabled = true;
    Base.Audience.Columns = 25;
    Base.Audience.Rows = 20;
    Base.Audience.AreaLimitM2 = 909.f;
    Base.Audience.FirstSlotLocalCm = FVector(-1540, 1212.5, 0);
    Base.Audience.EntryLocalCm = FVector(-1630, 3582.5, 0);
    Base.Audience.ExitLocalCm = FVector(1630, 3582.5, 0);
    Base.Audience.FocusLocalCm = FVector(0, 406.25, 200);
    TArray<FText> Errors;
    if (!TestTrue(TEXT("Stage and authored queue/audience fixture is valid"), Base.IsValid(Errors)))
    {
        for (const FText& Error : Errors) AddError(Error.ToString());
        return false;
    }
    Base.QueueSettings.bEnabled = true;
    Base.QueueSettings.MaxWaitingTeams = 12;
    Base.QueueSettings.LaneWidthCm = 600.f;
    for (const EFFZSQueueLayout Layout : {EFFZSQueueLayout::Authored, EFFZSQueueLayout::Left, EFFZSQueueLayout::Right, EFFZSQueueLayout::UShape})
    {
        FText Error;
        FFFZSFacilitySpec Generated = Base;
        Generated.QueueLayout = Layout;
        const bool bGenerated = FFFZSQueueGeometry::ApplySelectedLayout(Generated, Error);
        if (!TestTrue(*FString::Printf(TEXT("Common lane %d preserves the stage audience: %s"), static_cast<uint8>(Layout), *Error.ToString()), bGenerated))
        {
            AddError(Error.ToString());
            continue;
        }
        TestEqual(TEXT("The waiting lane keeps all requested team slots"), Generated.QueueRules.MaxWaitingAgents, 12);
        TestTrue(TEXT("Audience geometry is unchanged"),
            FFFZSAudienceLayout::StaticStruct()->CompareScriptStruct(&Base.Audience, &Generated.Audience, 0));
        const FVector Head = Generated.QueueSplinePointsCm[0];
        for (const double Offset : {-299.0, 299.0})
            TestTrue(TEXT("The waiting corridor retains its full six-metre width"),
                FFFZSQueueGeometry::IsLocalPointInsideReservation(Head + FVector(Offset, 0, 0),
                    Generated.FootprintSizeCells, Generated.QueueReservedRects));
        TestTrue(TEXT("The fixed exit remains protected"), FFFZSQueueGeometry::IsLocalPointInsideReservation(
            FVector(-1217.5, 900, 0), Generated.FootprintSizeCells, Generated.QueueReservedRects));
        FFFZSFacilitySpec ScaledBase = Base;
        ScaledBase.QueueLayout = Layout;
        ScaledBase.Audience.AreaLimitM2 = 3000.f; // This geometry check explicitly grants the larger audience area.
        const FFFZSFacilitySpec Scaled = FFZSFacilityUpgradeRules::BuildEffectiveSpec(ScaledBase, 2, 1.5f);
        TestTrue(TEXT("The fixed audience also remains valid at an upgraded facility scale"), Scaled.IsValid(Errors));
    }
    return true;
}

#endif
