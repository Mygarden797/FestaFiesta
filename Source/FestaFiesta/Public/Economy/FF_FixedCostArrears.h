#pragma once

#include "CoreMinimal.h"

/**
 * 임대료 미납액.
 * [참고] economy_spec.md §3.7 · §5 S6.
 *
 * 원칙 : 빚을 질 수 있는 것은 플레이어가 거절할 수 없는 것뿐이다.
 * 오직 임대료 -- 축제장 계약은 이미 되어 있고 중간에 나갈 수 없으니, 못 낸 만큼이 여기 쌓인다.
 *
 * 미납은 축제 종료까지 누적된다. 수익·환급이 들어오면 잔액에 남기지 않고 먼저 갚는다.
 * 상환은 임대료 지출로 원장에 기록된다.
 *
 * 이자는 없다.
 * 
 * [미정] 축제 종료 시점에 미납이 남았을 때의 처리
 */

struct FESTAFIESTA_API FFF_FixedCostArrears
{
	// 고정비 미납액을 추가한다.
	void AddArrears(int32 Amount);

	// 현재 누적된 전체 고정비 미납액.
	int32 GetTotalArrears() const { return TotalArrears; }

	// 미납된 고정비가 없는지 확인한다.
	bool HasNoArrears() const { return TotalArrears == 0; }

	// 축제가 바뀔 때 미납 상태를 초기화한다.
	// 하루 경계에서는 호출하지 않는다.
	void ResetArrears();

	// Budget 범위 안에서 미납 고정비를 상환하고 실제 상환액을 반환한다.
	int32 RepayArrears(int32 Budget);

private:
	// 현재까지 누적된 전체 고정비 미납액.
	int32 TotalArrears = 0;
};
