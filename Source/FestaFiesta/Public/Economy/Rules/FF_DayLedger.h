#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"

// 회계 장부
// 당일 원인별/시설별 매출 및 지출 분해 — Report 화면 전용
struct FESTAFIESTA_API FFF_DayLedger
{
	FFF_DayLedger();
	
	void RecordSpend(EFF_SpendReason Reason, int32 Amount);
	void RecordEarn(EFF_EarnReason Reason, int32 Amount, FName FacilityId = NAME_None);

	// 시설 철거 환급 전용. TotalRevenue를 안 건드리는 Refund()와 짝이라 EarnByReason에 안 섞는다.
	void RecordRefund(int32 Amount);

	// Signed construction corrections preserve every unrelated revenue/spend entry.
	bool CanAdjustConstruction(int64 InstallSpendDelta, int64 RefundDelta) const;
	bool AdjustConstruction(int64 InstallSpendDelta, int64 RefundDelta);

	// 하루 경계에서 호출
	void Reset();

	// 다른 날의 원장을 더한다. 축제 전체 합계를 만들 때 쓴다. int32 상한에서 포화한다.
	void Accumulate(const FFF_DayLedger& Other);

	// 저장·복원용 평탄한 표현. 배열 길이가 현재 enum 개수와 다르면(옛 저장) 겹치는 칸만 옮긴다.
	void Export(TArray<int32>& OutSpend, TArray<int32>& OutEarn, int32& OutRefund,
		TArray<FName>& OutFacilityIds, TArray<int32>& OutFacilityRevenue) const;
	void Import(const TArray<int32>& InSpend, const TArray<int32>& InEarn, int32 InRefund,
		const TArray<FName>& InFacilityIds, const TArray<int32>& InFacilityRevenue);

	int32 GetSpendByReason(EFF_SpendReason Reason) const;
	int32 GetEarnByReason(EFF_EarnReason Reason) const;
	int32 GetTotalRefund() const { return TotalRefund; }

	// Report 부스별 표
	// FacilityId 문자열로 정렬해서 읽을 것
	const TMap<FName, int32>& GetRevenueByFacility() const { return RevenueByFacility; }

private:
	TArray<int32> SpendByReason;
	TArray<int32> EarnByReason;
	int32 TotalRefund = 0;

	TMap<FName, int32> RevenueByFacility;
};
