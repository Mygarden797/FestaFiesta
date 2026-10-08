#include "FF_FFZSQueueGeometry.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSGridLibrary.h"
#include "FF_FFZSTypes.h"

#define LOCTEXT_NAMESPACE "FFZSQueueLayout"

bool FFFZSQueueGeometry::ApplySelectedLayout(FFFZSFacilitySpec& InOutSpec, FText& OutError)
{
    OutError = FText::GetEmpty();
    if (InOutSpec.QueueLayout == EFFZSQueueLayout::Authored)
    {
        return true;
    }
    if (InOutSpec.QueueLayout != EFFZSQueueLayout::Left &&
        InOutSpec.QueueLayout != EFFZSQueueLayout::Right &&
        InOutSpec.QueueLayout != EFFZSQueueLayout::UShape)
    {
        OutError = LOCTEXT("UnknownLayout", "알 수 없는 대기열 형태입니다.");
        return false;
    }
    const int32 Count = InOutSpec.QueueRules.MaxWaitingAgents;
    const double Spacing = InOutSpec.QueueRules.SlotSpacingCm;
    const double Tolerance = InOutSpec.QueueRules.SlotArrivalToleranceCm;
    if (!InOutSpec.UsesServiceQueue() ||
        Count <= 0 || Count > UFFZSDeveloperSettings::MaxQueueWaitingAgents ||
        !FMath::IsFinite(Spacing) || Spacing <= 0.0 || !FMath::IsFinite(Tolerance) || Tolerance < 0.0 ||
        InOutSpec.FootprintSizeCells.X <= 0 || InOutSpec.FootprintSizeCells.Y <= 0)
    {
        OutError = LOCTEXT("NoQueue", "대기열을 사용하는 정상 시설에서만 형태를 바꿀 수 있습니다.");
        return false;
    }
    if (InOutSpec.QueueLayout == EFFZSQueueLayout::UShape && Count < 4)
    {
        OutError = LOCTEXT("UCapacity", "U자형에는 최소 네 개의 대기 자리가 필요합니다. 좌측형이나 우측형을 선택하세요.");
        return false;
    }

    FFFZSFacilitySpec Candidate = InOutSpec;
    FFFZSAnchorDefinition* Entry = Candidate.Anchors.FindByPredicate([](const FFFZSAnchorDefinition& Anchor)
    {
        return Anchor.Type == EFFZSAnchorType::Entry && Anchor.Index == 0;
    });
    const FFFZSAnchorDefinition* Service = Candidate.Anchors.FindByPredicate([](const FFFZSAnchorDefinition& Anchor)
    {
        return Anchor.Type == EFFZSAnchorType::Service && Anchor.Index == 0;
    });
    if (!Entry || !Service)
    {
        OutError = LOCTEXT("MissingAnchors", "입구와 서비스 지점이 모두 있어야 대기열 형태를 바꿀 수 있습니다.");
        return false;
    }

    const FVector ServicePoint = Service->LocalTransform.GetLocation();
    const FVector HalfSize(Candidate.FootprintSizeCells.X * 0.5, Candidate.FootprintSizeCells.Y * 0.5, 0.0);
    // Keep the authored queue frontage. Service anchors may intentionally sit just past a
    // building corner; using Service alone would unexpectedly rotate those facilities' queues.
    const FVector Frontage = Candidate.QueueSplinePointsCm.IsEmpty() ? ServicePoint : Candidate.QueueSplinePointsCm[0];
    const bool bXFace = FMath::Abs(Frontage.X) / HalfSize.X > FMath::Abs(Frontage.Y) / HalfSize.Y;
    const FVector Outward = bXFace ? FVector(Frontage.X < 0.0 ? -1.0 : 1.0, 0.0, 0.0)
        : FVector(0.0, Frontage.Y < 0.0 ? -1.0 : 1.0, 0.0);
    const FVector Right(Outward.Y, -Outward.X, 0.0);
    const double HalfLaneWidth = FMath::Max(50.0, Tolerance + 1.0);
    const double Separation = FMath::Max(Spacing, FMath::Max(1.0, Tolerance * 2.0));
    double HeadDistance = (bXFace ? HalfSize.X : HalfSize.Y) + HalfLaneWidth;
    for (const FFFZSAnchorDefinition& Anchor : Candidate.Anchors)
    {
        if (Anchor.Type != EFFZSAnchorType::Entry || Anchor.Index != 0)
        {
            HeadDistance = FMath::Max(HeadDistance,
                FVector::DotProduct(Anchor.LocalTransform.GetLocation(), Outward) + Separation + HalfLaneWidth);
        }
    }
    const FVector Head = ServicePoint + Outward * (HeadDistance - FVector::DotProduct(ServicePoint, Outward));
    Candidate.QueueSplinePointsCm = {Head};
    FVector TailTangent;
    if (Candidate.QueueLayout == EFFZSQueueLayout::UShape)
    {
        // Turns fall exactly on slot centres: no diagonal shortening below arrival tolerance.
        const int32 EdgeCount = Count - 1;
        const int32 FirstLeg = FMath::Max(1, EdgeCount / 3);
        const int32 Bridge = FMath::Max(1, EdgeCount / 3);
        const int32 LastLeg = EdgeCount - FirstLeg - Bridge;
        Candidate.QueueSplinePointsCm.Add(Head - Right * (FirstLeg * Spacing));
        Candidate.QueueSplinePointsCm.Add(Candidate.QueueSplinePointsCm.Last() + Outward * (Bridge * Spacing));
        Candidate.QueueSplinePointsCm.Add(Candidate.QueueSplinePointsCm.Last() + Right * (LastLeg * Spacing));
        TailTangent = Right;
    }
    else
    {
        TailTangent = Candidate.QueueLayout == EFFZSQueueLayout::Left ? -Right : Right;
        Candidate.QueueSplinePointsCm.Add(Head + TailTangent * (FMath::Max(1, Count - 1) * Spacing));
    }
    FVector LastSlot;
    if (!TrySampleLinearPolylineAtDistance(Candidate.QueueSplinePointsCm, (Count - 1) * Spacing, LastSlot))
    {
        OutError = LOCTEXT("InvalidGeneratedPath", "대기 자리 경로를 생성할 수 없습니다.");
        return false;
    }
    const FVector EntryPoint = LastSlot + TailTangent * Separation;
    Entry->LocalTransform.SetLocation(EntryPoint);
    Entry->LocalTransform.SetRotation((-TailTangent).Rotation().Quaternion());

    TArray<FVector> ReservedPath = Candidate.QueueSplinePointsCm;
    ReservedPath.Add(EntryPoint);
    Candidate.QueueReservedRects.Reset();
    Candidate.QueueReservedCells.Reset();
    const auto ReserveSegment = [&](const FVector& Start, const FVector& End, const bool bClipBuilding) -> bool
    {
        const FVector2D A = LocalPointCmToReservationGrid(Start, Candidate.FootprintSizeCells);
        const FVector2D B = LocalPointCmToReservationGrid(End, Candidate.FootprintSizeCells);
        const double MinX = FMath::FloorToDouble(FMath::Min(A.X, B.X) - HalfLaneWidth);
        const double MinY = FMath::FloorToDouble(FMath::Min(A.Y, B.Y) - HalfLaneWidth);
        const double MaxX = FMath::CeilToDouble(FMath::Max(A.X, B.X) + HalfLaneWidth);
        const double MaxY = FMath::CeilToDouble(FMath::Max(A.Y, B.Y) + HalfLaneWidth);
        if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MinY) || !FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY) ||
            MinX < MIN_int32 || MinY < MIN_int32 || MaxX > MAX_int32 || MaxY > MAX_int32)
        {
            OutError = LOCTEXT("LayoutOverflow", "대기열의 크기가 지원 범위를 넘었습니다.");
            return false;
        }
        const FFFZSCellRect Rect(FIntPoint(static_cast<int32>(MinX), static_cast<int32>(MinY)),
            FIntPoint(static_cast<int32>(MaxX), static_cast<int32>(MaxY)));
        if (!bClipBuilding)
        {
            Candidate.QueueReservedRects.Add(Rect);
            return true;
        }
        // Existing service points may lie exactly on the building edge. Reserve their
        // accessible surroundings without reclassifying any solid footprint as queue space.
        const auto AddIfValid = [&](const FIntPoint Min, const FIntPoint Max)
        {
            const FFFZSCellRect Part(Min, Max);
            if (Part.IsValid()) Candidate.QueueReservedRects.Add(Part);
        };
        const int32 Width = Candidate.FootprintSizeCells.X;
        const int32 Height = Candidate.FootprintSizeCells.Y;
        AddIfValid(Rect.Min, FIntPoint(FMath::Min(Rect.Max.X, 0), Rect.Max.Y));
        AddIfValid(FIntPoint(FMath::Max(Rect.Min.X, Width), Rect.Min.Y), Rect.Max);
        const int32 MiddleMinX = FMath::Max(Rect.Min.X, 0);
        const int32 MiddleMaxX = FMath::Min(Rect.Max.X, Width);
        AddIfValid(FIntPoint(MiddleMinX, Rect.Min.Y), FIntPoint(MiddleMaxX, FMath::Min(Rect.Max.Y, 0)));
        AddIfValid(FIntPoint(MiddleMinX, FMath::Max(Rect.Min.Y, Height)), FIntPoint(MiddleMaxX, Rect.Max.Y));
        return true;
    };
    for (int32 Index = 1; Index < ReservedPath.Num(); ++Index)
    {
        if (!ReserveSegment(ReservedPath[Index - 1], ReservedPath[Index], false)) return false;
    }
    // Switching layouts must not expose a formerly protected exit/service body to a new
    // neighbouring building. Preserve fixed anchor room and the head-to-service approach.
    if (!ReserveSegment(ServicePoint, Head, true)) return false;
    for (const FFFZSAnchorDefinition& Anchor : Candidate.Anchors)
    {
        const FVector Point = Anchor.LocalTransform.GetLocation();
        if (!ReserveSegment(Point, Point, true)) return false;
    }
    Candidate.QueueReservedRects = UFFZSGridLibrary::CanonicalizeCellRects(Candidate.QueueReservedRects);
    TArray<FText> Errors;
    if (!Candidate.IsValid(Errors))
    {
        OutError = Errors.IsEmpty() ? LOCTEXT("InvalidLayout", "이 시설에서는 해당 대기열 형태를 사용할 수 없습니다.") : Errors[0];
        return false;
    }
    InOutSpec = MoveTemp(Candidate);
    return true;
}

#undef LOCTEXT_NAMESPACE
