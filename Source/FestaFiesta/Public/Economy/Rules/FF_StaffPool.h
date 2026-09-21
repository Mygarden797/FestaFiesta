#pragma once

#include "CoreMinimal.h"

/**
 * Staff 인원.
 * 무페이 기본 팀과 축제 동안 고용한 인원으로 이루어진다.
 * [참고] economy_spec.md §3.7 · §2.5.1 · §5 S6.
 *
 * 고용은 **1회 구매이고 축제가 끝날 때까지 남는다.** 하루 경계에서 줄어들지 않는다 —
 *
 * 영구라서 값어치가 남은 날짜에 비례한다 — 일찍 살수록 이득이고, 그것이 규칙이다.
 * 총원은 MaxTotalStaffCount 로 막는다(예전의 '하루 상한'이 아니라 축제 전체 상한이다).
 *
 * [주의] 스태프는 **시설 성능에 관여하지 않는다.** 시설의 회전율·객단가는 시설 강화가
 *        올리고(UFF_FacilityUpgradeSubsystem), 스태프는 사건 대응에 쓰인다.
 *        두 축을 겹치면 같은 것을 두 번 사게 된다.
 *
 * [미정] 사건에 몇 명을 어떻게 보내는가는 아직 없다. UFF_FestivalEventSubsystem 에서 붙인다.
 */

struct FESTAFIESTA_API FFF_StaffPool
{
	void Initialize(int32 InBaseCount);

	// 무페이 기본 팀 인원. 축제 내내 줄지 않는다.
	int32 GetBaseStaffCount() const { return BaseStaffCount; }

	// 이번 축제에 고용한 인원. 하루 경계를 넘어 유지된다.
	int32 GetHiredStaffCount() const { return HiredStaffCount; }

	// 지금 쓸 수 있는 전체 Staff 인원 = 기본 팀 + 고용 인원.
	int32 GetTotalStaffCount() const
	{
		return BaseStaffCount + HiredStaffCount;
	}

	// Budget 안에서 고용비를 지불할 수 있는 최대 인원을 계산한다.
	// RemainingCap 을 넘지 않으며 상태는 변경하지 않는다.
	int32 CalcAffordableHireCount(
		int32 HireCostGold,
		int32 Budget,
		int32 RemainingCap) const;

	// 고용 인원을 설정한다. 음수는 0 으로 막는다.
	void SetHiredStaffCount(int32 InHiredStaffCount);

	// InStaffCount 명을 고용하는 총 비용을 계산한다. 상태는 변경하지 않는다.
	static int32 CalcHireCost(
		int32 InStaffCount,
		int32 HireCostGold);

private:
	// 무페이 기본 팀.
	int32 BaseStaffCount = 0;

	// 이번 축제에 고용한 인원. 새 축제에서만 0 으로 돌아간다.
	int32 HiredStaffCount = 0;
};
