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
	
	TMap<int32, TArray<FVector>> ColumnsByParty;
	TMap<int32, float> LaneYawByParty;

	// 하나의 팀 자리 안에 배치된 멤버별 위치를 Crowd의 queue column으로 묶는다.
	// 멤버가 늘어도 저작된 다음 팀 자리를 소비하지 않는다.
	for (const FFF_SlotAssignment& Assignment : Assignments)
	{
		TArray<FVector>& Column = ColumnsByParty.FindOrAdd(Assignment.PartyId);
		if (Column.Num() <= Assignment.PartyMemberIndex)
		{
			Column.SetNum(Assignment.PartyMemberIndex + 1);
		}
		
		Column[Assignment.PartyMemberIndex] = Assignment.TargetLocation;
		if (Assignment.PartyMemberIndex == 0)
		{
			LaneYawByParty.Add(Assignment.PartyId, Assignment.TargetYaw);
		}
	}	
		
	for (const TPair<int32, TArray<FVector>>& It : ColumnsByParty)
	{
		if (!Crowd->SetPartyQueueColumn(It.Key, It.Value,LaneYawByParty.FindRef(It.Key)))
		{
			++MemberCountMismatches;
		}
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
	
	Crowd->ClearPartyQueueColumn(PartyId);

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

	Crowd->ClearPartyQueueColumn(PartyId);
	// 서비스를 마쳤으면 퇴장 지점으로 보낸다. 퇴장 경로는 대기열 입구와 분리돼 있어서
	// 나가는 사람과 들어오는 사람이 서로를 막지 않는다.
	//
	// 다음 목적지 결정은 여기서 걸지 않는다. Exit 에 물리적으로 도착한 뒤에 걸어야
	// "나가면서 이미 다음 곳으로 방향을 틀어버리는" 그림이 안 나온다.
	// 도착 판정은 Crowd 가 소유하므로 그쪽 Arrived 신호에서 이어받는다.
	if (ExitType == EFF_QueueExit::Served)
	{
		Crowd->ApplyPartyNeedRelief(PartyId);
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
						// 리더에게만 준다. 멤버는 포메이션을 따라 함께 빠져나간다.
						// Members[0] 이 리더라는 보장은 없다. 아래 재평가와 같은 방식으로 리더를 찾는다.
						Crowd->SetAgentDestination(Crowd->GetPartyLeader(Members[0]), ExitLocation);
						++AppliedSlotTargets;
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
