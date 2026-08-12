// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FestivalFunds.h"

void FFF_FestivalFunds::Initialize(int32 InInitialGold)
{
	Gold = FMath::Max(0, InInitialGold);
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
