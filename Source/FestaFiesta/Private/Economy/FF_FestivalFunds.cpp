// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FestivalFunds.h"

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
