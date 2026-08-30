// FF_FFZSGridLibrary.cpp

// 1 cm 좌표 변환, 압축 사각형 연산과 fingerprint를 구현.
// 흐름: 부작용 없는 순수 계산으로 입력 Spec을 Cell 집합과 안정 식별값으로 변환.
// 불변식: 1 Cell=1cm 계약과 정수 오버플로 방어를 지키며 압축 영역은 정확한 합집합을 보존.
// 실패/성능: 비용은 개별 셀 수가 아니라 사각형 수에 비례하며 비정상 면적 입력은 조기에 거절.
 

#include "FF_FFZSGridLibrary.h"

#include "FF_FFZSDeveloperSettings.h"
#include "FF_FFZSFacilityDefinition.h"
#include "FF_FFZSOrientedGeometry.h"
#include "Engine/AssetManager.h"
#include "Misc/Crc.h"
#include "Internationalization/Text.h"

#define LOCTEXT_NAMESPACE "FFZSGridLibrary"

// 좌표 계약
// FacilitySpec 영역은 footprint 좌하단 기준 로컬 1 cm 정수 좌표이고, Placement.OriginCell을 더해 Zone 좌표가 된다.
// 레거시 QuarterTurns는 로컬 원점 주위 양의 Yaw 90도 회전이며, 회전 후에만 OriginCell을 더한다.
// 거리 값은 cm(= Unreal Unit), 배치 Cell 한 칸은 고정 1 cm다.
// 압축 함수는 exact union/subtract 뒤 결정적 정렬을 유지하며 개별 셀 materialization은 10,000개로 제한한다.
 
namespace
{
    // 악성/손상 데이터가 64비트 면적 계산이나 게임 스레드 작업을 무한히 키우지 못하게 하는 절대 상한이다.
    // [하드코딩 제거] 같은 값을 두 번 적지 않도록 설정 헤더의 상수를 그대로 재사용한다.
    // 예전에는 여기와 UFFZSDeveloperSettings에 10,000,000이 따로 적혀 있어 한쪽만 바꾸면
    // 설정 Clamp 범위와 실제 하드 상한이 조용히 어긋날 수 있었다.
    constexpr int32 AbsoluteMaxEnumeratedCellsPerFacility =
        UFFZSDeveloperSettings::DefaultMaxEnumeratedCellsPerFacility;
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

    // Source에서 Cut 교집합을 빼 최대 네 개의 서로 겹치지 않는 사각형으로 분할한다. 
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

    // [최적화] 이미 정규화되어 서로 겹치지 않는 집합의 면적을 중복 정규화 없이 더한다.
    // GetCellRectArea는 임의 입력을 받으므로 내부에서 CanonicalizeCellRects를 한 번 더 돌린다.
    // 정규 출력이 확실한 자리(예: TryMaterializeCellRects)에서는 그 재정규화가 순수 낭비다.
    int64 SumDisjointRectArea(const TArray<FFFZSCellRect>& CanonicalRects)
    {
        int64 Total = 0;
        for (const FFFZSCellRect& Rect : CanonicalRects)
        {
            const int64 Area = Rect.GetArea();
            if (Area < 0 || Total > MAX_int64 - Area)
            {
                return -1;
            }
            Total += Area;
        }
        return Total;
    }

    // 임의 Yaw에서도 회전 전 로컬 공간의 union/subtract를 먼저 끝내야 영역 사이에
    // 보수적 AABB가 끼어들지 않는다. 이 로컬 결과만 OBB로 변환해 권위 합집합을 보존한다.
    bool TryBuildLocalQueueRects(
        const FFFZSFacilitySpec& Spec,
        TArray<FFFZSCellRect>& OutRects)
    {
        OutRects = Spec.QueueReservedRects;
        OutRects.Reserve(OutRects.Num() + Spec.QueueReservedCells.Num());
        for (const FIntPoint& Cell : Spec.QueueReservedCells)
        {
            if (Cell.X == MAX_int32 || Cell.Y == MAX_int32)
            {
                OutRects.Reset();
                return false;
            }
            OutRects.Emplace(Cell, FIntPoint(Cell.X + 1, Cell.Y + 1));
        }
        if (OutRects.ContainsByPredicate([](const FFFZSCellRect& Rect) { return !Rect.IsValid(); }))
        {
            OutRects.Reset();
            return false;
        }
        OutRects = UFFZSGridLibrary::CanonicalizeCellRects(OutRects);
        return true;
    }

    struct FFacilityLocalRegions
    {
        TArray<FFFZSCellRect> Footprint;
        TArray<FFFZSCellRect> Clearance;
        TArray<FFFZSCellRect> Queue;
        TArray<FFFZSCellRect> Reserved;
    };

    FFacilityLocalRegions BuildFacilityLocalRegions(const FFFZSFacilitySpec& Spec)
    {
        FFacilityLocalRegions Regions;
        if (!UFFZSGridLibrary::CanEnumerateFacilityCells(Spec))
        {
            return Regions;
        }

        Regions.Footprint.Add(FFFZSCellRect(FIntPoint::ZeroValue, Spec.FootprintSizeCells));
        if (!TryBuildLocalQueueRects(Spec, Regions.Queue))
        {
            return {};
        }

        const FFFZSCellRect Expanded(
            FIntPoint(-Spec.ClearanceCells, -Spec.ClearanceCells),
            FIntPoint(
                Spec.FootprintSizeCells.X + Spec.ClearanceCells,
                Spec.FootprintSizeCells.Y + Spec.ClearanceCells));
        if (!Expanded.IsValid())
        {
            return {};
        }

        TArray<FFFZSCellRect> Cuts = Regions.Footprint;
        Cuts.Append(Regions.Queue);
        Regions.Clearance = SubtractRectSets(TArray<FFFZSCellRect>{ Expanded }, Cuts);

        TArray<FFFZSCellRect> ReservedInput = Regions.Footprint;
        ReservedInput.Append(Regions.Clearance);
        ReservedInput.Append(Regions.Queue);
        Regions.Reserved = UFFZSGridLibrary::CanonicalizeCellRects(ReservedInput);
        return Regions;
    }

    bool ConvertLocalRectsToOriented(
        const TArray<FFFZSCellRect>& LocalRects,
        const FFFZSGridPlacement& Placement,
        TArray<FFFZSOrientedRect>& OutRects)
    {
        OutRects.Reset();
        OutRects.Reserve(LocalRects.Num());
        const float YawDegrees = UFFZSGridLibrary::YawCentidegreesToDegrees(
            UFFZSGridLibrary::GetEffectiveYawCentidegrees(Placement));
        for (const FFFZSCellRect& LocalRect : LocalRects)
        {
            const FFFZSOrientedRect Oriented = FFZSOrientedGeometry::MakeFromLocalRect(
                LocalRect, Placement.OriginCell, YawDegrees);
            if (!Oriented.IsValid() ||
                !FFZSOrientedGeometry::ToConservativeCellRect(Oriented).IsValid())
            {
                OutRects.Reset();
                return false;
            }
            OutRects.Add(Oriented);
        }
        return true;
    }

    TArray<FFFZSCellRect> MakeConservativeRects(
        const TArray<FFFZSOrientedRect>& OrientedRects)
    {
        TArray<FFFZSCellRect> Result;
        Result.Reserve(OrientedRects.Num());
        for (const FFFZSOrientedRect& Oriented : OrientedRects)
        {
            const FFFZSCellRect Bounds = FFZSOrientedGeometry::ToConservativeCellRect(Oriented);
            if (!Bounds.IsValid())
            {
                return {};
            }
            Result.Add(Bounds);
        }
        return UFFZSGridLibrary::CanonicalizeCellRects(Result);
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

bool UFFZSGridLibrary::HasSafeFacilitySpecCardinality(const FFFZSFacilitySpec& Spec)
{
    return Spec.QueueReservedRects.Num() <= UFFZSDeveloperSettings::MaxQueueReservedRects &&
        Spec.QueueReservedCells.Num() <= UFFZSDeveloperSettings::MaxLegacyQueueReservedCells &&
        Spec.QueueSplinePointsCm.Num() <= UFFZSDeveloperSettings::MaxQueueSplinePoints &&
        Spec.Anchors.Num() <= UFFZSDeveloperSettings::MaxFacilityAnchors &&
        Spec.AllowedZoneIds.Num() <= UFFZSDeveloperSettings::MaxAllowedZoneIds &&
        Spec.QueueRules.MaxWaitingAgents <= UFFZSDeveloperSettings::MaxQueueWaitingAgents &&
        Spec.QueueRules.ServiceCapacity <= UFFZSDeveloperSettings::MaxFacilityServiceCapacity;
}

bool UFFZSGridLibrary::CanEnumerateFacilityCells(const FFFZSFacilitySpec& Spec)
{
    if (Spec.FootprintSizeCells.X <= 0 || Spec.FootprintSizeCells.Y <= 0 ||
        Spec.ClearanceCells < 0 || !HasSafeFacilitySpecCardinality(Spec))
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

int32 UFFZSGridLibrary::NormalizeYawCentidegrees(const int32 YawCentidegrees)
{
    constexpr int32 FullTurnCentidegrees = 36000;
    return ((YawCentidegrees % FullTurnCentidegrees) + FullTurnCentidegrees) %
        FullTurnCentidegrees;
}

int32 UFFZSGridLibrary::DegreesToYawCentidegrees(const float YawDegrees)
{
    if (!FMath::IsFinite(YawDegrees))
    {
        return 0;
    }
    const double ReducedDegrees = FMath::Fmod(static_cast<double>(YawDegrees), 360.0);
    return NormalizeYawCentidegrees(FMath::RoundToInt(ReducedDegrees * 100.0));
}

float UFFZSGridLibrary::YawCentidegreesToDegrees(const int32 YawCentidegrees)
{
    return static_cast<float>(NormalizeYawCentidegrees(YawCentidegrees)) * 0.01f;
}

int32 UFFZSGridLibrary::GetEffectiveYawCentidegrees(const FFFZSGridPlacement& Placement)
{
    return Placement.GetEffectiveYawCentidegrees();
}

void UFFZSGridLibrary::NormalizePlacement(FFFZSGridPlacement& InOutPlacement)
{
    InOutPlacement.YawCentidegrees = GetEffectiveYawCentidegrees(InOutPlacement);
    InOutPlacement.QuarterTurns = 0;
}

bool UFFZSGridLibrary::TryRotateCell(
    const FIntPoint Cell,
    const int32 QuarterTurns,
    FIntPoint& OutRotatedCell)
{
    int64 X = Cell.X;
    int64 Y = Cell.Y;
    switch (NormalizeQuarterTurns(QuarterTurns))
    {
    case 1: X = -static_cast<int64>(Cell.Y); Y = Cell.X; break;
    case 2: X = -static_cast<int64>(Cell.X); Y = -static_cast<int64>(Cell.Y); break;
    case 3: X = Cell.Y; Y = -static_cast<int64>(Cell.X); break;
    default: break;
    }
    return TryIntPoint(X, Y, OutRotatedCell);
}

FIntPoint UFFZSGridLibrary::RotateCell(const FIntPoint Cell, const int32 QuarterTurns)
{
    FIntPoint Rotated = FIntPoint::ZeroValue;
    TryRotateCell(Cell, QuarterTurns, Rotated);
    return Rotated;
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
    // [최적화] 세로 병합 대상을 "직전 밴드가 만든 Rect"로 한정한다.
    // Y 밴드는 오름차순이고 각 밴드 Rect의 Max.Y는 그 밴드의 Y1이므로, Max.Y == Y0을 만족하는
    // 후보는 언제나 바로 앞 밴드에만 있다. 예전에는 밴드마다 Result 전체를 선형 탐색해
    // 조각이 많은 입력에서 출력 Rect 수의 제곱에 비례하는 비용이 났다.
    // (X0, X1) 구간을 키로 한 밴드 단위 색인이므로 결과 Rect와 최종 정렬 순서는 이전과 같다.
    TMap<FIntPoint, int32> PreviousBandRectIndices;
    TMap<FIntPoint, int32> CurrentBandRectIndices;
    TArray<FIntPoint> Intervals;
    for (int32 EdgeIndex = 1; EdgeIndex < YEdges.Num(); ++EdgeIndex)
    {
        const int32 Y0 = YEdges[EdgeIndex - 1];
        const int32 Y1 = YEdges[EdgeIndex];
        Intervals.Reset();
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

        CurrentBandRectIndices.Reset();
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

            const FIntPoint SpanKey(X0, X1);
            int32 RectIndex = INDEX_NONE;
            if (const int32* PreviousRectIndex = PreviousBandRectIndices.Find(SpanKey);
                PreviousRectIndex && Result[*PreviousRectIndex].Max.Y == Y0)
            {
                Result[*PreviousRectIndex].Max.Y = Y1;
                RectIndex = *PreviousRectIndex;
            }
            else
            {
                RectIndex = Result.Emplace(FIntPoint(X0, Y0), FIntPoint(X1, Y1));
            }
            CurrentBandRectIndices.Add(SpanKey, RectIndex);
        }
        PreviousBandRectIndices = MoveTemp(CurrentBandRectIndices);
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

// footprint 하나만 필요한 호출자(배치 Transform 계산 등)를 위해 최소 경로를 유지한다.
TArray<FFFZSCellRect> UFFZSGridLibrary::GetFootprintRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }
    const TArray<FFFZSCellRect> LocalFootprint = {
        FFFZSCellRect(FIntPoint::ZeroValue, Spec.FootprintSizeCells)
    };
    TArray<FFFZSOrientedRect> OrientedFootprint;
    return ConvertLocalRectsToOriented(LocalFootprint, Placement, OrientedFootprint)
        ? MakeConservativeRects(OrientedFootprint)
        : TArray<FFFZSCellRect>{};
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetQueueRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    if (!CanEnumerateFacilityCells(Spec))
    {
        return {};
    }
    TArray<FFFZSCellRect> LocalQueue;
    TArray<FFFZSOrientedRect> OrientedQueue;
    return TryBuildLocalQueueRects(Spec, LocalQueue) &&
        ConvertLocalRectsToOriented(LocalQueue, Placement, OrientedQueue)
        ? MakeConservativeRects(OrientedQueue)
        : TArray<FFFZSCellRect>{};
}

// [최적화] Clearance/Reserved는 서로의 중간 결과를 공유하므로 공통 빌더 하나로 계산한다.
// 예전에는 GetClearanceRects가 GetFootprintRects/GetQueueRects를 다시 호출하고,
// GetReservedRects가 그 셋을 또 호출해 같은 예산 검사와 Queue 정규화를 여러 번 반복했다.
TArray<FFFZSCellRect> UFFZSGridLibrary::GetClearanceRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FFFZSCellRect> Footprint;
    TArray<FFFZSCellRect> Clearance;
    TArray<FFFZSCellRect> Queue;
    TArray<FFFZSCellRect> Reserved;
    BuildPlacementRegions(Spec, Placement, Footprint, Clearance, Queue, Reserved);
    return Clearance;
}

TArray<FFFZSCellRect> UFFZSGridLibrary::GetReservedRects(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement)
{
    TArray<FFFZSCellRect> Footprint;
    TArray<FFFZSCellRect> Clearance;
    TArray<FFFZSCellRect> Queue;
    TArray<FFFZSCellRect> Reserved;
    BuildPlacementRegions(Spec, Placement, Footprint, Clearance, Queue, Reserved);
    return Reserved;
}

void UFFZSGridLibrary::BuildPlacementOrientedRegions(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement,
    TArray<FFFZSOrientedRect>& OutFootprintRects,
    TArray<FFFZSOrientedRect>& OutClearanceRects,
    TArray<FFFZSOrientedRect>& OutQueueRects,
    TArray<FFFZSOrientedRect>& OutReservedRects)
{
    OutFootprintRects.Reset();
    OutClearanceRects.Reset();
    OutQueueRects.Reset();
    OutReservedRects.Reset();

    const FFacilityLocalRegions Local = BuildFacilityLocalRegions(Spec);
    if (Local.Footprint.IsEmpty() || Local.Reserved.IsEmpty() ||
        !ConvertLocalRectsToOriented(Local.Footprint, Placement, OutFootprintRects) ||
        !ConvertLocalRectsToOriented(Local.Clearance, Placement, OutClearanceRects) ||
        !ConvertLocalRectsToOriented(Local.Queue, Placement, OutQueueRects) ||
        !ConvertLocalRectsToOriented(Local.Reserved, Placement, OutReservedRects))
    {
        OutFootprintRects.Reset();
        OutClearanceRects.Reset();
        OutQueueRects.Reset();
        OutReservedRects.Reset();
    }
}

bool UFFZSGridLibrary::DoOrientedRectSetsOverlap(
    const TArray<FFFZSOrientedRect>& A,
    const TArray<FFFZSOrientedRect>& B)
{
    for (const FFFZSOrientedRect& RectA : A)
    {
        const FBox2D BoundsA = FFZSOrientedGeometry::GetAabb(RectA);
        if (!BoundsA.bIsValid)
        {
            continue;
        }
        for (const FFFZSOrientedRect& RectB : B)
        {
            const FBox2D BoundsB = FFZSOrientedGeometry::GetAabb(RectB);
            if (!BoundsB.bIsValid ||
                BoundsA.Max.X <= BoundsB.Min.X || BoundsA.Min.X >= BoundsB.Max.X ||
                BoundsA.Max.Y <= BoundsB.Min.Y || BoundsA.Min.Y >= BoundsB.Max.Y)
            {
                continue;
            }
            if (FFZSOrientedGeometry::IntersectsPositiveArea(RectA, RectB))
            {
                return true;
            }
        }
    }
    return false;
}

void UFFZSGridLibrary::BuildPlacementRegions(
    const FFFZSFacilitySpec& Spec,
    const FFFZSGridPlacement& Placement,
    TArray<FFFZSCellRect>& OutFootprintRects,
    TArray<FFFZSCellRect>& OutClearanceRects,
    TArray<FFFZSCellRect>& OutQueueRects,
    TArray<FFFZSCellRect>& OutReservedRects)
{
    TArray<FFFZSOrientedRect> Footprint;
    TArray<FFFZSOrientedRect> Clearance;
    TArray<FFFZSOrientedRect> Queue;
    TArray<FFFZSOrientedRect> Reserved;
    BuildPlacementOrientedRegions(Spec, Placement, Footprint, Clearance, Queue, Reserved);
    OutFootprintRects = MakeConservativeRects(Footprint);
    OutClearanceRects = MakeConservativeRects(Clearance);
    OutQueueRects = MakeConservativeRects(Queue);
    OutReservedRects = MakeConservativeRects(Reserved);
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
    // [최적화] 이미 정규화된 집합이므로 GetCellRectArea가 내부에서 다시 정규화하지 않도록 한다.
    const int64 Area = SumDisjointRectArea(Canonical);
    if (Area < 0 || Area > MaxCells || Area > MAX_int32)
    {
        return false;
    }

    OutCells.Reserve(static_cast<int32>(Area));
    for (const FFFZSCellRect& Rect : Canonical)
    {
        for (int64 X = Rect.Min.X; X < static_cast<int64>(Rect.Max.X); ++X)
        {
            for (int64 Y = Rect.Min.Y; Y < static_cast<int64>(Rect.Max.Y); ++Y)
            {
                OutCells.Emplace(static_cast<int32>(X), static_cast<int32>(Y));
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


// [하드코딩 제거] 이 목록은 이제 "새 프로젝트 시드"일 뿐 런타임 판정의 권위 목록이 아니다.
// 권위 목록은 UFFZSDeveloperSettings::ProjectZoneIds(Config=Game)이며, 설정 CDO 생성자만
// 여기서 초기값을 복사한다. 설정을 로드하는 도중 다시 설정을 조회하는 재귀를 피하기 위해
// 시드 조회와 실제 조회(GetAcceptedGameplayZoneIds)를 별도 함수로 나눈다.
const TArray<FName>& UFFZSGridLibrary::GetDefaultProjectZoneIds()
{
    static const TArray<FName> DefaultProjectZoneIds = {
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
    return DefaultProjectZoneIds;
}

TArray<FName> UFFZSGridLibrary::GetAcceptedGameplayZoneIds()
{
    // 담당자가 Project Settings에 추가한 ID(예: 테스트 Zone)도 그대로 허용된다.
    // 예전처럼 코드 목록만 보면, 설정에는 있으나 코드에는 없는 ID가 Preset 적용에서만
    // 거부되어 ValidateZoneData 결과와 어긋나는 문제가 생긴다.
    const UFFZSDeveloperSettings* Settings = GetDefault<UFFZSDeveloperSettings>();
    return (Settings && !Settings->ProjectZoneIds.IsEmpty())
        ? Settings->ProjectZoneIds
        : GetDefaultProjectZoneIds();
}

FFFZSCommand UFFZSGridLibrary::MakeInstallCommand(const FFFZSFacilitySpec& Spec, const FName ZoneId, const FFFZSGridPlacement Placement, const int64 ExpectedWorldRevision)
{
    FFFZSCommand Command;
    Command.CommandId = FGuid::NewGuid();
    Command.TransactionId = FGuid::NewGuid();
    Command.ExpectedWorldRevision = ExpectedWorldRevision;
    Command.Type = EFFZSCommandType::Install;
    Command.FacilityInstanceId = FGuid::NewGuid();
    // Legacy Blueprint assets may still pass a copied Spec. In production the ID is
    // only a lookup key: rebuild the command from the registered PrimaryDataAsset so
    // this compatibility surface cannot become a second authored source.
    const FFFZSFacilitySpec* EffectiveSpec = &Spec;
#if WITH_DEV_AUTOMATION_TESTS
    const bool bUseSyntheticAutomationSpec = GIsAutomationTesting;
#else
    constexpr bool bUseSyntheticAutomationSpec = false;
#endif
    if (!bUseSyntheticAutomationSpec && !Spec.FacilityDefinitionId.IsNone())
    {
        if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
        {
            const FPrimaryAssetId AssetId(
                UFFZSFacilityDefinition::GetFacilityDefinitionAssetType(),
                Spec.FacilityDefinitionId);
            UFFZSFacilityDefinition* Definition =
                AssetManager->GetPrimaryAssetObject<UFFZSFacilityDefinition>(AssetId);
            if (!Definition)
            {
                Definition = Cast<UFFZSFacilityDefinition>(
                    AssetManager->GetPrimaryAssetPath(AssetId).TryLoad());
            }
            if (Definition)
            {
                EffectiveSpec = &Definition->Spec;
            }
        }
    }
    Command.FacilitySpec = *EffectiveSpec;
    Command.ZoneId = ZoneId;
    Command.Placement = Placement;
    NormalizePlacement(Command.Placement);
    return Command;
}

FFFZSCommand UFFZSGridLibrary::MakeInstallCommandFromDefinition(
    const UFFZSFacilityDefinition* Definition,
    const FName ZoneId,
    const FFFZSGridPlacement Placement,
    const int64 ExpectedWorldRevision)
{
    return Definition
        ? MakeInstallCommand(Definition->Spec, ZoneId, Placement, ExpectedWorldRevision)
        : FFFZSCommand{};
}

// 내구성 있는 Payload 동일성 문자열이다. CommandId는 영수증 조회 키이므로 의도적으로 제외하고,
// 그 ID로 재시도했을 때 같아야 하는 Transaction/Revision/Placement/Spec 전체를 포함한다.
// v5 문자열은 길이 접두사를 붙여 구분자 포함 값의 모호성을 없애고, float은 17자리 유효숫자,
// FText는 문화권 비의존 직렬화로 기록한다. 배열은 저작 순서까지 의미에 포함하므로 순서 변경도
// 다른 Payload다. format 버전 변경은 기존 영수증 호환 정책과 함께 다뤄야 한다.
 
FString UFFZSGridLibrary::BuildCommandFingerprint(const FFFZSCommand& Command)
{
    if (!HasSafeFacilitySpecCardinality(Command.FacilitySpec))
    {
        return TEXT("ZFCommand.v5.InvalidUnsafeFacilitySpecCardinality");
    }
    FString Canonical;
    Canonical.Reserve(2048);

    // [최적화] 필드마다 임시 FString을 만들지 않고 결과 버퍼에 직접 이어 붙인다.
    // 출력 바이트는 이전(`"%s=%d:" + Value + ";"`)과 완전히 동일하다. 이 문자열은 저장된
    // 멱등성 영수증의 키이므로 형식이 1바이트라도 달라지면 안 된다.
    const auto AppendString = [&Canonical](const TCHAR* Label, const FString& Value)
    {
        Canonical.Appendf(TEXT("%s=%d:%s;"), Label, Value.Len(), *Value);
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

    AppendString(TEXT("format"), TEXT("FFZSCommand.v5"));
    AppendString(TEXT("transaction_id"), Command.TransactionId.ToString(EGuidFormats::Digits));
    AppendInt(TEXT("expected_world_revision"), Command.ExpectedWorldRevision);
    AppendInt(TEXT("command_type"), static_cast<int64>(Command.Type));
    AppendString(TEXT("facility_instance_id"), Command.FacilityInstanceId.ToString(EGuidFormats::Digits));
    AppendString(TEXT("zone_id"), Command.ZoneId.ToString());
    AppendInt(TEXT("placement.origin_x"), Command.Placement.OriginCell.X);
    AppendInt(TEXT("placement.origin_y"), Command.Placement.OriginCell.Y);
    AppendInt(TEXT("placement.yaw_centidegrees"), GetEffectiveYawCentidegrees(Command.Placement));

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

    // v5는 압축 사각형 원본과 구 호환 단일 셀을 모두 기록한다. 잘못된 사양도 서로 다른
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
    // 기존 Schema 2 명령/영수증은 세 필드가 없었으며 그 의미는 SourceMeshSize + identity였다.
    // 기본값일 때 생략하면 기존 저장의 멱등성 fingerprint와 호환되고, 새 동작을 선택한 경우에만
    // 명시적으로 확장해 서로 다른 시각 사양을 같은 명령으로 취급하지 않는다.
    const bool bHasExactVisualOffset =
        Spec.VisualOffsetCm.X != 0.0 ||
        Spec.VisualOffsetCm.Y != 0.0 ||
        Spec.VisualOffsetCm.Z != 0.0;
    const bool bHasExactVisualRotation =
        Spec.VisualRotation.Pitch != 0.0f ||
        Spec.VisualRotation.Yaw != 0.0f ||
        Spec.VisualRotation.Roll != 0.0f;
    if (Spec.VisualSizingMode != EFFZSVisualSizingMode::SourceMeshSize ||
        bHasExactVisualOffset ||
        bHasExactVisualRotation)
    {
        AppendInt(TEXT("spec.visual_sizing_mode"), static_cast<int64>(Spec.VisualSizingMode));
        AppendVector(TEXT("spec.visual_offset_cm"), Spec.VisualOffsetCm);
        AppendFloat(TEXT("spec.visual_rotation.pitch"), Spec.VisualRotation.Pitch);
        AppendFloat(TEXT("spec.visual_rotation.yaw"), Spec.VisualRotation.Yaw);
        AppendFloat(TEXT("spec.visual_rotation.roll"), Spec.VisualRotation.Roll);
    }
    // 기본 무음 설정은 기존 v5 영수증과 동일한 지문을 유지한다. 시설 오디오를 저작한 경우에는
    // 표현 설정 전체를 포함해 Data Asset과 설치 명령의 정확 일치 검사가 사운드 교체도 감지한다.
    if (Spec.AudioSettings.HasNonDefaultValues())
    {
        AppendString(TEXT("spec.audio.ambient_sound"),
            Spec.AudioSettings.AmbientSound.ToSoftObjectPath().ToString());
        AppendString(TEXT("spec.audio.attenuation"),
            Spec.AudioSettings.Attenuation.ToSoftObjectPath().ToString());
        AppendInt(TEXT("spec.audio.override_audible_range"),
            Spec.AudioSettings.bOverrideAudibleRange ? 1 : 0);
        AppendFloat(TEXT("spec.audio.full_volume_radius_cm"),
            Spec.AudioSettings.FullVolumeRadiusCm);
        AppendFloat(TEXT("spec.audio.max_audible_distance_cm"),
            Spec.AudioSettings.MaxAudibleDistanceCm);
        AppendString(TEXT("spec.audio.concurrency"),
            Spec.AudioSettings.Concurrency.ToSoftObjectPath().ToString());
        AppendFloat(TEXT("spec.audio.volume_multiplier"),
            Spec.AudioSettings.VolumeMultiplier);
        AppendFloat(TEXT("spec.audio.pitch_multiplier"),
            Spec.AudioSettings.PitchMultiplier);
        AppendVector(TEXT("spec.audio.local_offset_cm"),
            Spec.AudioSettings.LocalOffsetCm);
        AppendFloat(TEXT("spec.audio.fade_in_seconds"),
            Spec.AudioSettings.FadeInDurationSeconds);
        AppendFloat(TEXT("spec.audio.fade_out_seconds"),
            Spec.AudioSettings.FadeOutDurationSeconds);
        AppendInt(TEXT("spec.audio.play_only_while_active"),
            Spec.AudioSettings.bPlayOnlyWhileActive ? 1 : 0);
        AppendInt(TEXT("spec.audio.loop"), Spec.AudioSettings.bLoop ? 1 : 0);
    }
    AppendFloat(TEXT("spec.placement_obstacle_height_cm"), Spec.PlacementObstacleHeightCm);
    // 새 필드의 기본값 0(무제한)은 기존 v5 영수증 지문과 완전히 같은 문자열을 유지한다.
    // 검증 전 음수 입력도 서로 다른 의미 Payload이므로 0이 아닐 때는 지문에 기록한다.
    if (Spec.MaxInstancesPerWorld != 0)
    {
        AppendInt(TEXT("spec.max_instances_per_world"), Spec.MaxInstancesPerWorld);
    }
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
    case EFFZSPlacementFailure::None: return LOCTEXT("None", "실패 없음");
    case EFFZSPlacementFailure::ZoneNotFound: return LOCTEXT("ZoneNotFound", "Zone을 찾을 수 없습니다");
    case EFFZSPlacementFailure::FacilityNotFound: return LOCTEXT("FacilityNotFound", "Facility를 찾을 수 없습니다");
    case EFFZSPlacementFailure::DuplicateZoneId: return LOCTEXT("DuplicateZoneId", "Zone ID가 중복되었습니다");
    case EFFZSPlacementFailure::DuplicateFacilityId: return LOCTEXT("DuplicateFacilityId", "Facility ID가 중복되었습니다");
    case EFFZSPlacementFailure::CategoryNotAllowed: return LOCTEXT("CategoryNotAllowed", "이 Zone에서는 해당 Facility 카테고리가 허용되지 않습니다");
    case EFFZSPlacementFailure::OutsideZone: return LOCTEXT("OutsideZone", "Footprint, 여유 공간 또는 최대 Queue 영역이 Zone 밖에 있습니다");
    case EFFZSPlacementFailure::InvalidGround: return LOCTEXT("InvalidGround", "필요한 지면을 찾을 수 없습니다");
    case EFFZSPlacementFailure::ExcessSlope: return LOCTEXT("ExcessSlope", "지면 경사가 설정된 제한을 초과했습니다");
    case EFFZSPlacementFailure::HeightVarianceExceeded: return LOCTEXT("HeightVariance", "지면 높이 편차가 설정된 제한을 초과했습니다");
    case EFFZSPlacementFailure::PlacementCollision: return LOCTEXT("PlacementCollision", "Facility Footprint가 점유된 Cell 또는 World 장애물과 충돌합니다");
    case EFFZSPlacementFailure::InsufficientClearance: return LOCTEXT("InsufficientClearance", "필요한 여유 공간이 막혀 있습니다");
    case EFFZSPlacementFailure::QueueFootprintBlocked: return LOCTEXT("QueueBlocked", "최대 Queue Footprint가 막혀 있습니다");
    case EFFZSPlacementFailure::ProtectedRouteBlocked: return LOCTEXT("ProtectedRoute", "배치 영역이 보호 경로 또는 보호 영역과 겹칩니다");
    case EFFZSPlacementFailure::RequiredRouteDisconnected: return LOCTEXT("RouteDisconnected", "필수 내비게이션 지점의 경로가 끊겨 있습니다");
    case EFFZSPlacementFailure::AccessibilityRouteBlocked: return LOCTEXT("AccessibilityBlocked", "배치 영역이 접근성 경로를 막습니다");
    case EFFZSPlacementFailure::WrongStage: return LOCTEXT("WrongStage", "현재 Phase에서는 World 변경이 허용되지 않습니다");
    case EFFZSPlacementFailure::FacilityOccupied: return LOCTEXT("Occupied", "Facility를 이동하거나 제거하기 전에 닫고 Queue를 비우십시오");
    case EFFZSPlacementFailure::InvalidFacilityState: return LOCTEXT("InvalidState", "현재 Facility 상태에서는 이 명령을 실행할 수 없습니다");
    case EFFZSPlacementFailure::StaleWorldRevision: return LOCTEXT("StaleRevision", "명령을 준비한 뒤 World가 변경되었습니다");
    case EFFZSPlacementFailure::DuplicatePayloadMismatch: return LOCTEXT("DuplicateMismatch", "동일한 Command ID가 다른 Payload에 재사용되었습니다");
    case EFFZSPlacementFailure::NavigationUnavailable: return LOCTEXT("NavigationUnavailable", "내비게이션 데이터를 사용할 수 없습니다");
    case EFFZSPlacementFailure::NavRebuildTimeout: return LOCTEXT("NavTimeout", "내비게이션 재빌드 시간이 초과되었습니다");
    case EFFZSPlacementFailure::DataInvalid: return LOCTEXT("DataInvalid", "Facility 또는 Zone 데이터가 잘못되었습니다");
    case EFFZSPlacementFailure::ExternalPolicyRejected: return LOCTEXT("PolicyRejected", "외부 팀 Policy가 변경을 거부했습니다");
    case EFFZSPlacementFailure::InsufficientFunds: return LOCTEXT("InsufficientFunds", "현재 잔액이 부족합니다");
    case EFFZSPlacementFailure::BuildLimitReached: return LOCTEXT("BuildLimitReached", "이 시설은 건축 가능 수량 한도에 도달했습니다");
    default: return LOCTEXT("InternalError", "Zone/Facility 내부 오류가 발생했습니다");
    }
}

#undef LOCTEXT_NAMESPACE
