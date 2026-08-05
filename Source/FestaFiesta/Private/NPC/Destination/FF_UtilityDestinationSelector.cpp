// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "Engine/DataTable.h"

DEFINE_LOG_CATEGORY(LogFestaFiestaNpc);

namespace
{
	float GetCoefficient(const UDataTable* Table, FName CoefficientId, float DefaultValue)
	{
		return FFF_UtilityCoefficientHelpers::GetCoefficient(Table, CoefficientId, DefaultValue);
	}

	float NormalizeUpper(float Value, float Ref)
	{
		if (Ref <= 0.f)
		{
			return 0.f;
		}
		return FMath::Clamp(Value / Ref, 0.f, 1.f);
	}
}

FFF_DestinationDecision FFF_UtilityDestinationSelector::Choose(
	const TArray<FFF_DestinationCandidate>& Candidates,
	const TMap<FName, float>& PreferenceVectorByCategory,
	const UDataTable* UtilityCoefficientTable,
	int32 GlobalSimulationSeed,
	int32 StableAgentId,
	int32 ChoiceRoundIndex)
{
	const float WeightAttraction = GetCoefficient(UtilityCoefficientTable, TEXT("weight_attraction"), 1.f);
	const float LambdaAffinity = GetCoefficient(UtilityCoefficientTable, TEXT("lambda_affinity"), 0.6f);
	const float WeightDistance = GetCoefficient(UtilityCoefficientTable, TEXT("weight_distance"), 0.8f);
	const float WeightWait = GetCoefficient(UtilityCoefficientTable, TEXT("weight_wait"), 0.7f);
	const float WeightDensity = GetCoefficient(UtilityCoefficientTable, TEXT("weight_density"), 0.5f);
	const float NoChoiceUtility = GetCoefficient(UtilityCoefficientTable, TEXT("no_choice_utility"), 0.15f);
	const float UtilityNoiseScale = FMath::Max(GetCoefficient(UtilityCoefficientTable, TEXT("utility_noise_scale"), 0.3f), 0.05f);
	const float DistanceRefMeters = GetCoefficient(UtilityCoefficientTable, TEXT("distance_ref_meters"), 400.f);
	const float WaitRefMinutes = GetCoefficient(UtilityCoefficientTable, TEXT("wait_ref_minutes"), 30.f);
	const float DensityRefRatio = GetCoefficient(UtilityCoefficientTable, TEXT("density_ref_ratio"), 1.f);
	const float PreferenceVectorScaleRef = GetCoefficient(UtilityCoefficientTable, TEXT("preference_vector_scale_ref"), 17.f);

	TArray<FFF_DestinationUtilityBreakdown> Breakdowns;
	Breakdowns.Reserve(Candidates.Num() + 1);

	for (const FFF_DestinationCandidate& Candidate : Candidates)
	{
		if (!Candidate.bIsOpen)
		{
			continue;
		}

		const float AttractionNorm = FMath::Clamp(Candidate.BaseAttraction, 0.f, 1.f);

		const float PreferenceRaw = PreferenceVectorByCategory.FindRef(Candidate.Category);
		const float PreferenceNorm = FMath::Clamp(PreferenceRaw / PreferenceVectorScaleRef, -1.f, 1.f);

		const float DistanceNorm = NormalizeUpper(Candidate.DistanceMeters, DistanceRefMeters);
		const float WaitNorm = NormalizeUpper(Candidate.EstimatedWaitMinutes, WaitRefMinutes);
		const float OccupancyRatio = Candidate.Capacity > 0.f ? (Candidate.CurrentOccupancy / Candidate.Capacity) : 0.f;
		const float DensityNorm = NormalizeUpper(OccupancyRatio, DensityRefRatio);

		FFF_DestinationUtilityBreakdown Breakdown;
		Breakdown.DestinationId = Candidate.DestinationId;
		Breakdown.AttractionTerm = WeightAttraction * AttractionNorm;
		Breakdown.PreferenceTerm = LambdaAffinity * PreferenceNorm;
		Breakdown.DistanceCostTerm = -WeightDistance * DistanceNorm;
		Breakdown.WaitCostTerm = -WeightWait * WaitNorm;
		Breakdown.DensityCostTerm = -WeightDensity * DensityNorm;
		Breakdown.TotalUtility = Breakdown.AttractionTerm + Breakdown.PreferenceTerm + Breakdown.DistanceCostTerm + Breakdown.WaitCostTerm + Breakdown.DensityCostTerm;
		Breakdowns.Add(Breakdown);
	}

	FFF_DestinationUtilityBreakdown NoChoice;
	NoChoice.DestinationId = NAME_None;
	NoChoice.TotalUtility = NoChoiceUtility;
	Breakdowns.Add(NoChoice);

	// softmax — 최댓값을 빼서 exp 오버플로를 막는다 (선택 확률 자체는 바뀌지 않음)
	float MaxUtility = Breakdowns[0].TotalUtility;
	for (const FFF_DestinationUtilityBreakdown& B : Breakdowns)
	{
		MaxUtility = FMath::Max(MaxUtility, B.TotalUtility);
	}

	float SumExp = 0.f;
	for (FFF_DestinationUtilityBreakdown& B : Breakdowns)
	{
		B.ChoiceProbability = FMath::Exp((B.TotalUtility - MaxUtility) / UtilityNoiseScale);
		SumExp += B.ChoiceProbability;
	}
	for (FFF_DestinationUtilityBreakdown& B : Breakdowns)
	{
		B.ChoiceProbability /= SumExp;
	}

	// Decision 시드: 회차 인덱스를 TraitKey 문자열에 접어넣어 SpawnGeneration의 의미를 건드리지 않는다.
	const FName RoundTraitKey(*FString::Printf(TEXT("destination_choice_round_%d"), ChoiceRoundIndex));
	const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(GlobalSimulationSeed, StableAgentId, RoundTraitKey, 0, TEXT("Decision"));
	const float U = FFF_DeterministicRandom::MakeUniformFraction(Seed);

	float Cumulative = 0.f;
	int32 ChosenIndex = Breakdowns.Num() - 1; // 부동소수 오차로 끝까지 못 고르면 no_choice로 귀결
	for (int32 Index = 0; Index < Breakdowns.Num(); ++Index)
	{
		Cumulative += Breakdowns[Index].ChoiceProbability;
		if (U < Cumulative)
		{
			ChosenIndex = Index;
			break;
		}
	}

	FFF_DestinationDecision Decision;
	Decision.AllCandidateScores = MoveTemp(Breakdowns);
	Decision.ChosenDestinationId = Decision.AllCandidateScores[ChosenIndex].DestinationId;
	Decision.bIsNoChoice = (Decision.ChosenDestinationId == NAME_None);

	UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[목적지 선택] Agent=%d Round=%d 선택=%s (no_choice=%d, U=%.4f)"),
		StableAgentId, ChoiceRoundIndex,
		Decision.bIsNoChoice ? TEXT("no_choice") : *Decision.ChosenDestinationId.ToString(),
		Decision.bIsNoChoice, U);
	for (const FFF_DestinationUtilityBreakdown& B : Decision.AllCandidateScores)
	{
		UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("  - %s: Attraction=%.3f Preference=%.3f Distance=%.3f Wait=%.3f Density=%.3f Total=%.3f Prob=%.3f"),
			B.DestinationId.IsNone() ? TEXT("no_choice") : *B.DestinationId.ToString(),
			B.AttractionTerm, B.PreferenceTerm, B.DistanceCostTerm, B.WaitCostTerm, B.DensityCostTerm, B.TotalUtility, B.ChoiceProbability);
	}

	return Decision;
}

bool FFF_UtilityDestinationSelector::ShouldReevaluate(
	float CurrentWaitMinutes, float PersonalWaitThresholdMinutes,
	float CurrentDensityRatio, float PersonalDensityThresholdRatio,
	float SecondsSinceLastEvaluation, float PersonalReevaluateIntervalSeconds,
	bool bDestinationOpenStateChanged, bool bLiveEventStateChanged)
{
	if (bDestinationOpenStateChanged || bLiveEventStateChanged)
	{
		return true;
	}
	if (CurrentWaitMinutes > PersonalWaitThresholdMinutes)
	{
		return true;
	}
	if (CurrentDensityRatio > PersonalDensityThresholdRatio)
	{
		return true;
	}
	if (SecondsSinceLastEvaluation >= PersonalReevaluateIntervalSeconds)
	{
		return true;
	}
	return false;
}
