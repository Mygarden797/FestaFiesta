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
	int32 ChoiceRoundIndex,
	const FFF_NeedState& CurrentNeeds,
	int32 ConsecutiveNoChoiceCount,
	float NoveltySeekingMultiplier)
{
	const float WeightAttraction = GetCoefficient(UtilityCoefficientTable, TEXT("weight_attraction"), 1.f);
	const float LambdaAffinity = GetCoefficient(UtilityCoefficientTable, TEXT("lambda_affinity"), 0.6f);
	const float WeightDistance = GetCoefficient(UtilityCoefficientTable, TEXT("weight_distance"), 0.8f);
	const float WeightWait = GetCoefficient(UtilityCoefficientTable, TEXT("weight_wait"), 0.7f);
	const float WeightDensity = GetCoefficient(UtilityCoefficientTable, TEXT("weight_density"), 0.5f);
	// 1.8 은 임의값이 아니다 — weight_attraction(1.0) + lambda_affinity(0.6) = 1.6 보다 크게 잡은
	// 것이고, 그래야 한계 Need 가 "가장 매력적이고 취향에도 맞는 대안"을 이긴다.
	// 이 기본값이 1.6 이하로 내려가면 그 부등식이 깨진다(NeedBeatsBestLikedAlternative 테스트가 잡는다).
	// utility_coefficients_v0.csv 의 weight_need 와 반드시 같게 유지할 것.
	const float WeightNeed = GetCoefficient(UtilityCoefficientTable, TEXT("weight_need"), 1.8f);
	const float WeightRevisit = GetCoefficient(UtilityCoefficientTable, TEXT("weight_revisit"), 0.5f);
	// 0 이하면 방문 잔상이 즉시 사라진다 = 신규성 선호 꺼짐. 나눗셈 보호도 겸한다.
	const float RevisitMemoryMinutes = GetCoefficient(UtilityCoefficientTable, TEXT("revisit_memory_minutes"), 60.f);
	// 음수 배수는 "가본 곳을 더 좋아한다"가 되어 의도와 반대다. 호출부 버그로 보고 0 으로 막는다.
	const float SafeNoveltySeeking = FMath::Max(NoveltySeekingMultiplier, 0.f);
	const float NoChoiceUtilityBase = GetCoefficient(UtilityCoefficientTable, TEXT("no_choice_utility"), 0.15f);
	const float NoChoiceDecayPerRound = GetCoefficient(UtilityCoefficientTable, TEXT("no_choice_utility_decay_per_round"), 0.03f);
	const float UtilityNoiseScale = FMath::Max(GetCoefficient(UtilityCoefficientTable, TEXT("utility_noise_scale"), 0.3f), 0.05f);
	const float DistanceRefMeters = GetCoefficient(UtilityCoefficientTable, TEXT("distance_ref_meters"), 400.f);
	const float WaitRefMinutes = GetCoefficient(UtilityCoefficientTable, TEXT("wait_ref_minutes"), 30.f);
	const float DensityRefRatio = GetCoefficient(UtilityCoefficientTable, TEXT("density_ref_ratio"), 1.f);
	const float PreferenceVectorScaleRef = GetCoefficient(UtilityCoefficientTable, TEXT("preference_vector_scale_ref"), 17.f);

	// Need 성분별 급함 가중치. 기본값은 FFF_NeedPriorityWeights 선언과 같게 유지한다
	FFF_NeedPriorityWeights NeedPriority;
	NeedPriority.Restroom    = GetCoefficient(UtilityCoefficientTable, TEXT("need_priority_restroom"), 1.00f);
	NeedPriority.Hunger      = GetCoefficient(UtilityCoefficientTable, TEXT("need_priority_hunger"), 0.85f);
	NeedPriority.Thirst      = GetCoefficient(UtilityCoefficientTable, TEXT("need_priority_thirst"), 0.75f);
	NeedPriority.Fatigue     = GetCoefficient(UtilityCoefficientTable, TEXT("need_priority_fatigue"), 0.60f);
	NeedPriority.Stimulation = GetCoefficient(UtilityCoefficientTable, TEXT("need_priority_stimulation"), 0.50f);

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

		// need 항: 이 시설이 해소하는 Need 중 가장 급한 값. 합이 아니라 최대값인 이유는
		// FFF_NeedReliefFlags::GetMaxUrgency 주석 참고. 이 항이 재방문 문제도 같이 해결한다 —
		// 배고픔을 푼 직후에는 Hunger가 0이 되어 같은 식음료 부스의 효용이 저절로 내려간다.
		const float NeedUrgencyNorm = Candidate.NeedRelief.GetMaxUrgency(CurrentNeeds, NeedPriority);

		// revisit 항: 최근에 가본 곳일수록 크고 시간이 지나면 0 으로 감쇠한다(선형).
		//
		// 감쇠형인 이유: 영구 페널티면 "제일 좋아하는 부스에 두 번은 안 간다"가 되어 오히려
		// 부자연스럽다. 실제로는 마음에 든 곳을 다시 가기도 한다.
		//
		// MinutesSinceLastVisit 음수 = 안 가본 곳이고 그게 기본값이다. 0(방금 다녀옴)과 뜻이
		// 완전히 다르므로 여기서 갈라준다 — FFF_VisitHistory 가 안 붙어 있으면 이 항 전체가 0 이다.
		float RevisitPenaltyNorm = 0.f;
		if (Candidate.MinutesSinceLastVisit >= 0.f && RevisitMemoryMinutes > KINDA_SMALL_NUMBER)
		{
			RevisitPenaltyNorm = FMath::Clamp(
				1.f - (Candidate.MinutesSinceLastVisit / RevisitMemoryMinutes), 0.f, 1.f);
		}

		FFF_DestinationUtilityBreakdown Breakdown;
		Breakdown.DestinationId = Candidate.DestinationId;
		Breakdown.AttractionTerm = WeightAttraction * AttractionNorm;
		Breakdown.PreferenceTerm = LambdaAffinity * PreferenceNorm;
		Breakdown.DistanceCostTerm = -WeightDistance * DistanceNorm;
		Breakdown.WaitCostTerm = -WeightWait * WaitNorm;
		Breakdown.DensityCostTerm = -WeightDensity * DensityNorm;
		Breakdown.NeedTerm = WeightNeed * NeedUrgencyNorm;
		Breakdown.RevisitPenaltyTerm = -WeightRevisit * RevisitPenaltyNorm * SafeNoveltySeeking;
		Breakdown.TotalUtility = Breakdown.AttractionTerm + Breakdown.PreferenceTerm + Breakdown.DistanceCostTerm
			+ Breakdown.WaitCostTerm + Breakdown.DensityCostTerm + Breakdown.NeedTerm
			+ Breakdown.RevisitPenaltyTerm;
		Breakdowns.Add(Breakdown);
	}

	// 연속으로 아무것도 못 골랐을수록 no_choice의 매력을 깎는다 = 인내심 소모.
	// 없으면 "혼잡해서 배회 → 다음 재평가에 같은 후보 → 또 배회"가 영원히 반복된다.
	// 0 아래로는 내려가지 않게 한다 — 음수가 되면 no_choice가 사실상 사라져 대기시간 클램프와
	// 같은 상태가 되고, 이 대안을 둔 이유 자체가 무의미해진다.
	const float NoChoiceUtility = FMath::Max(
		NoChoiceUtilityBase - NoChoiceDecayPerRound * static_cast<float>(FMath::Max(ConsecutiveNoChoiceCount, 0)),
		0.f);

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

	// Decision 시드: 회차 인덱스를 정수로 접는다.
	// 예전에는 FName(*FString::Printf(TEXT("destination_choice_round_%d"), Round)) 로 문자열에
	// 접어넣었는데, 그러면 매 선택마다 FString 할당 + 전역 이름 테이블 조회가 발생했다
	// (8만 명 × 회차 수). FName 은 해제되지도 않는다. MakeDecisionSeed 주석 참조.
	const uint32 Seed = FFF_DeterministicRandom::MakeDecisionSeed(
		GlobalSimulationSeed, StableAgentId, ChoiceRoundIndex);
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

	UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[목적지 선택] Agent=%d Round=%d 선택=%s (no_choice=%d, U=%.4f, 연속no_choice=%d, no_choice효용=%.4f)"),
		StableAgentId, ChoiceRoundIndex,
		Decision.bIsNoChoice ? TEXT("no_choice") : *Decision.ChosenDestinationId.ToString(),
		Decision.bIsNoChoice, U, ConsecutiveNoChoiceCount, NoChoiceUtility);
	for (const FFF_DestinationUtilityBreakdown& B : Decision.AllCandidateScores)
	{
		UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("  - %s: Attraction=%.3f Preference=%.3f Distance=%.3f Wait=%.3f Density=%.3f Need=%.3f Revisit=%.3f Total=%.3f Prob=%.3f"),
			B.DestinationId.IsNone() ? TEXT("no_choice") : *B.DestinationId.ToString(),
			B.AttractionTerm, B.PreferenceTerm, B.DistanceCostTerm, B.WaitCostTerm, B.DensityCostTerm,
			B.NeedTerm, B.RevisitPenaltyTerm, B.TotalUtility, B.ChoiceProbability);
	}

	return Decision;
}

EFF_NoChoiceAction FFF_UtilityDestinationSelector::ResolveNoChoiceAction(
	float RemainingStayMinutes,
	int32 ConsecutiveNoChoiceCount,
	int32 NoChoiceLeavePatienceRounds,
	float CurrentZoneDensityRatio,
	float PersonalDensityThresholdRatio)
{
	// 1. 체류시간 소진 — 퇴장의 주 원인. 이 게이트가 먼저 와야 인원 보존이 지켜진다
	if (RemainingStayMinutes <= 0.f)
	{
		UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[no_choice 행동] Leave (체류시간 소진)"));
		return EFF_NoChoiceAction::Leave;
	}

	// 2. 연속 no_choice 인내 한계 — 혼잡의 간접 결과로서의 퇴장. 0 이하면 이 조건을 끈다
	if (NoChoiceLeavePatienceRounds > 0 && ConsecutiveNoChoiceCount >= NoChoiceLeavePatienceRounds)
	{
		UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[no_choice 행동] Leave (연속 no_choice %d회 >= 한계 %d회)"),
			ConsecutiveNoChoiceCount, NoChoiceLeavePatienceRounds);
		return EFF_NoChoiceAction::Leave;
	}

	// 3. 현재 Zone이 개인 임계값보다 혼잡 — 다른 Zone으로 옮긴다
	if (CurrentZoneDensityRatio > PersonalDensityThresholdRatio)
	{
		UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[no_choice 행동] Relocate (Zone 밀집 %.3f > 개인 임계 %.3f)"),
			CurrentZoneDensityRatio, PersonalDensityThresholdRatio);
		return EFF_NoChoiceAction::Relocate;
	}

	// 4. 기본값
	UE_LOG(LogFestaFiestaNpc, Verbose, TEXT("[no_choice 행동] Wander (연속 %d회)"), ConsecutiveNoChoiceCount);
	return EFF_NoChoiceAction::Wander;
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
