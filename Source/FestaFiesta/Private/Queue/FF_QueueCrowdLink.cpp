// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_QueueCrowdLink.h"

#include "CrowdSystem/Systems/FF_CrowdSimulationSubsystem.h"
#include "CrowdSystem/Types/FF_FestivalCrowdTypes.h"
#include "Engine/World.h"
#include "Queue/FF_QueueSubsystem.h"
#include "Zone/FF_ZoneSystemQueueBridgeSubsystem.h"

bool UFF_QueueCrowdLink::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UFF_QueueCrowdLink::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Initialize 가 아니라 OnWorldBeginPlay 에서 묶는다.
	// 서브시스템 생성 순서는 보장되지 않아서 Initialize 시점엔 Queue 가 아직 없을 수 있다.
	BindToQueue();
}

void UFF_QueueCrowdLink::Deinitialize()
{
	UnbindFromQueue();
	Super::Deinitialize();
}

UFF_QueueSubsystem* UFF_QueueCrowdLink::GetQueue() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_QueueSubsystem>() : nullptr;
}

UFF_CrowdSimulationSubsystem* UFF_QueueCrowdLink::GetCrowd() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_CrowdSimulationSubsystem>() : nullptr;
}

UFF_ZoneSystemQueueBridgeSubsystem* UFF_QueueCrowdLink::GetBridge() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UFF_ZoneSystemQueueBridgeSubsystem>() : nullptr;
}

void UFF_QueueCrowdLink::BindToQueue()
{
	if (bBound)
	{
		return;
	}

	UFF_QueueSubsystem* Queue = GetQueue();
	if (!Queue)
	{
		return;
	}

	Queue->OnSlotTargetsUpdated.AddDynamic(this, &UFF_QueueCrowdLink::HandleSlotTargetsUpdated);
	Queue->OnPartyRejected.AddDynamic(this, &UFF_QueueCrowdLink::HandlePartyRejected);
	Queue->OnPartyExited.AddDynamic(this, &UFF_QueueCrowdLink::HandlePartyExited);
	bBound = true;
}

void UFF_QueueCrowdLink::UnbindFromQueue()
{
	if (!bBound)
	{
		return;
	}

	if (UFF_QueueSubsystem* Queue = GetQueue())
	{
		Queue->OnSlotTargetsUpdated.RemoveDynamic(this, &UFF_QueueCrowdLink::HandleSlotTargetsUpdated);
		Queue->OnPartyRejected.RemoveDynamic(this, &UFF_QueueCrowdLink::HandlePartyRejected);
		Queue->OnPartyExited.RemoveDynamic(this, &UFF_QueueCrowdLink::HandlePartyExited);
	}
	bBound = false;
}

void UFF_QueueCrowdLink::SetLinkEnabled(bool bEnabled)
{
	bLinkEnabled = bEnabled;
}

void UFF_QueueCrowdLink::ResetStats()
{
	AppliedSlotTargets = 0;
	MemberCountMismatches = 0;
	DecisionRequests = 0;
}

void UFF_QueueCrowdLink::HandleSlotTargetsUpdated(int32 FacilityId, const TArray<FFF_SlotAssignment>& Assignments)
{
	if (!bLinkEnabled)
	{
		return;
	}

	UFF_CrowdSimulationSubsystem* Crowd = GetCrowd();
	if (!Crowd)
	{
		return;
	}

	// 배정은 Party 별로 연속이다. Party 가 바뀌는 지점에서 끊어 한 번에 처리한다.
	int32 Cursor = 0;
	while (Cursor < Assignments.Num())
	{
		const int32 PartyId = Assignments[Cursor].PartyId;

		int32 End = Cursor;
		while (End < Assignments.Num() && Assignments[End].PartyId == PartyId)
		{
			++End;
		}

		// 인덱스는 캐시하지 않고 매번 새로 받는다. 슬롯이 재사용되므로 저장하면
		// 시간이 지난 뒤 다른 사람을 가리킨다.
		const TArray<int32> Members = Crowd->GetPartyMembers(PartyId);

		const int32 SlotCount = End - Cursor;
		if (Members.Num() != SlotCount)
		{
			// 가입 시점 인원과 현재 살아 있는 멤버 수가 다르다. 일부가 디스폰됐거나
			// 가입 요청의 MemberCount 가 실제와 달랐다는 뜻이다.
			// 있는 만큼만 적용한다 — 아무것도 안 하면 남은 사람들이 제자리에 얼어붙는다.
			++MemberCountMismatches;
		}

		const int32 ApplyCount = FMath::Min(Members.Num(), SlotCount);
		for (int32 Offset = 0; Offset < ApplyCount; ++Offset)
		{
			Crowd->SetAgentDestination(Members[Offset], Assignments[Cursor + Offset].TargetLocation);
			++AppliedSlotTargets;
		}

		Cursor = End;
	}
}

void UFF_QueueCrowdLink::HandlePartyRejected(int32 FacilityId, int32 PartyId, EFF_QueueJoinResult Reason)
{
	if (!bLinkEnabled)
	{
		return;
	}

	// 도착했는데 만석이거나 폐쇄였다. 그대로 두면 Party 가 입구 앞에 서서 아무것도 안 한다.
	UFF_CrowdSimulationSubsystem* Crowd = GetCrowd();
	if (!Crowd)
	{
		return;
	}

	const TArray<int32> Members = Crowd->GetPartyMembers(PartyId);
	if (Members.Num() == 0)
	{
		return;
	}

	Crowd->RequestDecision(Crowd->GetPartyLeader(Members[0]), EFF_DecisionReason::JoinRejected);
	++DecisionRequests;
}

void UFF_QueueCrowdLink::HandlePartyExited(int32 FacilityId, int32 PartyId, EFF_QueueExit ExitType, float WaitedSeconds)
{
	if (!bLinkEnabled)
	{
		return;
	}

	UFF_CrowdSimulationSubsystem* Crowd = GetCrowd();
	if (!Crowd)
	{
		return;
	}

	const TArray<int32> Members = Crowd->GetPartyMembers(PartyId);
	if (Members.Num() == 0)
	{
		return;
	}

	// 서비스를 마쳤으면 퇴장 지점으로 보낸다. 퇴장 경로는 대기열 입구와 분리돼 있어서
	// 나가는 사람과 들어오는 사람이 서로를 막지 않는다.
	//
	// 다음 목적지 결정은 여기서 걸지 않는다. Exit 에 물리적으로 도착한 뒤에 걸어야
	// "나가면서 이미 다음 곳으로 방향을 틀어버리는" 그림이 안 나온다.
	// 도착 판정은 Crowd 가 소유하므로 그쪽 Arrived 신호에서 이어받는다.
	if (ExitType == EFF_QueueExit::Served)
	{
		if (const UFF_ZoneSystemQueueBridgeSubsystem* Bridge = GetBridge())
		{
			const FGuid ZoneFacilityId = Bridge->FindZoneFacilityId(FacilityId);
			if (ZoneFacilityId.IsValid())
			{
				if (const UFF_QueueSubsystem* Queue = GetQueue())
				{
					const FVector ExitLocation = Queue->GetFacilityExitLocation(FacilityId);
					if (!ExitLocation.IsNearlyZero())
					{
						for (const int32 AgentIndex : Members)
						{
							Crowd->SetAgentDestination(AgentIndex, ExitLocation);
							++AppliedSlotTargets;
						}
						return;
					}
				}
			}
		}
		// 퇴장 지점을 못 구했으면 그 자리에 세워두지 말고 재평가시킨다.
	}

	// 포기·취소는 보상 없이 즉시 재평가한다. 폐쇄로 취소된 경우는 시설이 사라진 것이므로
	// 그 자리에서 다시 고르게 해야 한다.
	const EFF_DecisionReason Reason = (ExitType == EFF_QueueExit::Cancelled)
		? EFF_DecisionReason::Interrupted
		: EFF_DecisionReason::Arrived;

	Crowd->RequestDecision(Crowd->GetPartyLeader(Members[0]), Reason);
	++DecisionRequests;
}
