#include "FF_FFZSSpatialHash.h"

namespace
{
    bool HasFiniteOrderedBounds(const FBox2D& Bounds)
    {
        return Bounds.bIsValid && FMath::IsFinite(Bounds.Min.X) && FMath::IsFinite(Bounds.Min.Y) &&
            FMath::IsFinite(Bounds.Max.X) && FMath::IsFinite(Bounds.Max.Y) &&
            Bounds.Min.X <= Bounds.Max.X && Bounds.Min.Y <= Bounds.Max.Y;
    }

    bool IntersectsIncludingBoundary(const FBox2D& A, const FBox2D& B)
    {
        return A.Min.X <= B.Max.X && A.Max.X >= B.Min.X &&
            A.Min.Y <= B.Max.Y && A.Max.Y >= B.Min.Y;
    }

    bool GuidLess(const FGuid& A, const FGuid& B)
    {
        if (A.A != B.A) { return A.A < B.A; }
        if (A.B != B.B) { return A.B < B.B; }
        if (A.C != B.C) { return A.C < B.C; }
        return A.D < B.D;
    }
}

FFFZSSpatialHash2D::FFFZSSpatialHash2D(const double InBucketSizeCm)
    : BucketSizeCm(FMath::IsFinite(InBucketSizeCm) && InBucketSizeCm >= 1.0 ? InBucketSizeCm : 2000.0)
{
}

bool FFFZSSpatialHash2D::TryGetBucketRange(const FBox2D& Bounds, const int32 MaximumBucketCount,
    FIntPoint& OutFirst, FIntPoint& OutLast) const
{
    if (!HasFiniteOrderedBounds(Bounds))
    {
        return false;
    }

    const double FirstX = FMath::FloorToDouble(Bounds.Min.X / BucketSizeCm);
    const double FirstY = FMath::FloorToDouble(Bounds.Min.Y / BucketSizeCm);
    const double LastX = FMath::FloorToDouble(Bounds.Max.X / BucketSizeCm);
    const double LastY = FMath::FloorToDouble(Bounds.Max.Y / BucketSizeCm);
    if (FirstX < MIN_int32 || FirstY < MIN_int32 || LastX > MAX_int32 || LastY > MAX_int32)
    {
        return false;
    }

    OutFirst = FIntPoint(static_cast<int32>(FirstX), static_cast<int32>(FirstY));
    OutLast = FIntPoint(static_cast<int32>(LastX), static_cast<int32>(LastY));
    const int64 Width = static_cast<int64>(OutLast.X) - OutFirst.X + 1;
    const int64 Height = static_cast<int64>(OutLast.Y) - OutFirst.Y + 1;
    // Division avoids overflow even for a box spanning the complete int32 bucket range.
    return Width > 0 && Height > 0 && Width <= MaximumBucketCount &&
        Height <= MaximumBucketCount / Width;
}

bool FFFZSSpatialHash2D::InsertOrUpdate(const FGuid& Id, const FBox2D& WorldBounds)
{
    Remove(Id);
    FEntry& Entry = Entries.Add(Id);
    Entry.Bounds = WorldBounds;
    Entry.bValidBounds = HasFiniteOrderedBounds(WorldBounds);

    FIntPoint First;
    FIntPoint Last;
    if (!TryGetBucketRange(WorldBounds, MaximumBucketsPerEntry, First, Last))
    {
        FallbackEntries.Add(Id);
        return Entry.bValidBounds;
    }

    // int64 loop counters also make a one-bucket box at MAX_int32 safe to enumerate.
    for (int64 Y = First.Y; Y <= static_cast<int64>(Last.Y); ++Y)
    {
        for (int64 X = First.X; X <= static_cast<int64>(Last.X); ++X)
        {
            const FIntPoint Key(static_cast<int32>(X), static_cast<int32>(Y));
            Buckets.FindOrAdd(Key).Add(Id);
            Entry.BucketKeys.Add(Key);
        }
    }
    return true;
}

void FFFZSSpatialHash2D::Remove(const FGuid& Id)
{
    const FEntry* Entry = Entries.Find(Id);
    if (!Entry)
    {
        return;
    }
    for (const FIntPoint& Key : Entry->BucketKeys)
    {
        if (TSet<FGuid>* Bucket = Buckets.Find(Key))
        {
            Bucket->Remove(Id);
            if (Bucket->IsEmpty())
            {
                Buckets.Remove(Key);
            }
        }
    }
    FallbackEntries.Remove(Id);
    Entries.Remove(Id);
}

void FFFZSSpatialHash2D::Reset()
{
    Entries.Reset();
    Buckets.Reset();
    FallbackEntries.Reset();
}

void FFFZSSpatialHash2D::Query(const FBox2D& WorldBounds, TArray<FGuid>& OutCandidates) const
{
    OutCandidates.Reset();
    const bool bValidQuery = HasFiniteOrderedBounds(WorldBounds);
    const auto IsCandidate = [&](const FEntry& Entry)
    {
        return !bValidQuery || !Entry.bValidBounds || IntersectsIncludingBoundary(WorldBounds, Entry.Bounds);
    };

    FIntPoint First;
    FIntPoint Last;
    if (!TryGetBucketRange(WorldBounds, MaximumBucketsPerQuery, First, Last))
    {
        // This is bounded by actual records, not map area or coordinate magnitude.
        for (const TPair<FGuid, FEntry>& Pair : Entries)
        {
            if (IsCandidate(Pair.Value))
            {
                OutCandidates.Add(Pair.Key);
            }
        }
    }
    else
    {
        TSet<FGuid> Candidates = FallbackEntries;
        for (int64 Y = First.Y; Y <= static_cast<int64>(Last.Y); ++Y)
        {
            for (int64 X = First.X; X <= static_cast<int64>(Last.X); ++X)
            {
                if (const TSet<FGuid>* Bucket = Buckets.Find(
                    FIntPoint(static_cast<int32>(X), static_cast<int32>(Y))))
                {
                    for (const FGuid& Id : *Bucket)
                    {
                        Candidates.Add(Id);
                    }
                }
            }
        }
        for (const FGuid& Id : Candidates)
        {
            if (const FEntry* Entry = Entries.Find(Id); Entry && IsCandidate(*Entry))
            {
                OutCandidates.Add(Id);
            }
        }
    }
    OutCandidates.Sort(GuidLess);
}
