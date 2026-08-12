// FF_FFZSTestFixtures.cpp

#include "Tests/FF_FFZSTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "FFZSTestFixtures"

namespace
{
    constexpr int32 TestFixtureLegacyMeterCellScale = 100;

    FFFZSAnchorDefinition MakeAnchor(
        const EFFZSAnchorType Type,
        const int32 Index,
        const FVector& LocationCm,
        const float YawDegrees = 0.0f)
    {
        FFFZSAnchorDefinition Anchor;
        Anchor.Type = Type;
        Anchor.Index = Index;
        Anchor.LocalTransform = FTransform(FRotator(0.0f, YawDegrees, 0.0f), LocationCm);
        return Anchor;
    }

    FFFZSCellRect MakeLegacyMeterRect(
        const int32 MinX,
        const int32 MinY,
        const int32 MaxX,
        const int32 MaxY)
    {
        return FFFZSCellRect(
            FIntPoint(MinX * TestFixtureLegacyMeterCellScale, MinY * TestFixtureLegacyMeterCellScale),
            FIntPoint(MaxX * TestFixtureLegacyMeterCellScale, MaxY * TestFixtureLegacyMeterCellScale));
    }

    void ConfigureStraightQueue(FFFZSFacilitySpec& Spec, const int32 LegacyQueueColumn)
    {
        constexpr double LegacyCellSizeCm = 100.0;
        const int32 FootprintWidthMeters = Spec.FootprintSizeCells.X / TestFixtureLegacyMeterCellScale;
        const int32 FootprintHeightMeters = Spec.FootprintSizeCells.Y / TestFixtureLegacyMeterCellScale;
        const int32 FirstQueueRow = FootprintHeightMeters;
        const double QueueXcm =
            (LegacyQueueColumn + 0.5 - FootprintWidthMeters * 0.5) * LegacyCellSizeCm;
        const double QueueStartYcm =
            (FootprintHeightMeters * 0.5 + 0.5) * LegacyCellSizeCm;
        const double QueueTailYcm = QueueStartYcm +
            (Spec.QueueRules.MaxWaitingAgents - 1) * Spec.QueueRules.SlotSpacingCm;
        const int32 LastQueueRow = FMath::FloorToInt(
            QueueTailYcm / LegacyCellSizeCm + FootprintHeightMeters * 0.5);

        Spec.QueueReservedRects.Add(MakeLegacyMeterRect(
            LegacyQueueColumn,
            FirstQueueRow,
            LegacyQueueColumn + 1,
            LastQueueRow + 1));
        Spec.QueueSplinePointsCm = {
            FVector(QueueXcm, QueueStartYcm, 0.0),
            FVector(QueueXcm, QueueTailYcm, 0.0)
        };
    }
}

FFFZSFacilitySpec FFZSTest::MakeValidFacilitySpec(const EFFZSTestFacilityVariant Variant)
{
    FFFZSFacilitySpec Spec;
    Spec.SchemaVersion = 2;
    Spec.ClearanceCells = 100;
    Spec.QueueRules.SlotSpacingCm = 115.0f;
    Spec.QueueRules.SlotArrivalToleranceCm = 25.0f;
    Spec.QueueRules.NavigationProjectionExtentCm = 30.0f;
    Spec.QueueRules.BlockedTimeoutSeconds = 5.0f;
    Spec.QueueRules.MaxRepathAttempts = 2;

    switch (Variant)
    {
    case EFFZSTestFacilityVariant::FoodTruck:
        Spec.FacilityDefinitionId = TEXT("FD_Booth_FoodTruckBasic");
        Spec.DisplayName = LOCTEXT("FoodTruck", "Basic Food Truck");
        Spec.Category = EFFZSFacilityCategory::Booth;
        Spec.FootprintSizeCells = FIntPoint(600, 300);
        Spec.QueueRules.MaxWaitingAgents = 12;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 6.0f;
        Spec.QueueRules.AbandonAfterSeconds = 30.0f;
        Spec.InstallCostKey = TEXT("Cost.FoodTruck.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.FoodTruck.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.FoodTruck.Basic");
        Spec.QueueReservedRects = {
            MakeLegacyMeterRect(5, 3, 6, 9),
            MakeLegacyMeterRect(0, 8, 5, 9),
            MakeLegacyMeterRect(0, 3, 1, 8)
        };
        Spec.QueueSplinePointsCm = {
            FVector(250.0f, 200.0f, 0.0f),
            FVector(250.0f, 750.0f, 0.0f),
            FVector(-250.0f, 750.0f, 0.0f),
            FVector(-250.0f, 200.0f, 0.0f)
        };
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(250.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(250.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-250.0f, 200.0f, 0.0f), 90.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Toilet:
        Spec.FacilityDefinitionId = TEXT("FD_Toilet_Basic");
        Spec.DisplayName = LOCTEXT("Toilet", "Basic Restroom");
        Spec.Category = EFFZSFacilityCategory::Toilet;
        Spec.FootprintSizeCells = FIntPoint(400, 400);
        Spec.QueueRules.MaxWaitingAgents = 8;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 4.0f;
        Spec.QueueRules.AbandonAfterSeconds = 30.0f;
        Spec.InstallCostKey = TEXT("Cost.Toilet.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Toilet.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Toilet.Basic");
        Spec.QueueReservedRects = { MakeLegacyMeterRect(1, 4, 3, 16) };
        Spec.QueueSplinePointsCm = {
            FVector(0.0f, 250.0f, 0.0f),
            FVector(0.0f, 1200.0f, 0.0f)
        };
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(0.0f, 250.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-75.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(75.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-200.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Info:
        Spec.FacilityDefinitionId = TEXT("FD_Info_Basic");
        Spec.DisplayName = LOCTEXT("Info", "Basic Information Booth");
        Spec.Category = EFFZSFacilityCategory::Info;
        Spec.FootprintSizeCells = FIntPoint(300, 200);
        Spec.QueueRules.MaxWaitingAgents = 6;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 4.0f;
        Spec.QueueRules.AbandonAfterSeconds = 25.0f;
        Spec.InstallCostKey = TEXT("Cost.Info.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Info.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Info.Basic");
        ConfigureStraightQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(0.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(0.0f, 100.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-150.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Rest:
        Spec.FacilityDefinitionId = TEXT("FD_Rest_Basic");
        Spec.DisplayName = LOCTEXT("Rest", "Basic Rest Area");
        Spec.Category = EFFZSFacilityCategory::Rest;
        Spec.FootprintSizeCells = FIntPoint(400, 300);
        Spec.QueueRules.MaxWaitingAgents = 4;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 12.0f;
        Spec.QueueRules.AbandonAfterSeconds = 60.0f;
        Spec.InstallCostKey = TEXT("Cost.Rest.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Rest.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Rest.Basic");
        ConfigureStraightQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-200.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Performance:
        Spec.FacilityDefinitionId = TEXT("FD_Performance_Basic");
        Spec.DisplayName = LOCTEXT("Performance", "Basic Performance Stage");
        Spec.Category = EFFZSFacilityCategory::Performance;
        Spec.FootprintSizeCells = FIntPoint(800, 500);
        Spec.QueueRules.MaxWaitingAgents = 16;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 10.0f;
        Spec.QueueRules.AbandonAfterSeconds = 45.0f;
        Spec.InstallCostKey = TEXT("Cost.Performance.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Performance.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Performance.Basic");
        ConfigureStraightQueue(Spec, 3);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 300.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-125.0f, 250.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(125.0f, 250.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-400.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Safety:
        Spec.FacilityDefinitionId = TEXT("FD_Safety_Basic");
        Spec.DisplayName = LOCTEXT("Safety", "Basic Safety Post");
        Spec.Category = EFFZSFacilityCategory::Safety;
        Spec.FootprintSizeCells = FIntPoint(400, 300);
        Spec.QueueRules.MaxWaitingAgents = 6;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 8.0f;
        Spec.QueueRules.AbandonAfterSeconds = 15.0f;
        Spec.InstallCostKey = TEXT("Cost.Safety.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Safety.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Safety.Basic");
        ConfigureStraightQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-200.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::PhotoSpot:
        Spec.FacilityDefinitionId = TEXT("FD_PhotoSpot_Basic");
        Spec.DisplayName = LOCTEXT("PhotoSpot", "Basic Photo Spot");
        Spec.Category = EFFZSFacilityCategory::PhotoSpot;
        Spec.FootprintSizeCells = FIntPoint(500, 300);
        Spec.QueueRules.MaxWaitingAgents = 8;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 5.0f;
        Spec.QueueRules.AbandonAfterSeconds = 25.0f;
        Spec.InstallCostKey = TEXT("Cost.PhotoSpot.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.PhotoSpot.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.PhotoSpot.Basic");
        ConfigureStraightQueue(Spec, 2);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(0.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(0.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-250.0f, 0.0f, 0.0f), 180.0f)
        };
        break;

    case EFFZSTestFacilityVariant::Count:
    default:
        Spec.SchemaVersion = 0;
        break;
    }

    return Spec;
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_DEV_AUTOMATION_TESTS
