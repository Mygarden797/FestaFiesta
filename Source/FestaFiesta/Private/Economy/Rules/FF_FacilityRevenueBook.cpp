#include "Economy/Rules/FF_FacilityRevenueBook.h"

namespace
{
	// 상한에서 포화한다. 매출은 음수가 없으므로 아래쪽은 볼 필요가 없다.
	int32 SaturatingAdd(const int32 Current, const int32 Amount)
	{
		return Current > MAX_int32 - Amount ? MAX_int32 : Current + Amount;
	}
}

bool FFF_FacilityRevenueBook::Record(const FGuid& FacilityInstanceId, const int32 Amount)
{
	if (!FacilityInstanceId.IsValid() || Amount <= 0)
	{
		return false;
	}

	FFF_FacilityInstanceRevenue& Entry = Entries.FindOrAdd(FacilityInstanceId);
	Entry.TodayGold = SaturatingAdd(Entry.TodayGold, Amount);
	Entry.TotalGold = SaturatingAdd(Entry.TotalGold, Amount);
	Entry.TodayPaymentCount = SaturatingAdd(Entry.TodayPaymentCount, 1);
	Entry.TotalPaymentCount = SaturatingAdd(Entry.TotalPaymentCount, 1);
	return true;
}

void FFF_FacilityRevenueBook::BeginNewDay()
{
	for (TPair<FGuid, FFF_FacilityInstanceRevenue>& Pair : Entries)
	{
		Pair.Value.TodayGold = 0;
		Pair.Value.TodayPaymentCount = 0;
	}
}

void FFF_FacilityRevenueBook::Reset()
{
	Entries.Reset();
}

void FFF_FacilityRevenueBook::Export(
	TArray<FGuid>& OutIds, TArray<FFF_FacilityInstanceRevenue>& OutRevenues) const
{
	OutIds.Reset();
	OutRevenues.Reset();
	for (const TPair<FGuid, FFF_FacilityInstanceRevenue>& Pair : Entries)
	{
		OutIds.Add(Pair.Key);
		OutRevenues.Add(Pair.Value);
	}
}

void FFF_FacilityRevenueBook::Import(
	const TArray<FGuid>& InIds, const TArray<FFF_FacilityInstanceRevenue>& InRevenues)
{
	Entries.Reset();
	for (int32 Index = 0; Index < FMath::Min(InIds.Num(), InRevenues.Num()); ++Index)
	{
		if (InIds[Index].IsValid())
		{
			Entries.Add(InIds[Index], InRevenues[Index]);
		}
	}
}

bool FFF_FacilityRevenueBook::Find(
	const FGuid& FacilityInstanceId, FFF_FacilityInstanceRevenue& OutRevenue) const
{
	const FFF_FacilityInstanceRevenue* Found = Entries.Find(FacilityInstanceId);
	OutRevenue = Found ? *Found : FFF_FacilityInstanceRevenue();
	return Found != nullptr;
}
