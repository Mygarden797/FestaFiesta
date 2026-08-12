#include "FF_FFZSFacilityEditorHelpers.h"

#include "FF_FFZSQueueGeometry.h"
#include "FF_FFZSVisualGeometry.h"

namespace FFZSFacilityEditorHelpersPrivate
{
    int32 CeilToPlacementCell(const double SizeCm)
    {
        // Placement bounds are authoritative. Never round a genuinely oversized mesh
        // down: even a sub-millimetre overhang must reserve the next 1 cm cell.
        return FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(SizeCm)));
    }

    FBox TransformCardinalBoundsExact(
        const FBox& SourceBounds,
        const FRotator& Rotation,
        const FVector& Offset)
    {
        FBox Result(EForceInit::ForceInit);
        const int32 QuarterTurns = ((FMath::RoundToInt(
            FRotator::NormalizeAxis(Rotation.Yaw) / 90.0) % 4) + 4) % 4;
        for (int32 XIndex = 0; XIndex < 2; ++XIndex)
        {
            for (int32 YIndex = 0; YIndex < 2; ++YIndex)
            {
                for (int32 ZIndex = 0; ZIndex < 2; ++ZIndex)
                {
                    const double X = XIndex == 0 ? SourceBounds.Min.X : SourceBounds.Max.X;
                    const double Y = YIndex == 0 ? SourceBounds.Min.Y : SourceBounds.Max.Y;
                    const double Z = ZIndex == 0 ? SourceBounds.Min.Z : SourceBounds.Max.Z;
                    FVector Rotated;
                    switch (QuarterTurns)
                    {
                    case 1: Rotated = FVector(-Y, X, Z); break;
                    case 2: Rotated = FVector(-X, -Y, Z); break;
                    case 3: Rotated = FVector(Y, -X, Z); break;
                    default: Rotated = FVector(X, Y, Z); break;
                    }
                    Result += Rotated + Offset;
                }
            }
        }
        return Result;
    }

    bool IsFinitePolyline(const TArray<FVector>& Points)
    {
        return Points.Num() >= 2 && !Points.ContainsByPredicate(
            [](const FVector& Point)
            {
                return Point.ContainsNaN();
            });
    }
}

bool FFFZSFacilityEditorHelpers::TryCalculateFootprintCells(
    const FVector& MeshBoundsSizeCm,
    FIntPoint& OutFootprintSizeCells)
{
    OutFootprintSizeCells = FIntPoint::ZeroValue;

    if (MeshBoundsSizeCm.ContainsNaN() ||
        MeshBoundsSizeCm.X <= 0.0 ||
        MeshBoundsSizeCm.Y <= 0.0 ||
        MeshBoundsSizeCm.X > static_cast<double>(MAX_int32) ||
        MeshBoundsSizeCm.Y > static_cast<double>(MAX_int32))
    {
        return false;
    }

    OutFootprintSizeCells.X = FFZSFacilityEditorHelpersPrivate::CeilToPlacementCell(MeshBoundsSizeCm.X);
    OutFootprintSizeCells.Y = FFZSFacilityEditorHelpersPrivate::CeilToPlacementCell(MeshBoundsSizeCm.Y);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryCalculateFootprintCellsFromSourceBounds(
    const FBox& SourceBounds,
    const FRotator& VisualRotation,
    const FVector& VisualOffsetCm,
    FIntPoint& OutFootprintSizeCells)
{
    OutFootprintSizeCells = FIntPoint::ZeroValue;
    if (!SourceBounds.IsValid ||
        SourceBounds.Min.ContainsNaN() ||
        SourceBounds.Max.ContainsNaN() ||
        VisualOffsetCm.ContainsNaN() ||
        !FMath::IsFinite(VisualRotation.Pitch) ||
        !FMath::IsFinite(VisualRotation.Yaw) ||
        !FMath::IsFinite(VisualRotation.Roll))
    {
        return false;
    }

    const FBox AuthoredBounds = FFZSVisualGeometry::IsCardinalYawRotation(VisualRotation)
        ? FFZSFacilityEditorHelpersPrivate::TransformCardinalBoundsExact(
            SourceBounds, VisualRotation, VisualOffsetCm)
        : FFZSVisualGeometry::TransformBounds(
            SourceBounds,
            FTransform(VisualRotation, VisualOffsetCm, FVector::OneVector));
    if (!AuthoredBounds.IsValid || AuthoredBounds.Min.ContainsNaN() || AuthoredBounds.Max.ContainsNaN())
    {
        return false;
    }

    const FVector CenteredSizeCm(
        2.0 * FMath::Max(FMath::Abs(AuthoredBounds.Min.X), FMath::Abs(AuthoredBounds.Max.X)),
        2.0 * FMath::Max(FMath::Abs(AuthoredBounds.Min.Y), FMath::Abs(AuthoredBounds.Max.Y)),
        AuthoredBounds.GetSize().Z);
    return TryCalculateFootprintCells(CenteredSizeCm, OutFootprintSizeCells);
}

bool FFFZSFacilityEditorHelpers::TryCalculateRequiredQueueLengthCm(
    const int32 Capacity,
    const double SlotSpacingCm,
    double& OutRequiredLengthCm)
{
    OutRequiredLengthCm = 0.0;
    if (Capacity <= 0 || !FMath::IsFinite(SlotSpacingCm) || SlotSpacingCm <= 0.0)
    {
        return false;
    }

    OutRequiredLengthCm = static_cast<double>(Capacity - 1) * SlotSpacingCm;
    return FMath::IsFinite(OutRequiredLengthCm);
}

bool FFFZSFacilityEditorHelpers::TryCalculateMaxWaitingAgents(
    const TArray<FVector>& QueuePointsCm,
    const double SlotSpacingCm,
    int32& OutMaxWaitingAgents)
{
    OutMaxWaitingAgents = 0;
    if (!FFZSFacilityEditorHelpersPrivate::IsFinitePolyline(QueuePointsCm) ||
        !FMath::IsFinite(SlotSpacingCm) ||
        SlotSpacingCm <= 0.0)
    {
        return false;
    }

    int32 FirstNonFinitePointIndex = INDEX_NONE;
    const double LengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(
        QueuePointsCm,
        &FirstNonFinitePointIndex);
    if (FirstNonFinitePointIndex != INDEX_NONE || !FMath::IsFinite(LengthCm))
    {
        return false;
    }

    const double Capacity = FMath::FloorToDouble(LengthCm / SlotSpacingCm) + 1.0;
    if (!FMath::IsFinite(Capacity) || Capacity < 1.0 || Capacity > static_cast<double>(MAX_int32))
    {
        return false;
    }

    OutMaxWaitingAgents = static_cast<int32>(Capacity);
    return true;
}

bool FFFZSFacilityEditorHelpers::TryResizeQueueTail(
    const TArray<FVector>& QueuePointsCm,
    const double RequiredLengthCm,
    TArray<FVector>& OutQueuePointsCm)
{
    using namespace FFZSFacilityEditorHelpersPrivate;

    OutQueuePointsCm.Reset();
    if (!IsFinitePolyline(QueuePointsCm) ||
        !FMath::IsFinite(RequiredLengthCm) ||
        RequiredLengthCm < 0.0)
    {
        return false;
    }

    const double CurrentLengthCm = FFFZSQueueGeometry::GetLinearPolylineLengthCm(QueuePointsCm);
    if (!FMath::IsFinite(CurrentLengthCm))
    {
        return false;
    }

    if (FMath::Abs(CurrentLengthCm - RequiredLengthCm) <= UE_DOUBLE_KINDA_SMALL_NUMBER)
    {
        OutQueuePointsCm = QueuePointsCm;
        return true;
    }

    if (RequiredLengthCm < CurrentLengthCm)
    {
        OutQueuePointsCm.Reserve(QueuePointsCm.Num());
        OutQueuePointsCm.Add(QueuePointsCm[0]);

        if (RequiredLengthCm <= UE_DOUBLE_KINDA_SMALL_NUMBER)
        {
            OutQueuePointsCm.Add(QueuePointsCm[0]);
            return true;
        }

        double RemainingLengthCm = RequiredLengthCm;
        for (int32 Index = 1; Index < QueuePointsCm.Num(); ++Index)
        {
            const FVector& SegmentStart = QueuePointsCm[Index - 1];
            const FVector& SegmentEnd = QueuePointsCm[Index];
            const double SegmentLengthCm = FVector::Distance(SegmentStart, SegmentEnd);
            if (!FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(SegmentLengthCm))
            {
                OutQueuePointsCm.Add(SegmentEnd);
                continue;
            }

            if (RemainingLengthCm > SegmentLengthCm)
            {
                OutQueuePointsCm.Add(SegmentEnd);
                RemainingLengthCm -= SegmentLengthCm;
                continue;
            }

            const double SegmentAlpha = FMath::Clamp(RemainingLengthCm / SegmentLengthCm, 0.0, 1.0);
            const FVector NewTail = FMath::Lerp(SegmentStart, SegmentEnd, SegmentAlpha);
            if (NewTail != OutQueuePointsCm.Last())
            {
                OutQueuePointsCm.Add(NewTail);
            }
            else if (OutQueuePointsCm.Num() == 1)
            {
                OutQueuePointsCm.Add(NewTail);
            }
            return OutQueuePointsCm.Num() >= 2;
        }

        OutQueuePointsCm.Reset();
        return false;
    }

    FVector ExtensionDirection = FVector::ZeroVector;
    for (int32 Index = QueuePointsCm.Num() - 1; Index > 0; --Index)
    {
        FVector Segment = QueuePointsCm[Index] - QueuePointsCm[Index - 1];
        const double SegmentLengthCm = Segment.Length();
        if (FFFZSQueueGeometry::IsUsableLinearSegmentLengthCm(SegmentLengthCm))
        {
            ExtensionDirection = Segment / SegmentLengthCm;
            break;
        }
    }

    if (ExtensionDirection.IsNearlyZero())
    {
        return false;
    }

    OutQueuePointsCm = QueuePointsCm;
    OutQueuePointsCm.Add(
        QueuePointsCm.Last() + ExtensionDirection * (RequiredLengthCm - CurrentLengthCm));
    return true;
}
