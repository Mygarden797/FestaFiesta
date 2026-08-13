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

void FFF_FestivalFunds::Refund(int32 Amount)
{
	if (Amount <= 0)
	{
		return;
	}

	// 상한은 없지만 int32 를 넘기면 음수 잔액이 된다. 지출 거부와 구분되지 않는 상태라 여기서 막는다.
	Gold = (Amount > MAX_int32 - Gold) ? MAX_int32 : Gold + Amount;
}
