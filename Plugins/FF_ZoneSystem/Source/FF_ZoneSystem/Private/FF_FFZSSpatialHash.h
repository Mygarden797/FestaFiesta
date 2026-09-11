#pragma once

#include "CoreMinimal.h"

// Sparse broad-phase index in world XY centimeters. Gameplay ownership and exact geometry
// remain outside this index; bucket boundaries never become placement boundaries.
class FF_ZONESYSTEM_API FFFZSSpatialHash2D
{
public:
    explicit FFFZSSpatialHash2D(double InBucketSizeCm = 2000.0);

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
