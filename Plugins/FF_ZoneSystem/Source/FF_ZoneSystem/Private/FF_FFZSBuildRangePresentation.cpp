#include "FF_FFZSBuildRangePresentation.h"

#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSQueueGeometry.h"
#include "SceneManagement.h"

bool FFZSBuildRangePresentation::HasRegionFailure(
    const FFFZSPlacementValidation& Validation, const ERegion Region)
{
    // The validator supplies region-level reasons, not an intersection polygon. Highlight only
    // the implicated region; funds/stage/navigation errors must not invent a physical collision.
    switch (Region)
    {
    case ERegion::Footprint:
        return Validation.Reasons.Contains(EFFZSPlacementFailure::PlacementCollision) ||
            Validation.Reasons.Contains(EFFZSPlacementFailure::InvalidGround) ||
            Validation.Reasons.Contains(EFFZSPlacementFailure::ExcessSlope) ||
            Validation.Reasons.Contains(EFFZSPlacementFailure::HeightVarianceExceeded);
    case ERegion::Clearance:
        return Validation.Reasons.Contains(EFFZSPlacementFailure::InsufficientClearance);
    case ERegion::Queue:
        return Validation.Reasons.Contains(EFFZSPlacementFailure::QueueFootprintBlocked);
    default:
        return false;
    }
}

int32 FFZSBuildRangePresentation::ResolveMinorLineStride(
    const float ProjectedStepPixels, const float MinimumSpacingPixels)
{
    if (!FMath::IsFinite(ProjectedStepPixels) || ProjectedStepPixels <= 0.0f) return 1;
    const float Minimum = FMath::IsFinite(MinimumSpacingPixels)
        ? FMath::Clamp(MinimumSpacingPixels, 2.0f, 32.0f) : 9.0f;
    if (ProjectedStepPixels >= Minimum) return 1;
    if (ProjectedStepPixels * 2.0f >= Minimum) return 2;
    if (ProjectedStepPixels * 4.0f >= Minimum) return 4;
    return 0; // Major/boundary lines remain; selected facility detail is handled separately.
}

void FFZSBuildRangePresentation::AppendOutline(TArray<FBatchedLine>& Lines,
    const FWorldCorners& Corners, const FLinearColor& Color, const bool bDashed,
    const bool bAccentCorners, const UFFZSBuildPresentationSettings& Style)
{
    const float Thickness = FMath::Clamp(Style.RangeLineThickness, 0.5f, 6.0f);
    const double Dash = FMath::Clamp(Style.ClearanceDashLengthCm, 1.0f, 200.0f);
    const double Gap = FMath::Clamp(Style.ClearanceDashGapCm, 1.0f, 200.0f);
    for (int32 Edge = 0; Edge < 4; ++Edge)
    {
        const FVector A = Corners[Edge];
        const FVector B = Corners[(Edge + 1) % 4];
        const double Length = FVector::Distance(A, B);
        if (!FMath::IsFinite(Length) || Length <= UE_SMALL_NUMBER) continue;
        // Long edges remain bounded without changing either endpoint or the reserved region.
        const double Period = FMath::Max(Dash + Gap, Length / 64.0);
        if (bDashed)
        {
            for (double Offset = 0.0; Offset < Length; Offset += Period)
            {
                const double End = FMath::Min(Length, Offset + Period * Dash / (Dash + Gap));
                Lines.Emplace(FMath::Lerp(A, B, Offset / Length), FMath::Lerp(A, B, End / Length),
                    Color, 0.0f, Thickness, SDPG_World);
            }
        }
        else
        {
            Lines.Emplace(A, B, Color, 0.0f, Thickness, SDPG_World);
        }
        if (bAccentCorners)
        {
            const double Fraction = FMath::Min(0.18, 30.0 / Length);
            Lines.Emplace(A, FMath::Lerp(A, B, Fraction), Color, 0.0f, Thickness + 0.7f, SDPG_World);
            Lines.Emplace(FMath::Lerp(A, B, 1.0 - Fraction), B, Color, 0.0f, Thickness + 0.7f, SDPG_World);
        }
    }
}

void FFZSBuildRangePresentation::AppendQueueSlots(TArray<FBatchedLine>& Lines,
    const FFFZSFacilitySpec& Spec, const FTransform& FacilityTransform,
    const FLinearColor& Color, const UFFZSBuildPresentationSettings& Style)
{
    if (!Style.bShowQueueSlots || Spec.QueueRules.MaxWaitingAgents <= 0) return;
    // This is a drawing budget, not a queue capacity or a substitute reservation.
    constexpr int32 MaxDisplayedSlots = 128;
    TArray<FVector> Slots;
    if (!FFFZSQueueGeometry::BuildLinearQueueSlotPoints(Spec.QueueSplinePointsCm,
        FMath::Min(Spec.QueueRules.MaxWaitingAgents, MaxDisplayedSlots), Spec.QueueRules.SlotSpacingCm, Slots)) return;
    const double Radius = FMath::Clamp(Style.QueueSlotRadiusCm, 2.0f, 50.0f);
    const double Lift = FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
    for (const FVector& Slot : Slots)
    {
        FWorldCorners Corners;
        const FVector Offsets[] = { FVector(-Radius, 0, 0), FVector(0, -Radius, 0),
            FVector(Radius, 0, 0), FVector(0, Radius, 0) };
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Corners[Index] = FacilityTransform.TransformPosition(Slot + Offsets[Index]) + FVector::UpVector * Lift;
        }
        AppendOutline(Lines, Corners, Color, false, false, Style);
    }
}
