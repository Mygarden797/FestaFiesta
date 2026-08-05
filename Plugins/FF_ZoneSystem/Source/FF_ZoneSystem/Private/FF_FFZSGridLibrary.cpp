/**
 * @file FF_FFZSGridLibrary.cpp
 * @brief 1 cm 좌표 변환, 압축 사각형 연산, 내장 Preset과 fingerprint를 구현한다.
 *
 * 흐름: 부작용 없는 순수 계산으로 입력 Spec을 Cell 집합과 안정 식별값으로 변환한다.
 * 불변식: 1 Cell=1cm 계약과 정수 오버플로 방어를 지키며 압축 영역은 정확한 합집합을 보존한다.
 * 실패/성능: 비용은 개별 셀 수가 아니라 사각형 수에 비례하며 비정상 면적 입력은 조기에 거절한다.
 */

#include "FF_FFZSGridLibrary.h"

#include "FF_FFZSDeveloperSettings.h"
#include "Misc/Crc.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "FFZSGridLibrary"

/*
 * 좌표 계약
 * - FacilitySpec 영역은 footprint 좌하단 기준 로컬 1 cm 정수 좌표이고, Placement.OriginCell을 더해 Zone 좌표가 된다.
 * - QuarterTurns는 로컬 원점 주위 90도 회전이며, 회전 후에만 OriginCell을 더한다.
 * - 거리 값은 cm(= Unreal Unit), 배치 Cell 한 칸은 고정 1 cm다.
 * 압축 함수는 exact union/subtract 뒤 결정적 정렬을 유지하며 개별 셀 materialization은 10,000개로 제한한다.
 */
namespace
{
    /** 악성/손상 데이터가 64비트 면적 계산이나 게임 스레드 작업을 무한히 키우지 못하게 하는 절대 상한이다. */
    constexpr int32 AbsoluteMaxEnumeratedCellsPerFacility = 1'000'000;
    constexpr int32 LegacyMeterCellScale = 100;

    bool IsPositiveCellProductWithinLimit(const int64 Width, const int64 Height, const int64 CellLimit)
    {
        // Width*Height를 먼저 계산하지 않아 오버플로를 피한다.
        return Width > 0 && Height > 0 && Width <= CellLimit / Height;
    }

    bool TryIntPoint(const int64 X, const int64 Y, FIntPoint& OutPoint)
    {
        if (X < MIN_int32 || X > MAX_int32 || Y < MIN_int32 || Y > MAX_int32)
        {
            return false;
        }
        OutPoint = FIntPoint(static_cast<int32>(X), static_cast<int32>(Y));
        return true;
    }

    FFFZSCellRect TranslateRectChecked(const FFFZSCellRect& Rect, const FIntPoint Offset)
    {
        FIntPoint Min;
        FIntPoint Max;
        if (!Rect.IsValid() ||
            !TryIntPoint(static_cast<int64>(Rect.Min.X) + Offset.X, static_cast<int64>(Rect.Min.Y) + Offset.Y, Min) ||
            !TryIntPoint(static_cast<int64>(Rect.Max.X) + Offset.X, static_cast<int64>(Rect.Max.Y) + Offset.Y, Max))
        {
            return {};
        }
        return FFFZSCellRect(Min, Max);
    }

    /** Source에서 Cut 교집합을 빼 최대 네 개의 서로 겹치지 않는 사각형으로 분할한다. */
    TArray<FFFZSCellRect> SubtractOneRect(const FFFZSCellRect& Source, const FFFZSCellRect& Cut)
    {
        if (!Source.IsValid() || !Source.Intersects(Cut))
        {
            return Source.IsValid() ? TArray<FFFZSCellRect>{ Source } : TArray<FFFZSCellRect>{};
        }

        const int32 IX0 = FMath::Max(Source.Min.X, Cut.Min.X);
        const int32 IY0 = FMath::Max(Source.Min.Y, Cut.Min.Y);
        const int32 IX1 = FMath::Min(Source.Max.X, Cut.Max.X);
        const int32 IY1 = FMath::Min(Source.Max.Y, Cut.Max.Y);
        TArray<FFFZSCellRect> Result;
        if (Source.Min.Y < IY0) Result.Emplace(Source.Min, FIntPoint(Source.Max.X, IY0));
        if (IY1 < Source.Max.Y) Result.Emplace(FIntPoint(Source.Min.X, IY1), Source.Max);
        if (Source.Min.X < IX0) Result.Emplace(FIntPoint(Source.Min.X, IY0), FIntPoint(IX0, IY1));
        if (IX1 < Source.Max.X) Result.Emplace(FIntPoint(IX1, IY0), FIntPoint(Source.Max.X, IY1));
        return Result;
    }

    TArray<FFFZSCellRect> SubtractRectSets(
        const TArray<FFFZSCellRect>& Sources,
        const TArray<FFFZSCellRect>& Cuts)
    {
        TArray<FFFZSCellRect> Working = UFFZSGridLibrary::CanonicalizeCellRects(Sources);
        for (const FFFZSCellRect& Cut : UFFZSGridLibrary::CanonicalizeCellRects(Cuts))
        {
            TArray<FFFZSCellRect> Next;
            for (const FFFZSCellRect& Source : Working)
            {
                Next.Append(SubtractOneRect(Source, Cut));
            }
            Working = MoveTemp(Next);
            if (Working.IsEmpty())
            {
                break;
            }
        }
        return UFFZSGridLibrary::CanonicalizeCellRects(Working);
    }

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
            FIntPoint(MinX * LegacyMeterCellScale, MinY * LegacyMeterCellScale),
            FIntPoint(MaxX * LegacyMeterCellScale, MaxY * LegacyMeterCellScale));
    }

    /**
     * 이전 1 m 프리셋의 물리적 queue 폭/길이를 그대로 보존하되 단일 셀 목록 대신 한 사각형으로 저장한다.
     * QueueSplinePointsCm와 Anchor는 원래부터 cm였으므로 값을 바꾸지 않는다.
     */
    void ConfigureStraightPrototypeQueue(FFFZSFacilitySpec& Spec, const int32 LegacyQueueColumn)
    {
        constexpr double LegacyCellSizeCm = 100.0;
        const int32 FootprintWidthMeters = Spec.FootprintSizeCells.X / LegacyMeterCellScale;
        const int32 FootprintHeightMeters = Spec.FootprintSizeCells.Y / LegacyMeterCellScale;
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

int32 UFFZSGridLibrary::GetMaxEnumeratedCellsPerFacility()
{
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    const int32 ConfiguredLimit = Settings
        ? Settings->MaxEnumeratedCellsPerFacility
        : UFFZSDeveloperSettings::DefaultMaxEnumeratedCellsPerFacility;
    return FMath::Clamp(ConfiguredLimit, 1, AbsoluteMaxEnumeratedCellsPerFacility);
}

bool UFFZSGridLibrary::CanEnumerateFacilityCells(const FFFZSFacilitySpec& Spec)
{
    if (Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0 || Spec.ClearanceCells < 0)
    {
        return false;
    }

    const int64 Limit = GetMaxEnumeratedCellsPerFacility();
    const int64 ExpandedWidth = static_cast<int64>(Spec.FootprintSizeCells.X) + 2LL * Spec.ClearanceCells;
    const int64 ExpandedHeight = static_cast<int64>(Spec.FootprintSizeCells.Y) + 2LL * Spec.ClearanceCells;
    if (!IsPositiveCellProductWithinLimit(ExpandedWidth, ExpandedHeight, Limit))
    {
        return false;
    }

    TArray<FFFZSCellRect> LocalQueue = Spec.QueueReservedRects;
    if (Spec.QueueReservedCells.Num() > UFFZSDeveloperSettings::MaxMaterializedDebugCells)
    {
        // 새 스키마가 거대한 단일 셀 배열을 다시 도입하지 못하게 명시적으로 거부한다.
        return false;
    }
    for (const FIntPoint& Cell : Spec.QueueReservedCells)
    {
        if (Cell.X == MAX_int32 || Cell.Y == MAX_int32)
        {
            return false;
        }
        LocalQueue.Emplace(Cell, FIntPoint(Cell.X + 1, Cell.Y + 1));
    }
    for (const FFFZSCellRect& Rect : LocalQueue)
    {
        if (!Rect.IsValid())
        {
            return false;
        }
    }

    const int64 ExpandedArea = ExpandedWidth * ExpandedHeight;
    const int64 QueueArea = GetCellRectArea(LocalQueue);
    return QueueArea >= 0 && QueueArea <= Limit - ExpandedArea;
}

int32 UFFZSGridLibrary::NormalizeQuarterTurns(const int32 QuarterTurns)
{
    return ((QuarterTurns % 4) + 4) % 4;
}

FIntPoint UFFZSGridLibrary::RotateCell(const FIntPoint Cell, const int32 QuarterTurns)
{
    switch (NormalizeQuarterTurns(QuarterTurns))
    {
    case 1: return FIntPoint(-Cell.Y, Cell.X);
    case 2: return FIntPoint(-Cell.X, -Cell.Y);
    case 3: return FIntPoint(Cell.Y, -Cell.X);
    default: return Cell;
    }
}

FFFZSCellRect UFFZSGridLibrary::RotateRect(const FFFZSCellRect& Rect, const int32 QuarterTurns)
{
    if (!Rect.IsValid())
    {
        return {};
    }

    int64 MinX = Rect.Min.X;
    int64 MinY = Rect.Min.Y;
    int64 MaxX = Rect.Max.X;
    int64 MaxY = Rect.Max.Y;
    switch (NormalizeQuarterTurns(QuarterTurns))
    {
    case 1:
        // 셀 인덱스 회전 x'=-y, y'=x의 집합 경계다. 연속 좌표 회전(-Max)이 아니라
        // 포함되는 마지막 셀 -(Max-1)을 써야 하므로 음수축 경계에 +1이 필요하다.
        MinX = 1LL - Rect.Max.Y; MinY = Rect.Min.X;
        MaxX = 1LL - Rect.Min.Y; MaxY = Rect.Max.X;
        break;
    case 2:
        MinX = 1LL - Rect.Max.X; MinY = 1LL - Rect.Max.Y;
        MaxX = 1LL - Rect.Min.X; MaxY = 1LL - Rect.Min.Y;
        break;
    case 3:
        MinX = Rect.Min.Y; MinY = 1LL - Rect.Max.X;
        MaxX = Rect.Max.Y; MaxY = 1LL - Rect.Min.X;
        break;
    default:
        break;
    }

    FIntPoint RotatedMin;
    FIntPoint RotatedMax;
    return TryIntPoint(MinX, MinY, RotatedMin) && TryIntPoint(MaxX, MaxY, RotatedMax)
        ? FFFZSCellRect(RotatedMin, RotatedMax)
        : FFFZSCellRect{};
}

TArray<FFFZSCellRect> UFFZSGridLibrary::CanonicalizeCellRects(const TArray<FFFZSCellRect>& Rects)
{
    TArray<int32> YEdges;
    for (const FFFZSCellRect& Rect : Rects)
    {
        if (Rect.IsValid())
        {
            YEdges.Add(Rect.Min.Y);
            YEdges.Add(Rect.Max.Y);
        }
    }
    YEdges.Sort();
    for (int32 Index = YEdges.Num() - 1; Index > 0; --Index)
    {
        if (YEdges[Index] == YEdges[Index - 1])
        {
            YEdges.RemoveAt(Index, 1, EAllowShrinking::No);
        }
    }

    TArray<FFFZSCellRect> Result;
    for (int32 EdgeIndex = 1; EdgeIndex < YEdges.Num(); ++EdgeIndex)
    {
        const int32 Y0 = YEdges[EdgeIndex - 1];
        const int32 Y1 = YEdges[EdgeIndex];
        TArray<FIntPoint> Intervals;
        for (const FFFZSCellRect& Rect : Rects)
        {
            if (Rect.IsValid() && Rect.Min.Y <= Y0 && Rect.Max.Y >= Y1)
            {
                Intervals.Emplace(Rect.Min.X, Rect.Max.X);
            }
        }
        Intervals.Sort([](const FIntPoint& A, const FIntPoint& B)
        {
            return A.X != B.X ? A.X < B.X : A.Y < B.Y;
        });

        int32 IntervalIndex = 0;
        while (IntervalIndex < Intervals.Num())
        {
            int32 X0 = Intervals[IntervalIndex].X;
            int32 X1 = Intervals[IntervalIndex].Y;
            ++IntervalIndex;
            while (IntervalIndex < Intervals.Num() && Intervals[IntervalIndex].X <= X1)
            {
                X1 = FMath::Max(X1, Intervals[IntervalIndex].Y);
                ++IntervalIndex;
            }

            bool bExtended = false;
            for (FFFZSCellRect& Existing : Result)
            {
                if (Existing.Min.X == X0 && Existing.Max.X == X1 && Existing.Max.Y == Y0)
                {
                    Existing.Max.Y = Y1;
                    bExtended = true;
                    break;
                }
            }
            if (!bExtended)
            {
                Result.Emplace(FIntPoint(X0, Y0), FIntPoint(X1, Y1));
            }
        }
    }

    Result.Sort([](const FFFZSCellRect& A, const FFFZSCellRect& B)
    {
        if (A.Min.Y != B.Min.Y) return A.Min.Y < B.Min.Y;
        if (A.Min.X != B.Min.X) return A.Min.X < B.Min.X;
        if (A.Max.Y != B.Max.Y) return A.Max.Y < B.Max.Y;
        return A.Max.X < B.Max.X;
    });
    return Result;
}

bool UFFZSGridLibrary::DoCellRectSetsOverlap(
    const TArray<FFFZSCellRect>& A,
    const TArray<FFFZSCellRect>& B)
{
    for (const FFFZSCellRect& Left : A)
    {
        for (const FFFZSCellRect& Right : B)
        {
            if (Left.IsValid() && Right.IsValid() && Left.Intersects(Right))
            {
                return true;
            }
        }
    }
    return false;
}

bool UFFZSGridLibrary::IsCellInRects(const FIntPoint Cell, const TArray<FFFZSCellRect>& Rects)
{
    return Rects.ContainsByPredicate([Cell](const FFFZSCellRect& Rect) { return Rect.Contains(Cell); });
}

int64 UFFZSGridLibrary::GetCellRectArea(const TArray<FFFZSCellRect>& Rects)
{
    int64 Total = 0;
    for (const FFFZSCellRect& Rect : CanonicalizeCellRects(Rects))
    {
        const int64 Width = Rect.GetWidth();
        const int64 Height = Rect.GetHeight();
        if (Width <= 0 || Height <= 0 || Width > MAX_int64 / Height)
        {
            return -1;
        }
        const int64 Area = Width * Height;
        if (Total > MAX_int64 - Area)
        {
            return -1;
        }
        Total += Area;
    }
    return Total;
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetFootprintRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }
    const FFFZSCellRect Local(FIntPoint::ZeroValue, Spec.FootprintSizeCells);
    const FFFZSCellRect World = TranslateRectChecked(RotateRect(Local, Placement.QuarterTurns), Placement.OriginCell);
    return World.IsValid() ? TArray<FFFZSCellRect>{ World } : TArray<FFFZSCellRect>{};
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetQueueRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }

    TArray<FFFZSCellRect> LocalRects = Spec.QueueReservedRects;
    for (const FIntPoint& Cell : Spec.QueueReservedCells)
    {
        if (Cell.X == MAX_int32 || Cell.Y == MAX_int32)
        {
            return {};
        }
        LocalRects.Emplace(Cell, FIntPoint(Cell.X + 1, Cell.Y + 1));
    }

    TArray<FFFZSCellRect> Result;
    for (const FFFZSCellRect& Local : LocalRects)
    {
        const FFFZSCellRect World = TranslateRectChecked(RotateRect(Local, Placement.QuarterTurns), Placement.OriginCell);
        if (!World.IsValid())
        {
            return {};
        }
        Result.Add(World);
    }
    return CanonicalizeCellRects(Result);
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetClearanceRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }

    const FFFZSCellRect LocalExpanded(
        FIntPoint(-Spec.ClearanceCells, -Spec.ClearanceCells),
        FIntPoint(Spec.FootprintSizeCells.X + Spec.ClearanceCells,
            Spec.FootprintSizeCells.Y + Spec.ClearanceCells));
    const FFFZSCellRect Expanded = TranslateRectChecked(
        RotateRect(LocalExpanded, Placement.QuarterTurns), Placement.OriginCell);
    if (!Expanded.IsValid())
    {
        return {};
    }

    const TArray<FFFZSCellRect> FootprintRects = GetFootprintRects(Spec, Placement);
    const bool bHasQueueInput =
        !Spec.QueueReservedRects.IsEmpty() || !Spec.QueueReservedCells.IsEmpty();
    const TArray<FFFZSCellRect> QueueRects = GetQueueRects(Spec, Placement);
    // Queue 좌표만 오버플로한 경우 이를 "Queue 없음"으로 취급하면 clearance 일부가 권위 결과로 샌다.
    if (FootprintRects.IsEmpty() || (bHasQueueInput && QueueRects.IsEmpty()))
    {
        return {};
    }

    TArray<FFFZSCellRect> Cuts = FootprintRects;
    Cuts.Append(QueueRects);
    return SubtractRectSets(TArray<FFFZSCellRect>{ Expanded }, Cuts);
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetReservedRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }

    const TArray<FFFZSCellRect> FootprintRects = GetFootprintRects(Spec, Placement);
    const bool bHasQueueInput =
        !Spec.QueueReservedRects.IsEmpty() || !Spec.QueueReservedCells.IsEmpty();
    const TArray<FFFZSCellRect> QueueRects = GetQueueRects(Spec, Placement);

    // Expanded 자체도 별도로 검사한다. Clearance 결과는 정상적으로 빈 합집합일 수 있어
    // 빈 배열만으로는 좌표 오버플로와 "남은 clearance 없음"을 구분할 수 없기 때문이다.
    const FFFZSCellRect LocalExpanded(
        FIntPoint(-Spec.ClearanceCells, -Spec.ClearanceCells),
        FIntPoint(Spec.FootprintSizeCells.X + Spec.ClearanceCells,
            Spec.FootprintSizeCells.Y + Spec.ClearanceCells));
    const FFFZSCellRect Expanded = TranslateRectChecked(
        RotateRect(LocalExpanded, Placement.QuarterTurns), Placement.OriginCell);
    if (FootprintRects.IsEmpty() || !Expanded.IsValid() ||
        (bHasQueueInput && QueueRects.IsEmpty()))
    {
        return {};
    }

    TArray<FFFZSCellRect> Result = FootprintRects;
    Result.Append(GetClearanceRects(Spec, Placement));
    Result.Append(QueueRects);
    return CanonicalizeCellRects(Result);
}

bool UFFZSGridLibrary::TryMaterializeCellRects(
    const TArray<FFFZSCellRect>& Rects,
    const int32 MaxCells,
    TArray<FIntPoint>& OutCells)
{
    OutCells.Reset();
    if (MaxCells < 0 || Rects.ContainsByPredicate(
        [](const FFFZSCellRect& Rect) { return !Rect.IsValid(); }))
    {
        // CanonicalizeCellRects는 합집합 계산용으로 무효 Rect를 버린다. 디버그 펼치기 API는
        // 손상 입력을 조용히 빈 성공으로 바꾸지 않고 원래 계약대로 전체 실패해야 한다.
        return false;
    }
    const TArray<FFFZSCellRect> Canonical = CanonicalizeCellRects(Rects);
    const int64 Area = GetCellRectArea(Canonical);
    if (Area < 0 || Area > MaxCells || Area > MAX_int32)
    {
        return false;
    }

    OutCells.Reserve(static_cast<int32>(Area));
    for (const FFFZSCellRect& Rect : Canonical)
    {
        for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
        {
            for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
            {
                OutCells.Emplace(X, Y);
            }
        }
    }
    return true;
}

TArray<FIntPoint> UFFZSGridLibrary::GetFootprintCells(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FIntPoint> Result;
    TryMaterializeCellRects(GetFootprintRects(Spec, Placement),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result);
    return Result;
}

TArray<FIntPoint> UFFZSGridLibrary::GetClearanceCells(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FIntPoint> Result;
    TryMaterializeCellRects(GetClearanceRects(Spec, Placement),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result);
    return Result;
}

TArray<FIntPoint> UFFZSGridLibrary::GetQueueCells(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FIntPoint> Result;
    TryMaterializeCellRects(GetQueueRects(Spec, Placement),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result);
    return Result;
}

TArray<FIntPoint> UFFZSGridLibrary::GetReservedCells(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FIntPoint> Result;
    TryMaterializeCellRects(GetReservedRects(Spec, Placement),
        UFFZSDeveloperSettings::MaxMaterializedDebugCells, Result);
    return Result;
}

FFFZSFacilitySpec UFFZSGridLibrary::MakeBuiltInFacilitySpec(const EFFZSBuiltInFacilityPreset Preset)
{
    FFFZSFacilitySpec Spec;
    Spec.SchemaVersion = 2;
    Spec.ClearanceCells = 100;
    Spec.QueueRules.SlotSpacingCm = 115.0f;
    Spec.QueueRules.SlotArrivalToleranceCm = 25.0f;
    Spec.QueueRules.NavigationProjectionExtentCm = 30.0f;
    Spec.QueueRules.BlockedTimeoutSeconds = 5.0f;
    Spec.QueueRules.MaxRepathAttempts = 2;

    // FoodTruckBasic and ToiletBasic are accepted baselines whose approved authored field values
    // remain compatible. Every other numeric preset value below is a prototype hypothesis that
    // must be measured in playtests before production tuning.
    // FoodTruckBasic/ToiletBasic은 승인된 기준 수치이므로 저작 값의 회귀를 허용하지 않는다.
    // 나머지 프리셋 수치는 플레이테스트로 측정하고 조정해야 하는 가설이며, 공통 기본값을 먼저
    // 채운 뒤 case별 차이만 덮어써 누락 필드가 프리셋마다 달라지는 것을 막는다.
    switch (Preset)
    {
    case EFFZSBuiltInFacilityPreset::FoodTruckBasic:
    {
        Spec.FacilityDefinitionId = TEXT("FD_Booth_FoodTruckBasic");
        Spec.DisplayName = LOCTEXT("FoodTruckBasic", "Basic Food Truck");
        Spec.Category = EFFZSFacilityCategory::Booth;
        Spec.FootprintSizeCells = FIntPoint(600, 300);
        Spec.QueueRules.MaxWaitingAgents = 12;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 6.0f;
        Spec.QueueRules.AbandonAfterSeconds = 30.0f;
        Spec.InstallCostKey = TEXT("Cost.FoodTruck.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.FoodTruck.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.FoodTruck.Basic");

        // 구 1 m 프리셋의 U자형 16셀(각 100x100 cm)을 세 압축 사각형으로 보존한다.
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
    }
    case EFFZSBuiltInFacilityPreset::ToiletBasic:
    {
        Spec.FacilityDefinitionId = TEXT("FD_Toilet_Basic");
        Spec.DisplayName = LOCTEXT("ToiletBasic", "Basic Restroom");
        Spec.Category = EFFZSFacilityCategory::Toilet;
        Spec.FootprintSizeCells = FIntPoint(400, 400);
        Spec.QueueRules.MaxWaitingAgents = 8;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 4.0f;
        Spec.QueueRules.AbandonAfterSeconds = 30.0f;
        Spec.InstallCostKey = TEXT("Cost.Toilet.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Toilet.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Toilet.Basic");

        // 구 1 m 프리셋의 2열 x 12행 예약 폭을 한 사각형으로 보존한다.
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
    }
    case EFFZSBuiltInFacilityPreset::InfoBasic:
    {
        // Prototype hypothesis: a compact single-desk booth serves six visitors without spillover.
        Spec.FacilityDefinitionId = TEXT("FD_Info_Basic");
        Spec.DisplayName = LOCTEXT("InfoBasic", "Basic Information Booth");
        Spec.Category = EFFZSFacilityCategory::Info;
        Spec.FootprintSizeCells = FIntPoint(300, 200);
        Spec.QueueRules.MaxWaitingAgents = 6;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 4.0f;
        Spec.QueueRules.AbandonAfterSeconds = 25.0f;
        Spec.InstallCostKey = TEXT("Cost.Info.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Info.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Info.Basic");
        ConfigureStraightPrototypeQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(0.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(0.0f, 100.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-150.0f, 0.0f, 0.0f), 180.0f)
        };
        break;
    }
    case EFFZSBuiltInFacilityPreset::RestBasic:
    {
        // Prototype hypothesis: two access points and four waiting places support a small rest area.
        Spec.FacilityDefinitionId = TEXT("FD_Rest_Basic");
        Spec.DisplayName = LOCTEXT("RestBasic", "Basic Rest Area");
        Spec.Category = EFFZSFacilityCategory::Rest;
        Spec.FootprintSizeCells = FIntPoint(400, 300);
        Spec.QueueRules.MaxWaitingAgents = 4;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 12.0f;
        Spec.QueueRules.AbandonAfterSeconds = 60.0f;
        Spec.InstallCostKey = TEXT("Cost.Rest.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Rest.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Rest.Basic");
        ConfigureStraightPrototypeQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-200.0f, 0.0f, 0.0f), 180.0f)
        };
        break;
    }
    case EFFZSBuiltInFacilityPreset::PerformanceBasic:
    {
        // Prototype hypothesis: a two-gate stage footprint absorbs a sixteen-person entry queue.
        Spec.FacilityDefinitionId = TEXT("FD_Performance_Basic");
        Spec.DisplayName = LOCTEXT("PerformanceBasic", "Basic Performance Stage");
        Spec.Category = EFFZSFacilityCategory::Performance;
        Spec.FootprintSizeCells = FIntPoint(800, 500);
        Spec.QueueRules.MaxWaitingAgents = 16;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 10.0f;
        Spec.QueueRules.AbandonAfterSeconds = 45.0f;
        Spec.InstallCostKey = TEXT("Cost.Performance.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Performance.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Performance.Basic");
        ConfigureStraightPrototypeQueue(Spec, 3);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 300.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-125.0f, 250.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(125.0f, 250.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-400.0f, 0.0f, 0.0f), 180.0f)
        };
        break;
    }
    case EFFZSBuiltInFacilityPreset::SafetyBasic:
    {
        // Prototype hypothesis: two response stations serve six waiting visitors with a short timeout.
        Spec.FacilityDefinitionId = TEXT("FD_Safety_Basic");
        Spec.DisplayName = LOCTEXT("SafetyBasic", "Basic Safety Post");
        Spec.Category = EFFZSFacilityCategory::Safety;
        Spec.FootprintSizeCells = FIntPoint(400, 300);
        Spec.QueueRules.MaxWaitingAgents = 6;
        Spec.QueueRules.ServiceCapacity = 2;
        Spec.QueueRules.ServiceDurationSeconds = 8.0f;
        Spec.QueueRules.AbandonAfterSeconds = 15.0f;
        Spec.InstallCostKey = TEXT("Cost.Safety.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.Safety.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.Safety.Basic");
        ConfigureStraightPrototypeQueue(Spec, 1);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(-50.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(-75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 1, FVector(75.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-200.0f, 0.0f, 0.0f), 180.0f)
        };
        break;
    }
    case EFFZSBuiltInFacilityPreset::PhotoSpotBasic:
    {
        // Prototype hypothesis: one interaction point and eight slots prevent photo-queue spillover.
        Spec.FacilityDefinitionId = TEXT("FD_PhotoSpot_Basic");
        Spec.DisplayName = LOCTEXT("PhotoSpotBasic", "Basic Photo Spot");
        Spec.Category = EFFZSFacilityCategory::PhotoSpot;
        Spec.FootprintSizeCells = FIntPoint(500, 300);
        Spec.QueueRules.MaxWaitingAgents = 8;
        Spec.QueueRules.ServiceCapacity = 1;
        Spec.QueueRules.ServiceDurationSeconds = 5.0f;
        Spec.QueueRules.AbandonAfterSeconds = 25.0f;
        Spec.InstallCostKey = TEXT("Cost.PhotoSpot.Basic");
        Spec.RevenueRuleKey = TEXT("Revenue.PhotoSpot.Basic");
        Spec.ExperienceEffectKey = TEXT("Experience.PhotoSpot.Basic");
        ConfigureStraightPrototypeQueue(Spec, 2);
        Spec.Anchors = {
            MakeAnchor(EFFZSAnchorType::Entry, 0, FVector(0.0f, 200.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Service, 0, FVector(0.0f, 150.0f, 0.0f), -90.0f),
            MakeAnchor(EFFZSAnchorType::Exit, 0, FVector(-250.0f, 0.0f, 0.0f), 180.0f)
        };
        break;
    }
    default:
        Spec.SchemaVersion = 0;
        break;
    }

    return Spec;
}

TArray<FName> UFFZSGridLibrary::GetAcceptedGameplayZoneIds()
{
    return {
        TEXT("GZ_SunsetBeachClub"),
        TEXT("GZ_DadaPocha"),
        TEXT("GZ_DadaYajang"),
        TEXT("GZ_DadaepoForest"),
        TEXT("GZ_FountainPirates"),
        TEXT("GZ_SunsetLounge"),
        TEXT("GZ_FleaMarket"),
        TEXT("GZ_SunsetDogPark"),
        TEXT("GZ_SunsetWellness"),
        TEXT("GZ_PublicParkingWest"),
        TEXT("GZ_TemporaryParkingEast"),
        TEXT("GZ_Operations")
    };
}

FFFZSCommand UFFZSGridLibrary::MakeInstallCommand(const FFFZSFacilitySpec& Spec, const FName ZoneId, const FFFZSGridPlacement Placement, const int64 ExpectedWorldRevision)
{
    FFFZSCommand Command;
    Command.CommandId = FGuid::NewGuid();
    Command.TransactionId = FGuid::NewGuid();
    Command.ExpectedWorldRevision = ExpectedWorldRevision;
    Command.Type = EFFZSCommandType::Install;
    Command.FacilityInstanceId = FGuid::NewGuid();
    Command.FacilitySpec = Spec;
    Command.ZoneId = ZoneId;
    Command.Placement = Placement;
    Command.Placement.QuarterTurns = NormalizeQuarterTurns(Command.Placement.QuarterTurns);
    return Command;
}

/*
 * 내구성 있는 Payload 동일성 문자열이다. CommandId는 영수증 조회 키이므로 의도적으로 제외하고,
 * 그 ID로 재시도했을 때 같아야 하는 Transaction/Revision/Placement/Spec 전체를 포함한다.
 * v4 문자열은 길이 접두사를 붙여 구분자 포함 값의 모호성을 없애고, float은 17자리 유효숫자,
 * FText는 문화권 비의존 직렬화로 기록한다. 배열은 저작 순서까지 의미에 포함하므로 순서 변경도
 * 다른 Payload다. format 버전 변경은 기존 영수증 호환 정책과 함께 다뤄야 한다.
 */
FString UFFZSGridLibrary::BuildCommandFingerprint(const FFFZSCommand& Command)
{
    FString Canonical;
    Canonical.Reserve(2048);

    const auto AppendString = [&Canonical](const TCHAR* Label, const FString& Value)
    {
        Canonical += FString::Printf(TEXT("%s=%d:"), Label, Value.Len());
        Canonical += Value;
        Canonical += TEXT(";");
    };
    const auto AppendInt = [&AppendString](const TCHAR* Label, const int64 Value)
    {
        AppendString(Label, FString::Printf(TEXT("%lld"), static_cast<long long>(Value)));
    };
    const auto AppendFloat = [&AppendString](const TCHAR* Label, const double Value)
    {
        AppendString(Label, FString::Printf(TEXT("%.17g"), Value));
    };
    const auto AppendVector = [&AppendFloat](const FString& Label, const FVector& Value)
    {
        AppendFloat(*(Label + TEXT(".x")), Value.X);
        AppendFloat(*(Label + TEXT(".y")), Value.Y);
        AppendFloat(*(Label + TEXT(".z")), Value.Z);
    };
    const auto AppendTransform = [&AppendFloat, &AppendVector](const FString& Label, const FTransform& Value)
    {
        AppendVector(Label + TEXT(".location"), Value.GetLocation());
        const FQuat Rotation = Value.GetRotation();
        AppendFloat(*(Label + TEXT(".rotation.x")), Rotation.X);
        AppendFloat(*(Label + TEXT(".rotation.y")), Rotation.Y);
        AppendFloat(*(Label + TEXT(".rotation.z")), Rotation.Z);
        AppendFloat(*(Label + TEXT(".rotation.w")), Rotation.W);
        AppendVector(Label + TEXT(".scale"), Value.GetScale3D());
    };

    AppendString(TEXT("format"), TEXT("FFZSCommand.v4"));
    AppendString(TEXT("transaction_id"), Command.TransactionId.ToString(EGuidFormats::Digits));
    AppendInt(TEXT("expected_world_revision"), Command.ExpectedWorldRevision);
    AppendInt(TEXT("command_type"), static_cast<int64>(Command.Type));
    AppendString(TEXT("facility_instance_id"), Command.FacilityInstanceId.ToString(EGuidFormats::Digits));
    AppendString(TEXT("zone_id"), Command.ZoneId.ToString());
    AppendInt(TEXT("placement.origin_x"), Command.Placement.OriginCell.X);
    AppendInt(TEXT("placement.origin_y"), Command.Placement.OriginCell.Y);
    AppendInt(TEXT("placement.quarter_turns"), NormalizeQuarterTurns(Command.Placement.QuarterTurns));

    const FFFZSFacilitySpec& Spec = Command.FacilitySpec;
    AppendInt(TEXT("spec.schema_version"), Spec.SchemaVersion);
    AppendString(TEXT("spec.definition_id"), Spec.FacilityDefinitionId.ToString());
    FString CanonicalDisplayName;
    FTextStringHelper::WriteToBuffer(CanonicalDisplayName, Spec.DisplayName);
    AppendString(TEXT("spec.display_name"), CanonicalDisplayName);
    AppendInt(TEXT("spec.category"), static_cast<int64>(Spec.Category));
    AppendInt(TEXT("spec.footprint_x"), Spec.FootprintSizeCells.X);
    AppendInt(TEXT("spec.footprint_y"), Spec.FootprintSizeCells.Y);
    AppendInt(TEXT("spec.clearance_cells"), Spec.ClearanceCells);

    // v4는 압축 사각형 원본과 구 호환 단일 셀을 모두 기록한다. 잘못된 사양도 서로 다른
    // 멱등성 payload가 같은 지문으로 합쳐지지 않도록 검증 전 원본 순서를 의도적으로 보존한다.
    AppendInt(TEXT("spec.queue_rects.count"), Spec.QueueReservedRects.Num());
    for (int32 Index = 0; Index < Spec.QueueReservedRects.Num(); ++Index)
    {
        const FFFZSCellRect& Rect = Spec.QueueReservedRects[Index];
        AppendInt(*FString::Printf(TEXT("spec.queue_rects[%d].min_x"), Index), Rect.Min.X);
        AppendInt(*FString::Printf(TEXT("spec.queue_rects[%d].min_y"), Index), Rect.Min.Y);
        AppendInt(*FString::Printf(TEXT("spec.queue_rects[%d].max_x"), Index), Rect.Max.X);
        AppendInt(*FString::Printf(TEXT("spec.queue_rects[%d].max_y"), Index), Rect.Max.Y);
    }
    AppendInt(TEXT("spec.legacy_queue_cells.count"), Spec.QueueReservedCells.Num());
    for (int32 Index = 0; Index < Spec.QueueReservedCells.Num(); ++Index)
    {
        AppendInt(*FString::Printf(TEXT("spec.legacy_queue_cells[%d].x"), Index), Spec.QueueReservedCells[Index].X);
        AppendInt(*FString::Printf(TEXT("spec.legacy_queue_cells[%d].y"), Index), Spec.QueueReservedCells[Index].Y);
    }

    AppendInt(TEXT("spec.queue_spline.count"), Spec.QueueSplinePointsCm.Num());
    for (int32 Index = 0; Index < Spec.QueueSplinePointsCm.Num(); ++Index)
    {
        AppendVector(FString::Printf(TEXT("spec.queue_spline[%d]"), Index), Spec.QueueSplinePointsCm[Index]);
    }

    const FFFZSQueueRules& Queue = Spec.QueueRules;
    AppendInt(TEXT("spec.queue.max_waiting"), Queue.MaxWaitingAgents);
    AppendFloat(TEXT("spec.queue.slot_spacing_cm"), Queue.SlotSpacingCm);
    AppendInt(TEXT("spec.queue.service_capacity"), Queue.ServiceCapacity);
    AppendFloat(TEXT("spec.queue.service_duration_seconds"), Queue.ServiceDurationSeconds);
    AppendFloat(TEXT("spec.queue.abandon_after_seconds"), Queue.AbandonAfterSeconds);
    AppendFloat(TEXT("spec.queue.arrival_tolerance_cm"), Queue.SlotArrivalToleranceCm);
    AppendFloat(TEXT("spec.queue.nav_projection_cm"), Queue.NavigationProjectionExtentCm);
    AppendFloat(TEXT("spec.queue.blocked_timeout_seconds"), Queue.BlockedTimeoutSeconds);
    AppendInt(TEXT("spec.queue.max_repath_attempts"), Queue.MaxRepathAttempts);

    AppendInt(TEXT("spec.anchors.count"), Spec.Anchors.Num());
    for (int32 Index = 0; Index < Spec.Anchors.Num(); ++Index)
    {
        const FFFZSAnchorDefinition& Anchor = Spec.Anchors[Index];
        AppendInt(*FString::Printf(TEXT("spec.anchors[%d].type"), Index), static_cast<int64>(Anchor.Type));
        AppendInt(*FString::Printf(TEXT("spec.anchors[%d].index"), Index), Anchor.Index);
        AppendTransform(FString::Printf(TEXT("spec.anchors[%d].transform"), Index), Anchor.LocalTransform);
    }

    AppendInt(TEXT("spec.allowed_zones.count"), Spec.AllowedZoneIds.Num());
    for (int32 Index = 0; Index < Spec.AllowedZoneIds.Num(); ++Index)
    {
        AppendString(*FString::Printf(TEXT("spec.allowed_zones[%d]"), Index), Spec.AllowedZoneIds[Index].ToString());
    }
    AppendString(TEXT("spec.install_cost_key"), Spec.InstallCostKey.ToString());
    AppendString(TEXT("spec.revenue_rule_key"), Spec.RevenueRuleKey.ToString());
    AppendString(TEXT("spec.experience_effect_key"), Spec.ExperienceEffectKey.ToString());
    AppendString(TEXT("spec.visual_mesh"), Spec.VisualMesh.ToSoftObjectPath().ToString());
    AppendFloat(TEXT("spec.placement_obstacle_height_cm"), Spec.PlacementObstacleHeightCm);
    return Canonical;
}

// CRC32는 빠른 비교/진단용 축약값이다. 충돌 없는 멱등성 판정에는 위 전체 Fingerprint를 사용한다.
uint32 UFFZSGridLibrary::HashCommandPayload(const FFFZSCommand& Command)
{
    const FString Fingerprint = BuildCommandFingerprint(Command);
    return FCrc::StrCrc32(*Fingerprint);
}

FText UFFZSGridLibrary::PlacementFailureToText(const EFFZSPlacementFailure Failure)
{
    switch (Failure)
    {
    case EFFZSPlacementFailure::None: return LOCTEXT("None", "No failure");
    case EFFZSPlacementFailure::ZoneNotFound: return LOCTEXT("ZoneNotFound", "Zone not found");
    case EFFZSPlacementFailure::FacilityNotFound: return LOCTEXT("FacilityNotFound", "Facility not found");
    case EFFZSPlacementFailure::DuplicateZoneId: return LOCTEXT("DuplicateZoneId", "Duplicate Zone ID");
    case EFFZSPlacementFailure::DuplicateFacilityId: return LOCTEXT("DuplicateFacilityId", "Duplicate Facility ID");
    case EFFZSPlacementFailure::CategoryNotAllowed: return LOCTEXT("CategoryNotAllowed", "Facility category is not allowed in this Zone");
    case EFFZSPlacementFailure::OutsideZone: return LOCTEXT("OutsideZone", "Footprint, clearance, or maximum queue area is outside the Zone");
    case EFFZSPlacementFailure::InvalidGround: return LOCTEXT("InvalidGround", "Required ground could not be found");
    case EFFZSPlacementFailure::ExcessSlope: return LOCTEXT("ExcessSlope", "Ground slope exceeds the configured limit");
    case EFFZSPlacementFailure::HeightVarianceExceeded: return LOCTEXT("HeightVariance", "Ground height variance exceeds the configured limit");
    case EFFZSPlacementFailure::PlacementCollision: return LOCTEXT("PlacementCollision", "Facility footprint collides with an occupied cell or world obstacle");
    case EFFZSPlacementFailure::InsufficientClearance: return LOCTEXT("InsufficientClearance", "The required clearance ring is blocked");
    case EFFZSPlacementFailure::QueueFootprintBlocked: return LOCTEXT("QueueBlocked", "The maximum queue footprint is blocked");
    case EFFZSPlacementFailure::ProtectedRouteBlocked: return LOCTEXT("ProtectedRoute", "Placement intersects a protected route or area");
    case EFFZSPlacementFailure::RequiredRouteDisconnected: return LOCTEXT("RouteDisconnected", "A required navigation point is disconnected");
    case EFFZSPlacementFailure::AccessibilityRouteBlocked: return LOCTEXT("AccessibilityBlocked", "Placement blocks an accessibility route");
    case EFFZSPlacementFailure::WrongStage: return LOCTEXT("WrongStage", "World changes are not permitted in the current phase");
    case EFFZSPlacementFailure::FacilityOccupied: return LOCTEXT("Occupied", "Close and drain the facility before moving or removing it");
    case EFFZSPlacementFailure::InvalidFacilityState: return LOCTEXT("InvalidState", "Facility state does not allow this command");
    case EFFZSPlacementFailure::StaleWorldRevision: return LOCTEXT("StaleRevision", "The world changed after this command was prepared");
    case EFFZSPlacementFailure::DuplicatePayloadMismatch: return LOCTEXT("DuplicateMismatch", "The Command ID was reused with a different payload");
    case EFFZSPlacementFailure::NavigationUnavailable: return LOCTEXT("NavigationUnavailable", "Navigation data is unavailable");
    case EFFZSPlacementFailure::NavRebuildTimeout: return LOCTEXT("NavTimeout", "Navigation rebuild timed out");
    case EFFZSPlacementFailure::DataInvalid: return LOCTEXT("DataInvalid", "Facility or Zone data is invalid");
    case EFFZSPlacementFailure::ExternalPolicyRejected: return LOCTEXT("PolicyRejected", "An external team policy rejected the change");
    default: return LOCTEXT("InternalError", "Internal Zone/Facility error");
    }
}

#undef LOCTEXT_NAMESPACE
