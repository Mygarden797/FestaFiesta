#include "Economy/Rules/FF_FixedCostArrears.h"

void FFF_FixedCostArrears::AddArrears(int32 Amount)
{
	if (Amount <= 0) return;
	
	if (!ensureMsgf(Amount <= MAX_int32 - TotalArrears,
		TEXT("누적 미납액이 int32 를 넘었다. TotalArrears=%d"), TotalArrears))
	{
		// 넘치면 음수가 되어 "미납액으로 돈을 버는" 경로가 된다. 상한에서 멈춘다.
		TotalArrears = MAX_int32;
		return;
	}
	
	TotalArrears += Amount;
}

void FFF_FixedCostArrears::ResetArrears()
{
	TotalArrears = 0;
}

int32 FFF_FixedCostArrears::RepayArrears(int32 Budget)
{
	// Budget : 이번에 상환에 사용할 수 있는 새로 들어온 돈
	if (Budget <= 0 || TotalArrears <= 0) return 0;
	
	const int32 Repaid = FMath::Min(Budget, TotalArrears);
	TotalArrears -= Repaid;
	
	return Repaid;
}
