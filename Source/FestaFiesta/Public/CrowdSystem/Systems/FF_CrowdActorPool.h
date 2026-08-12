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
	void Initialise(int32 InNumActorSlots, int32 InNumAgents);

	void ReleaseAll();

	int32 Bind(int32 AgentIndex);

	int32 Release(int32 AgentIndex);

	int32 GetBoundSlot(int32 AgentIndex) const;

	bool IsBound(int32 AgentIndex) const { return GetBoundSlot(AgentIndex) != INDEX_NONE; }

	int32 GetFreeSlotCount() const { return FreeActorSlots.Num(); }
	int32 GetBoundCount() const { return BoundCount; }
	int32 GetNumActorSlots() const { return NumActorSlots; }
	int32 GetNumAgents() const { return AgentToActorSlot.Num(); }

private:
	TArray<int32> FreeActorSlots;

	TArray<int32> AgentToActorSlot;

	int32 NumActorSlots = 0;

	int32 BoundCount = 0;
};
