#include "Economy/Rules/FF_BoothRevenueRules.h"

int32 FFF_BoothRevenueRules::FindRevenuePerUse(const TMap<FName, int32>& RevenueTable, FName MinorToken)
{
	const int32* Found = RevenueTable.Find(MinorToken);
	return Found ? FMath::Max(0, *Found) : 0;
}

int32 FFF_BoothRevenueRules::ResolveCommission(int32 RevenuePerUse, int32 MemberCount, float CommissionRate)
{
	if (RevenuePerUse <= 0 || CommissionRate <= 0.f) return 0;
	
	if (!ensureMsgf(MemberCount > 0,
		TEXT("부스 이용 인원이 %d 이다. 파티 퇴장 신호가 빈 파티를 보냈다."), MemberCount))
	{
		return 0;
	}
	
	// 인원을 먼저 곱하고 마지막에 한 번만 반올림한다.
	// 1인씩 반올림해 더하면 오차가 인원수만큼 쌓인다.
	const double Gross = static_cast<double>(RevenuePerUse) * MemberCount;
	return FMath::RoundToInt(Gross * CommissionRate);
}
