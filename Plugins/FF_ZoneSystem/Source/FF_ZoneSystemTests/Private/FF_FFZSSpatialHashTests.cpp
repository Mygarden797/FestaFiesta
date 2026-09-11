#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../../FF_ZoneSystem/Private/FF_FFZSSpatialHash.h"
#include <limits>

namespace
{
    FGuid SpatialId(const uint32 Value)
    {
        return FGuid(0, 0, 0, Value);
    }

    FBox2D SpatialBox(const double MinX, const double MinY, const double MaxX, const double MaxY)
    {
        return FBox2D(FVector2D(MinX, MinY), FVector2D(MaxX, MaxY));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSSpatialHashBoundaryTest,
    "FF_ZoneSystem.Unit.SpatialHash.NegativeCoordinatesBoundaryAndDeduplication",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSpatialHashBoundaryTest::RunTest(const FString& Parameters)
{
    FFFZSSpatialHash2D Index(1000.0);
    TestTrue(TEXT("A multi-bucket region registers"),
        Index.InsertOrUpdate(SpatialId(3), SpatialBox(-1500, -1500, 1500, 1500)));
    Index.InsertOrUpdate(SpatialId(1), SpatialBox(-2000, -2000, -1000, -1000));
    Index.InsertOrUpdate(SpatialId(2), SpatialBox(-900, -900, -800, -800));
    TArray<FGuid> Found;
    Index.Query(SpatialBox(-1000, -1000, -1000, -1000), Found);
    TestTrue(TEXT("Touching and enclosing bounds are included once in deterministic order"),
        Found == TArray<FGuid>{SpatialId(1), SpatialId(3)});
    Index.Query(SpatialBox(-2000, -2000, 2000, 2000), Found);
    TestTrue(TEXT("Many shared buckets do not duplicate an ID"),
        Found == TArray<FGuid>{SpatialId(1), SpatialId(2), SpatialId(3)});
    Index.Query(SpatialBox(-750, -750, -700, -700), Found);
    TestTrue(TEXT("Sharing a bucket without AABB overlap is excluded"),
        Found == TArray<FGuid>{SpatialId(3)});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSSpatialHashMutationTest,
    "FF_ZoneSystem.Unit.SpatialHash.MoveRemoveAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSpatialHashMutationTest::RunTest(const FString& Parameters)
{
    FFFZSSpatialHash2D Index;
    const FGuid Id = SpatialId(1);
    const FBox2D OldBounds = SpatialBox(-100, -100, 100, 100);
    const FBox2D NewBounds = SpatialBox(10000, 10000, 10100, 10100);
    Index.InsertOrUpdate(Id, OldBounds);
    Index.InsertOrUpdate(Id, NewBounds);
    TestEqual(TEXT("Moving replaces the original record"), Index.Num(), 1);
    TArray<FGuid> Found;
    Index.Query(OldBounds, Found);
    TestTrue(TEXT("Old buckets do not retain the moved facility"), Found.IsEmpty());
    Index.Query(NewBounds, Found);
    TestTrue(TEXT("New buckets contain the moved facility"), Found == TArray<FGuid>{Id});
    Index.Remove(Id);
    Index.Remove(Id);
    Index.Query(NewBounds, Found);
    TestTrue(TEXT("Removal is complete and idempotent"), Found.IsEmpty() && Index.Num() == 0);
    Index.InsertOrUpdate(Id, OldBounds);
    Index.Reset();
    Index.Query(OldBounds, Found);
    TestTrue(TEXT("Reset clears records and buckets"), Found.IsEmpty() && Index.Num() == 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSSpatialHashFallbackTest,
    "FF_ZoneSystem.Unit.SpatialHash.OversizedAndInvalidBoundsRemainConservative",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSpatialHashFallbackTest::RunTest(const FString& Parameters)
{
    FFFZSSpatialHash2D Index(1.0);
    const FBox2D Huge = SpatialBox(-1.0e30, -1.0e30, 1.0e30, 1.0e30);
    const FBox2D Origin = SpatialBox(-1, -1, 1, 1);
    const FBox2D Invalid(EForceInit::ForceInit);
    TestTrue(TEXT("A huge valid box uses fallback without enumerating its area"),
        Index.InsertOrUpdate(SpatialId(1), Huge));
    TestFalse(TEXT("Invalid bounds report failure while preserving a conservative candidate"),
        Index.InsertOrUpdate(SpatialId(2), Invalid));
    Index.InsertOrUpdate(SpatialId(3), SpatialBox(500, 500, 501, 501));
    TArray<FGuid> Found;
    Index.Query(Origin, Found);
    TestTrue(TEXT("A local query includes oversized and unknown bounds"),
        Found == TArray<FGuid>{SpatialId(1), SpatialId(2)});
    Index.Query(Huge, Found);
    TestTrue(TEXT("A huge query scans actual records"),
        Found == TArray<FGuid>{SpatialId(1), SpatialId(2), SpatialId(3)});
    Index.Query(Invalid, Found);
    TestEqual(TEXT("An invalid query does not silently lose candidates"), Found.Num(), 3);
    TestFalse(TEXT("NaN cannot enter integer bucket conversion"), Index.InsertOrUpdate(SpatialId(4),
        SpatialBox(std::numeric_limits<double>::quiet_NaN(), 0, 1, 1)));
    TestFalse(TEXT("Reversed bounds are conservative"),
        Index.InsertOrUpdate(SpatialId(5), SpatialBox(10, 10, -10, -10)));
    Index.Query(Origin, Found);
    TestTrue(TEXT("Malformed records remain candidates until repaired"),
        Found == TArray<FGuid>{SpatialId(1), SpatialId(2), SpatialId(4), SpatialId(5)});
    Index.InsertOrUpdate(SpatialId(2), SpatialBox(500, 500, 501, 501));
    Index.Remove(SpatialId(1));
    Index.Remove(SpatialId(4));
    Index.Remove(SpatialId(5));
    Index.Query(Origin, Found);
    TestTrue(TEXT("Repair and removal clear fallback membership"), Found.IsEmpty());
    const double Edge = static_cast<double>(MAX_int32);
    Index.InsertOrUpdate(SpatialId(6), SpatialBox(Edge, Edge, Edge, Edge));
    Index.Query(SpatialBox(Edge, Edge, Edge, Edge), Found);
    TestTrue(TEXT("A bucket at MAX_int32 cannot overflow its enumeration loop"),
        Found == TArray<FGuid>{SpatialId(6)});
    TestEqual(TEXT("An invalid bucket size falls back to the default"),
        FFFZSSpatialHash2D(0).GetBucketSizeCm(), 2000.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFFZSSpatialHashReferenceTest,
    "FF_ZoneSystem.Unit.SpatialHash.MatchesBruteForceAcrossBucketSizes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFFZSSpatialHashReferenceTest::RunTest(const FString& Parameters)
{
    FRandomStream Random(74119);
    TArray<FBox2D> Bounds;
    for (int32 I = 0; I < 256; ++I)
    {
        const FVector2D Min(Random.FRandRange(-100000.0f, 100000.0f),
            Random.FRandRange(-100000.0f, 100000.0f));
        const FVector2D Extent(Random.FRandRange(0.0f, 12000.0f), Random.FRandRange(0.0f, 12000.0f));
        Bounds.Emplace(Min, Min + Extent);
    }
    // Exercise the fallback list along with ordinary entries in the same reference queries.
    Bounds.Add(SpatialBox(-1.0e20, -200, 1.0e20, 200));
    for (const double BucketSize : {100.0, 1000.0, 2000.0, 25000.0})
    {
        FFFZSSpatialHash2D Index(BucketSize);
        for (int32 I = Bounds.Num() - 1; I >= 0; --I)
        {
            Index.InsertOrUpdate(SpatialId(I + 1), Bounds[I]);
        }
        for (int32 QueryIndex = 0; QueryIndex < 100; ++QueryIndex)
        {
            const FVector2D Min(Random.FRandRange(-120000.0f, 120000.0f),
                Random.FRandRange(-120000.0f, 120000.0f));
            const FBox2D QueryBounds(Min, Min + FVector2D(17000.0, 23000.0));
            TArray<FGuid> Expected;
            for (int32 I = 0; I < Bounds.Num(); ++I)
            {
                if (Bounds[I].Intersect(QueryBounds))
                {
                    Expected.Add(SpatialId(I + 1));
                }
            }
            TArray<FGuid> Actual;
            Index.Query(QueryBounds, Actual);
            if (!TestTrue(FString::Printf(TEXT("Bucket %.0f query %d matches exhaustive AABB search"),
                BucketSize, QueryIndex), Actual == Expected))
            {
                return false;
            }
        }
    }
    return true;
}

#endif
