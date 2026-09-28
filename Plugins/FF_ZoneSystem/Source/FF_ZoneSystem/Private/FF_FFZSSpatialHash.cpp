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
    // 곱셈 대신 나눗셈으로 검사해 좌표 범위가 매우 커도 정수 넘침이 발생하지 않게 한다.
    return Width > 0 && Height > 0 && Width <= MaximumBucketCount &&
        Height <= MaximumBucketCount / Width;
}

// 같은 ID의 예전 위치를 지운 뒤 새 검색 묶음에 등록한다.
bool FFFZSSpatialHash2D::InsertOrUpdate(const FGuid& Id, const FBox2D& WorldBounds)
{
    // 같은 위치의 시설을 재등록할 때는 검색 묶음을 지웠다가 다시 만들 필요가 없다.
    // 근사 비교를 쓰면 조금 움직인 시설을 놓칠 수 있으므로 좌표가 정확히 같을 때만 생략한다.
    if (const FEntry* Existing = Entries.Find(Id);
        Existing && Existing->bValidBounds && HasFiniteOrderedBounds(WorldBounds) &&
        Existing->Bounds.Min == WorldBounds.Min && Existing->Bounds.Max == WorldBounds.Max)
    {
        return true;
    }

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

    const int64 BucketCount = (static_cast<int64>(Last.X) - First.X + 1) *
        (static_cast<int64>(Last.Y) - First.Y + 1);
    Entry.BucketKeys.Reserve(static_cast<int32>(BucketCount));
    // 위 검사에서 최대 묶음 수가 보장된다. 반복 변수는 최대 int32 좌표에서도 증가할 수 있게 넓게 둔다.
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
        // 거대한 영역은 빈 묶음을 끝없이 순회하지 않고 실제 등록된 시설 수만큼만 검사한다.
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
