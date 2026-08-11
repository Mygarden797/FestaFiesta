// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Destination/FF_UtilityDestinationSelector.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Engine/DataTable.h"

// utility_ai_spec.md 목표5 "최소 검증 시나리오" 5개를 그대로 테스트로 옮긴다.

namespace FFF_UtilityDestinationSelectorTestHelpers
{
	// utility_coefficients_v0.csv 의 기본값과 같게 유지한다. need 항의 기대값 계산에 쓰인다
	constexpr float TestWeightNeed = 1.8f;
	constexpr float TestPriorityRestroom = 1.00f;
	constexpr float TestPriorityHunger = 0.85f;
	constexpr float TestPriorityThirst = 0.75f;
	constexpr float TestPriorityFatigue = 0.60f;

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
		AddRow(TEXT("weight_need"), TestWeightNeed);
		AddRow(TEXT("no_choice_utility_decay_per_round"), 0.03f);
		AddRow(TEXT("no_choice_leave_patience_rounds"), 5.f);
		AddRow(TEXT("need_priority_restroom"), TestPriorityRestroom);
		AddRow(TEXT("need_priority_hunger"), TestPriorityHunger);
		AddRow(TEXT("need_priority_thirst"), TestPriorityThirst);
		AddRow(TEXT("need_priority_fatigue"), TestPriorityFatigue);
		AddRow(TEXT("need_priority_stimulation"), 0.50f);
		return Table;
	}

	FFF_NeedState MakeNeeds(float Hunger, float Thirst, float Restroom, float Fatigue, float Stimulation)
	{
		FFF_NeedState Needs;
		Needs.Hunger = Hunger;
		Needs.Thirst = Thirst;
		Needs.Restroom = Restroom;
		Needs.Fatigue = Fatigue;
		Needs.Stimulation = Stimulation;
		return Needs;
	}

	float FindNeedTerm(const FFF_DestinationDecision& Decision, FName Id)
	{
		for (const FFF_DestinationUtilityBreakdown& B : Decision.AllCandidateScores)
		{
			if (B.DestinationId == Id)
			{
				return B.NeedTerm;
			}
		}
		return -1.f;
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

// --- Need 항 (2026-08-06 추가) ---

// Need가 급하면 그 Need를 해소하는 시설의 효용이 올라간다. Need를 안 주면 예전과 동일해야 한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_UrgentNeedRaisesRelievingCandidate, "FestaFiesta.NPC.UtilityDestinationSelector.UrgentNeedRaisesRelievingCandidate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_UrgentNeedRaisesRelievingCandidate::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();

	// 취향은 화장실보다 식음료가 좋지만, 화장실 Need가 한계에 가깝다
	FFF_DestinationCandidate Toilet = MakeCandidate(TEXT("Toilet"), NAME_None, 0.2f, 60.f, 3.f, 5.f, 20.f);
	Toilet.NeedRelief.bRelievesRestroom = true;

	FFF_DestinationCandidate Food = MakeCandidate(TEXT("Food"), TEXT("food_stall"), 0.8f, 40.f, 3.f, 5.f, 20.f);
	Food.NeedRelief.bRelievesHunger = true;

	TArray<FFF_DestinationCandidate> Candidates = { Toilet, Food };
	TMap<FName, float> Preference;
	Preference.Add(TEXT("food_stall"), 12.f);

	// Need 없음 → 취향·매력도가 높은 식음료가 이긴다
	const FFF_DestinationDecision WithoutNeed = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0);
	TestTrue(TEXT("Need가 없으면 취향·매력도가 높은 후보가 이긴다"),
		FindProbability(WithoutNeed, TEXT("Food")) > FindProbability(WithoutNeed, TEXT("Toilet")));
	TestEqual(TEXT("Need를 안 넘기면 need 항이 0이다 (기존 동작 보존)"), FindNeedTerm(WithoutNeed, TEXT("Food")), 0.f);

	// 화장실 Need 0.95 → 취향을 뒤집는다
	const FFF_NeedState Urgent = MakeNeeds(0.1f, 0.f, 0.95f, 0.f, 0.f);
	const FFF_DestinationDecision WithNeed = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, Urgent);
	TestTrue(TEXT("화장실 Need가 급하면 취향을 이기고 화장실이 선택 확률이 높다"),
		FindProbability(WithNeed, TEXT("Toilet")) > FindProbability(WithNeed, TEXT("Food")));
	TestTrue(TEXT("해소하지 않는 Need는 그 후보의 need 항에 들어가지 않는다"),
		FindNeedTerm(WithNeed, TEXT("Food")) < FindNeedTerm(WithNeed, TEXT("Toilet")));

	// weight_need 보정의 반대쪽 경계 — 가벼운 Need는 취향을 뒤집지 못해야 한다.
	// 이게 깨지면 weight_need가 너무 커서 Need가 모든 선택을 지배한다는 뜻이다.
	const FFF_NeedState Mild = MakeNeeds(0.1f, 0.f, 0.3f, 0.f, 0.f);
	const FFF_DestinationDecision WithMildNeed = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, Mild);
	TestTrue(TEXT("가벼운 화장실 Need(0.3)는 취향을 뒤집지 못한다 — Need가 모든 선택을 지배하지 않는다"),
		FindProbability(WithMildNeed, TEXT("Food")) > FindProbability(WithMildNeed, TEXT("Toilet")));
	return true;
}

// Need를 여러 개 해소하는 시설은 합이 아니라 "가장 급한 것"만 반영한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NeedTermUsesMaxNotSum, "FestaFiesta.NPC.UtilityDestinationSelector.NeedTermUsesMaxNotSum", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NeedTermUsesMaxNotSum::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();

	// Rest는 Fatigue·Thirst 둘 다 해소한다 (오아시스 존)
	FFF_DestinationCandidate Rest = MakeCandidate(TEXT("Rest"), TEXT("rest_area"), 0.5f, 50.f, 0.f, 0.f, 30.f);
	Rest.NeedRelief.bRelievesFatigue = true;
	Rest.NeedRelief.bRelievesThirst = true;

	TArray<FFF_DestinationCandidate> Candidates = { Rest };
	const TMap<FName, float> Preference;

	// Fatigue 0.4(*0.60=0.24) + Thirst 0.3(*0.75=0.225). 합이면 0.465, 최대면 0.24
	const FFF_NeedState Needs = MakeNeeds(0.f, 0.3f, 0.f, 0.4f, 0.f);
	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, Needs);

	const float ExpectedMax = FMath::Max(0.4f * TestPriorityFatigue, 0.3f * TestPriorityThirst);
	const float NeedTerm = FindNeedTerm(Decision, TEXT("Rest"));
	TestEqual(TEXT("need 항은 (성분값 * 성분우선순위)의 최대값 * weight_need 이다"), NeedTerm, TestWeightNeed * ExpectedMax, 1e-4f);
	TestTrue(TEXT("need 항이 두 성분의 합보다 작다 — 3가지 해소 시설이 항상 이기는 것을 막는다"),
		NeedTerm < TestWeightNeed * (0.4f * TestPriorityFatigue + 0.3f * TestPriorityThirst));
	return true;
}

// Need를 해소한 직후에는 같은 시설의 효용이 저절로 내려간다 (재방문 문제 해결 확인)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_RelievedNeedRemovesRevisitPull, "FestaFiesta.NPC.UtilityDestinationSelector.RelievedNeedRemovesRevisitPull", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_RelievedNeedRemovesRevisitPull::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();

	FFF_DestinationCandidate Food = MakeCandidate(TEXT("Food"), TEXT("food_stall"), 0.5f, 50.f, 5.f, 10.f, 30.f);
	Food.NeedRelief.bRelievesHunger = true;
	FFF_DestinationCandidate Stage = MakeCandidate(TEXT("Stage"), TEXT("performance_stage"), 0.5f, 50.f, 5.f, 10.f, 30.f);

	TArray<FFF_DestinationCandidate> Candidates = { Food, Stage };
	const TMap<FName, float> Preference;

	const FFF_DestinationDecision Hungry = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, Table, 1, 1, 0, MakeNeeds(0.9f, 0.f, 0.f, 0.f, 0.f));
	const FFF_DestinationDecision Fed = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, Table, 1, 1, 1, MakeNeeds(0.0f, 0.f, 0.f, 0.f, 0.f));

	TestTrue(TEXT("배고플 때는 식음료 부스가 무대보다 확률이 높다"),
		FindProbability(Hungry, TEXT("Food")) > FindProbability(Hungry, TEXT("Stage")));
	TestTrue(TEXT("배고픔이 해소되면 식음료 부스의 우위가 사라진다"),
		FindProbability(Fed, TEXT("Food")) < FindProbability(Hungry, TEXT("Food")));
	return true;
}

// Need 값이 같으면 성분별 우선순위가 순서를 정한다 (화장실 > 배고픔)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NeedPriorityBreaksTies, "FestaFiesta.NPC.UtilityDestinationSelector.NeedPriorityBreaksTies", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NeedPriorityBreaksTies::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();

	// 거리·대기·밀집·매력도를 완전히 같게 두어 need 항만 차이가 나게 한다
	FFF_DestinationCandidate Toilet = MakeCandidate(TEXT("Toilet"), NAME_None, 0.4f, 50.f, 3.f, 5.f, 20.f);
	Toilet.NeedRelief.bRelievesRestroom = true;
	FFF_DestinationCandidate Food = MakeCandidate(TEXT("Food"), NAME_None, 0.4f, 50.f, 3.f, 5.f, 20.f);
	Food.NeedRelief.bRelievesHunger = true;

	TArray<FFF_DestinationCandidate> Candidates = { Toilet, Food };
	const TMap<FName, float> Preference;

	// 두 Need가 정확히 같은 값(0.5)
	const FFF_NeedState Tied = MakeNeeds(0.5f, 0.f, 0.5f, 0.f, 0.f);
	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, Tied);

	TestTrue(TEXT("Need 값이 같으면 더 급한 성분(화장실)을 해소하는 시설이 이긴다"),
		FindProbability(Decision, TEXT("Toilet")) > FindProbability(Decision, TEXT("Food")));
	TestEqual(TEXT("화장실 need 항 = 0.5 * 1.0 * weight_need"),
		FindNeedTerm(Decision, TEXT("Toilet")), TestWeightNeed * 0.5f * TestPriorityRestroom, 1e-4f);
	TestEqual(TEXT("배고픔 need 항 = 0.5 * 0.85 * weight_need"),
		FindNeedTerm(Decision, TEXT("Food")), TestWeightNeed * 0.5f * TestPriorityHunger, 1e-4f);

	// 다만 우선순위가 값 차이를 뒤집지는 않는다 — 배고픔이 훨씬 급하면 배고픔이 이겨야 한다
	const FFF_NeedState VeryHungry = MakeNeeds(0.95f, 0.f, 0.3f, 0.f, 0.f);
	const FFF_DestinationDecision Decision2 = FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 1, VeryHungry);
	TestTrue(TEXT("우선순위가 낮아도 값이 훨씬 크면 이긴다 (하드 우선순위가 아니다)"),
		FindProbability(Decision2, TEXT("Food")) > FindProbability(Decision2, TEXT("Toilet")));
	return true;
}

// --- no_choice 감쇠·행동 (2026-08-06 추가) ---

// 연속으로 아무것도 못 고를수록 no_choice의 매력이 깎인다 = 무한 배회 방지
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NoChoiceDecaysWithConsecutiveRounds, "FestaFiesta.NPC.UtilityDestinationSelector.NoChoiceDecaysWithConsecutiveRounds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NoChoiceDecaysWithConsecutiveRounds::RunTest(const FString& Parameters)
{
	UDataTable* Table = MakeTestCoefficientTable();

	// 전부 혼잡하고 먼 후보 — no_choice가 매력적인 상황
	TArray<FFF_DestinationCandidate> Candidates;
	Candidates.Add(MakeCandidate(TEXT("Crowded"), TEXT("food_stall"), 0.3f, 380.f, 28.f, 49.f, 50.f));
	const TMap<FName, float> Preference;

	const float Prob0 = FindProbability(
		FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, FFF_NeedState(), 0), NAME_None);
	const float Prob3 = FindProbability(
		FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, FFF_NeedState(), 3), NAME_None);
	const float Prob10 = FindProbability(
		FFF_UtilityDestinationSelector::Choose(Candidates, Preference, Table, 1, 1, 0, FFF_NeedState(), 10), NAME_None);

	TestTrue(TEXT("연속 no_choice가 쌓이면 no_choice 확률이 내려간다"), Prob3 < Prob0);
	TestTrue(TEXT("더 쌓이면 더 내려간다 (단조 감소)"), Prob10 < Prob3);
	TestTrue(TEXT("효용이 0에서 멈춰 no_choice가 완전히 사라지지는 않는다"), Prob10 > 0.f);
	return true;
}

// no_choice 행동 우선순위 캐스케이드
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NoChoiceActionCascade, "FestaFiesta.NPC.UtilityDestinationSelector.NoChoiceActionCascade", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NoChoiceActionCascade::RunTest(const FString& Parameters)
{
	// 1. 체류시간 소진이 최우선 — 혼잡하지 않아도 퇴장
	TestTrue(TEXT("체류시간이 소진되면 Leave"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(0.f, 0, 5, 0.1f, 0.85f) == EFF_NoChoiceAction::Leave);

	// 2. 연속 no_choice 한계 도달 → 퇴장. 혼잡 조건보다 먼저 본다
	TestTrue(TEXT("연속 no_choice가 한계에 도달하면 Leave"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(60.f, 5, 5, 0.99f, 0.85f) == EFF_NoChoiceAction::Leave);

	// 3. Zone 혼잡 → 구역 이동
	TestTrue(TEXT("현재 Zone이 개인 임계값보다 혼잡하면 Relocate"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(60.f, 1, 5, 0.9f, 0.85f) == EFF_NoChoiceAction::Relocate);

	// 4. 기본값
	TestTrue(TEXT("그 외에는 Wander"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(60.f, 1, 5, 0.3f, 0.85f) == EFF_NoChoiceAction::Wander);

	// 체류시간이 남아 있으면 혼잡만으로는 퇴장하지 않는다 — 인원 보존이 깨지지 않게 하는 불변식
	TestFalse(TEXT("체류시간이 남아 있고 인내 한계 전이면 혼잡만으로 퇴장하지 않는다"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(120.f, 0, 5, 1.5f, 0.85f) == EFF_NoChoiceAction::Leave);

	// 인내 한계를 끄면(0 이하) 연속 횟수가 아무리 쌓여도 퇴장하지 않는다
	TestFalse(TEXT("no_choice_leave_patience_rounds가 0이면 그 조건이 꺼진다"),
		FFF_UtilityDestinationSelector::ResolveNoChoiceAction(120.f, 99, 0, 0.1f, 0.85f) == EFF_NoChoiceAction::Leave);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// revisit 항 — 신규성 선호 (사용자 확정, 2026-08-10)
// ─────────────────────────────────────────────────────────────────────────────

// 방금 다녀온 곳은 확률이 내려가고, 시간이 지나면 회복한다.
// 이게 없으면 같은 카테고리 부스들의 점수가 동일해져서 softmax 가 균등 분포가 되고,
// 우연히 같은 곳을 또 간다 — 매출이 한 부스에 몰린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_RevisitPenaltyDecaysOverTime, "FestaFiesta.NPC.UtilityDestinationSelector.RevisitPenaltyDecaysOverTime", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_RevisitPenaltyDecaysOverTime::RunTest(const FString& Parameters)
{
	// 완전히 동일한 두 부스. 방문 이력만 다르다
	TArray<FFF_DestinationCandidate> Candidates = {
		MakeCandidate(TEXT("BoothA"), TEXT("food_stall"), 0.5f, 100.f, 5.f, 10.f, 100.f),
		MakeCandidate(TEXT("BoothB"), TEXT("food_stall"), 0.5f, 100.f, 5.f, 10.f, 100.f),
	};
	const TMap<FName, float> Preference = { { TEXT("food_stall"), 5.f } };

	// 이력이 없으면 둘의 효용이 같아야 한다
	const FFF_DestinationDecision Neutral = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0);
	const float NeutralA = FindProbability(Neutral, TEXT("BoothA"));
	const float NeutralB = FindProbability(Neutral, TEXT("BoothB"));
	TestEqual(TEXT("이력이 없으면 동일한 두 부스의 확률이 같다"), NeutralA, NeutralB, 0.0001f);

	// A 를 방금 다녀왔다 → A 의 확률이 B 보다 낮아야 한다
	Candidates[0].MinutesSinceLastVisit = 0.f;
	const FFF_DestinationDecision JustVisited = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0);
	TestTrue(TEXT("방금 다녀온 곳의 확률이 더 낮다"),
		FindProbability(JustVisited, TEXT("BoothA")) < FindProbability(JustVisited, TEXT("BoothB")));

	// 30분 전(기억 60분의 절반) → 페널티가 절반으로 줄어 확률 차이도 줄어야 한다
	Candidates[0].MinutesSinceLastVisit = 30.f;
	const FFF_DestinationDecision HalfDecayed = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0);
	const float GapJustVisited =
		FindProbability(JustVisited, TEXT("BoothB")) - FindProbability(JustVisited, TEXT("BoothA"));
	const float GapHalf =
		FindProbability(HalfDecayed, TEXT("BoothB")) - FindProbability(HalfDecayed, TEXT("BoothA"));
	TestTrue(TEXT("시간이 지나면 페널티가 줄어 확률 차이가 좁혀진다"), GapHalf < GapJustVisited);
	TestTrue(TEXT("그래도 아직 차이는 남아 있다"), GapHalf > 0.f);

	// 기억 시간을 넘기면 처음 가는 곳과 같아진다 — 영구 페널티가 아니라는 것이 핵심이다.
	// 영구 페널티면 "제일 좋아하는 부스에 두 번은 안 간다"가 되어 부자연스럽다.
	Candidates[0].MinutesSinceLastVisit = 120.f;
	const FFF_DestinationDecision FullyDecayed = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0);
	TestEqual(TEXT("기억 시간을 넘기면 안 가본 곳과 같다"),
		FindProbability(FullyDecayed, TEXT("BoothA")),
		FindProbability(FullyDecayed, TEXT("BoothB")), 0.0001f);
	return true;
}

// novelty_seeking_multiplier 가 페널티 크기를 개체별로 스케일한다. 0 이면 이력을 신경 쓰지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NoveltySeekingScalesPenalty, "FestaFiesta.NPC.UtilityDestinationSelector.NoveltySeekingScalesPenalty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NoveltySeekingScalesPenalty::RunTest(const FString& Parameters)
{
	TArray<FFF_DestinationCandidate> Candidates = {
		MakeCandidate(TEXT("BoothA"), TEXT("food_stall"), 0.5f, 100.f, 5.f, 10.f, 100.f),
		MakeCandidate(TEXT("BoothB"), TEXT("food_stall"), 0.5f, 100.f, 5.f, 10.f, 100.f),
	};
	Candidates[0].MinutesSinceLastVisit = 0.f;   // A 를 방금 다녀왔다
	const TMap<FName, float> Preference = { { TEXT("food_stall"), 5.f } };

	auto GapFor = [&Candidates, &Preference](float NoveltySeeking)
	{
		const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
			Candidates, Preference, nullptr, 1, 1, 0, FFF_NeedState(), 0, NoveltySeeking);
		return FindProbability(Decision, TEXT("BoothB")) - FindProbability(Decision, TEXT("BoothA"));
	};

	const float GapIndifferent = GapFor(0.f);
	const float GapAverage = GapFor(1.f);
	const float GapSeeker = GapFor(1.6f);

	TestEqual(TEXT("0 이면 방문 이력을 아예 신경 쓰지 않는다"), GapIndifferent, 0.f, 0.0001f);
	TestTrue(TEXT("배수가 크면 가본 곳을 더 강하게 피한다"), GapSeeker > GapAverage);
	TestTrue(TEXT("평균 개체도 가본 곳을 피한다"), GapAverage > 0.f);

	// 음수는 "가본 곳을 더 좋아한다"가 되어 의도와 반대다 — 0 으로 막는다
	TestEqual(TEXT("음수 배수는 0 으로 막는다"), GapFor(-2.f), 0.f, 0.0001f);
	return true;
}

// 급한 Need 는 방문 이력을 무시한다. weight_revisit(0.5) × novelty(1.6) = 0.8 < weight_need(1.8).
//
// 두 후보의 매력도·취향·거리·밀집을 전부 같게 두고 **차이를 Need 와 방문 이력에만** 둔다.
// (처음엔 "방금 다녀온 화장실 vs 취향에 맞는 포토존"으로 짰는데, 그건 attraction·preference 차이가
//  섞여서 revisit 페널티를 분리하지 못했다 — 실제로 테스트가 실패했고 그게 맞는 결과였다.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_UrgentNeedOverridesRevisitPenalty, "FestaFiesta.NPC.UtilityDestinationSelector.UrgentNeedOverridesRevisitPenalty", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_UrgentNeedOverridesRevisitPenalty::RunTest(const FString& Parameters)
{
	// 방금 다녀왔지만 급한 욕구를 풀어주는 화장실
	FFF_DestinationCandidate VisitedToilet =
		MakeCandidate(TEXT("ToiletA"), NAME_None, 0.f, 100.f, 0.f, 10.f, 100.f);
	VisitedToilet.NeedRelief.bRelievesRestroom = true;
	VisitedToilet.MinutesSinceLastVisit = 0.f;

	// 안 가본 곳이지만 아무 Need 도 풀어주지 않는다. 그 외 조건은 위와 완전히 동일하다
	const FFF_DestinationCandidate FreshButUseless =
		MakeCandidate(TEXT("PlainB"), NAME_None, 0.f, 100.f, 0.f, 10.f, 100.f);

	const TArray<FFF_DestinationCandidate> Candidates = { VisitedToilet, FreshButUseless };

	FFF_NeedState Urgent;
	Urgent.Restroom = 1.f;

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
		Candidates, TMap<FName, float>(), nullptr, 1, 1, 0, Urgent, 0, /*NoveltySeeking=*/1.6f);

	// 화장실 = need 1.8 - revisit 0.8 = +1.0 / 대안 = 0 → 급함이 이긴다
	TestTrue(TEXT("한계 Need 는 방금 다녀온 곳이라도 안 가본 무용지물을 이긴다"),
		FindProbability(Decision, TEXT("ToiletA")) > FindProbability(Decision, TEXT("PlainB")));

	// Need 가 없으면 뒤집힌다 — 페널티만 남으므로 안 가본 곳이 이긴다
	const FFF_DestinationDecision NoNeed = FFF_UtilityDestinationSelector::Choose(
		Candidates, TMap<FName, float>(), nullptr, 1, 1, 0, FFF_NeedState(), 0, 1.6f);
	TestTrue(TEXT("Need 가 없으면 방금 다녀온 곳이 진다"),
		FindProbability(NoNeed, TEXT("ToiletA")) < FindProbability(NoNeed, TEXT("PlainB")));
	return true;
}

// weight_need 기본값이 부등식을 지키는지 코드 기본값만으로 검증한다.
//
// !!! 이 테스트가 왜 필요한가 !!!
// 2026-08-10 에 `weight_need` 의 **코드 기본값이 1.2, CSV 값이 1.8** 로 어긋나 있는 것을 발견했다.
// CSV 에 적힌 1.8 의 근거가 "weight_attraction(1.0) + lambda_affinity(0.6) = 1.6 보다 크다"인데
// 1.2 는 그 부등식을 깬다 — DataTable 이 안 붙은 경로에서 급한 욕구가 취향을 못 이겼다.
// 계수 로딩은 DataTable 이 필요해 테스트가 어렵지만, **부등식은 코드 기본값만으로 검증할 수 있다.**
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_UtilityDestinationSelector_NeedBeatsBestLikedAlternative, "FestaFiesta.NPC.UtilityDestinationSelector.NeedBeatsBestLikedAlternative", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_UtilityDestinationSelector_NeedBeatsBestLikedAlternative::RunTest(const FString& Parameters)
{
	// 가능한 최고의 대안: 매력도 만점(1.0) + 취향 만점(preference_vector_scale_ref 만큼)
	const FFF_DestinationCandidate BestLiked =
		MakeCandidate(TEXT("BestLiked"), TEXT("food_stall"), 1.0f, 0.f, 0.f, 0.f, 100.f);

	// 매력도 0, 취향 없음. 오직 한계 Need 를 해소한다는 것만 다르다
	FFF_DestinationCandidate UrgentRelief =
		MakeCandidate(TEXT("UrgentRelief"), NAME_None, 0.f, 0.f, 0.f, 0.f, 100.f);
	UrgentRelief.NeedRelief.bRelievesRestroom = true;

	const TArray<FFF_DestinationCandidate> Candidates = { BestLiked, UrgentRelief };
	// preference_vector_scale_ref 기본값 17 → raw 17 이면 preference_norm 이 1.0(만점)
	const TMap<FName, float> Preference = { { TEXT("food_stall"), 17.f } };

	FFF_NeedState AtLimit;
	AtLimit.Restroom = 1.f;   // need_priority_restroom 은 1.0 고정이라 가중치가 그대로 걸린다

	const FFF_DestinationDecision Decision = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0, AtLimit);

	TestTrue(TEXT("한계 Need 가 '가장 매력적이고 취향에도 맞는 대안'을 이긴다 — weight_need > weight_attraction + lambda_affinity"),
		FindProbability(Decision, TEXT("UrgentRelief")) > FindProbability(Decision, TEXT("BestLiked")));

	// 반대로 가벼운 Need(0.3)는 취향을 뒤집지 못한다 — 1.8 × 0.3 = 0.54 < 1.6
	FFF_NeedState Mild;
	Mild.Restroom = 0.3f;
	const FFF_DestinationDecision MildDecision = FFF_UtilityDestinationSelector::Choose(
		Candidates, Preference, nullptr, 1, 1, 0, Mild);
	TestTrue(TEXT("가벼운 Need 는 취향을 뒤집지 못한다"),
		FindProbability(MildDecision, TEXT("UrgentRelief")) < FindProbability(MildDecision, TEXT("BestLiked")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
