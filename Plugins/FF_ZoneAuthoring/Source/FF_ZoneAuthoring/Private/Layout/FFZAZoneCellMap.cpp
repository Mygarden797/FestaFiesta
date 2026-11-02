// FFZAZoneCellMap.cpp
// 역할: Zone 다각형을 주사선(scanline) 방식으로 칸에 칠하고 겹침/이웃/소유 칸 수를 집계한다.
#include "FFZAZoneCellMap.h"

#include "FFZAPolygonMath.h"

void FFFZAZoneCellMap::Reset()
{
    NumX = 0;
    NumY = 0;
    FirstOwner.Reset();
    OverlapFlags.Reset();
    OwnedCellCounts.Reset();
    AdjacentPairs.Reset();
    Overlaps.Reset();
}

bool FFFZAZoneCellMap::Build(const TArray<TArray<FVector2D>>& WorldPolygons, const double RequestedCellSizeCm,
    const int32 MaxCellCount)
{
    Reset();
    const int32 ZoneCount = WorldPolygons.Num();
    if (ZoneCount == 0 || ZoneCount > MaxZoneCount || MaxCellCount <= 0)
    {
        return false;
    }

    // 1) 모든 Zone을 덮는 범위를 구한다.
    FBox2D Bounds(ForceInit);
    for (const TArray<FVector2D>& Polygon : WorldPolygons)
    {
        if (Polygon.Num() >= 3)
        {
            Bounds += FFZAPolygonMath::GetBounds(Polygon);
        }
    }
    if (!Bounds.bIsValid)
    {
        return false;
    }

    // 2) 셀 크기를 정한다. 셀 수 상한을 넘으면 면적에 맞춰 키운다.
    double CellSize = FMath::Max(1.0, RequestedCellSizeCm);
    const double Width = FMath::Max(1.0, Bounds.Max.X - Bounds.Min.X);
    const double Height = FMath::Max(1.0, Bounds.Max.Y - Bounds.Min.Y);
    const double RequiredCells = (Width / CellSize + 2.0) * (Height / CellSize + 2.0);
    if (RequiredCells > static_cast<double>(MaxCellCount))
    {
        CellSize = FMath::Sqrt((Width + 2.0 * CellSize) * (Height + 2.0 * CellSize) / static_cast<double>(MaxCellCount)) * 1.05;
    }
    CellSizeCm = CellSize;

    // 가장자리 판정이 잘리지 않도록 한 칸 여유를 둔다.
    Origin = FVector2D(Bounds.Min.X - CellSize, Bounds.Min.Y - CellSize);
    NumX = FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(Width / CellSize)) + 2);
    NumY = FMath::Max(1, static_cast<int32>(FMath::CeilToDouble(Height / CellSize)) + 2);
    const int32 CellCount = NumX * NumY;

    FirstOwner.SetNumZeroed(CellCount);
    OverlapFlags.SetNumZeroed(CellCount);
    OwnedCellCounts.SetNumZeroed(ZoneCount);

    // 3) Zone마다 행(Row) 중심선으로 다각형을 잘라 칸 중심이 안에 있는 칸을 칠한다.
    TArray<double> Crossings;
    for (int32 ZoneIndex = 0; ZoneIndex < ZoneCount; ++ZoneIndex)
    {
        const TArray<FVector2D>& Polygon = WorldPolygons[ZoneIndex];
        if (Polygon.Num() < 3)
        {
            continue;
        }
        const FBox2D ZoneBounds = FFZAPolygonMath::GetBounds(Polygon);
        const int32 RowStart = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((ZoneBounds.Min.Y - Origin.Y) / CellSize)), 0, NumY - 1);
        const int32 RowEnd = FMath::Clamp(static_cast<int32>(FMath::CeilToDouble((ZoneBounds.Max.Y - Origin.Y) / CellSize)), 0, NumY - 1);
        const uint16 OwnerValue = static_cast<uint16>(ZoneIndex + 1);

        for (int32 Row = RowStart; Row <= RowEnd; ++Row)
        {
            const double CenterY = Origin.Y + (static_cast<double>(Row) + 0.5) * CellSize;
            FFZAPolygonMath::ScanlineCrossings(Polygon, CenterY, Crossings);
            for (int32 Pair = 0; Pair + 1 < Crossings.Num(); Pair += 2)
            {
                // 칸 중심 X가 [Crossings[Pair], Crossings[Pair+1]] 안에 있는 칸만 칠한다.
                const int32 ColStart = FMath::Max(0, static_cast<int32>(FMath::CeilToDouble((Crossings[Pair] - Origin.X) / CellSize - 0.5)));
                const int32 ColEnd = FMath::Min(NumX - 1, static_cast<int32>(FMath::FloorToDouble((Crossings[Pair + 1] - Origin.X) / CellSize - 0.5)));
                for (int32 Col = ColStart; Col <= ColEnd; ++Col)
                {
                    const int32 Cell = Row * NumX + Col;
                    const uint16 Existing = FirstOwner[Cell];
                    if (Existing == 0)
                    {
                        FirstOwner[Cell] = OwnerValue;
                        continue;
                    }
                    if (Existing == OwnerValue)
                    {
                        continue;
                    }
                    OverlapFlags[Cell] = 1;
                    const int32 A = FMath::Min<int32>(Existing - 1, ZoneIndex);
                    const int32 B = FMath::Max<int32>(Existing - 1, ZoneIndex);
                    FFFZAZoneCellOverlap* Found = Overlaps.FindByPredicate(
                        [A, B](const FFFZAZoneCellOverlap& Item) { return Item.ZoneIndexA == A && Item.ZoneIndexB == B; });
                    if (Found)
                    {
                        ++Found->CellCount;
                    }
                    else
                    {
                        FFFZAZoneCellOverlap& NewOverlap = Overlaps.AddDefaulted_GetRef();
                        NewOverlap.ZoneIndexA = A;
                        NewOverlap.ZoneIndexB = B;
                        NewOverlap.CellCount = 1;
                    }
                }
            }
        }
    }

    // 4) 소유 칸 수와 이웃 쌍을 집계한다. 겹친 칸은 판단에서 뺀다.
    for (int32 Row = 0; Row < NumY; ++Row)
    {
        for (int32 Col = 0; Col < NumX; ++Col)
        {
            const int32 Cell = Row * NumX + Col;
            if (FirstOwner[Cell] == 0 || OverlapFlags[Cell] != 0)
            {
                continue;
            }
            const int32 Owner = FirstOwner[Cell] - 1;
            ++OwnedCellCounts[Owner];

            const int32 NeighborCells[2] = {
                Col + 1 < NumX ? Cell + 1 : INDEX_NONE,
                Row + 1 < NumY ? Cell + NumX : INDEX_NONE
            };
            for (const int32 NeighborCell : NeighborCells)
            {
                if (NeighborCell == INDEX_NONE || FirstOwner[NeighborCell] == 0 || OverlapFlags[NeighborCell] != 0)
                {
                    continue;
                }
                const int32 Other = FirstOwner[NeighborCell] - 1;
                if (Other != Owner)
                {
                    AdjacentPairs.AddUnique(FIntPoint(FMath::Min(Owner, Other), FMath::Max(Owner, Other)));
                }
            }
        }
    }
    // 서로 겹친 Zone도 공간적으로 맞닿아 있으므로 이웃으로 본다(겹침 자체는 별도 오류로 보고된다).
    for (const FFFZAZoneCellOverlap& Overlap : Overlaps)
    {
        AdjacentPairs.AddUnique(FIntPoint(Overlap.ZoneIndexA, Overlap.ZoneIndexB));
    }
    AdjacentPairs.Sort([](const FIntPoint& L, const FIntPoint& R)
    {
        return L.X != R.X ? L.X < R.X : L.Y < R.Y;
    });
    return true;
}

bool FFFZAZoneCellMap::WorldToCell(const FVector2D& WorldXY, int32& OutX, int32& OutY) const
{
    if (!IsBuilt())
    {
        return false;
    }
    OutX = static_cast<int32>(FMath::FloorToDouble((WorldXY.X - Origin.X) / CellSizeCm));
    OutY = static_cast<int32>(FMath::FloorToDouble((WorldXY.Y - Origin.Y) / CellSizeCm));
    return OutX >= 0 && OutY >= 0 && OutX < NumX && OutY < NumY;
}

int32 FFFZAZoneCellMap::GetCellOwner(const int32 X, const int32 Y) const
{
    if (!IsBuilt() || X < 0 || Y < 0 || X >= NumX || Y >= NumY)
    {
        return INDEX_NONE;
    }
    const uint16 Value = FirstOwner[Y * NumX + X];
    return Value == 0 ? INDEX_NONE : static_cast<int32>(Value) - 1;
}

int32 FFFZAZoneCellMap::FindZoneIndexAt(const FVector2D& WorldXY) const
{
    int32 X = 0;
    int32 Y = 0;
    return WorldToCell(WorldXY, X, Y) ? GetCellOwner(X, Y) : INDEX_NONE;
}

bool FFFZAZoneCellMap::IsOverlapAt(const FVector2D& WorldXY) const
{
    int32 X = 0;
    int32 Y = 0;
    return WorldToCell(WorldXY, X, Y) && OverlapFlags[Y * NumX + X] != 0;
}

int32 FFFZAZoneCellMap::GetOwnedCellCount(const int32 ZoneIndex) const
{
    return OwnedCellCounts.IsValidIndex(ZoneIndex) ? OwnedCellCounts[ZoneIndex] : 0;
}
