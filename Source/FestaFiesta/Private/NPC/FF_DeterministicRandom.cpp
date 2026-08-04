// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/FF_DeterministicRandom.h"
#include "DataTables/FF_IndividualVariationRow.h"
#include "Engine/DataTable.h"
#include "Math/RandomStream.h"

uint32 FFF_DeterministicRandom::MakeTraitSeed(int32 GlobalSimulationSeed, int32 StableAgentId, FName TraitKey, int32 SpawnGeneration, FName RandomPurpose)
{
	uint32 Seed = GetTypeHash(GlobalSimulationSeed);
	Seed = HashCombine(Seed, GetTypeHash(StableAgentId));
	Seed = HashCombine(Seed, GetTypeHash(RandomPurpose));
	Seed = HashCombine(Seed, GetTypeHash(TraitKey));
	Seed = HashCombine(Seed, GetTypeHash(SpawnGeneration));
	return Seed;
}

float FFF_DeterministicRandom::MakeUniformFraction(uint32 TraitSeed)
{
	return FRandomStream(static_cast<int32>(TraitSeed)).GetFraction();
}

float FFF_DeterministicRandom::SampleTriangularFromFraction(float Min, float Mode, float Max, float UniformFraction01)
{
	const float U = FMath::Clamp(UniformFraction01, 0.f, 1.f);
	if (Max <= Min)
	{
		return Min;
	}

	const float ClampedMode = FMath::Clamp(Mode, Min, Max);
	const float RangeBelowMode = ClampedMode - Min;
	const float RangeAboveMode = Max - ClampedMode;
	const float Span = Max - Min;

	// C = 분포에서 Mode보다 낮은 쪽의 누적확률. RangeBelowMode가 0이면(Mode==Min) C도 0이 되어
	// 항상 아래쪽(else) 식으로만 값을 뽑는다 — 우측 비대칭 삼각분포가 자연스럽게 처리된다.
	const float C = RangeBelowMode / Span;

	if (U < C)
	{
		return Min + FMath::Sqrt(U * Span * RangeBelowMode);
	}
	return Max - FMath::Sqrt((1.f - U) * Span * RangeAboveMode);
}

namespace
{
	float ResolveUniformFraction(int32 GlobalSimulationSeed, int32 StableAgentId, FName TraitKey, int32 SpawnGeneration)
	{
		const uint32 Seed = FFF_DeterministicRandom::MakeTraitSeed(GlobalSimulationSeed, StableAgentId, TraitKey, SpawnGeneration, TEXT("Generation"));
		return FFF_DeterministicRandom::MakeUniformFraction(Seed);
	}

	float ShapeWithRow(const UDataTable* VariationTable, FName TraitKey, float UniformFraction01, float DefaultValue)
	{
		if (!VariationTable)
		{
			return DefaultValue;
		}
		const FFF_IndividualVariationRow* Row = VariationTable->FindRow<FFF_IndividualVariationRow>(TraitKey, TEXT("FFF_IndividualVariationResolver::Resolve"), /*bWarnIfRowMissing=*/false);
		if (!Row)
		{
			return DefaultValue;
		}
		return FFF_DeterministicRandom::SampleTriangularFromFraction(Row->MinValue, Row->ModeValue, Row->MaxValue, UniformFraction01);
	}
}

FFF_ResolvedIndividualTraits FFF_IndividualVariationResolver::Resolve(
	const UDataTable* VariationTable,
	int32 GlobalSimulationSeed,
	int32 StableAgentId,
	int32 SpawnGeneration,
	float CorrelationStrength)
{
	FFF_ResolvedIndividualTraits Out;
	const float Correlation = FMath::Clamp(CorrelationStrength, 0.f, 1.f);

	// leisureliness_axis: patience_multiplier·stay_duration_multiplier가 공유하는 성격축.
	const float LeisureFraction = ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("leisureliness_axis"), SpawnGeneration);
	Out.LeisurelinessAxis = ShapeWithRow(VariationTable, TEXT("leisureliness_axis"), LeisureFraction, 0.5f);

	// 상관 대상 2종: 고유 균등난수와 leisureliness 균등난수를 Correlation 비율로 블렌딩한 뒤 한 번만 분포에 통과시킨다.
	const float PatienceFraction = FMath::Lerp(ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("patience_multiplier"), SpawnGeneration), LeisureFraction, Correlation);
	Out.PatienceMultiplier = ShapeWithRow(VariationTable, TEXT("patience_multiplier"), PatienceFraction, 1.f);

	const float StayFraction = FMath::Lerp(ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("stay_duration_multiplier"), SpawnGeneration), LeisureFraction, Correlation);
	Out.StayDurationMultiplier = ShapeWithRow(VariationTable, TEXT("stay_duration_multiplier"), StayFraction, 1.f);

	// 나머지 4종은 v0에서 완전 독립 (independence_assumption).
	Out.WalkSpeedMultiplier = ShapeWithRow(VariationTable, TEXT("walk_speed_multiplier"),
		ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("walk_speed_multiplier"), SpawnGeneration), 1.f);

	Out.CrowdToleranceMultiplier = ShapeWithRow(VariationTable, TEXT("crowd_tolerance_multiplier"),
		ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("crowd_tolerance_multiplier"), SpawnGeneration), 1.f);

	Out.PreferenceStrengthMultiplier = ShapeWithRow(VariationTable, TEXT("preference_strength_multiplier"),
		ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("preference_strength_multiplier"), SpawnGeneration), 1.f);

	Out.ReevaluateIntervalMultiplier = ShapeWithRow(VariationTable, TEXT("reevaluate_interval_multiplier"),
		ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("reevaluate_interval_multiplier"), SpawnGeneration), 1.f);

	return Out;
}
