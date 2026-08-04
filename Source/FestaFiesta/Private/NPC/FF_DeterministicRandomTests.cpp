// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/FF_DeterministicRandom.h"
#include "DataTables/FF_IndividualVariationRow.h"
#include "DataTables/FF_UtilityCoefficientRow.h"
#include "Engine/DataTable.h"

// 6개 완료 조건(individual_variation_spec.md 8절)을 그대로 테스트 6개로 옮긴다.

namespace FFF_DeterministicRandomTestHelpers
{
	void AddVariationRow(UDataTable* Table, FName Key, float Min, float Mode, float Max)
	{
		FFF_IndividualVariationRow Row;
		Row.TraitKey = Key;
		Row.DistributionType = TEXT("triangular");
		Row.MinValue = Min;
		Row.ModeValue = Mode;
		Row.MaxValue = Max;
		Row.RandomPurpose = TEXT("Generation");
		Table->AddRow(Key, Row);
	}

	UDataTable* MakeTestVariationTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_IndividualVariationRow::StaticStruct();
		AddVariationRow(Table, TEXT("walk_speed_multiplier"), 0.85f, 1.00f, 1.15f);
		AddVariationRow(Table, TEXT("patience_multiplier"), 0.70f, 1.00f, 1.30f);
		AddVariationRow(Table, TEXT("crowd_tolerance_multiplier"), 0.70f, 1.00f, 1.30f);
		AddVariationRow(Table, TEXT("preference_strength_multiplier"), 0.60f, 1.00f, 1.40f);
		AddVariationRow(Table, TEXT("stay_duration_multiplier"), 0.70f, 0.90f, 1.60f);
		AddVariationRow(Table, TEXT("reevaluate_interval_multiplier"), 0.80f, 1.00f, 1.20f);
		AddVariationRow(Table, TEXT("leisureliness_axis"), 0.00f, 0.50f, 1.00f);
		return Table;
	}

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
		AddRow(TEXT("wait_threshold_minutes"), 15.f);
		AddRow(TEXT("density_threshold_ratio"), 0.85f);
		AddRow(TEXT("reevaluate_interval_seconds"), 120.f);
		return Table;
	}
}

// 1. 같은 GlobalSimulationSeed + StableAgentId는 몇 번을 다시 생성해도 모든 변주값이 동일하다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_SameSeedGivesSameResult, "FestaFiesta.NPC.DeterministicRandom.SameSeedGivesSameResult", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_SameSeedGivesSameResult::RunTest(const FString& Parameters)
{
	const uint32 SeedA = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("patience_multiplier"), 0, TEXT("Generation"));
	const uint32 SeedB = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("patience_multiplier"), 0, TEXT("Generation"));
	TestEqual(TEXT("같은 입력은 같은 TraitSeed를 낸다"), SeedA, SeedB);

	const float FractionA = FFF_DeterministicRandom::MakeUniformFraction(SeedA);
	const float FractionB = FFF_DeterministicRandom::MakeUniformFraction(SeedB);
	TestEqual(TEXT("같은 TraitSeed는 같은 균등난수를 낸다"), FractionA, FractionB);

	UDataTable* Table = FFF_DeterministicRandomTestHelpers::MakeTestVariationTable();
	const FFF_ResolvedIndividualTraits First = FFF_IndividualVariationResolver::Resolve(Table, 42, 7, 0, 0.3f);
	const FFF_ResolvedIndividualTraits Second = FFF_IndividualVariationResolver::Resolve(Table, 42, 7, 0, 0.3f);
	TestEqual(TEXT("같은 입력에 대한 Resolve는 PatienceMultiplier가 동일하다"), First.PatienceMultiplier, Second.PatienceMultiplier);
	TestEqual(TEXT("같은 입력에 대한 Resolve는 StayDurationMultiplier가 동일하다"), First.StayDurationMultiplier, Second.StayDurationMultiplier);
	return true;
}

// 2. GlobalSimulationSeed 또는 StableAgentId를 바꾸면 변주값이 달라진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_DifferentInputGivesDifferentResult, "FestaFiesta.NPC.DeterministicRandom.DifferentInputGivesDifferentResult", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_DifferentInputGivesDifferentResult::RunTest(const FString& Parameters)
{
	const uint32 SeedAgent7 = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("patience_multiplier"), 0, TEXT("Generation"));
	const uint32 SeedAgent8 = FFF_DeterministicRandom::MakeTraitSeed(42, 8, TEXT("patience_multiplier"), 0, TEXT("Generation"));
	TestNotEqual(TEXT("StableAgentId가 다르면 TraitSeed도 달라진다"), SeedAgent7, SeedAgent8);

	const uint32 SeedGlobal99 = FFF_DeterministicRandom::MakeTraitSeed(99, 7, TEXT("patience_multiplier"), 0, TEXT("Generation"));
	TestNotEqual(TEXT("GlobalSimulationSeed가 다르면 TraitSeed도 달라진다"), SeedAgent7, SeedGlobal99);
	return true;
}

// 3. 새 TraitKey를 추가해도 기존 TraitKey의 결과가 바뀌지 않는다 (독립 스트림).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_NewTraitDoesNotAffectExisting, "FestaFiesta.NPC.DeterministicRandom.NewTraitDoesNotAffectExisting", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_NewTraitDoesNotAffectExisting::RunTest(const FString& Parameters)
{
	const uint32 SeedBefore = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("walk_speed_multiplier"), 0, TEXT("Generation"));
	const float FractionBefore = FFF_DeterministicRandom::MakeUniformFraction(SeedBefore);

	// "새로 추가된 특성"을 하나 더 계산해도, 이미 계산해둔 walk_speed_multiplier의 Seed·값은 그대로다.
	const uint32 NewTraitSeed = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("newly_added_trait"), 0, TEXT("Generation"));
	FFF_DeterministicRandom::MakeUniformFraction(NewTraitSeed);

	const uint32 SeedAfter = FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("walk_speed_multiplier"), 0, TEXT("Generation"));
	const float FractionAfter = FFF_DeterministicRandom::MakeUniformFraction(SeedAfter);

	TestEqual(TEXT("새 TraitKey 계산 여부와 무관하게 기존 TraitSeed는 동일하다"), SeedBefore, SeedAfter);
	TestEqual(TEXT("새 TraitKey 계산 여부와 무관하게 기존 균등난수는 동일하다"), FractionBefore, FractionAfter);
	return true;
}

// 4. 모든 값은 정의된 Min/Max 범위 안에 있다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_ResultsStayWithinBounds, "FestaFiesta.NPC.DeterministicRandom.ResultsStayWithinBounds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_ResultsStayWithinBounds::RunTest(const FString& Parameters)
{
	// 삼각분포 자체: U 전 구간을 훑어 [Min,Max] 밖으로 나가지 않는지 확인.
	for (int32 i = 0; i <= 20; ++i)
	{
		const float U = i / 20.f;
		const float Sample = FFF_DeterministicRandom::SampleTriangularFromFraction(0.7f, 0.9f, 1.6f, U);
		TestTrue(FString::Printf(TEXT("U=%.2f 샘플 %.4f 은 [0.7,1.6] 안에 있어야 한다"), U, Sample), Sample >= 0.7f && Sample <= 1.6f);
	}

	// Resolve 결과: 여러 StableAgentId에 대해 6개 multiplier가 각자의 DataTable 범위 안에 있는지 확인.
	UDataTable* Table = FFF_DeterministicRandomTestHelpers::MakeTestVariationTable();
	for (int32 AgentId = 0; AgentId < 50; ++AgentId)
	{
		const FFF_ResolvedIndividualTraits Traits = FFF_IndividualVariationResolver::Resolve(Table, 42, AgentId, 0, 0.3f);
		TestTrue(TEXT("WalkSpeedMultiplier 범위"), Traits.WalkSpeedMultiplier >= 0.85f && Traits.WalkSpeedMultiplier <= 1.15f);
		TestTrue(TEXT("PatienceMultiplier 범위"), Traits.PatienceMultiplier >= 0.70f && Traits.PatienceMultiplier <= 1.30f);
		TestTrue(TEXT("CrowdToleranceMultiplier 범위"), Traits.CrowdToleranceMultiplier >= 0.70f && Traits.CrowdToleranceMultiplier <= 1.30f);
		TestTrue(TEXT("PreferenceStrengthMultiplier 범위"), Traits.PreferenceStrengthMultiplier >= 0.60f && Traits.PreferenceStrengthMultiplier <= 1.40f);
		TestTrue(TEXT("StayDurationMultiplier 범위"), Traits.StayDurationMultiplier >= 0.70f && Traits.StayDurationMultiplier <= 1.60f);
		TestTrue(TEXT("ReevaluateIntervalMultiplier 범위"), Traits.ReevaluateIntervalMultiplier >= 0.80f && Traits.ReevaluateIntervalMultiplier <= 1.20f);
		TestTrue(TEXT("LeisurelinessAxis 범위"), Traits.LeisurelinessAxis >= 0.f && Traits.LeisurelinessAxis <= 1.f);
	}
	return true;
}

// 5. 특성을 계산하는 순서를 바꿔도 각 특성의 최종값은 같다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_OrderIndependent, "FestaFiesta.NPC.DeterministicRandom.OrderIndependent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_OrderIndependent::RunTest(const FString& Parameters)
{
	// 순서 A: walk_speed 먼저, crowd_tolerance 나중.
	const float WalkSpeedFirst = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("walk_speed_multiplier"), 0, TEXT("Generation")));
	const float CrowdToleranceAfterWalk = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("crowd_tolerance_multiplier"), 0, TEXT("Generation")));

	// 순서 B: crowd_tolerance 먼저, walk_speed 나중.
	const float CrowdToleranceFirst = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("crowd_tolerance_multiplier"), 0, TEXT("Generation")));
	const float WalkSpeedAfterCrowd = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("walk_speed_multiplier"), 0, TEXT("Generation")));

	TestEqual(TEXT("계산 순서와 무관하게 walk_speed_multiplier 값은 같다"), WalkSpeedFirst, WalkSpeedAfterCrowd);
	TestEqual(TEXT("계산 순서와 무관하게 crowd_tolerance_multiplier 값은 같다"), CrowdToleranceAfterWalk, CrowdToleranceFirst);
	return true;
}

// 6. correlation_strength = 0일 때 patience_multiplier/stay_duration_multiplier가 완전 독립 계산과 동일한 값을 낸다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DeterministicRandom_ZeroCorrelationMatchesIndependent, "FestaFiesta.NPC.DeterministicRandom.ZeroCorrelationMatchesIndependent", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DeterministicRandom_ZeroCorrelationMatchesIndependent::RunTest(const FString& Parameters)
{
	UDataTable* Table = FFF_DeterministicRandomTestHelpers::MakeTestVariationTable();
	const FFF_ResolvedIndividualTraits Traits = FFF_IndividualVariationResolver::Resolve(Table, 42, 7, 0, 0.f);

	const float IndependentPatienceFraction = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("patience_multiplier"), 0, TEXT("Generation")));
	const float ExpectedPatience = FFF_DeterministicRandom::SampleTriangularFromFraction(0.70f, 1.00f, 1.30f, IndependentPatienceFraction);
	TestEqual(TEXT("correlation_strength=0이면 PatienceMultiplier가 완전 독립 계산과 같다"), Traits.PatienceMultiplier, ExpectedPatience);

	const float IndependentStayFraction = FFF_DeterministicRandom::MakeUniformFraction(
		FFF_DeterministicRandom::MakeTraitSeed(42, 7, TEXT("stay_duration_multiplier"), 0, TEXT("Generation")));
	const float ExpectedStay = FFF_DeterministicRandom::SampleTriangularFromFraction(0.70f, 0.90f, 1.60f, IndependentStayFraction);
	TestEqual(TEXT("correlation_strength=0이면 StayDurationMultiplier가 완전 독립 계산과 같다"), Traits.StayDurationMultiplier, ExpectedStay);
	return true;
}

// 개인화 임계값: 전역 계수 * 개체 변주 multiplier로 정확히 계산되는지 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_IndividualVariationResolver_ResolvesPersonalThresholds, "FestaFiesta.NPC.DeterministicRandom.ResolvesPersonalThresholds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_IndividualVariationResolver_ResolvesPersonalThresholds::RunTest(const FString& Parameters)
{
	FFF_ResolvedIndividualTraits Traits;
	Traits.PatienceMultiplier = 1.2f;
	Traits.CrowdToleranceMultiplier = 0.8f;
	Traits.ReevaluateIntervalMultiplier = 1.1f;

	UDataTable* CoefficientTable = FFF_DeterministicRandomTestHelpers::MakeTestCoefficientTable();
	const FFF_PersonalReevaluationThresholds Thresholds = FFF_IndividualVariationResolver::ResolvePersonalThresholds(Traits, CoefficientTable);

	TestTrue(TEXT("WaitThresholdMinutes = 15 * 1.2"), FMath::IsNearlyEqual(Thresholds.WaitThresholdMinutes, 18.f, 0.01f));
	TestTrue(TEXT("DensityThresholdRatio = 0.85 * 0.8"), FMath::IsNearlyEqual(Thresholds.DensityThresholdRatio, 0.68f, 0.01f));
	TestTrue(TEXT("ReevaluateIntervalSeconds = 120 * 1.1"), FMath::IsNearlyEqual(Thresholds.ReevaluateIntervalSeconds, 132.f, 0.01f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
