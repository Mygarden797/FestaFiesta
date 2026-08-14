// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 축제 자금 - 단위는 Gold(int32)이다. Gold 잔액을 들고 지출 가능 여부를 판정한다.
 * [참고] Gold 는 음수가 되지 않는다. 잔액 부족 시 "구매 거부"를 한다.
 * [참고] data/metadata/spec/economy_spec.md §3.1 / §4.2.
 * 
 * 상태 보관과 통지는 Festival/FF_FestivalProgressionSubsystem 이 맡는다.
 */
struct FESTAFIESTA_API FFF_FestivalFunds
{
	// 잔액을 초기 자금으로 맞춘다. Gold 는 축제가 바뀌면 초기화된다.
	void Initialize(int32 InInitialGold);

	// 현재 잔액
	int32 GetGold() const { return Gold; }

	// 이 금액을 지출할 수 있는지. 상태를 바꾸지 않는다.
	// 0 은 true (비용 0 인 명령이 있다)
	bool CanAfford(int32 Amount) const;

	// 지출한다. 성공하면 true 이고 잔액이 그만큼 줄어든다.
	bool TrySpend(int32 Amount);
	
	/* 
	 * 임대료처럼 거절할 수 없는 비용을 지불한다. 거부하지 않는다. (TrySpend와 규칙 반대)
	 * 잔액이 모자라면 있는 만큼만 내고 '실제 지불액'을 돌려준다.
	 * 부족분은 호출부가 FFF_FixedCostArrears 에 미납으로 남긴다.
	*/
	int32 PayMandatoryCost(int32 CostAmount);
	
	// Gold 와 TotalRevenue·TodayRevenue 를 함께 늘린다.
	void AddRevenue(int32 Amount);
	
	int32 GetTotalRevenue() const { return TotalRevenue; }
	
	int32 GetTodayRevenue() const { return TodayRevenue; }

	// 잔액을 늘린다. 음수는 무시한다. 상한이 없으므로 int32 오버플로만 막는다.
	void Refund(int32 Amount);
	
	// 하루 경계에서 '오늘 매출'만 리셋한다.
	void ResetTodayRevenue();

private:
	// 잔액 (Gold). 음수가 되지 않는다.
	int32 Gold = 0;
	
	// 누적 매출
	int32 TotalRevenue = 0;
	
	// 오늘 매출
	int32 TodayRevenue = 0;
	
};
