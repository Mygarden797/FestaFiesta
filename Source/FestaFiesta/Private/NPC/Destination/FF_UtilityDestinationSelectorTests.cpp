// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Engine/DataTable.h"

// utility_ai_spec.md 목표5 "최소 검증 시나리오" 5개를 그대로 테스트로 옮긴다.

namespace FFF_UtilityDestinationSelectorTestHelpers
{
	UDataTable* MakeTestCoefficientTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_UtilityCoefficientRow::StaticStruct();

		auto AddRow = [Table](FName Id, float DefaultValue)
		{
			FFF_UtilityCoefficientRow Row;
			Row.CoefficientId = Id;
			Row.DefaultValue = DefaultValue;
			Table->AddRow(Id, Row);
		};

		AddRow(TEXT("weight_attraction"), 1.0f);
		AddRow(TEXT("lambda_affinity"), 0.6f);
		AddRow(TEXT("weight_distance"), 0.8f);
		AddRow(TEXT("weight_wait"), 0.7f);
		AddRow(TEXT("weight_density"), 0.5f);
		AddRow(TEXT("no_choice_utility"), 0.15f);
		AddRow(TEXT("utility_noise_scale"), 0.3f);
		AddRow(TEXT("distance_ref_meters"), 400.f);
		AddRow(TEXT("wait_ref_minutes"), 30.f);
		AddRow(TEXT("density_ref_ratio"), 1.f);
		AddRow(TEXT("preference_vector_scale_ref"), 17.f);
		return Table;
	}

	FFF_DestinationCandidate MakeCandidate(FName Id, FName Category, float BaseAttraction, float DistanceMeters, float WaitMinutes, float Occupancy, float Capacity)
	{
		FFF_DestinationCandidate Candidate;
		Candidate.DestinationId = Id;
		Candidate.Category = Category;
		Candidate.BaseAttraction = BaseAttraction;
		Candidate.DistanceMeters = DistanceMeters;
		Candidate.EstimatedWaitMinutes = WaitMinutes;
		Candidate.CurrentOccupancy = Occupancy;
		Candidate.Capacity = Capacity;
		Candidate.bIsOpen = true;
		return Candidate;
	}

	float FindProbability(const FFF_DestinationDecision& Decision, FName Id)
	{
		for (const FFF_DestinationUtilityBreakdown& B : Decision.AllCandidateScores)
		{
			if (B.DestinationId == Id)
			{
				return B.ChoiceProbability;
			}
		}
		return -1.f;
	}
}

using namespace FFF_UtilityDestinationSelectorTestHelpers;

// 1. 취향 일치·가까움·한산함 → 선택 확률 높음
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_PreferredCloseCandidateWinsHigh, "FestaFiesta.NPC.UtilityDestinationSelector.PreferredCloseCandidateWinsHigh", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_PreferredCloseCandidateWinsHigh::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();
	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeCandidate(TEXT("Good"), TEXT("food_stall"), 0.8f, 20.f, 2.f, 5.f, 50.f));
	Candidates.Add(MakeCandidate(TEXT("Bad"), TEXT("nightlife_entertainment"), 0.3f, 350.f, 25.f, 45.f, 50.f));

	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), 12.f);
	Preference.Add(TEXT("nightlife_entertainment"), -12.f);

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0);
	const float GoodProb = FindProbability(Decision, TEXT("Good"));
	const float BadProb = FindProbability(Decision, TEXT("Bad"));
	const float NoChoiceProb = FindProbability(Decision, NAME_None);

	TestTrue(TEXT("취향 일치·가까움·한산함인 후보의 선택 확률이 더 높다"), GoodProb > BadProb);
	TestTrue(TEXT("취향 일치 후보가 no_choice보다 확률이 높다"), GoodProb > NoChoiceProb);
	return true;
}

// 2. 매력도 높지만 멀고 대기 김 → 확률 감소 (거리·대기 비용의 단조성 확인)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_FarAndSlowReducesProbability, "FestaFiesta.NPC.UtilityDestinationSelector.FarAndSlowReducesProbability", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_FarAndSlowReducesProbability::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();
	TMap<FName, float> Preference;
	Preference.Add(TEXT("performance_stage"), 5.f);

	TArray<FFF_DestinationCandidate> NearCandidates;
	NearCandidates.Add(MakeCandidate(TEXT("Stage"), TEXT("performance_stage"), 0.95f, 20.f, 2.f, 5.f, 50.f));
	const float NearProb = FindProbability(FFF_UtilityDestinationSelector::Choose(NearCandidates, Preference, Table, 1, 1, 0), TEXT("Stage"));

	TArray<FFF_DestinationCandidate> FarCandidates;
	FarCandidates.Add(MakeCandidate(TEXT("Stage"), TEXT("performance_stage"), 0.95f, 380.f, 28.f, 48.f, 50.f));
	const float FarProb = FindProbability(FFF_UtilityDestinationSelector::Choose(FarCandidates, Preference, Table, 1, 1, 0), TEXT("Stage"));

	TestTrue(TEXT("같은 매력도라도 멀고 대기가 길면 선택 확률이 줄어든다"), FarProb < NearProb);
	return true;
}

// 3. 모든 목적지 효용이 낮음 → no-choice 선택 가능
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_AllLowUtilityFavorsNoChoice, "FestaFiesta.NPC.UtilityDestinationSelector.AllLowUtilityFavorsNoChoice", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_AllLowUtilityFavorsNoChoice::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();
	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeCandidate(TEXT("A"), TEXT("food_stall"), 0.1f, 900.f, 40.f, 49.f, 50.f));
	Candidates.Add(MakeCandidate(TEXT("B"), TEXT("market_goods"), 0.1f, 900.f, 40.f, 49.f, 50.f));

	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), -10.f);
	Preference.Add(TEXT("market_goods"), -10.f);

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0);
	const float NoChoiceProb = FindProbability(Decision, NAME_None);
	const float AProb = FindProbability(Decision, TEXT("A"));
	const float BProb = FindProbability(Decision, TEXT("B"));

	TestTrue(TEXT("모든 후보 효용이 낮으면 no_choice 확률이 가장 높다"), NoChoiceProb > AProb && NoChoiceProb > BProb);
	return true;
}

// 4. 같은 Seed·같은 상태 → 동일 결정
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_SameSeedGivesSameDecision, "FestaFiesta.NPC.UtilityDestinationSelector.SameSeedGivesSameDecision", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_SameSeedGivesSameDecision::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();
	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeCandidate(TEXT("A"), TEXT("food_stall"), 0.6f, 100.f, 10.f, 20.f, 50.f));
	Candidates.Add(MakeCandidate(TEXT("B"), TEXT("market_goods"), 0.5f, 150.f, 12.f, 15.f, 50.f));
	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), 4.f);
	Preference.Add(TEXT("market_goods"), 2.f);

	const FFF_DestinationDecision First = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 7, 3, 2);
	const FFF_DestinationDecision Second = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 7, 3, 2);

	TestEqual(TEXT("같은 입력은 같은 목적지를 선택한다"), First.ChosenDestinationId, Second.ChosenDestinationId);
	TestEqual(TEXT("같은 입력은 같은 후보 개수를 낸다"), First.AllCandidateScores.Num(), Second.AllCandidateScores.Num());
	return true;
}

// 5. 다른 Seed → 통계 범위 내 다른 결과
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_DifferentAgentsProduceVariety, "FestaFiesta.NPC.UtilityDestinationSelector.DifferentAgentsProduceVariety", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_DifferentAgentsProduceVariety::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();
	TArray<FFF_DestinationCandidate> Candidates;
	// 두 후보의 효용을 비슷하게 맞춰서 노이즈에 따라 갈릴 여지를 준다.
	Candidates.Add(MakeCandidate(TEXT("A"), TEXT("food_stall"), 0.6f, 150.f, 10.f, 20.f, 50.f));
	Candidates.Add(MakeCandidate(TEXT("B"), TEXT("market_goods"), 0.6f, 150.f, 10.f, 20.f, 50.f));
	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), 1.f);
	Preference.Add(TEXT("market_goods"), 1.f);

	TSet<FName> DistinctChoices;
	for (int32 AgentId = 0; AgentId < 30; ++AgentId)
	{
		const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, AgentId, 0);
		DistinctChoices.Add(Decision.ChosenDestinationId);
	}

	TestTrue(TEXT("효용이 비슷한 후보들 사이에서 Agent마다 다른 선택이 나온다"), DistinctChoices.Num() > 1);
	return true;
}

// ShouldReevaluate — 재평가 트리거 조건
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_ShouldReevaluateTriggers, "FestaFiesta.NPC.UtilityDestinationSelector.ShouldReevaluateTriggers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_ShouldReevaluateTriggers::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("아무 트리거도 없으면 재평가하지 않는다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(5.f, 15.f, 0.5f, 0.85f, 10.f, 120.f, false, false));
	TestTrue(TEXT("대기시간이 개인 임계값을 넘으면 재평가한다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(20.f, 15.f, 0.5f, 0.85f, 10.f, 120.f, false, false));
	TestTrue(TEXT("혼잡도가 개인 임계값을 넘으면 재평가한다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(5.f, 15.f, 0.9f, 0.85f, 10.f, 120.f, false, false));
	TestTrue(TEXT("재평가 주기가 지나면 재평가한다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(5.f, 15.f, 0.5f, 0.85f, 130.f, 120.f, false, false));
	TestTrue(TEXT("목적지 개폐 상태가 바뀌면 재평가한다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(5.f, 15.f, 0.5f, 0.85f, 10.f, 120.f, true, false));
	TestTrue(TEXT("라이브 이벤트 상태가 바뀌면 재평가한다"),
		FFF_UtilityDestinationSelector::ShouldReevaluate(5.f, 15.f, 0.5f, 0.85f, 10.f, 120.f, false, true));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
