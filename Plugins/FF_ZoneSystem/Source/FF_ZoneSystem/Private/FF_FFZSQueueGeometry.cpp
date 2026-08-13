#include "FF_FFZSQueueGeometry.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSTypes.h"

bool FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(const double SegmentLengthCm)
{
    return FMath::IsFinite(SegmentLengthCm)
        && SegmentLengthCm > UE_DOUBLE_KINDA_SMALL_NUMBER;
}

double FFFZSQueueGeometry::GetLinearPolylineLengthCm(
    const TArray<FVector>& Points,
    int32* OutFirstNonFinitePointIndex)
{
    if (OutFirstNonFinitePointIndex)
    {
        *OutFirstNonFinitePointIndex = INDEX_NONE;
    }

    double LengthCm = 0.0;
    for (int32 Index = 0; Index < Points.Num(); ++Index)
    {
        if (Points[Index].ContainsNaN())
        {
            if (OutFirstNonFinitePointIndex)
            {
                *OutFirstNonFinitePointIndex = Index;
            }
            break;
        }
        if (Index > 0)
        {
            const double SegmentLengthCm = FVector::Distance(Points[Index - 1], Points[Index]);
            if (IsUsableLinearSegmentLengthCm(SegmentLengthCm))
            {
                LengthCm += SegmentLengthCm;
            }
        }
    }
    return LengthCm;
}

bool FFFZSQueueGeometry::TrySampleLinearPolylineAtDistance(
    const TArray<FVector>& Points,
    double DistanceCm,
    FVector& OutPoint)
{
    if (Points.IsEmpty())
    {
        return false;
    }

    if (!FMath::IsFinite(DistanceCm) ||
        Points.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); }))
    {
        return false;
    }

    if (DistanceCm <= 0.0)
    {
        OutPoint = Points[0];
        return true;
    }
    for (int32 Index = 1; Index < Points.Num(); ++Index)
    {
        const FVector& Start = Points[Index - 1];
        const FVector& End = Points[Index];
        const double SegmentLengthCm = FVector::Distance(Start, End);
        if (!IsUsableLinearSegmentLengthCm(SegmentLengthCm))
        {
            continue;
        }
        if (DistanceCm <= SegmentLengthCm)
        {
            OutPoint = FMath::Lerp(Start, End, DistanceCm / SegmentLengthCm);
            return true;
        }
        DistanceCm -= SegmentLengthCm;
    }

    OutPoint = Points.Last();
    return true;
}

bool FFFZSQueueGeometry::TrySampleLinearPolylineAtDistanceWithTangent(
    const TArray<FVector>& Points,
    double DistanceCm,
    FVector& OutPoint,
    FVector& OutUnitTangent)
{
    OutPoint = FVector::ZeroVector;
    OutUnitTangent = FVector::ZeroVector;
    if (Points.IsEmpty() ||
        !FMath::IsFinite(DistanceCm) ||
        Points.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); }))
    {
        return false;
    }

    const bool bClampToHead = DistanceCm <= 0.0;
    DistanceCm = FMath::Max(0.0, DistanceCm);
    FVector LastUsableTangent = FVector::ZeroVector;
    bool bFoundUsableSegment = false;
    for (int32 Index = 1; Index < Points.Num(); ++Index)
    {
        const FVector Segment = Points[Index] - Points[Index - 1];
        const double SegmentLengthCm = Segment.Length();
        if (!IsUsableLinearSegmentLengthCm(SegmentLengthCm))
        {
            continue;
        }

        const FVector UnitTangent = Segment / SegmentLengthCm;
        if (bClampToHead)
        {
            OutPoint = Points[0];
            OutUnitTangent = UnitTangent;
            return true;
        }
        bFoundUsableSegment = true;
        LastUsableTangent = UnitTangent;
        if (DistanceCm <= SegmentLengthCm)
        {
            OutPoint = FMath::Lerp(
                Points[Index - 1],
                Points[Index],
                DistanceCm / SegmentLengthCm);
            OutUnitTangent = UnitTangent;
            return true;
        }
        DistanceCm -= SegmentLengthCm;
    }

    if (!bFoundUsableSegment)
    {
        return false;
    }
    OutPoint = Points.Last();
    OutUnitTangent = LastUsableTangent;
    return true;
}

bool FFFZSQueueGeometry::BuildLinearQueueSlotPoints(
    const TArray<FVector>& Points,
    const int32 SlotCount,
    const double SlotSpacingCm,
    TArray<FVector>& OutSlotPoints)
{
    OutSlotPoints.Reset();
    if (SlotCount < 0 ||
        SlotCount > UFFZSDeveloperSettings::MaxQueueWaitingAgents ||
        !FMath::IsFinite(SlotSpacingCm) ||
        SlotSpacingCm < 0.0)
    {
        return false;
    }
    if (SlotCount == 0)
    {
        return true;
    }
    if (Points.IsEmpty() ||
        Points.ContainsByPredicate([](const FVector& Point) { return Point.ContainsNaN(); }))
    {
        return false;
    }

    TArray<double> SegmentLengthsCm;
    SegmentLengthsCm.Reserve(FMath::Max(0, Points.Num() - 1));
    for (int32 PointIndex = 1; PointIndex < Points.Num(); ++PointIndex)
    {
        const double SegmentLengthCm = FVector::Distance(
            Points[PointIndex - 1], Points[PointIndex]);
        SegmentLengthsCm.Add(IsUsableLinearSegmentLengthCm(SegmentLengthCm) ? SegmentLengthCm : 0.0);
    }

    OutSlotPoints.Reserve(SlotCount);
    int32 SegmentIndex = 0;
    double SegmentStartDistanceCm = 0.0;
    for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
    {
        const double TargetDistanceCm = static_cast<double>(SlotIndex) * SlotSpacingCm;
        if (TargetDistanceCm <= 0.0)
        {
            OutSlotPoints.Add(Points[0]);
            continue;
        }
        bool bSampledSegment = false;
        while (SegmentIndex < SegmentLengthsCm.Num())
        {
            const double SegmentLengthCm = SegmentLengthsCm[SegmentIndex];
            if (!IsUsableLinearSegmentLengthCm(SegmentLengthCm))
            {
                ++SegmentIndex;
                continue;
            }
            if (TargetDistanceCm <= SegmentStartDistanceCm + SegmentLengthCm)
            {
                const double Alpha = FMath::Clamp(
                    (TargetDistanceCm - SegmentStartDistanceCm) / SegmentLengthCm,
                    0.0,
                    1.0);
                OutSlotPoints.Add(FMath::Lerp(
                    Points[SegmentIndex],
                    Points[SegmentIndex + 1],
                    Alpha));
                bSampledSegment = true;
                break;
            }
            SegmentStartDistanceCm += SegmentLengthCm;
            ++SegmentIndex;
        }
        if (!bSampledSegment)
        {
            OutSlotPoints.Add(Points.Last());
        }
    }
    return true;
}

FVector2D FFFZSQueueGeometry::LocalPointCmToReservationGrid(
    const FVector& LocalPointCm,
    const FIntPoint FootprintSizeCells)
{
    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    return FVector2D(
        LocalPointCm.X / CellSizeCm + FootprintSizeCells.X * 0.5,
        LocalPointCm.Y / CellSizeCm + FootprintSizeCells.Y * 0.5);
}

bool FFFZSQueueGeometry::IsLocalPointInsideReservation(
    const FVector& LocalPointCm,
    const FIntPoint FootprintSizeCells,
    const TArray<FFFZSCellRect>& ReservedRects,
    const double BoundaryToleranceCm)
{
    if (LocalPointCm.ContainsNaN() ||
        FootprintSizeCells.X <= 0 ||
        FootprintSizeCells.Y <= 0 ||
        !FMath::IsFinite(BoundaryToleranceCm) ||
        BoundaryToleranceCm < 0.0)
    {
        return false;
    }

    constexpr double CellSizeCm = UFFZSDeveloperSettings::DefaultPlacementCellSizeCm;
    const FVector2D GridPoint =
        LocalPointCmToReservationGrid(LocalPointCm, FootprintSizeCells);
    const double ToleranceCells = BoundaryToleranceCm / CellSizeCm;
    return ReservedRects.ContainsByPredicate(
        [&GridPoint, ToleranceCells](const FFFZSCellRect& Rect)
        {
            return Rect.IsValid() &&
                GridPoint.X >= Rect.Min.X - ToleranceCells &&
                GridPoint.X <= Rect.Max.X + ToleranceCells &&
                GridPoint.Y >= Rect.Min.Y - ToleranceCells &&
                GridPoint.Y <= Rect.Max.Y + ToleranceCells;
        });
}
