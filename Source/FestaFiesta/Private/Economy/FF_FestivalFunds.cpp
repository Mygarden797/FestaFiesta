// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FestivalFunds.h"

void FFF_FestivalFunds::Initialize(int32 InInitialGold)
{
	Gold = FMath::Max(0, InInitialGold);
}

int32 FFF_FestivalFunds::GetAvailableGold() const
{
	// TODO : S2 에서 홀드 합계를 뺀다. 지금은 홀드가 없어 잔액과 같다.
	return Gold;
}

bool FFF_FestivalFunds::CanAfford(int32 Amount) const
{
	if (Amount < 0)
	{
		return false;
	}

	return Amount <= GetAvailableGold();
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
