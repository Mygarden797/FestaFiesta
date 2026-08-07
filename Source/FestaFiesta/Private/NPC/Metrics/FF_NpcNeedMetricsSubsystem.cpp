// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h" // LogFestaFiestaNpc

namespace
{
	// 퍼센트 비교 허용 오차. 이보다 작게 움직이면 "바뀌지 않았다"로 본다 —
	// 8만 명 규모에서 표본 1~2명 차이로 매 주기 브로드캐스트가 나가면 UI가 계속 다시 그린다.
	constexpr float RatioEpsilonPercent = 0.05f;

	bool IsValidNeedIndex(int32 Index)
	{
		return Index >= 0 && Index < FFF_NeedMetricsAggregator::NeedTypeCount;
	}
}

// --- FFF_NeedMetricsAggregator ---

void FFF_NeedMetricsAggregator::Begin()
{
	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		PendingUnmetCount[Index] = 0;
		PendingSum[Index] = 0.0;
	}
	PendingSampleCount = 0;
	bAggregating = true;
}

bool FFF_NeedMetricsAggregator::AddSample(const FFF_NeedState& Needs)
{
	if (!bAggregating)
	{
		return false;
	}

	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		const float Value = Needs.GetComponent(static_cast<EFF_NeedType>(Index));
		PendingSum[Index] += Value;
		if (Value >= UnmetThreshold)
		{
			++PendingUnmetCount[Index];
		}
	}
	++PendingSampleCount;
	return true;
}

void FFF_NeedMetricsAggregator::Commit(TArray<EFF_NeedType>& OutChangedNeeds)
{
	OutChangedNeeds.Reset();
	if (!bAggregating)
	{
		return;
	}
	bAggregating = false;

	CommittedSampleCount = PendingSampleCount;

	// 표본이 0이면 이전 확정값을 유지하지 않고 0으로 되돌린다 — 오래된 값을 현재값처럼
	// 보여주는 것이 0%보다 나쁘다. UI는 GetSampleCount()==0 으로 구분한다
	const float Divisor = PendingSampleCount > 0 ? static_cast<float>(PendingSampleCount) : 0.f;

	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		const float NewRatioPercent = Divisor > 0.f
			? (static_cast<float>(PendingUnmetCount[Index]) / Divisor) * 100.f
			: 0.f;

		CommittedAverage[Index] = Divisor > 0.f
			? static_cast<float>(PendingSum[Index] / Divisor)
			: 0.f;

		if (!FMath::IsNearlyEqual(CommittedUnmetRatioPercent[Index], NewRatioPercent, RatioEpsilonPercent))
		{
			CommittedUnmetRatioPercent[Index] = NewRatioPercent;
			OutChangedNeeds.Add(static_cast<EFF_NeedType>(Index));
		}
	}
}

float FFF_NeedMetricsAggregator::GetUnmetRatioPercent(EFF_NeedType NeedType) const
{
	const int32 Index = static_cast<int32>(NeedType);
	return IsValidNeedIndex(Index) ? CommittedUnmetRatioPercent[Index] : 0.f;
}

float FFF_NeedMetricsAggregator::GetAverage(EFF_NeedType NeedType) const
{
	const int32 Index = static_cast<int32>(NeedType);
	return IsValidNeedIndex(Index) ? CommittedAverage[Index] : 0.f;
}

// --- UFF_NpcNeedMetricsSubsystem ---

void UFF_NpcNeedMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogFestaFiestaNpc, Log,
		TEXT("UFF_NpcNeedMetricsSubsystem 초기화. 현재는 표본을 넣는 호출부가 없어 모든 미해소율이 0이다 ")
		TEXT("(NPC 런타임 층 미구현). UI는 GetSampleCount()==0 을 '데이터 없음'으로 처리할 것."));
}

void UFF_NpcNeedMetricsSubsystem::Deinitialize()
{
	OnNeedUnmetRatioChanged.Clear();
	Super::Deinitialize();
}

void UFF_NpcNeedMetricsSubsystem::AddSample(const FFF_NeedState& Needs)
{
	if (!Aggregator.AddSample(Needs))
	{
		// 조용히 버리지 않고 알린다 — 표본이 0인데 UI가 0%를 진짜 값으로 읽으면 원인을 찾기 어렵다
		UE_LOG(LogFestaFiestaNpc, Warning,
			TEXT("UFF_NpcNeedMetricsSubsystem::AddSample 이 BeginAggregation 없이 호출됐다 — 이 표본은 무시된다."));
	}
}

void UFF_NpcNeedMetricsSubsystem::CommitAggregation()
{
	TArray<EFF_NeedType> ChangedNeeds;
	Aggregator.Commit(ChangedNeeds);

	for (const EFF_NeedType NeedType : ChangedNeeds)
	{
		OnNeedUnmetRatioChanged.Broadcast(NeedType, Aggregator.GetUnmetRatioPercent(NeedType));
	}
}
