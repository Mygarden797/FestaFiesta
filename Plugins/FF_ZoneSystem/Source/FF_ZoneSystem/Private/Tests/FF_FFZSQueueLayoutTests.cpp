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
        TestTrue(TEXT("Legacy authored identity geometry stays byte-equivalent"),
            FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Authored, &Identity, 0));
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
    TestTrue(TEXT("Choosing Authored restores the exact original data"),
        FFFZSFacilitySpec::StaticStruct()->CompareScriptStruct(&Original, &Reverted, 0));

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

#endif
