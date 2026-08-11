// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/IndividualVariation/FF_DeterministicRandom.h"
#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "DataTables/FF_IndividualVariationRow.h"
#include "Engine/DataTable.h"
#include "Math/RandomStream.h"

uint32 FFF_DeterministicRandom::MakeStableNameHash(const FName Name)
{
	if (Name.IsNone())
	{
		// NAME_None 은 "키 없음"이다. 0 으로 고정해 두면 빈 문자열과도 같아지는데,
		// TraitKey 로 빈 이름을 쓰는 경우가 없어서 구분할 필요가 없다.
		return 0u;
	}

	// 힙 할당 없이 문자를 읽는다. TraitKey 는 짧아서 128 로 충분하고, 넘치면 빌더가 자동 확장한다.
	TStringBuilder<128> Builder;
	Name.AppendString(Builder);

	// FNV-1a. 엔진 버전·플랫폼·실행 순서와 무관하게 같은 문자열이면 같은 값이 나온다.
	uint32 Hash = 2166136261u;
	const TCHAR* Chars = Builder.GetData();
	for (int32 Index = 0; Index < Builder.Len(); ++Index)
	{
		// FName 비교가 대소문자를 무시하므로 소문자화해서 해시한다 — 안 하면 FName 으로는
		// 같다고 취급되는 두 이름이 서로 다른 시드를 낸다.
		Hash ^= static_cast<uint32>(FChar::ToLower(Chars[Index]));
		Hash *= 16777619u;
	}
	return Hash;
}

uint32 FFF_DeterministicRandom::MakeTraitSeed(int32 GlobalSimulationSeed, int32 StableAgentId, FName TraitKey, int32 SpawnGeneration, FName RandomPurpose)
{
	// 정수는 GetTypeHash 가 안정적이다(항등에 가까운 변환). FName 만 내용 해시로 바꾼다 —
	// GetTypeHash(FName) 은 프로세스마다 달라지는 인덱스를 해시한다(MakeStableNameHash 주석).
	uint32 Seed = GetTypeHash(GlobalSimulationSeed);
	Seed = HashCombine(Seed, GetTypeHash(StableAgentId));
	Seed = HashCombine(Seed, MakeStableNameHash(RandomPurpose));
	Seed = HashCombine(Seed, MakeStableNameHash(TraitKey));
	Seed = HashCombine(Seed, GetTypeHash(SpawnGeneration));
	return Seed;
}

uint32 FFF_DeterministicRandom::MakeDecisionSeed(int32 GlobalSimulationSeed, int32 StableAgentId, int32 ChoiceRoundIndex)
{
	// TraitKey 는 회차와 무관한 상수다. 회차는 아래에서 정수로 접는다.
	static const FName DecisionTraitKey(TEXT("destination_choice"));
	static const FName DecisionPurpose(TEXT("Decision"));

	uint32 Seed = MakeTraitSeed(GlobalSimulationSeed, StableAgentId, DecisionTraitKey, 0, DecisionPurpose);
	return HashCombine(Seed, GetTypeHash(ChoiceRoundIndex));
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

	// leisureliness 와 상관을 걸지 않는다 — "느긋한 사람이 새로운 걸 더 찾는다"는 근거가 없다.
	Out.NoveltySeekingMultiplier = ShapeWithRow(VariationTable, TEXT("novelty_seeking_multiplier"),
		ResolveUniformFraction(GlobalSimulationSeed, StableAgentId, TEXT("novelty_seeking_multiplier"), SpawnGeneration), 1.f);

	return Out;
}

FFF_ResolvedIndividualTraits FFF_IndividualVariationResolver::ResolveWithGroupSharing(
	const UDataTable* VariationTable,
	int32 GlobalSimulationSeed,
	int32 StableAgentId,
	int32 GroupId,
	int32 SpawnGeneration,
	float CorrelationStrength)
{
	FFF_ResolvedIndividualTraits Out = Resolve(VariationTable, GlobalSimulationSeed, StableAgentId, SpawnGeneration, CorrelationStrength);

	if (GroupId == INDEX_NONE)
	{
		// solo — 공유할 상대가 없다. Resolve() 한 번과 완전히 같은 결과여야 한다
		return Out;
	}

	// GroupId를 StableAgentId 자리에 넣어 "그룹의" 값을 뽑는다. 같은 GroupId면 멤버 전원이 같은 값을 얻는다.
	// group_composition_rules_v0.csv 의 group_shared_traits = walk_speed_multiplier;stay_duration_multiplier
	const FFF_ResolvedIndividualTraits GroupTraits = Resolve(VariationTable, GlobalSimulationSeed, GroupId, SpawnGeneration, CorrelationStrength);

	Out.WalkSpeedMultiplier = GroupTraits.WalkSpeedMultiplier;
	Out.StayDurationMultiplier = GroupTraits.StayDurationMultiplier;

	return Out;
}

FFF_PersonalReevaluationThresholds FFF_IndividualVariationResolver::ResolvePersonalThresholds(
	const FFF_ResolvedIndividualTraits& Traits,
	const UDataTable* UtilityCoefficientTable)
{
	FFF_PersonalReevaluationThresholds Out;
	const float GlobalWaitThreshold = FFF_UtilityCoefficientHelpers::GetCoefficient(UtilityCoefficientTable, TEXT("wait_threshold_minutes"), 15.f);
	const float GlobalDensityThreshold = FFF_UtilityCoefficientHelpers::GetCoefficient(UtilityCoefficientTable, TEXT("density_threshold_ratio"), 0.85f);
	const float GlobalReevaluateInterval = FFF_UtilityCoefficientHelpers::GetCoefficient(UtilityCoefficientTable, TEXT("reevaluate_interval_seconds"), 120.f);

	Out.WaitThresholdMinutes = GlobalWaitThreshold * Traits.PatienceMultiplier;
	Out.DensityThresholdRatio = GlobalDensityThreshold * Traits.CrowdToleranceMultiplier;
	Out.ReevaluateIntervalSeconds = GlobalReevaluateInterval * Traits.ReevaluateIntervalMultiplier;
	return Out;
}
