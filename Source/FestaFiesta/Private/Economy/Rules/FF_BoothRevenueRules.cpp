#include "Economy/Rules/FF_BoothRevenueRules.h"

int32 FFF_BoothRevenueRules::FindRevenuePerUse(const TMap<FName, int32>& RevenueTable, FName MinorToken)
{
	const int32* Found = RevenueTable.Find(MinorToken);
	return Found ? FMath::Max(0, *Found) : 0;
}

int32 FFF_BoothRevenueRules::ResolveCommission(int32 RevenuePerUse, int32 MemberCount, float CommissionRate, float RevenueMultiplier)
{
	if (RevenuePerUse <= 0 || !FMath::IsFinite(CommissionRate) || CommissionRate <= 0.f
		|| !FMath::IsFinite(RevenueMultiplier) || RevenueMultiplier <= 0.f) return 0;
	
	if (!ensureMsgf(MemberCount > 0,
		TEXT("부스 이용 인원이 %d 이다. 파티 퇴장 신호가 빈 파티를 보냈다."), MemberCount))
	{
		return 0;
	}
	
	// 기본 수수료를 먼저 반올림하면 9.6 * 1.5가 15로 부풀어 오른다. 최종 14.4만 반올림한다.
	const double Commission = static_cast<double>(RevenuePerUse) * MemberCount
		* static_cast<double>(CommissionRate) * static_cast<double>(RevenueMultiplier);
	if (Commission >= static_cast<double>(MAX_int32)) return MAX_int32;
	return FMath::RoundToInt32(Commission);
}
