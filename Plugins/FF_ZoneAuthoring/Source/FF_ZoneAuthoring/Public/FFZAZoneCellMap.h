// FFZAZoneCellMap.h
// 역할: 여러 Zone 다각형을 "칸(셀) 단위 소유 지도"로 바꿔 겹침·이웃·소속 조회를 빠르게 계산한다.
//
// 비유: 바닥에 모눈종이를 깔고 칸마다 "이 칸은 어느 Zone 땅인가"를 적어 둔 표.
//  - 한 칸에 Zone이 두 개 적히면 → 겹침(오류 후보)
//  - 옆 칸끼리 Zone이 다르면 → 두 Zone은 이웃
//  - 좌표 → 칸 번호 → Zone 은 계산 한 번이라 매우 빠르다(O(1)).
//
// 권위(최종 판정) 데이터가 아니다. 실제 설치 판정은 여전히 FF_ZoneSystem의 1 cm 정밀 다각형이 한다.
// 이 지도는 에디터 검사(겹침/이웃 자동 계산)와 UI용 빠른 조회를 위한 "파생 데이터"다.
#pragma once

#include "CoreMinimal.h"

struct FF_ZONEAUTHORING_API FFFZAZoneCellOverlap
{
    int32 ZoneIndexA = INDEX_NONE;
    int32 ZoneIndexB = INDEX_NONE;
    int32 CellCount = 0;
};

class FF_ZONEAUTHORING_API FFFZAZoneCellMap
{
public:
    // 셀 수 상한. 넘으면 셀 크기를 자동으로 키운다(메모리: 셀당 약 3바이트).
    static constexpr int32 DefaultMaxCellCount = 4 * 1024 * 1024;
    static constexpr int32 MaxZoneCount = 65534;

    // WorldPolygons[i]가 i번 Zone의 월드 XY 다각형(cm). 실패하면 false(빈 입력, Zone 수 초과 등).
    bool Build(const TArray<TArray<FVector2D>>& WorldPolygons, double RequestedCellSizeCm,
        int32 MaxCellCount = DefaultMaxCellCount);

    void Reset();
    bool IsBuilt() const { return NumX > 0 && NumY > 0; }

    // 좌표가 속한 Zone 인덱스. 어느 Zone에도 없으면 INDEX_NONE. 겹친 칸이면 먼저 칠한 Zone을 돌려준다.
    int32 FindZoneIndexAt(const FVector2D& WorldXY) const;
    bool IsOverlapAt(const FVector2D& WorldXY) const;

    // 서로 다른 Zone이 옆 칸(상하좌우)으로 붙어 있거나 겹친 쌍. X < Y 로 정렬·중복 제거되어 있다.
    const TArray<FIntPoint>& GetAdjacentZonePairs() const { return AdjacentPairs; }
    const TArray<FFFZAZoneCellOverlap>& GetOverlaps() const { return Overlaps; }

    // i번 Zone이 단독으로 소유한 칸 수(겹친 칸 제외).
    int32 GetOwnedCellCount(int32 ZoneIndex) const;

    double GetCellSizeCm() const { return CellSizeCm; }
    FVector2D GetOrigin() const { return Origin; }
    int32 GetNumX() const { return NumX; }
    int32 GetNumY() const { return NumY; }

    // 칸 (X, Y)의 소유 Zone(겹침 무시, 먼저 칠한 Zone). 범위 밖이면 INDEX_NONE.
    int32 GetCellOwner(int32 X, int32 Y) const;

private:
    bool WorldToCell(const FVector2D& WorldXY, int32& OutX, int32& OutY) const;

    double CellSizeCm = 100.0;
    FVector2D Origin = FVector2D::ZeroVector;
    int32 NumX = 0;
    int32 NumY = 0;
    TArray<uint16> FirstOwner;   // 0 = 없음, 그 외 Zone 인덱스 + 1
    TArray<uint8> OverlapFlags;  // 1 = 두 개 이상의 Zone이 칠한 칸
    TArray<int32> OwnedCellCounts;
    TArray<FIntPoint> AdjacentPairs;
    TArray<FFFZAZoneCellOverlap> Overlaps;
};
