#pragma once

#include "CoreMinimal.h"

// 부스 이용 1회의 매출·수수료 판정 용 순수 구조체
struct FESTAFIESTA_API FFF_BoothRevenueRules
{
	// 소분류 토큰의 1회 이용 매출(Gold). 표에 없으면 0 — 무료 시설과 값이 같다.
	// 둘을 구분해야 하는 쪽은 호출부다(네이밍 위반 진단).
	static int32 FindRevenuePerUse(const TMap<FName, int32>& RevenueTable, FName MinorToken);

	// 파티 하나가 부스를 한 번 이용했을 때 플레이어에게 들어오는 수수료(Gold).
	// 인원·수수료율·시설 수익 배율까지 곱한 뒤 한 번만 반올림한다. int32 상한은 포화한다.
	static int32 ResolveCommission(int32 RevenuePerUse, int32 MemberCount, float CommissionRate, float RevenueMultiplier = 1.0f);
};
