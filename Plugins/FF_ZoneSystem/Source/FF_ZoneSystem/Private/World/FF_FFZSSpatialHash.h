// FF_FFZSSpatialHash.h
// 역할: 월드 좌표를 검색 묶음으로 나누어 충돌 가능 시설과 지역만 빠르게 찾는다. 최종 충돌 판정은 호출자가 담당한다.
#pragma once

#include "CoreMinimal.h"

// 월드 XY 좌표(cm)를 검색 묶음으로 나눈 후보 목록이다. 실제 충돌 판정과 시설 소유권은 호출자가 관리한다.
// 묶음의 경계는 검색 속도를 위한 구분일 뿐 시설을 설치할 수 있는 경계가 아니다.
class FF_ZONESYSTEM_API FFFZSSpatialHash2D
{
public:
    // 검색 묶음 크기는 성능용 값이다. 실제 점유/저장 격자는 항상 별도의 1cm 계약을 따른다.
    static constexpr double DefaultBucketSizeCm = 2000.0;
    static constexpr double RegionBucketSizeCm = 10000.0;
    explicit FFFZSSpatialHash2D(double InBucketSizeCm = DefaultBucketSizeCm);

    // 기존 위치를 새 위치로 교체한다. 잘못된 경계는 false를 반환하되 후보에서 누락되지 않게 보관한다.
    // 너무 큰 정상 경계는 별도 목록에 저장한다. 좌표가 정확히 같으면 기존 묶음을 재사용한다.
    bool InsertOrUpdate(const FGuid& Id, const FBox2D& WorldBounds);
    void Remove(const FGuid& Id);
    void Reset();

    // 경계에 닿는 후보까지 모으고 중복을 없앤 뒤 GUID 순서로 반환한다.
    // 잘못된 검색 영역이면 모든 ID를 반환하며, 너무 큰 검색 영역은 저장된 경계만 검사한다.
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
