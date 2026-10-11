// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/Destination/FF_UtilityDestinationSelector.h" // LogFestaFiestaNpc
#include "Time/FF_FestivalStageSubsystem.h"
#include "Engine/GameInstance.h"

FFF_SatisfactionCoefficients FFF_SatisfactionCoefficients::LoadFromTable(const UDataTable* UtilityCoefficientTable)
{
	FFF_SatisfactionCoefficients Coefficients;

	Coefficients.WeightEnvironment = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_environment"), Coefficients.WeightEnvironment);
	Coefficients.WeightPersonalFit = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_personal_fit"), Coefficients.WeightPersonalFit);

	Coefficients.WeightEnvironmentWait = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_environment_wait"), Coefficients.WeightEnvironmentWait);
	Coefficients.WeightEnvironmentDensity = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_environment_density"), Coefficients.WeightEnvironmentDensity);

	Coefficients.WeightPersonalFitPreference = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_personal_fit_preference"), Coefficients.WeightPersonalFitPreference);
	Coefficients.WeightPersonalFitNeed = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_weight_personal_fit_need"), Coefficients.WeightPersonalFitNeed);

	Coefficients.WaitRefSeconds = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_wait_ref_seconds"), Coefficients.WaitRefSeconds);
	Coefficients.PreferenceTermRef = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("satisfaction_preference_term_ref"), Coefficients.PreferenceTermRef);

	Coefficients.NeedCoverageTargetHunger = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_coverage_target_hunger"), Coefficients.NeedCoverageTargetHunger);
	Coefficients.NeedCoverageTargetRestroom = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_coverage_target_restroom"), Coefficients.NeedCoverageTargetRestroom);
	Coefficients.NeedCoverageTargetStimulation = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_coverage_target_stimulation"), Coefficients.NeedCoverageTargetStimulation);
	Coefficients.NeedCoverageFloor = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_coverage_floor"), Coefficients.NeedCoverageFloor);

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
	PendingQueueSuccessRate = 1.f;
	bHasQueueSuccessRateThisWindow = false;
	PendingDensityWeightedScoreSum = 0.0;
	PendingDensityWeight = 0;
	PendingCrowdedWeight = 0;
	// 지난 창 이후에 확정된 결정의 취향 표본을 이번 창이 이어받는다. 한 번만 쓰이도록 비운다.
	PendingPreferenceSum = CarriedPreferenceSum;
	PendingPreferenceCount = CarriedPreferenceCount;
	PendingPreferenceMatchedCount = CarriedPreferenceMatchedCount;
	CarriedPreferenceSum = 0.0;
	CarriedPreferenceCount = 0;
	CarriedPreferenceMatchedCount = 0;
	PendingNeedFulfillmentSum = 0.0;
	PendingNeedFulfillmentCount = 0;
	PendingPenaltySum = 0.0;
	PendingPenaltyCount = 0;
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

bool FFF_SatisfactionAggregator::SetQueueSuccessRate(float SuccessRate01)
{
	if (!bAggregating)
	{
		return false;
	}
	PendingQueueSuccessRate = FMath::Clamp(SuccessRate01, 0.f, 1.f);
	bHasQueueSuccessRateThisWindow = true;
	return true;
}

bool FFF_SatisfactionAggregator::AddDensitySample(float Score0To100, int32 Weight, bool bCrowded)
{
	if (!bAggregating || Weight <= 0)
	{
		return false;
	}
	PendingDensityWeightedScoreSum += static_cast<double>(Score0To100) * Weight;
	PendingDensityWeight += Weight;
	PendingCrowdedWeight += bCrowded ? Weight : 0;
	return true;
}

bool FFF_SatisfactionAggregator::AddPreferenceSample(float PreferenceNorm)
{
	// 결정은 집계 창(Begin~Commit) 밖에서 확정된다. 집계는 AdvanceNeeds 한 번의 호출 안에서
	// 끝나고 결정 처리는 그다음에 도는데, 그때 거부하면 취향 표본이 하나도 못 들어온다.
	// 그래서 창 밖의 표본은 따로 모아 두었다가 다음 Begin 이 이어받는다.
	// 취향 항이 0 보다 크면 그 파티의 취향에 맞는 곳을 고른 것이다
	const int32 Matched = PreferenceNorm > 0.f ? 1 : 0;
	if (bAggregating)
	{
		PendingPreferenceSum += PreferenceNorm;
		++PendingPreferenceCount;
		PendingPreferenceMatchedCount += Matched;
	}
	else
	{
		CarriedPreferenceSum += PreferenceNorm;
		++CarriedPreferenceCount;
		CarriedPreferenceMatchedCount += Matched;
	}
	return true;
}

bool FFF_SatisfactionAggregator::AddNeedFulfillmentSample(float FulfillmentScore0To100)
{
	if (!bAggregating)
	{
		return false;
	}
	PendingNeedFulfillmentSum += FulfillmentScore0To100;
	++PendingNeedFulfillmentCount;
	return true;
}

bool FFF_SatisfactionAggregator::AddPenaltySample(float PenaltyPoints)
{
	if (!bAggregating)
	{
		return false;
	}
	PendingPenaltySum += FMath::Clamp(PenaltyPoints, 0.f, 100.f);
	++PendingPenaltyCount;
	return true;
}

namespace
{
	// 두 항 중 있는 것만 가중합하고 그 가중치로 정규화한다. 하나만 있어도 그럴듯한 값을
	// 내고, 둘 다 없으면 bOutHasAny=false로 알려서 상위 축이 이 축을 아예 건너뛰게 한다.
	int32 BlendTwo(bool bHasA, float WeightA, int32 ScoreA, bool bHasB, float WeightB, int32 ScoreB,
		bool& bOutHasAny)
	{
		double WeightSum = 0.0;
		double ScoreSum = 0.0;
		if (bHasA)
		{
			WeightSum += WeightA;
			ScoreSum += static_cast<double>(WeightA) * ScoreA;
		}
		if (bHasB)
		{
			WeightSum += WeightB;
			ScoreSum += static_cast<double>(WeightB) * ScoreB;
		}
		bOutHasAny = WeightSum > 0.0;
		return bOutHasAny ? FMath::Clamp(FMath::RoundToInt(static_cast<float>(ScoreSum / WeightSum)), 0, 100) : 0;
	}
}

bool FFF_SatisfactionAggregator::Commit(const FFF_SatisfactionCoefficients& Coefficients)
{
	if (!bAggregating)
	{
		return false;
	}
	bAggregating = false;

	// 감점을 빼기 전 점수에서 이번 주기의 평균 감점을 빼 최종 점수를 정한다.
	// 방문객 전체 평균이라, 일부 구역만 깎여도 그 구역의 인원 비율만큼만 전체 점수가 내려간다.
	// 표본이 없으면(활성 방문객 0) 감점도 없다. 값이 바뀌었으면 true.
	const auto ApplyPenalty = [this]() -> bool
	{
		CommittedPenaltyPoints = PendingPenaltyCount > 0
			? FMath::Clamp(FMath::RoundToInt(static_cast<float>(PendingPenaltySum / PendingPenaltyCount)), 0, 100)
			: 0;
		const int32 Resolved = FMath::Clamp(CommittedBaseSatisfaction - CommittedPenaltyPoints, 0, 100);
		const bool bChangedByPenalty = Resolved != CommittedSatisfaction;
		CommittedSatisfaction = Resolved;
		return bChangedByPenalty;
	};

	const bool bHasWaitSamples = PendingWaitCount > 0;
	const bool bHasSuccessRate = bHasQueueSuccessRateThisWindow;
	const bool bHasWaitInput = bHasWaitSamples || bHasSuccessRate;
	const bool bHasDensity = PendingDensityWeight > 0;
	const bool bHasPreference = PendingPreferenceCount > 0;
	const bool bHasNeed = PendingNeedFulfillmentCount > 0;

	// 표본이 하나도 없는 입력은 이번 주기 블렌드에서 뺀다(가중치를 0으로 취급) — 값을
	// 전부 0으로 되돌리면 "데이터 없음"이 "최악"으로 보인다. 넷 다 비면 직전 확정값을
	// 그대로 유지한다(헤더 주석 "표본 0일 때" 참조)
	// 단 감점은 이 경우에도 다시 반영한다 — 회복이 재료 없는 창에서 멈추면 안 된다.
	if (!bHasWaitInput && !bHasDensity && !bHasPreference && !bHasNeed)
	{
		CommittedSampleCount = 0;
		return ApplyPenalty();
	}

	if (bHasWaitInput)
	{
		// 완료된 방문이 이번 창에 하나도 없어도(전원 거절·이탈) 낙관적 기본값(100점)에서
		// 시작해 성공률만으로 깎는다 — 안 그러면 전원 거절당한 창이 대기 항목에서 통째로 빠진다
		const float RawWaitScore = bHasWaitSamples
			? FMath::Clamp(100.f - (static_cast<float>(PendingWaitSum / PendingWaitCount)
				/ FMath::Max(Coefficients.WaitRefSeconds, 1.f)) * 100.f, 0.f, 100.f)
			: 100.f;
		const float SuccessMultiplier = bHasSuccessRate ? PendingQueueSuccessRate : 1.f;
		CommittedWaitScore = FMath::Clamp(FMath::RoundToInt(RawWaitScore * SuccessMultiplier), 0, 100);
		CommittedAverageWaitSeconds = bHasWaitSamples ? static_cast<float>(PendingWaitSum / PendingWaitCount) : -1.f;
		CommittedQueueSuccessRate = SuccessMultiplier;
	}

	if (bHasDensity)
	{
		const float AvgDensityScore = static_cast<float>(PendingDensityWeightedScoreSum / PendingDensityWeight);
		CommittedDensityScore = FMath::Clamp(FMath::RoundToInt(AvgDensityScore), 0, 100);
		CommittedCrowdedShare = static_cast<float>(static_cast<double>(PendingCrowdedWeight) / PendingDensityWeight);
	}

	if (bHasPreference)
	{
		const float AvgPreferenceNorm = static_cast<float>(PendingPreferenceSum / PendingPreferenceCount);
		CommittedPreferenceScore = FMath::Clamp(FMath::RoundToInt((AvgPreferenceNorm + 1.f) * 50.f), 0, 100);
		CommittedPreferenceMatchShare = static_cast<float>(PendingPreferenceMatchedCount) / PendingPreferenceCount;
		CommittedPreferenceSampleCount = PendingPreferenceCount;
	}

	if (bHasNeed)
	{
		const float AvgNeedFulfillment = static_cast<float>(PendingNeedFulfillmentSum / PendingNeedFulfillmentCount);
		CommittedNeedFulfillmentScore = FMath::Clamp(FMath::RoundToInt(AvgNeedFulfillment), 0, 100);
	}

	// 2단 계층 블렌드. 축 내부(대기/밀도, 취향/Need)를 먼저 섞고, 그 축 두 개를 다시 섞는다.
	// 각 단계에서 "이번 주기에 실제로 들어온 것만" 가중합하고 그 가중치로 재정규화한다 —
	// CSV 가중치 합이 1이 아니어도(오기입) 항상 정규화되고, 일부 입력이 비어도 그럴듯한
	// 값을 낸다(위 헤더 struct 주석의 FESTPERF/ACSI 구조 참조)
	// 축 점수도 리프 입력과 같은 규칙을 따른다 — 이번 창에 그 축의 재료가 하나도 없으면
	// 0으로 덮지 않고 직전 값을 유지한다("데이터 없음"이 "0점"으로 보이면 안 된다)
	bool bHasEnvironment = false;
	const int32 NewEnvironmentScore = BlendTwo(
		bHasWaitInput, Coefficients.WeightEnvironmentWait, CommittedWaitScore,
		bHasDensity, Coefficients.WeightEnvironmentDensity, CommittedDensityScore,
		bHasEnvironment);
	if (bHasEnvironment)
	{
		CommittedEnvironmentScore = NewEnvironmentScore;
	}

	bool bHasPersonalFit = false;
	const int32 NewPersonalFitScore = BlendTwo(
		bHasPreference, Coefficients.WeightPersonalFitPreference, CommittedPreferenceScore,
		bHasNeed, Coefficients.WeightPersonalFitNeed, CommittedNeedFulfillmentScore,
		bHasPersonalFit);
	if (bHasPersonalFit)
	{
		CommittedPersonalFitScore = NewPersonalFitScore;
	}

	bool bHasTop = false;
	const int32 NewSatisfaction = BlendTwo(
		bHasEnvironment, Coefficients.WeightEnvironment, CommittedEnvironmentScore,
		bHasPersonalFit, Coefficients.WeightPersonalFit, CommittedPersonalFitScore,
		bHasTop);
	if (bHasTop)
	{
		CommittedBaseSatisfaction = NewSatisfaction;
	}

	// 표본 수는 "가장 적게 들어온 입력"으로 잡는다 — 하나라도 비면 부분 데이터라는
	// 사실이 대표값에 드러나야 UI가 "이번 주기는 일부만 반영됐다"를 알 수 있다.
	// 성공률은 비율이라 표본 개수가 아니므로 이 집계에서 뺀다
	int32 MinSampleCount = TNumericLimits<int32>::Max();
	if (bHasWaitSamples) MinSampleCount = FMath::Min(MinSampleCount, PendingWaitCount);
	if (bHasDensity) MinSampleCount = FMath::Min(MinSampleCount, static_cast<int32>(FMath::Min<int64>(PendingDensityWeight, TNumericLimits<int32>::Max())));
	if (bHasPreference) MinSampleCount = FMath::Min(MinSampleCount, PendingPreferenceCount);
	if (bHasNeed) MinSampleCount = FMath::Min(MinSampleCount, PendingNeedFulfillmentCount);
	CommittedSampleCount = (bHasWaitSamples || bHasDensity || bHasPreference || bHasNeed) ? MinSampleCount : 0;

	return ApplyPenalty();
}

// --- UFF_SatisfactionMetricsSubsystem ---

void UFF_SatisfactionMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UFF_FestivalStageSubsystem>();
	Super::Initialize(Collection);

	if (UFF_FestivalStageSubsystem* Stage = GetGameInstance()->GetSubsystem<UFF_FestivalStageSubsystem>())
	{
		Stage->OnFestivalStarted.AddUniqueDynamic(this, &UFF_SatisfactionMetricsSubsystem::HandleFestivalStarted);
	}

	UE_LOG(LogFestaFiestaNpc, Log,
		TEXT("UFF_SatisfactionMetricsSubsystem 초기화. 첫 확정 전까지는 중립값 50을 돌려준다 ")
		TEXT("(GetSampleCount()==0으로 '아직 안 걸림'을 구분할 것)."));
}

void UFF_SatisfactionMetricsSubsystem::ResetForNewFestival()
{
	Aggregator = FFF_SatisfactionAggregator();
	OnSimSatisfactionChanged.Broadcast(Aggregator.GetSimSatisfaction());
}

void UFF_SatisfactionMetricsSubsystem::Deinitialize()
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (UFF_FestivalStageSubsystem* Stage = GameInstance->GetSubsystem<UFF_FestivalStageSubsystem>())
		{
			Stage->OnFestivalStarted.RemoveDynamic(this, &UFF_SatisfactionMetricsSubsystem::HandleFestivalStarted);
		}
	}
	OnSimSatisfactionChanged.Clear();
	Super::Deinitialize();
}

void UFF_SatisfactionMetricsSubsystem::CommitAggregation(const FFF_SatisfactionCoefficients& Coefficients)
{
	LastCoefficients = Coefficients;
	if (Aggregator.Commit(Coefficients))
	{
		OnSimSatisfactionChanged.Broadcast(Aggregator.GetSimSatisfaction());
	}
}
