// FF_FFZSSpatialHash.h
// 역할: 월드 좌표를 검색 묶음으로 나누어 충돌 가능 시설과 지역만 빠르게 찾는다. 최종 충돌 판정은 호출자가 담당한다.
#pragma once

#include "CoreMinimal.h"

// Sparse broad-phase index in world XY centimeters. Gameplay ownership and exact geometry
// remain outside this index; bucket boundaries never become placement boundaries.
class FF_ZONESYSTEM_API FFFZSSpatialHash2D
{
public:
    // 검색 묶음 크기는 성능용 값이다. 실제 점유/저장 격자는 항상 별도의 1cm 계약을 따른다.
    static constexpr double DefaultBucketSizeCm = 2000.0;
    static constexpr double RegionBucketSizeCm = 10000.0;
    explicit FFFZSSpatialHash2D(double InBucketSizeCm = DefaultBucketSizeCm);

    // Replaces an existing record atomically from the caller's perspective. Invalid bounds
    // return false but retain the ID as a conservative candidate for every query. Oversized
    // valid bounds use a bounded fallback list and return true.
    bool InsertOrUpdate(const FGuid& Id, const FBox2D& WorldBounds);
    void Remove(const FGuid& Id);
    void Reset();

    // Replaces the output with unique candidates in numeric GUID order. Touching bounds
    // are included. Invalid queries conservatively return every ID; oversized queries scan
    // stored bounds rather than enumerating an unbounded number of empty buckets.
    void Query(const FBox2D& WorldBounds, TArray<FGuid>& OutCandidates) const;

    int32 Num() const { return Entries.Num(); }
    double GetBucketSizeCm() const { return BucketSizeCm; }

private:
    struct FEntry
    {
        FBox2D Bounds = FBox2D(EForceInit::ForceInit);
        TArray<FIntPoint> BucketKeys;
        bool bValidBounds = false;
    };

    static constexpr int32 MaximumBucketsPerEntry = 256;
    static constexpr int32 MaximumBucketsPerQuery = 4096;

    bool TryGetBucketRange(const FBox2D& Bounds, int32 MaximumBucketCount,
        FIntPoint& OutFirst, FIntPoint& OutLast) const;

    double BucketSizeCm;
    TMap<FGuid, FEntry> Entries;
    TMap<FIntPoint, TSet<FGuid>> Buckets;
    TSet<FGuid> FallbackEntries;
};
