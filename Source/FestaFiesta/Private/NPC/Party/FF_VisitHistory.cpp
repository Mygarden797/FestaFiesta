// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_VisitHistory.h"

#include "NPC/Destination/FF_UtilityDestinationSelector.h"

void FFF_VisitHistory::RecordVisit(const FName DestinationId, const float NowLiveMinutes)
{
	if (DestinationId.IsNone())
	{
		// no_choice 는 ChosenDestinationId 가 NAME_None 이다 — 그걸 "방문"으로 기록하면
		// 배회한 것이 이력 한 칸을 먹는다.
		return;
	}

	// 이미 있는 목적지면 새 칸을 쓰지 않고 시각만 갱신한다 —
	// 같은 곳을 반복해서 가는 개체가 이력 전체를 한 시설로 채우는 것을 막는다.
	const int32 ExistingIndex = DestinationIds.IndexOfByKey(DestinationId);
	if (ExistingIndex != INDEX_NONE)
	{
		VisitedAtLiveMinutes[ExistingIndex] = NowLiveMinutes;
		return;
	}

	if (DestinationIds.Num() < Capacity)
	{
		DestinationIds.Add(DestinationId);
		VisitedAtLiveMinutes.Add(NowLiveMinutes);
		NextWriteIndex = DestinationIds.Num() % Capacity;
		return;
	}

	// 가득 찼으면 링 인덱스 위치를 덮는다. 가장 오래된 칸을 찾아 정렬하지 않는 이유는
	// Capacity 가 작아도 8만 개체 × 매 방문마다 도는 비용이기 때문이다.
	const int32 WriteIndex = FMath::Clamp(NextWriteIndex, 0, Capacity - 1);
	DestinationIds[WriteIndex] = DestinationId;
	VisitedAtLiveMinutes[WriteIndex] = NowLiveMinutes;
	NextWriteIndex = (WriteIndex + 1) % Capacity;
}

float FFF_VisitHistory::GetMinutesSinceVisit(const FName DestinationId, const float NowLiveMinutes) const
{
	if (DestinationId.IsNone())
	{
		return -1.f;
	}

	const int32 Index = DestinationIds.IndexOfByKey(DestinationId);
	if (Index == INDEX_NONE || !VisitedAtLiveMinutes.IsValidIndex(Index))
	{
		// 안 가본 곳. 0 을 돌려주면 "방금 다녀온 곳"과 구분되지 않는다.
		return -1.f;
	}

	// 시간이 거꾸로 가는 것은 호출부 버그다. 음수를 돌려주면 "안 가본 곳"으로 읽히므로 0 으로 막는다.
	return FMath::Max(NowLiveMinutes - VisitedAtLiveMinutes[Index], 0.f);
}

int32 FFF_VisitHistory::ApplyToCandidates(
	TArray<FFF_DestinationCandidate>& InOutCandidates,
	const float NowLiveMinutes) const
{
	if (DestinationIds.IsEmpty())
	{
		return 0;
	}

	int32 AppliedCount = 0;
	for (FFF_DestinationCandidate& Candidate : InOutCandidates)
	{
		const float MinutesSince = GetMinutesSinceVisit(Candidate.DestinationId, NowLiveMinutes);
		if (MinutesSince < 0.f)
		{
			// 안 가본 곳은 건드리지 않는다 — 후보 기본값이 이미 "안 가봤다"다.
			continue;
		}
		Candidate.MinutesSinceLastVisit = MinutesSince;
		++AppliedCount;
	}
	return AppliedCount;
}
