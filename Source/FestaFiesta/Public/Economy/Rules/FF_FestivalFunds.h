// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 축제 자금 - 단위는 Gold(int32)이다. Gold 잔액을 들고 지출 가능 여부를 판정한다.
 *
 * Gold 는 음수가 되지 않는다. 잔액이 모자라면 그 지출은 거부되고 상태는 그대로다 —
 * 화면에서는 버튼이 막히는 것으로 나타난다.
 *
 * 거절할 수 없는 지출은 없다. 모든 지출은 플레이어가 누른 것이라 빚·미납이 생길 자리가 없다.
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

	// Gold 와 TotalRevenue·TodayRevenue 를 함께 늘린다.
	void AddRevenue(int32 Amount);
	
	int32 GetTotalRevenue() const { return TotalRevenue; }
	
	int32 GetTodayRevenue() const { return TodayRevenue; }

	// 잔액을 늘린다. 음수는 무시한다. 상한이 없으므로 int32 오버플로만 막는다.
	void Refund(int32 Amount);
	
	// 하루 경계에서 '오늘 매출'만 리셋한다.
	void ResetTodayRevenue();

	/* 저장에서 불러올 때 값을 그대로 되돌린다.

	   Initialize 와 나누는 이유는 뜻이 다르기 때문이다. 저쪽은 축제를 처음
	   시작하는 것이고, 이쪽은 있던 것을 되돌리는 것이다.

	   AddRevenue 로 맞추면 장부에 없던 수입이 기록돼 결산이 틀어진다.
	   그래서 값을 직접 넣는 경로를 따로 둔다. 저장 복원 말고는 쓰지 않는다.
	*/
	void RestoreFromSave(int32 InGold, int32 InTotalRevenue)
	{
		Gold = FMath::Max(0, InGold);
		TotalRevenue = FMath::Max(0, InTotalRevenue);

		// 오늘 수입은 하루가 시작될 때 0 이다. 저장은 정산 직후에만 하므로
		// 불러온 시점은 언제나 하루의 시작이다.
		TodayRevenue = 0;
	}

private:
	// 잔액 (Gold). 음수가 되지 않는다.
	int32 Gold = 0;
	
	// 누적 매출
	int32 TotalRevenue = 0;
	
	// 오늘 매출
	int32 TodayRevenue = 0;
	
};
