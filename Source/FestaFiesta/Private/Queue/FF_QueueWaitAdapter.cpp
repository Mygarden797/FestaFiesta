// Copyright Epic Games, Inc. All Rights Reserved.

#include "Queue/FF_QueueWaitAdapter.h"
#include "Queue/FF_QueueSubsystem.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h"

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
	//
	// bAcceptingNew 가 아니라 bIsOpen 을 본다. 전자는 '잠깐 만석'까지 false 로 만드는데,
	// 만석은 앞 Party 하나 빠지면 풀리는 순간 상태다. 그걸로 후보를 닫으면 인기 부스가
	// 결정할 때마다 사라졌다 나타났다 하고, 정작 "줄이 길어서 딴 데 간다"는 판단은
	// 점수에 나타나지 않는다. 만석은 위에서 계산한 대기시간이 커지는 것으로 이미 표현됐다.
	InOutCandidate.bIsOpen = InOutCandidate.bIsOpen && State.bIsOpen;
}

bool FFF_QueueWaitAdapter::ApplyToCandidate(
	const UFF_QueueSubsystem* QueueSubsystem,
	FFF_DestinationCandidate& InOutCandidate,
	int32 FacilityId,
	int32 IncomingMemberCount)
{
	if (!QueueSubsystem || FacilityId == INDEX_NONE)
	{
		return false;
	}

	if (!QueueSubsystem->IsFacilityRegistered(FacilityId))
	{
		// 시설 ID까지 나왔는데 Queue 에 없다 = 아직 스폰 안 됐거나 매핑이 낡았다.
		//
		// 여기서 그냥 넘어가면(fail-open) 후보가 "열려 있고 대기 0" 상태로 남는다.
		// 후보를 만드는 쪽은 EstimatedWaitMinutes 를 0 으로 두고 대기시간 소유권을 이 어댑터에
		// 넘겼기 때문에, 그 조합은 가능한 최고의 후보가 된다 — 존재하지 않는 시설이 1등이 된다.
		// 지금 "지금 들어갈 수 있는가"를 판정하는 곳이 여기 한 곳뿐이라 더더욱 열어두면 안 된다.
		// 그래서 fail-closed 한다: 이번 회차에서 빼고, 시끄럽게 남긴다.
		InOutCandidate.bIsOpen = false;
		InOutCandidate.EstimatedWaitMinutes = ToDecisionMinutes(FFF_QueueTimings::UnreachableWaitSeconds);
		UE_LOG(LogFestaFiestaQueue, Warning,
			TEXT("[대기열] 목적지 %s 가 Facility=%d 로 매핑되었지만 Queue에 미등록이다 — ")
			TEXT("이번 회차 후보에서 제외한다. 매핑을 확인할 것."),
			*InOutCandidate.DestinationId.ToString(), FacilityId);
		return false;
	}

	ApplyQueueStateToCandidate(InOutCandidate, QueueSubsystem->GetQueueState(FacilityId), IncomingMemberCount);
	return true;
}

int32 FFF_QueueWaitAdapter::ApplyToCandidates(
	const UFF_QueueSubsystem* QueueSubsystem,
	TArray<FFF_DestinationCandidate>& InOutCandidates,
	TFunctionRef<int32(FName)> ResolveFacilityId,
	int32 IncomingMemberCount)
{
	if (!QueueSubsystem)
	{
		return 0;
	}

	int32 AppliedCount = 0;
	for (FFF_DestinationCandidate& Candidate : InOutCandidates)
	{
		// INDEX_NONE = 줄이 없는 목적지(벤치·포토존 등). 정상 경로라 조용히 건너뛴다.
		const int32 FacilityId = ResolveFacilityId(Candidate.DestinationId);
		if (FacilityId == INDEX_NONE)
		{
			continue;
		}
		AppliedCount += ApplyToCandidate(QueueSubsystem, Candidate, FacilityId, IncomingMemberCount) ? 1 : 0;
	}

	UE_LOG(LogFestaFiestaQueue, Verbose, TEXT("[대기열] 후보 %d개 중 %d개에 대기열 상태를 반영했다 (일행 %d명)"),
		InOutCandidates.Num(), AppliedCount, IncomingMemberCount);
	return AppliedCount;
}

int32 FFF_QueueWaitAdapter::ApplyToCandidates(
	const UFF_QueueSubsystem* QueueSubsystem,
	TArray<FFF_DestinationCandidate>& InOutCandidates,
	const TMap<FName, int32>& DestinationIdToFacilityId,
	int32 IncomingMemberCount)
{
	// 표 버전은 함수 버전에 위임한다. 규칙이 두 벌로 갈라지면 언젠가 한쪽만 고쳐진다.
	return ApplyToCandidates(QueueSubsystem, InOutCandidates,
		[&DestinationIdToFacilityId](FName DestinationId)
		{
			const int32* Found = DestinationIdToFacilityId.Find(DestinationId);
			return Found ? *Found : INDEX_NONE;
		},
		IncomingMemberCount);
}
