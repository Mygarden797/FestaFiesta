// Fill out your copyright notice in the Description page of Project Settings.


#include "CrowdSystem/Systems/FF_CrowdActorPool.h"

void FCrowdActorPoolBinding::Initialise(int32 InNumActorSlots, int32 InNumAgents)
{
	NumActorSlots = FMath::Max(InNumActorSlots, 0);
	const int32 NumAgents = FMath::Max(InNumAgents, 0);

	AgentToActorSlot.Init(INDEX_NONE, NumAgents);

	FreeActorSlots.Reset(NumActorSlots);
	// 뒤에서 꺼내 쓰므로(Pop) 역순으로 넣어 0번 슬롯부터 나가게 한다.
	// 동작에는 영향이 없지만 로그를 읽을 때 슬롯 번호가 순서대로 보여 추적하기 쉽다.
	for (int32 i = NumActorSlots - 1; i >= 0; --i)
	{
		FreeActorSlots.Add(i);
	}

	BoundCount = 0;
}

void FCrowdActorPoolBinding::ReleaseAll()
{
	Initialise(NumActorSlots, AgentToActorSlot.Num());
}

int32 FCrowdActorPoolBinding::Bind(int32 AgentIndex)
{
	if (!AgentToActorSlot.IsValidIndex(AgentIndex))
	{
		return INDEX_NONE;
	}

	if (AgentToActorSlot[AgentIndex] != INDEX_NONE)
	{
		// 이미 바인딩됨. 여기서 그냥 새 슬롯을 붙이면 기존 슬롯이 장부에서 사라져
		// 영원히 회수되지 않는 슬롯 누수가 된다.
		return INDEX_NONE;
	}

	if (FreeActorSlots.IsEmpty())
	{
		// 풀 고갈. 오류가 아니다 — 이 Agent는 화면에 안 보일 뿐 시뮬레이션은 계속된다.
		return INDEX_NONE;
	}

	const int32 ActorSlot = FreeActorSlots.Pop();
	AgentToActorSlot[AgentIndex] = ActorSlot;
	++BoundCount;

	return ActorSlot;
}

int32 FCrowdActorPoolBinding::Release(int32 AgentIndex)
{
	if (!AgentToActorSlot.IsValidIndex(AgentIndex))
	{
		return INDEX_NONE;
	}

	const int32 ActorSlot = AgentToActorSlot[AgentIndex];
	if (ActorSlot == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	AgentToActorSlot[AgentIndex] = INDEX_NONE;
	FreeActorSlots.Push(ActorSlot);
	--BoundCount;

	return ActorSlot;
}

int32 FCrowdActorPoolBinding::GetBoundSlot(int32 AgentIndex) const
{
	return AgentToActorSlot.IsValidIndex(AgentIndex) ? AgentToActorSlot[AgentIndex] : INDEX_NONE;
}
