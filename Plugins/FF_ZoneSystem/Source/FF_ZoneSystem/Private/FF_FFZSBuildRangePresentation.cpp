// FF_FFZSBuildRangePresentation.cpp
// 역할: 시설 본체·관람 공간·대기열·배치 실패 영역의 선을 공통 규칙으로 그린다.
#include "FF_FFZSBuildRangePresentation.h"

#include "FF_FFZSBuildPresentationSettings.h"
#include "FF_FFZSBuildArea.h"
#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSOrientedGeometry.h"
#include "FF_FFZSQueueGeometry.h"
#include "SceneManagement.h"

void FFZSBuildRangePresentation::AppendPlanarSegments(TArray<FBatchedLine>& Lines,
    const TArray<FFFZSBuildAreaLineSegment>& Segments, const double SurfaceZ,
    const FLinearColor& Color, const float Thickness)
{
    if (!FMath::IsFinite(SurfaceZ)) return;
    for (const FFFZSBuildAreaLineSegment& Segment : Segments)
    {
        if (Segment.Start.ContainsNaN() || Segment.End.ContainsNaN() || Segment.Start.Equals(Segment.End)) continue;
        Lines.Emplace(FVector(Segment.Start, SurfaceZ), FVector(Segment.End, SurfaceZ),
            Color, 0.0f, Thickness, SDPG_Foreground);
    }
}

// The authority clips these candidate strokes against the true construction union, including holes.
void FFZSBuildRangePresentation::BuildFailureHatchSegments(const FFFZSOrientedRect& WorldRegion,
    TArray<FFFZSBuildAreaLineSegment>& OutSegments)
{
    OutSegments.Reset();
    if (!WorldRegion.IsValid()) return;
    const auto Corners = FFZSOrientedGeometry::GetCorners(WorldRegion);
    constexpr int32 HatchCount = 11;
    for (int32 Index = 1; Index <= HatchCount; ++Index)
    {
        const double T = 2.0 * Index / (HatchCount + 1);
        const double U0 = FMath::Max(0.0, T - 1.0);
        const double U1 = FMath::Min(1.0, T);
        OutSegments.Add({
            Corners[0] + (Corners[1] - Corners[0]) * U0 + (Corners[3] - Corners[0]) * (T - U0),
            Corners[0] + (Corners[1] - Corners[0]) * U1 + (Corners[3] - Corners[0]) * (T - U1)});
    }
    // Edges make small overhangs visible even if they fall between two diagonal strokes.
    for (int32 Edge = 0; Edge < 4; ++Edge)
        OutSegments.Add({Corners[Edge], Corners[(Edge + 1) % 4]});
}

void FFZSBuildRangePresentation::AppendBoundaryHatchSeeds(const FFFZSOrientedRect& WorldRegion,
    const TArray<FFFZSBuildAreaLineSegment>& BoundarySegments,
    TArray<FFFZSBuildAreaLineSegment>& InOutSegments)
{
    if (!WorldRegion.IsValid()) return;
    constexpr int32 MaxBoundarySeeds = 64;
    const double Radius = WorldRegion.HalfExtentCells.Size() * 2.0;
    const FVector2D A = FVector2D(Radius, Radius).GetRotated(WorldRegion.YawDegrees);
    const FVector2D B = FVector2D(Radius, -Radius).GetRotated(WorldRegion.YawDegrees);
    for (int32 Index = 0; Index < FMath::Min(BoundarySegments.Num(), MaxBoundarySeeds); ++Index)
    {
        const FVector2D Center = (BoundarySegments[Index].Start + BoundarySegments[Index].End) * 0.5;
        if (Center.ContainsNaN()) continue;
        // One diagonal may run along a boundary; two directions ensure an interior witness.
        InOutSegments.Add({Center - A, Center + A});
        InOutSegments.Add({Center - B, Center + B});
    }
}

void FFZSBuildRangePresentation::AppendPlacementRanges(TArray<FBatchedLine>& Lines,
    const FFFZSPlacementValidation& Validation, const FTransform& ZoneTransform,
    const double SurfaceZ, const UFFZSBuildPresentationSettings& Style)
{
    constexpr int32 MaxRects = UFFZSDeveloperSettings::MaxDisplayedRangeRectsPerGroup;
    const auto DrawRect = [&Lines, &ZoneTransform, SurfaceZ, &Style](const FFFZSOrientedRect& Rect,
        const FLinearColor& Color, const bool bDashed, const bool bAccentCorners)
    {
        const auto RegionCorners = FFZSOrientedGeometry::GetCorners(Rect);
        FWorldCorners Corners;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Corners[Index] = ZoneTransform.TransformPosition(FVector(
                RegionCorners[Index] * UFFZSDeveloperSettings::DefaultPlacementCellSizeCm, 0.0));
            Corners[Index].Z = SurfaceZ;
        }
        const int32 FirstLine = Lines.Num();
        AppendOutline(Lines, Corners, Color, bDashed, bAccentCorners, Style, true);
        if (bAccentCorners)
        {
            // Emphasize only the pending facility's exact footprint. The screen-space
            // renderer adds its dark underlay, separating this border from the grid.
            for (int32 Index = FirstLine; Index < Lines.Num(); ++Index)
            {
                Lines[Index].Thickness = FMath::Max(3.5f, Lines[Index].Thickness + 1.0f);
                Lines[Index].Color.A = 1.0f;
            }
        }
    };
    const auto DrawGroup = [&DrawRect](const TArray<FFFZSOrientedRect>& Rects,
        const FLinearColor& Color, const ERegion Region)
    {
        const bool bDashed = Region == ERegion::Audience && Rects.Num() <= 64;
        // Never replace detailed reservations with a bounding box that invents reserved land.
        for (int32 Index = 0; Index < FMath::Min(Rects.Num(), MaxRects); ++Index)
            if (Rects[Index].IsValid()) DrawRect(Rects[Index], Color, bDashed, Region == ERegion::Footprint);
    };
    DrawGroup(Validation.FootprintOrientedRects, Style.FootprintColor, ERegion::Footprint);
    DrawGroup(Validation.QueueOrientedRects, Style.QueueColor, ERegion::Queue);
    DrawGroup(Validation.AudienceOrientedRects, Style.AudienceColor, ERegion::Audience);
}

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

namespace
{
    double ObstructionCross2D(const FVector2D& A, const FVector2D& B)
    {
        return A.X * B.Y - A.Y * B.X;
    }

    TArray<FVector2D> ClipToReservation(TArray<FVector2D> Polygon,
        const FFFZSOrientedRect& Region, const double Padding)
    {
        const FVector2D Axes[] = {FVector2D(1, 0).GetRotated(Region.YawDegrees),
            FVector2D(0, 1).GetRotated(Region.YawDegrees)};
        for (int32 Side = 0; Side < 4 && !Polygon.IsEmpty(); ++Side)
        {
            const int32 Axis = Side / 2;
            const FVector2D Normal = Axes[Axis] * ((Side % 2) ? 1.0 : -1.0);
            const double Extent = (Axis == 0 ? Region.HalfExtentCells.X : Region.HalfExtentCells.Y) + Padding;
            TArray<FVector2D> Clipped;
            for (int32 Index = 0; Index < Polygon.Num(); ++Index)
            {
                const FVector2D A = Polygon[Index], B = Polygon[(Index + 1) % Polygon.Num()];
                const double DA = Extent - FVector2D::DotProduct(A - Region.CenterCell, Normal);
                const double DB = Extent - FVector2D::DotProduct(B - Region.CenterCell, Normal);
                if (DA >= 0.0) Clipped.Add(A);
                if ((DA >= 0.0) != (DB >= 0.0)) Clipped.Add(FMath::Lerp(A, B, DA / (DA - DB)));
            }
            Polygon = MoveTemp(Clipped);
        }
        return Polygon;
    }

    void AppendPolygonOutline(TArray<FBatchedLine>& Lines, const TArray<FVector2D>& Polygon,
        const double Z, const FLinearColor& Color, const float Thickness)
    {
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D A = Polygon[Index], B = Polygon[(Index + 1) % Polygon.Num()];
            if (!A.Equals(B, 0.001))
                Lines.Emplace(FVector(A, Z), FVector(B, Z), Color, 0.0f, Thickness, SDPG_Foreground);
        }
    }
}

bool FFZSBuildRangePresentation::BuildObstructionFootprints(const FFFZSPlacementObstruction& Obstruction,
    TArray<FVector2D>& OutNearbyBounds, TArray<FVector2D>& OutOverlap)
{
    OutNearbyBounds.Reset();
    OutOverlap.Reset();
    if (Obstruction.Kind == EFFZSPlacementObstructionKind::GroundSupport ||
        !Obstruction.WorldBounds.IsValid || !Obstruction.AffectedWorldRegion.IsValid()) return false;
    const FTransform Transform(Obstruction.WorldRotation, Obstruction.WorldCenter);
    TArray<FVector2D> Points;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        const FVector2D Point(Transform.TransformPosition(Obstruction.WorldHalfExtent * FVector(
            (Index & 1) ? 1 : -1, (Index & 2) ? 1 : -1, (Index & 4) ? 1 : -1)));
        if (Point.ContainsNaN()) return false;
        Points.AddUnique(Point);
    }
    if (Points.Num() < 3) return false;
    Points.Sort([](const FVector2D& A, const FVector2D& B) { return A.X == B.X ? A.Y < B.Y : A.X < B.X; });
    // The projection of an oriented box is a convex hull of at most eight points.
    TArray<FVector2D> Hull;
    for (const FVector2D& Point : Points)
    {
        while (Hull.Num() >= 2 && ObstructionCross2D(Hull.Last() - Hull[Hull.Num() - 2], Point - Hull.Last()) <= 0.0)
            Hull.Pop(EAllowShrinking::No);
        Hull.Add(Point);
    }
    const int32 LowerCount = Hull.Num();
    for (int32 Index = Points.Num() - 2; Index >= 0; --Index)
    {
        while (Hull.Num() > LowerCount && ObstructionCross2D(Hull.Last() - Hull[Hull.Num() - 2], Points[Index] - Hull.Last()) <= 0.0)
            Hull.Pop(EAllowShrinking::No);
        Hull.Add(Points[Index]);
    }
    Hull.Pop(EAllowShrinking::No);
    // Ground projection keeps submerged or tall bounds visible from the build camera.
    // A nearby clipped footprint avoids tiny, off-screen brackets on a merged level mesh.
    OutNearbyBounds = ClipToReservation(Hull, Obstruction.AffectedWorldRegion, 200.0);
    OutOverlap = ClipToReservation(MoveTemp(Hull), Obstruction.AffectedWorldRegion, 0.0);
    return OutNearbyBounds.Num() >= 3 && OutOverlap.Num() >= 3;
}

bool FFZSBuildRangePresentation::AppendObstructionFeedback(TArray<FBatchedLine>& Lines,
    const FFFZSPlacementObstruction& Obstruction, const double SurfaceZ,
    const FLinearColor& BoundsColor, const FLinearColor& OverlapColor,
    FFFZSOrientedRect* OutRectangularOverlap)
{
    if (OutRectangularOverlap) *OutRectangularOverlap = FFFZSOrientedRect();
    TArray<FVector2D> Bounds, Overlap;
    if (!FMath::IsFinite(SurfaceZ) || !BuildObstructionFootprints(Obstruction, Bounds, Overlap)) return false;
    // Bound the hatch work independently of collider size and clip each stroke to the overlap.
    double MinDiagonal = TNumericLimits<double>::Max(), MaxDiagonal = -TNumericLimits<double>::Max();
    for (const FVector2D& Point : Overlap)
    {
        MinDiagonal = FMath::Min(MinDiagonal, Point.X + Point.Y);
        MaxDiagonal = FMath::Max(MaxDiagonal, Point.X + Point.Y);
    }
    for (int32 Hatch = 1; Hatch <= 11; ++Hatch)
    {
        const double Diagonal = FMath::Lerp(MinDiagonal, MaxDiagonal, Hatch / 12.0);
        TArray<FVector2D, TInlineAllocator<12>> Intersections;
        for (int32 Edge = 0; Edge < Overlap.Num(); ++Edge)
        {
            const FVector2D A = Overlap[Edge], B = Overlap[(Edge + 1) % Overlap.Num()];
            const double DA = A.X + A.Y - Diagonal, DB = B.X + B.Y - Diagonal;
            if (FMath::Abs(DA) <= 0.0001) Intersections.AddUnique(A);
            if ((DA < 0.0 && DB > 0.0) || (DA > 0.0 && DB < 0.0))
                Intersections.AddUnique(FMath::Lerp(A, B, DA / (DA - DB)));
        }
        if (Intersections.Num() < 2) continue;
        Intersections.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
        Lines.Emplace(FVector(Intersections[0], SurfaceZ), FVector(Intersections.Last(), SurfaceZ),
            OverlapColor, 0.0f, 2.0f, SDPG_Foreground);
    }
    AppendPolygonOutline(Lines, Overlap, SurfaceZ, OverlapColor, 2.5f);
    AppendPolygonOutline(Lines, Bounds, SurfaceZ + 0.5, BoundsColor, 3.5f);
    if (OutRectangularOverlap && Overlap.Num() == 4)
    {
        const FVector2D A = Overlap[1] - Overlap[0], B = Overlap[2] - Overlap[1];
        if (A.Size() > 0.01 && B.Size() > 0.01 &&
            FMath::Abs(FVector2D::DotProduct(A.GetSafeNormal(), B.GetSafeNormal())) < 0.00001 &&
            (Overlap[0] + Overlap[2]).Equals(Overlap[1] + Overlap[3], 0.01))
        {
            *OutRectangularOverlap = FFFZSOrientedRect((Overlap[0] + Overlap[2]) * 0.5,
                FVector2D(A.Size(), B.Size()) * 0.5, FMath::RadiansToDegrees(FMath::Atan2(A.Y, A.X)));
        }
    }
    return true;
}

// 설치 실패 사유를 본체·관람 공간·대기열 중 해당하는 표시 영역에 연결한다.
bool FFZSBuildRangePresentation::HasRegionFailure(
    const FFFZSPlacementValidation& Validation, const ERegion Region)
{
    return Validation.Reasons.ContainsByPredicate([Region](const EFFZSPlacementFailure Reason)
        { return IsFailureForRegion(Reason, Region); });
}

EFFZSPlacementFailure FFZSBuildRangePresentation::ResolveDisplayedFailureReason(
    const FFFZSPlacementValidation& Validation, const EFFZSPlacementFailure RequestedReason)
{
    if (Validation.bIsValid || Validation.Reasons.IsEmpty()) return EFFZSPlacementFailure::None;
    if (RequestedReason != EFFZSPlacementFailure::None && Validation.Reasons.Contains(RequestedReason)) return RequestedReason;
    return Validation.Reasons[0];
}

bool FFZSBuildRangePresentation::IsFailureForRegion(const EFFZSPlacementFailure Reason, const ERegion Region)
{
    // Never let other simultaneous failures leak into the currently explained problem.
    switch (Region)
    {
    case ERegion::Footprint:
        return Reason == EFFZSPlacementFailure::PlacementCollision ||
            Reason == EFFZSPlacementFailure::InvalidGround ||
            Reason == EFFZSPlacementFailure::ExcessSlope ||
            Reason == EFFZSPlacementFailure::HeightVarianceExceeded;
    case ERegion::Clearance:
        return false;
    case ERegion::Audience:
        return Reason == EFFZSPlacementFailure::InsufficientClearance;
    case ERegion::Queue:
        return Reason == EFFZSPlacementFailure::QueueFootprintBlocked;
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
    const bool bAccentCorners, const UFFZSBuildPresentationSettings& Style, const bool bForeground)
{
    const float Thickness = FMath::Clamp(Style.RangeLineThickness, 0.5f, 6.0f) + (bForeground ? 0.65f : 0.0f);
    const uint8 DepthPriority = bForeground ? SDPG_Foreground : SDPG_World;
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
                    Color, 0.0f, Thickness, DepthPriority);
            }
        }
        else
        {
            Lines.Emplace(A, B, Color, 0.0f, Thickness, DepthPriority);
        }
        if (bAccentCorners)
        {
            const double Fraction = FMath::Min(0.18, 30.0 / Length);
            Lines.Emplace(A, FMath::Lerp(A, B, Fraction), Color, 0.0f, Thickness + 0.7f, DepthPriority);
            Lines.Emplace(FMath::Lerp(A, B, 1.0 - Fraction), B, Color, 0.0f, Thickness + 0.7f, DepthPriority);
        }
    }
}

// 대기열 외곽 안에 실제 경로와 앞줄 방향을 함께 보여 준다. 슬롯 사이를 직선으로 잇지 않아
// L/U자 대기열의 모서리를 잘라 표시하지 않으며 비정상 콘텐츠의 선 수는 제한한다.
void FFZSBuildRangePresentation::AppendQueuePath(TArray<FBatchedLine>& Lines,
    const FFFZSFacilitySpec& Spec, const FTransform& FacilityTransform,
    const FLinearColor& Color, const UFFZSBuildPresentationSettings& Style, const bool bForeground)
{
    if (!Spec.UsesServiceQueue() || Spec.QueueRules.MaxWaitingAgents <= 0 ||
        Spec.QueueSplinePointsCm.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); })) return;
    constexpr int32 MaxDisplayedSegments = 128;
    const double Lift = FMath::Clamp(Style.RangeSurfaceLiftCm, 0.5f, 20.0f);
    const uint8 DepthPriority = bForeground ? SDPG_Foreground : SDPG_World;
    const float Thickness = FMath::Clamp(Style.RangeLineThickness + 0.8f, 1.5f, 6.0f);
    bool bHeadArrowDrawn = false;
    const int32 LastPoint = FMath::Min(Spec.QueueSplinePointsCm.Num() - 1, MaxDisplayedSegments);
    for (int32 Index = 1; Index <= LastPoint; ++Index)
    {
        const FVector A = FacilityTransform.TransformPosition(Spec.QueueSplinePointsCm[Index - 1]) + FVector::UpVector * Lift;
        const FVector B = FacilityTransform.TransformPosition(Spec.QueueSplinePointsCm[Index]) + FVector::UpVector * Lift;
        const double Length = FVector::Distance(A, B);
        if (!FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(Length)) continue;
        Lines.Emplace(A, B, Color, 0.0f, Thickness, DepthPriority);
        if (!bHeadArrowDrawn)
        {
            const FVector Direction = (B - A) / Length;
            const FVector Side = FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
            const double Arm = FMath::Min(Length * 0.35, 28.0);
            Lines.Emplace(A + Direction * Arm + Side * Arm * 0.6, A, Color, 0.0f, Thickness + 0.6f, DepthPriority);
            Lines.Emplace(A + Direction * Arm - Side * Arm * 0.6, A, Color, 0.0f, Thickness + 0.6f, DepthPriority);
            bHeadArrowDrawn = true;
        }
    }
}

// 실제 줄 서는 위치에만 작은 표식을 그린다. 대기 인원 제한은 변경하지 않는다.
void FFZSBuildRangePresentation::AppendQueueSlots(TArray<FBatchedLine>& Lines,
    const FFFZSFacilitySpec& Spec, const FTransform& FacilityTransform,
    const FLinearColor& Color, const UFFZSBuildPresentationSettings& Style, const bool bForeground)
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
        AppendOutline(Lines, Corners, Color, false, false, Style, bForeground);
    }
}
