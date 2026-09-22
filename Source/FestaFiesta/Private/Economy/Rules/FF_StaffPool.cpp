#include "Economy/Rules/FF_StaffPool.h"

void FFF_StaffPool::Initialize(int32 InBaseCount)
{
	BaseStaffCount = FMath::Max(0, InBaseCount);
	// 고용 인원은 축제 단위다. 하루 경계가 아니라 여기서만 0 으로 돌아간다.
	HiredStaffCount = 0;
}

int32 FFF_StaffPool::CalcAffordableHireCount(int32 HireCostGold, int32 Budget,
	int32 RemainingCap) const
{
	if (RemainingCap <= 0) return 0;

	// 상한까지 고용
	if (HireCostGold <= 0) return RemainingCap;

	if (Budget <= 0) return 0;

	return FMath::Min(RemainingCap, Budget / HireCostGold);
}

void FFF_StaffPool::SetHiredStaffCount(int32 InHiredStaffCount)
{
	HiredStaffCount = FMath::Max(0, InHiredStaffCount);
}

int32 FFF_StaffPool::CalcHireCost(int32 InStaffCount, int32 HireCostGold)
{
	if (InStaffCount <= 0 || HireCostGold <= 0) return 0;

	// int32 곱셈 오버플로우 방지
	const int64 Total = static_cast<int64>(InStaffCount) * static_cast<int64>(HireCostGold);

	if (!ensureMsgf(Total <= MAX_int32,
		TEXT("고용비 총액이 int32 를 넘었다. Count=%d Cost=%d"), InStaffCount, HireCostGold))
	{
		// 넘치면 음수가 되어 "지출로 돈을 버는" 경로가 된다. 상한에서 멈춘다.
		return MAX_int32;
	}

	return static_cast<int32>(Total);
}
