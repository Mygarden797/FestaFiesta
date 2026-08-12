// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Agent <-> Actor 슬롯 바인딩 장부.
 *
 * "몇 번 Agent가 몇 번 Actor 슬롯을 쓰고 있는가"만 관리한다. Actor 자체는 모른다 —
 * 숨김 처리나 위치 갱신 같은 실제 조작은 이 장부를 쓰는 UFF_CrowdSimulationSubsystem이 한다.
 *
 * 떼어낸 이유는 이 부분이 버그가 나기 쉬운데 눈으로는 확인이 어렵기 때문이다.
 * 이중 바인딩, 해제 안 된 슬롯 누수, 풀 고갈 처리 — 전부 인덱스 장부의 문제이고
 * UWorld 없이 자동화 테스트로 덮을 수 있다.
 *
 * 불변식: 언제나 (빈 슬롯 수 + 바인딩된 슬롯 수) == 전체 슬롯 수.
 */
struct FESTAFIESTA_API FCrowdActorPoolBinding
{
	/** 장부를 만든다. 모든 Actor 슬롯이 비어 있고 모든 Agent가 미바인딩 상태로 시작한다. */
	void Initialise(int32 InNumActorSlots, int32 InNumAgents);

	/** 전부 미바인딩 상태로 되돌린다. 크기는 유지한다. */
	void ReleaseAll();

	/**
	 * AgentIndex에 빈 Actor 슬롯을 하나 붙인다.
	 *
	 * @return 붙은 Actor 슬롯 인덱스. 이미 바인딩됐거나, 풀이 고갈됐거나,
	 *         AgentIndex가 범위 밖이면 INDEX_NONE.
	 *         풀 고갈은 오류가 아니다 — 화면에 안 보일 뿐 시뮬레이션은 계속된다.
	 */
	int32 Bind(int32 AgentIndex);

	/**
	 * AgentIndex의 Actor 슬롯을 풀로 되돌린다.
	 *
	 * @return 반환된 Actor 슬롯 인덱스. 바인딩돼 있지 않았으면 INDEX_NONE.
	 */
	int32 Release(int32 AgentIndex);

	/** AgentIndex에 붙은 Actor 슬롯. 없으면 INDEX_NONE. */
	int32 GetBoundSlot(int32 AgentIndex) const;

	bool IsBound(int32 AgentIndex) const { return GetBoundSlot(AgentIndex) != INDEX_NONE; }

	int32 GetFreeSlotCount() const { return FreeActorSlots.Num(); }
	int32 GetBoundCount() const { return BoundCount; }
	int32 GetNumActorSlots() const { return NumActorSlots; }
	int32 GetNumAgents() const { return AgentToActorSlot.Num(); }

private:
	/** 현재 비어 있는 Actor 슬롯 인덱스 목록. 스택처럼 뒤에서 꺼내 쓴다. */
	TArray<int32> FreeActorSlots;

	/** AgentIndex -> Actor 슬롯. 미바인딩은 INDEX_NONE. */
	TArray<int32> AgentToActorSlot;

	int32 NumActorSlots = 0;

	/** 현재 바인딩된 수. AgentToActorSlot을 매번 세지 않기 위해 증감으로 관리한다. */
	int32 BoundCount = 0;
};
