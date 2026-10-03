#include "Economy/Rules/FF_DayLedger.h"

namespace 
{
	// 컴파일 타임 상수 - 배열 크기 확정
	constexpr int32 SpendReasonCount = static_cast<int32>(EFF_SpendReason::FacilityUpgrade) + 1;
	constexpr int32 EarnReasonCount = static_cast<int32>(EFF_EarnReason::IncidentReward) + 1;
}

FFF_DayLedger::FFF_DayLedger()
{
	// N칸 만들고 전부 0으로 채우기
	SpendByReason.SetNumZeroed(SpendReasonCount);
	EarnByReason.SetNumZeroed(EarnReasonCount);
}

void FFF_DayLedger::RecordSpend(EFF_SpendReason Reason, int32 Amount)
{
	if (!ensureMsgf(Amount >= 0, TEXT("지출 기록은 음수일 수 없다. Reason=%d Amount=%d"),
		static_cast<int32>(Reason), Amount))
	{
		return;
	}
	
	SpendByReason[static_cast<int32>(Reason)] += Amount;
}

void FFF_DayLedger::RecordEarn(EFF_EarnReason Reason, int32 Amount, FName FacilityId)
{
	if (!ensureMsgf(Amount >= 0, TEXT("매출 기록은 음수일 수 없다. Reason=%d Amount=%d"),
		static_cast<int32>(Reason), Amount))
	{
		return;
	}

	EarnByReason[static_cast<int32>(Reason)] += Amount;
	
	// FacilityId가 없는 매출(입장료·일일보너스 등)은 부스별 표에 안 들어간다
	if (!FacilityId.IsNone())
	{
		RevenueByFacility.FindOrAdd(FacilityId) += Amount;
	}
}

void FFF_DayLedger::RecordRefund(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	TotalRefund += Amount;
}

void FFF_DayLedger::Reset()
{
	for (int32& Value : SpendByReason) Value = 0;
	for (int32& Value : EarnByReason) Value = 0;
	TotalRefund = 0;
	RevenueByFacility.Reset(); // 할당해둔 내부 메모리는 재사용할 수 있게 유지
}

namespace
{
	int32 SaturatingSum(const int32 A, const int32 B)
	{
		return A > MAX_int32 - B ? MAX_int32 : A + B;
	}
}

void FFF_DayLedger::Accumulate(const FFF_DayLedger& Other)
{
	for (int32 Index = 0; Index < SpendByReason.Num(); ++Index)
	{
		SpendByReason[Index] = SaturatingSum(SpendByReason[Index], Other.SpendByReason[Index]);
	}
	for (int32 Index = 0; Index < EarnByReason.Num(); ++Index)
	{
		EarnByReason[Index] = SaturatingSum(EarnByReason[Index], Other.EarnByReason[Index]);
	}
	TotalRefund = SaturatingSum(TotalRefund, Other.TotalRefund);
	for (const TPair<FName, int32>& Pair : Other.RevenueByFacility)
	{
		int32& Slot = RevenueByFacility.FindOrAdd(Pair.Key);
		Slot = SaturatingSum(Slot, Pair.Value);
	}
}

void FFF_DayLedger::Export(TArray<int32>& OutSpend, TArray<int32>& OutEarn, int32& OutRefund,
	TArray<FName>& OutFacilityIds, TArray<int32>& OutFacilityRevenue) const
{
	OutSpend = SpendByReason;
	OutEarn = EarnByReason;
	OutRefund = TotalRefund;
	OutFacilityIds.Reset();
	OutFacilityRevenue.Reset();
	for (const TPair<FName, int32>& Pair : RevenueByFacility)
	{
		OutFacilityIds.Add(Pair.Key);
		OutFacilityRevenue.Add(Pair.Value);
	}
}

void FFF_DayLedger::Import(const TArray<int32>& InSpend, const TArray<int32>& InEarn, const int32 InRefund,
	const TArray<FName>& InFacilityIds, const TArray<int32>& InFacilityRevenue)
{
	Reset();
	for (int32 Index = 0; Index < FMath::Min(InSpend.Num(), SpendByReason.Num()); ++Index)
	{
		SpendByReason[Index] = FMath::Max(InSpend[Index], 0);
	}
	for (int32 Index = 0; Index < FMath::Min(InEarn.Num(), EarnByReason.Num()); ++Index)
	{
		EarnByReason[Index] = FMath::Max(InEarn[Index], 0);
	}
	TotalRefund = FMath::Max(InRefund, 0);
	for (int32 Index = 0; Index < FMath::Min(InFacilityIds.Num(), InFacilityRevenue.Num()); ++Index)
	{
		if (!InFacilityIds[Index].IsNone() && InFacilityRevenue[Index] > 0)
		{
			RevenueByFacility.Add(InFacilityIds[Index], InFacilityRevenue[Index]);
		}
	}
}

bool FFF_DayLedger::CanAdjustConstruction(const int64 InstallSpendDelta, const int64 RefundDelta) const
{
	const int64 Spend = int64(GetSpendByReason(EFF_SpendReason::FacilityInstall)) + InstallSpendDelta;
	const int64 Refund = int64(TotalRefund) + RefundDelta;
	return Spend >= 0 && Spend <= MAX_int32 && Refund >= 0 && Refund <= MAX_int32;
}

bool FFF_DayLedger::AdjustConstruction(const int64 InstallSpendDelta, const int64 RefundDelta)
{
	if (!CanAdjustConstruction(InstallSpendDelta, RefundDelta)) return false;
	SpendByReason[static_cast<int32>(EFF_SpendReason::FacilityInstall)] += int32(InstallSpendDelta);
	TotalRefund += int32(RefundDelta);
	return true;
}

int32 FFF_DayLedger::GetSpendByReason(EFF_SpendReason Reason) const
{
	return SpendByReason[static_cast<int32>(Reason)];
}

int32 FFF_DayLedger::GetEarnByReason(EFF_EarnReason Reason) const
{
	return EarnByReason[static_cast<int32>(Reason)];
}
