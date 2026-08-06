// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_QueueWaitAdapter.h"
#include "Queue/FF_QueueSubsystem.h"
#include "NPC/FF_UtilityDestinationSelector.h"

float FFF_QueueWaitAdapter::ComputeExpectedWaitSeconds(const FFF_QueueState& State, int32 IncomingMemberCount)
{
	if (State.ServiceRate <= 0.f)
	{
		// 처리율 0 = 영원히 안 빠지는 줄. 0을 돌려주면 "대기 없음"으로 읽혀서
		// 모든 Party가 이 시설로 몰린다. 반대 방향으로 틀리게 둔다.
		// float::Max 대신 유한한 24시간을 쓴다 — Queue 코어와 같은 센티널이다.
		return FFF_QueueTimings::UnreachableWaitSeconds;
	}
	const int32 Ahead = FMath::Max(0, State.RemainingServiceMemberCount);
	return static_cast<float>(Ahead + FMath::Max(0, IncomingMemberCount)) / State.ServiceRate;
}

void FFF_QueueWaitAdapter::ApplyQueueStateToCandidate(
	FFF_DestinationCandidate& InOutCandidate,
	const FFF_QueueState& State,
	int32 IncomingMemberCount)
{
	const float WaitSeconds = ComputeExpectedWaitSeconds(State, IncomingMemberCount);
	InOutCandidate.EstimatedWaitMinutes = ToDecisionMinutes(WaitSeconds);

	// AND 병합. 대입이 아니다 — 호출부가 이미 닫아둔 시설을 열어주면 안 된다.
	InOutCandidate.bIsOpen = InOutCandidate.bIsOpen && State.bAcceptingNew;
}

int32 FFF_QueueWaitAdapter::ApplyToCandidates(
	const UFF_QueueSubsystem* QueueSubsystem,
	TArray<FFF_DestinationCandidate>& InOutCandidates,
	const TMap<FName, int32>& DestinationIdToFacilityId,
	int32 IncomingMemberCount)
{
	if (!QueueSubsystem)
	{
		return 0;
	}

	int32 AppliedCount = 0;
	for (FFF_DestinationCandidate& Candidate : InOutCandidates)
	{
		const int32* FacilityIdPtr = DestinationIdToFacilityId.Find(Candidate.DestinationId);
		if (!FacilityIdPtr)
		{
			// 목적지-시설 매핑에 없는 후보. 줄이 없는 목적지일 수도 있으므로 조용히 건너뛴다.
			continue;
		}
		if (!QueueSubsystem->IsFacilityRegistered(*FacilityIdPtr))
		{
			// 매핑에는 있는데 Queue에 미등록이다 = 아직 스폰 안 됐거나 매핑표가 낡았다.
			// 값을 덮지는 않는다(호출부가 다른 근거로 채웠을 수 있다) — 대신 반드시 시끄럽게 남긴다.
			// 조용히 넘어가면 그 시설이 "대기 0인 최고의 후보"로 남아 원인을 찾기 어려워진다.
			UE_LOG(LogFestaFiestaQueue, Warning,
				TEXT("[대기열] 목적지 %s 가 Facility=%d 로 매핑되었지만 Queue에 미등록이다 — 매핑표를 확인할 것."),
				*Candidate.DestinationId.ToString(), *FacilityIdPtr);
			continue;
		}

		ApplyQueueStateToCandidate(Candidate, QueueSubsystem->GetQueueState(*FacilityIdPtr), IncomingMemberCount);
		++AppliedCount;
	}

	UE_LOG(LogFestaFiestaQueue, Verbose, TEXT("[대기열] 후보 %d개 중 %d개에 대기열 상태를 반영했다 (일행 %d명)"),
		InOutCandidates.Num(), AppliedCount, IncomingMemberCount);
	return AppliedCount;
}
