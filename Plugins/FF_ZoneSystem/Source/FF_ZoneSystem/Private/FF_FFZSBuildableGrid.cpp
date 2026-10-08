// FF_FFZSBuildableGrid.cpp
// 역할: 건설 가능한 칸을 표시용 마스크에 기록하고 이어진 경계선을 만든다. 실제 설치 점유는 변경하지 않는다.
#include "FF_FFZSBuildableGrid.h"

void FFFZSBuildableGridMask::Reset()
{
    MinLocalCm = FVector2D::ZeroVector;
    StepCm = 100.0;
    NumX = NumY = BuildableCellCount = 0;
    Cells.Reset();
    DeferredWorldObstacles.Reset();
    ObstacleHeightCm = 250.0;
    bIncludeGroundSurfaceSupport = true;
    FacilityCategory = EFFZSFacilityCategory::Booth;
}

bool FFFZSBuildableGridMask::Initialize(
    const FVector2D& MinCm, const FVector2D& MaxCm, const int32 RequestedStepCm,
    const FVector2D& AlignmentOriginLocalCm)
{
    Reset();
    if (MinCm.ContainsNaN() || MaxCm.ContainsNaN() || AlignmentOriginLocalCm.ContainsNaN() || RequestedStepCm < 25 ||
        RequestedStepCm > 1000 || MinCm.X >= MaxCm.X || MinCm.Y >= MaxCm.Y ||
        MinCm.X < MIN_int32 || MinCm.Y < MIN_int32 || MaxCm.X > MAX_int32 || MaxCm.Y > MAX_int32)
    {
        return false;
    }

    StepCm = RequestedStepCm;
    // Legacy callers align to their region origin. Unified land supplies the world origin in
    // region coordinates so changing regions cannot shift the displayed manipulation grid.
    // Doubling preserves the requested lattice while reducing detail within the same budget.
    for (int32 Attempt = 0; Attempt < 32; ++Attempt, StepCm *= 2.0)
    {
        const FVector2D AlignedMin(
            AlignmentOriginLocalCm.X + FMath::FloorToDouble((MinCm.X - AlignmentOriginLocalCm.X) / StepCm) * StepCm,
            AlignmentOriginLocalCm.Y + FMath::FloorToDouble((MinCm.Y - AlignmentOriginLocalCm.Y) / StepCm) * StepCm);
        const FVector2D AlignedMax(
            AlignmentOriginLocalCm.X + FMath::CeilToDouble((MaxCm.X - AlignmentOriginLocalCm.X) / StepCm) * StepCm,
            AlignmentOriginLocalCm.Y + FMath::CeilToDouble((MaxCm.Y - AlignmentOriginLocalCm.Y) / StepCm) * StepCm);
        const double CountX = FMath::RoundToDouble((AlignedMax.X - AlignedMin.X) / StepCm);
        const double CountY = FMath::RoundToDouble((AlignedMax.Y - AlignedMin.Y) / StepCm);
        if (CountX >= 1.0 && CountY >= 1.0 && CountX * CountY <= MaximumCellCount)
        {
            MinLocalCm = AlignedMin;
            NumX = static_cast<int32>(CountX);
            NumY = static_cast<int32>(CountY);
            Cells.Init(false, NumX * NumY);
            return true;
        }
    }
    Reset();
    return false;
}

bool FFFZSBuildableGridMask::IsBuildable(const int32 X, const int32 Y) const
{
    return X >= 0 && Y >= 0 && X < NumX && Y < NumY && Cells[Y * NumX + X];
}

void FFFZSBuildableGridMask::SetBuildable(const int32 X, const int32 Y, const bool bBuildable)
{
    if (X >= 0 && Y >= 0 && X < NumX && Y < NumY)
    {
        const int32 Index = Y * NumX + X;
        if (Cells[Index] != bBuildable)
        {
            Cells[Index] = bBuildable;
            BuildableCellCount += bBuildable ? 1 : -1;
        }
    }
}

void FFZSBuildableGrid::BuildSegments(
    const FFFZSBuildableGridMask& Mask,
    TArray<FFFZSBuildableGridSegment>& OutSegments)
{
    OutSegments.Reset();
    if (Mask.GetBuildableCellCount() == 0)
    {
        return;
    }
    OutSegments.Reserve((Mask.GetNumX() + Mask.GetNumY() + 2) * 2);
    const FVector2D Origin = Mask.GetMinLocalCm();
    const double Step = Mask.GetStepCm();
    const auto AppendAxis = [&](const bool bVertical)
    {
        const int32 LineCount = (bVertical ? Mask.GetNumX() : Mask.GetNumY()) + 1;
        const int32 AxisCellCount = bVertical ? Mask.GetNumY() : Mask.GetNumX();
        for (int32 Line = 0; Line < LineCount; ++Line)
        {
            int32 RunStart = 0;
            int32 PreviousKind = 0;
            for (int32 Position = 0; Position <= AxisCellCount; ++Position)
            {
                const bool bBefore = Position < AxisCellCount && (bVertical
                    ? Mask.IsBuildable(Line - 1, Position) : Mask.IsBuildable(Position, Line - 1));
                const bool bAfter = Position < AxisCellCount && (bVertical
                    ? Mask.IsBuildable(Line, Position) : Mask.IsBuildable(Position, Line));
                const int32 Kind = bBefore && bAfter ? 1 : (bBefore || bAfter ? 2 : 0);
                if (Kind == PreviousKind)
                {
                    continue;
                }
                if (PreviousKind != 0)
                {
                    FFFZSBuildableGridSegment& Segment = OutSegments.AddDefaulted_GetRef();
                    Segment.StartLocalCm = Origin + (bVertical
                        ? FVector2D(Line, RunStart) : FVector2D(RunStart, Line)) * Step;
                    Segment.EndLocalCm = Origin + (bVertical
                        ? FVector2D(Line, Position) : FVector2D(Position, Line)) * Step;
                    Segment.bBoundary = PreviousKind == 2;
                }
                RunStart = Position;
                PreviousKind = Kind;
            }
        }
    };
    AppendAxis(true);
    AppendAxis(false);
}
