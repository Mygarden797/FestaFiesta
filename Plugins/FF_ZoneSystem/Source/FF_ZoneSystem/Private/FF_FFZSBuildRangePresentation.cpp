// FF_FFZSBuildRangePresentation.cpp
// 역할: 시설 본체·여유 공간·대기열·배치 실패 영역의 선을 공통 규칙으로 그린다.
#include "FF_FFZSBuildRangePresentation.h"

#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSQueueGeometry.h"
#include "SceneManagement.h"

// 실패한 실제 영역 안에만 사선을 추가한다. 사선 수를 고정해 큰 시설도 비용을 제한한다.
void FFZSBuildRangePresentation::AppendFailureHatching(TArray<FBatchedLine>& Lines,
    const FWorldCorners& Corners, const FLinearColor& Color)
{
    for (const FVector& Corner : Corners) if (Corner.ContainsNaN()) return;
    // Clip diagonals in the rectangle's own UV frame, preserving arbitrary yaw and aspect.
    constexpr int32 HatchCount = 11;
    for (int32 Index = 1; Index <= HatchCount; ++Index)
    {
        const double T = 2.0 * Index / (HatchCount + 1);
        const double U0 = FMath::Max(0.0, T - 1.0);
        const double U1 = FMath::Min(1.0, T);
        const FVector A = Corners[0] + (Corners[1] - Corners[0]) * U0 + (Corners[3] - Corners[0]) * (T - U0);
        const FVector B = Corners[0] + (Corners[1] - Corners[0]) * U1 + (Corners[3] - Corners[0]) * (T - U1);
        Lines.Emplace(A, B, Color, 0.0f, 1.4f, SDPG_Foreground);
    }
    for (int32 Edge = 0; Edge < 4; ++Edge)
        Lines.Emplace(Corners[Edge], Corners[(Edge + 1) % 4], Color, 0.0f, 2.5f, SDPG_Foreground);
}

// 배치를 막은 물체 둘레에 모서리 표시를 추가한다.
void FFZSBuildRangePresentation::AppendObstructionOutline(TArray<FBatchedLine>& Lines,
    const FFFZSPlacementObstruction& Obstruction, const FLinearColor& Color)
{
    if (!Obstruction.WorldBounds.IsValid) return;
    const FTransform Transform(Obstruction.WorldRotation, Obstruction.WorldCenter);
    FVector Corners[8];
    for (int32 Index = 0; Index < 8; ++Index)
        Corners[Index] = Transform.TransformPosition(Obstruction.WorldHalfExtent * FVector(
            (Index & 1) ? 1 : -1, (Index & 2) ? 1 : -1, (Index & 4) ? 1 : -1));
    // Corner brackets and vertical edges differ from the failed area's diagonal hatching.
    for (int32 Index = 0; Index < 8; ++Index)
    {
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            const int32 Other = Index ^ (1 << Axis);
            if (Index >= Other) continue;
            const FVector A = Corners[Index], B = Corners[Other];
            const double Fraction = FMath::Min(0.35, 60.0 / FMath::Max(1.0, FVector::Distance(A, B)));
            Lines.Emplace(A, FMath::Lerp(A, B, Fraction), Color, 0.0f, 3.0f, SDPG_Foreground);
            Lines.Emplace(FMath::Lerp(A, B, 1.0 - Fraction), B, Color, 0.0f, 3.0f, SDPG_Foreground);
        }
    }
}

// 설치 실패 사유를 본체·여유 공간·대기열 중 해당하는 표시 영역에 연결한다.
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

// 화면에서 너무 가까운 보조선만 생략해 멀리서도 격자가 읽히게 한다.
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

// 회전된 원래 범위를 실선 또는 점선으로 그리며 긴 변의 점선 수를 제한한다.
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

// 실제 줄 서는 위치에만 작은 표식을 그린다. 대기 인원 제한은 변경하지 않는다.
void FFZSBuildRangePresentation::AppendQueueSlots(TArray<FBatchedLine>& Lines,
    const FFFZSFacilitySpec& Spec, const FTransform& FacilityTransform,
    const FLinearColor& Color, const UFFZSBuildPresentationSettings& Style)
{
    if (!Style.bShowQueueSlots || !Spec.UsesServiceQueue() || Spec.QueueRules.MaxWaitingAgents <= 0) return;
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
