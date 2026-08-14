#pragma once

#include "CoreMinimal.h"

/**
 * Staff 인원. 
 * 무페이 기본 팀과 오늘 고용한 1일 알바로 이루어진다.
 * [참고] economy_spec.md §3.7 · §2.5.1 · §5 S6.
 * 
 * 임금은 아침마다 고용하는 알바에만 붙는다(하루 최대 MaxTemporaryStaffCount 명).
 * 고용 조건은 그날 하루뿐이다. 다음 날 아침에 다시 정한다. 기본 팀은 줄지 않는다.
 *
 * [미정] 어느 시설에 몇 명을 배정하는가는 아직 없다. S9 에서 붙인다.
 */

struct FESTAFIESTA_API FFF_StaffPool
{
	void Initialize(int32 InBaseCount);
	
	// 무페이 기본 팀 인원.
	int32 GetBaseStaffCount() const { return BaseStaffCount; }

	// 오늘 하루 고용한 임시 Staff 인원.
	int32 GetTemporaryStaffCount() const { return TemporaryStaffCount; }

	// 오늘 실제로 사용할 수 있는 전체 Staff 인원 = 기본 팀 + 당일 고용 인원.
	int32 GetTotalStaffCount() const
	{
		return BaseStaffCount + TemporaryStaffCount;
	}
	
	// Budget 안에서 하루치 임금을 지불할 수 있는 최대 임시 Staff 수를 계산한다.
	// MaxTemporaryStaffCount를 넘지 않으며 상태는 변경하지 않는다.
	int32 CalcAffordableTemporaryStaffCount(
		int32 WagePerStaffGold,
		int32 Budget,
		int32 MaxTemporaryStaffCount) const;

	// 오늘 하루 고용할 임시 Staff 인원을 설정한다.
	void SetTemporaryStaffCount(int32 InTemporaryStaffCount);

	// 임시 Staff InStaffCount명의 하루치 총 임금을 계산한다.
	// 상태는 변경하지 않는다.
	static int32 CalcTemporaryStaffWage(
		int32 InStaffCount,
		int32 WagePerStaffGold);

private:
	// 무페이 기본 팀.
	int32 BaseStaffCount = 0;

	// 오늘 하루 고용한 임시 Staff.
	int32 TemporaryStaffCount = 0;
};
