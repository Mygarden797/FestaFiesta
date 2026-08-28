// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/Rules/FF_FestivalFunds.h"

void FFF_FestivalFunds::Initialize(int32 InInitialGold)
{
	Gold = FMath::Max(0, InInitialGold);
	TotalRevenue = 0;
	TodayRevenue = 0;
}

bool FFF_FestivalFunds::CanAfford(int32 Amount) const
{
	if (Amount < 0)
	{
		return false;
	}

	return Amount <= Gold;
}

bool FFF_FestivalFunds::TrySpend(int32 Amount)
{
	if (!CanAfford(Amount))
	{
		return false;
	}

	Gold -= Amount;
	return true;
}

int32 FFF_FestivalFunds::PayMandatoryCost(int32 CostAmount)
{
	if (!ensureMsgf(CostAmount >= 0, TEXT("강제 징수액은 음수일 수 없다. CostAmount=%d"), CostAmount))
	{
		// 음수를 허용하면 임대료 정산이 돈을 버는 경로가 된다.
		return 0;
	}
	
	const int32 Paid = FMath::Min(CostAmount, Gold);
	Gold -= Paid;
	return Paid;
}

void FFF_FestivalFunds::ResetTodayRevenue()
{
	TodayRevenue = 0;
}

void FFF_FestivalFunds::AddRevenue(int32 Amount)
{
	if (!ensureMsgf(Amount >= 0, TEXT("매출은 음수일 수 없다. Amount=%d"), Amount))
	{
		// 매출은 설계상 음수가 될 수 없으므로, 불변식 위반 시 진단 후 처리를 중단한다.
		return;
	}

	Gold += Amount;
	TotalRevenue += Amount;
	TodayRevenue += Amount;
}

void FFF_FestivalFunds::Refund(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	// 상한은 없지만 int32 를 넘기면 음수 잔액이 된다. 지출 거부와 구분되지 않는 상태라 여기서 막는다.
	Gold = (Amount > MAX_int32 - Gold) ? MAX_int32 : Gold + Amount;
}
