#include "Economy/FF_StaffPool.h"

void FFF_StaffPool::Initialize(int32 InBaseCount)
{
	BaseStaffCount = FMath::Max(0, InBaseCount);
	TemporaryStaffCount = 0;
}

int32 FFF_StaffPool::CalcAffordableTemporaryStaffCount(int32 WagePerStaffGold, int32 Budget,
	int32 MaxTemporaryStaffCount) const
{
	if (MaxTemporaryStaffCount <= 0) return 0;
	
	// 상한까지 고용
	if (WagePerStaffGold <= 0) return MaxTemporaryStaffCount;
	
	if (Budget <= 0) return 0;
	
	return FMath::Min(MaxTemporaryStaffCount, Budget / WagePerStaffGold);
}

void FFF_StaffPool::SetTemporaryStaffCount(int32 InTemporaryStaffCount)
{
	TemporaryStaffCount = FMath::Max(0, InTemporaryStaffCount);
}

int32 FFF_StaffPool::CalcTemporaryStaffWage(int32 InStaffCount, int32 WagePerStaffGold)
{
	if (InStaffCount <= 0 || WagePerStaffGold <= 0) return 0;
	
	// int32 곱셈 오버플로우 방지
	const int64 Total = static_cast<int64>(InStaffCount) * static_cast<int64>(WagePerStaffGold);
	
	if (!ensureMsgf(Total <= MAX_int32,
		TEXT("임금 총액이 int32 를 넘었다. Count=%d Wage=%d"), InStaffCount, WagePerStaffGold))
	{
		// 넘치면 음수가 되어 "지출로 돈을 버는" 경로가 된다. 상한에서 멈춘다.
		return MAX_int32;
	}
	
	return static_cast<int32>(Total);
}