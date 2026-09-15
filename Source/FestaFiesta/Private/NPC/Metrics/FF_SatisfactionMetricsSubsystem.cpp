// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h" // LogFestaFiestaNpc

FFF_SatisfactionCoefficients FFF_SatisfactionCoefficients::LoadFromTable(const UDataTable* UtilityCoefficientTable)
{
	FFF_SatisfactionCoefficients Coefficients;

	Coefficients.WeightWait = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_wait"), Coefficients.WeightWait);
	Coefficients.WeightDensity = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_density"), Coefficients.WeightDensity);
	Coefficients.WeightPreference = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_preference"), Coefficients.WeightPreference);

	Coefficients.WaitRefSeconds = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_wait_ref_seconds"), Coefficients.WaitRefSeconds);
	Coefficients.PreferenceTermRef = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_preference_term_ref"), Coefficients.PreferenceTermRef);

	static const TCHAR* LOSGradeCoefficientIds[6] =
	{
		TEXT("satisfaction_los_a_score"), TEXT("satisfaction_los_b_score"), TEXT("satisfaction_los_c_score"),
		TEXT("satisfaction_los_d_score"), TEXT("satisfaction_los_e_score"), TEXT("satisfaction_los_f_score"),
	};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Coefficients.LOSGradeScore[Index] = FFF_UtilityCoefficientHelpers::GetCoefficient(
			UtilityCoefficientTable, LOSGradeCoefficientIds[Index], Coefficients.LOSGradeScore[Index]);
	}

	return Coefficients;
}

// --- FFF_SatisfactionAggregator ---

void FFF_SatisfactionAggregator::Begin()
{
	PendingWaitSum = 0.0;
	PendingWaitCount = 0;
	PendingDensityWeightedScoreSum = 0.0;
	PendingDensityWeight = 0;
	PendingPreferenceSum = 0.0;
	PendingPreferenceCount = 0;
	bAggregating = true;
}

bool FFF_SatisfactionAggregator::AddWaitSample(float CompletedWaitSeconds)
{
	if (!bAggregating)
	{
		return false;
	}
	PendingWaitSum += CompletedWaitSeconds;
	++PendingWaitCount;
	return true;
}

bool FFF_SatisfactionAggregator::AddDensitySample(float Score0To100, int32 Weight)
{
	if (!bAggregating || Weight <= 0)
	{
		return false;
	}
	PendingDensityWeightedScoreSum += static_cast<double>(Score0To100) * Weight;
	PendingDensityWeight += Weight;
	return true;
}

bool FFF_SatisfactionAggregator::AddPreferenceSample(float PreferenceNorm)
{
	if (!bAggregating)
	{
		return false;
	}
	PendingPreferenceSum += PreferenceNorm;
	++PendingPreferenceCount;
	return true;
}

bool FFF_SatisfactionAggregator::Commit(const FFF_SatisfactionCoefficients& Coefficients)
{
	if (!bAggregating)
	{
		return false;
	}
	bAggregating = false;

	// 표본이 0인 입력은 이번 주기 블렌드에서 뺀다(가중치를 0으로 취급) — 세 값을
	// 전부 0으로 되돌리면 "데이터 없음"이 "최악"으로 보인다. 셋 다 비면 직전 확정값을
	// 그대로 유지한다(헤더 주석 "표본 0일 때" 참조)
	const bool bHasWait = PendingWaitCount > 0;
	const bool bHasDensity = PendingDensityWeight > 0;
	const bool bHasPreference = PendingPreferenceCount > 0;

	if (!bHasWait && !bHasDensity && !bHasPreference)
	{
		CommittedSampleCount = 0;
		return false;
	}

	if (bHasWait)
	{
		const float AvgWaitSeconds = static_cast<float>(PendingWaitSum / PendingWaitCount);
		const float WaitRef = FMath::Max(Coefficients.WaitRefSeconds, 1.f);
		CommittedWaitScore = FMath::Clamp(FMath::RoundToInt(100.f - (AvgWaitSeconds / WaitRef) * 100.f), 0, 100);
	}

	if (bHasDensity)
	{
		const float AvgDensityScore = static_cast<float>(PendingDensityWeightedScoreSum / PendingDensityWeight);
		CommittedDensityScore = FMath::Clamp(FMath::RoundToInt(AvgDensityScore), 0, 100);
	}

	if (bHasPreference)
	{
		const float AvgPreferenceNorm = static_cast<float>(PendingPreferenceSum / PendingPreferenceCount);
		CommittedPreferenceScore = FMath::Clamp(FMath::RoundToInt((AvgPreferenceNorm + 1.f) * 50.f), 0, 100);
	}

	// 이번 주기에 실제로 들어온 입력만 가중합에 넣고, 그 입력들의 가중치로 다시
	// 정규화한다 — 밀도가 아직 하나도 안 들어온 초반 몇 주기에도 대기·취향 둘만으로
	// 그럴듯한 값을 낸다. CSV 가중치 합이 1이 아니어도(오기입) 여기서 항상 정규화된다
	double WeightSum = 0.0;
	double ScoreSum = 0.0;
	if (bHasWait)
	{
		WeightSum += Coefficients.WeightWait;
		ScoreSum += static_cast<double>(Coefficients.WeightWait) * CommittedWaitScore;
	}
	if (bHasDensity)
	{
		WeightSum += Coefficients.WeightDensity;
		ScoreSum += static_cast<double>(Coefficients.WeightDensity) * CommittedDensityScore;
	}
	if (bHasPreference)
	{
		WeightSum += Coefficients.WeightPreference;
		ScoreSum += static_cast<double>(Coefficients.WeightPreference) * CommittedPreferenceScore;
	}

	const int32 NewSatisfaction = WeightSum > 0.0
		? FMath::Clamp(FMath::RoundToInt(static_cast<float>(ScoreSum / WeightSum)), 0, 100)
		: CommittedSatisfaction;

	// 표본 수는 "가장 적게 들어온 입력"으로 잡는다 — 하나라도 비면 부분 데이터라는
	// 사실이 대표값에 드러나야 UI가 "이번 주기는 일부만 반영됐다"를 알 수 있다
	int32 MinSampleCount = TNumericLimits<int32>::Max();
	if (bHasWait) MinSampleCount = FMath::Min(MinSampleCount, PendingWaitCount);
	if (bHasDensity) MinSampleCount = FMath::Min(MinSampleCount, static_cast<int32>(FMath::Min<int64>(PendingDensityWeight, TNumericLimits<int32>::Max())));
	if (bHasPreference) MinSampleCount = FMath::Min(MinSampleCount, PendingPreferenceCount);
	CommittedSampleCount = MinSampleCount;

	const bool bChanged = NewSatisfaction != CommittedSatisfaction;
	CommittedSatisfaction = NewSatisfaction;
	return bChanged;
}

// --- UFF_SatisfactionMetricsSubsystem ---

void UFF_SatisfactionMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogFestaFiestaNpc, Log,
		TEXT("UFF_SatisfactionMetricsSubsystem 초기화. 첫 확정 전까지는 중립값 50을 돌려준다 ")
		TEXT("(GetSampleCount()==0으로 '아직 안 걸림'을 구분할 것)."));
}

void UFF_SatisfactionMetricsSubsystem::Deinitialize()
{
	OnSimSatisfactionChanged.Clear();
	Super::Deinitialize();
}

void UFF_SatisfactionMetricsSubsystem::CommitAggregation(const FFF_SatisfactionCoefficients& Coefficients)
{
	if (Aggregator.Commit(Coefficients))
	{
		OnSimSatisfactionChanged.Broadcast(Aggregator.GetSimSatisfaction());
	}
}
