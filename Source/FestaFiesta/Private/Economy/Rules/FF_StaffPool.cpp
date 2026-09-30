#include "Economy/Rules/FF_StaffPool.h"

#include "Economy/Rules/FF_StaffEventRules.h"

FFF_StaffMember& FFF_StaffPool::AddMember(int32 InHiredOnDay, bool bIsBase)
{
	FFF_StaffMember& Member = AllStaff.AddDefaulted_GetRef();
	Member.StaffId = NextStaffId++;
	Member.TrainingLevel = 1;
	Member.HiredOnDay = InHiredOnDay;
	Member.LastPromotedDay = INDEX_NONE;
	Member.bIsBaseStaff = bIsBase;
	return Member;
}

void FFF_StaffPool::Initialize(int32 InBaseCount)
{
	AllStaff.Reset();
	NextStaffId = 1;
	BaseStaffCount = FMath::Max(0, InBaseCount);

	// 기본 팀은 축제 시작 전부터 있던 사람들이라 HiredOnDay 0 — Day1 Setup 부터 승급 가능하다.
	for (int32 Index = 0; Index < BaseStaffCount; ++Index)
	{
		AddMember(/*InHiredOnDay*/ 0, /*bIsBase*/ true);
	}
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
	const int32 Target = FMath::Max(0, InHiredStaffCount);
	const int32 Current = GetHiredStaffCount();

	if (Target > Current)
	{
		// 날짜를 모르는 호출부다 — 지금부터 승급 가능한 것으로 본다(HiredOnDay 0).
		for (int32 Index = 0; Index < Target - Current; ++Index)
		{
			AddMember(/*InHiredOnDay*/ 0, /*bIsBase*/ false);
		}
	}
	else if (Target < Current)
	{
		// 가장 최근에 고용한 인원부터 줄인다. 기본 팀은 항상 bIsBaseStaff 로 보호된다.
		int32 ToRemove = Current - Target;
		for (int32 Index = AllStaff.Num() - 1; Index >= 0 && ToRemove > 0; --Index)
		{
			if (!AllStaff[Index].bIsBaseStaff)
			{
				AllStaff.RemoveAt(Index);
				--ToRemove;
			}
		}
	}
}

void FFF_StaffPool::HireStaffMembers(int32 RequestedCount, int32 CurrentDay)
{
	for (int32 Index = 0; Index < FMath::Max(0, RequestedCount); ++Index)
	{
		AddMember(CurrentDay, /*bIsBase*/ false);
	}
}

void FFF_StaffPool::RestoreAllStaff(const TArray<FFF_StaffMember>& InStaff)
{
	AllStaff = InStaff;

	BaseStaffCount = 0;
	int32 HighestId = 0;
	for (const FFF_StaffMember& Member : AllStaff)
	{
		if (Member.bIsBaseStaff)
		{
			++BaseStaffCount;
		}
		HighestId = FMath::Max(HighestId, Member.StaffId);
	}

	// 복원 후에 더 고용해도 기존 StaffId 와 안 겹치게 한다.
	NextStaffId = HighestId + 1;
}

bool FFF_StaffPool::TryPromoteStaffMember(int32 StaffId, int32 CurrentDay)
{
	FFF_StaffMember* Member = AllStaff.FindByPredicate(
		[StaffId](const FFF_StaffMember& Candidate) { return Candidate.StaffId == StaffId; });

	if (Member == nullptr || !FFF_StaffEventRules::CanPromote(*Member, CurrentDay))
	{
		return false;
	}

	++Member->TrainingLevel;
	Member->LastPromotedDay = CurrentDay;
	return true;
}

bool FFF_StaffPool::SetAssignedFacility(int32 StaffId, FGuid FacilityId)
{
	FFF_StaffMember* Member = AllStaff.FindByPredicate(
		[StaffId](const FFF_StaffMember& Candidate) { return Candidate.StaffId == StaffId; });
	if (!Member || Member->AssignedFacilityId == FacilityId) return false;
	Member->AssignedFacilityId = FacilityId;
	return true;
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
