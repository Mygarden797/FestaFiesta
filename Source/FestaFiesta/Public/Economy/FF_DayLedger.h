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
	
	// 하루 경계에서 호출
	void Reset();
	
	int32 GetSpendByReason(EFF_SpendReason Reason) const;
	int32 GetEarnByReason(EFF_EarnReason Reason) const;
	
	// Report 부스별 표
	// FacilityId 문자열로 정렬해서 읽을 것
	const TMap<FName, int32>& GetRevenueByFacility() const { return RevenueByFacility; }
	
private:
	TArray<int32> SpendByReason;
	TArray<int32> EarnByReason;
	
	TMap<FName, int32> RevenueByFacility;
};
