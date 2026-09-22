// FF_FFZSSpatialHash.cpp
// 역할: 월드 좌표를 검색 묶음으로 나누어 충돌 가능 시설과 지역만 빠르게 찾는다. 최종 충돌 판정은 호출자가 담당한다.
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
    : BucketSizeCm(FMath::IsFinite(InBucketSizeCm) && InBucketSizeCm >= 1.0 ? InBucketSizeCm : DefaultBucketSizeCm)
{
}

// 좌표와 검색 묶음 개수를 검사해 비정상 영역을 무제한 반복하지 않게 한다.
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

// 같은 ID의 예전 위치를 지운 뒤 새 검색 묶음에 등록한다.
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

// 시설 또는 지역이 사라지면 검색 묶음과 우회 목록의 참조를 함께 제거한다.
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

// 검색 목록 전체를 비운다. 실제 시설 액터를 삭제하는 함수가 아니다.
void FFFZSSpatialHash2D::Reset()
{
    Entries.Reset();
    Buckets.Reset();
    FallbackEntries.Reset();
}

// 겹칠 가능성이 있는 ID를 중복 없이 안정적인 순서로 반환한다.
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
