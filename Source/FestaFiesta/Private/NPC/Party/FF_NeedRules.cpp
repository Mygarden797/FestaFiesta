// Copyright Epic Games, Inc. All Rights Reserved.

#include "NPC/Party/FF_NeedRules.h"

#include "NPC/Destination/FF_UtilityCoefficientHelpers.h"
#include "NPC/IndividualVariation/FF_DeterministicRandom.h"

namespace
{
	// FFF_NeedState 의 성분 하나에 값을 쓴다. GetComponent 의 반대 방향이다.
	// 구조체에 SetComponent 를 두지 않은 이유: Need 값을 임의로 덮어쓰는 경로를 늘리면
	// "증가는 Advance, 해소는 Relieve" 규칙을 우회할 수 있게 된다. 규칙 파일 안에만 둔다.
	void SetNeedComponent(FFF_NeedState& Needs, EFF_NeedType NeedType, float Value)
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      Needs.Hunger = Value;      break;
		case EFF_NeedType::Thirst:      Needs.Thirst = Value;      break;
		case EFF_NeedType::Restroom:    Needs.Restroom = Value;    break;
		case EFF_NeedType::Fatigue:     Needs.Fatigue = Value;     break;
		case EFF_NeedType::Stimulation: Needs.Stimulation = Value; break;
		default: break;
		}
	}

	// FFF_NeedReliefFlags 의 성분별 플래그. 플래그 5개를 분기 없이 순회하기 위한 것이다.
	bool RelievesNeed(const FFF_NeedReliefFlags& Flags, EFF_NeedType NeedType)
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      return Flags.bRelievesHunger;
		case EFF_NeedType::Thirst:      return Flags.bRelievesThirst;
		case EFF_NeedType::Restroom:    return Flags.bRelievesRestroom;
		case EFF_NeedType::Fatigue:     return Flags.bRelievesFatigue;
		case EFF_NeedType::Stimulation: return Flags.bRelievesStimulation;
		default:                        return false;
		}
	}

	constexpr int32 NeedTypeCount = static_cast<int32>(EFF_NeedType::Count);

	/**
	 * 입장 시점 Need 흩뿌리기용 TraitKey. 성분마다 다른 키를 쓰는 이유는 두 가지다:
	 *   1. 성분별로 독립된 난수 스트림이 된다 — 배고픈 사람이 화장실도 급하다는 상관을 넣을 근거가 없다
	 *   2. 나중에 성분을 추가해도 기존 성분의 값이 바뀌지 않는다(FFF_DeterministicRandom 설계)
	 * 개체 변주(walk_speed_multiplier 등)와는 RandomPurpose 로 갈라진다.
	 */
	FName GetInitialNeedTraitKey(EFF_NeedType NeedType)
	{
		switch (NeedType)
		{
		case EFF_NeedType::Hunger:      return TEXT("need_initial_hunger");
		case EFF_NeedType::Thirst:      return TEXT("need_initial_thirst");
		case EFF_NeedType::Restroom:    return TEXT("need_initial_restroom");
		case EFF_NeedType::Fatigue:     return TEXT("need_initial_fatigue");
		case EFF_NeedType::Stimulation: return TEXT("need_initial_stimulation");
		default:                        return NAME_None;
		}
	}
}

FFF_NeedState FFF_NeedRules::Advance(
	const FFF_NeedState& Current,
	float ElapsedMinutes,
	const FFF_NeedRiseRates& RiseRates)
{
	FFF_NeedState Result = Current;

	// 음수·0 경과는 아무것도 하지 않는다. 음수를 그대로 계산하면 Need 가 조용히 줄어든다.
	if (ElapsedMinutes <= 0.f)
	{
		return Result;
	}

	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		const EFF_NeedType NeedType = static_cast<EFF_NeedType>(Index);
		const float RisePerMinute = RiseRates.GetRisePerMinute(NeedType);
		if (RisePerMinute <= 0.f)
		{
			// MinutesToLimit 이 0 = 이 Need 는 아직 켜지 않았다. P0 외 3종이 여기로 온다.
			continue;
		}

		const float Raised = Current.GetComponent(NeedType) + RisePerMinute * ElapsedMinutes;

		// 1.0 상한은 weight_need(1.8) 부등식의 전제다 — FF_NeedRules.h Advance 주석 참조.
		SetNeedComponent(Result, NeedType, FMath::Clamp(Raised, 0.f, 1.f));
	}

	return Result;
}

FFF_NeedState FFF_NeedRules::Relieve(
	const FFF_NeedState& Current,
	const FFF_NeedReliefFlags& ReliefFlags,
	const FFF_NeedReliefAmounts& ReliefAmounts)
{
	FFF_NeedState Result = Current;

	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		const EFF_NeedType NeedType = static_cast<EFF_NeedType>(Index);
		if (!RelievesNeed(ReliefFlags, NeedType))
		{
			continue;
		}

		const float ReliefAmount = ReliefAmounts.Get(NeedType);
		if (ReliefAmount <= 0.f)
		{
			// 해소량 0 = 아직 값을 정하지 않은 성분(Phase 대기). 플래그가 켜져 있어도 안 깎는다.
			continue;
		}

		const float Relieved = Current.GetComponent(NeedType) - ReliefAmount;
		SetNeedComponent(Result, NeedType, FMath::Clamp(Relieved, 0.f, 1.f));
	}

	return Result;
}

FFF_StayConsumption FFF_NeedRules::ConsumeStay(
	const FFF_NeedState& Needs,
	float ElapsedMinutes,
	float DrainMultiplierAtMaxNeed,
	const FFF_NeedPriorityWeights& Priority)
{
	FFF_StayConsumption Result;

	// 음수·0 경과는 아무것도 소모하지 않는다.
	if (ElapsedMinutes <= 0.f)
	{
		return Result;
	}

	// 1.0 아래는 "Need 가 쌓이면 더 오래 머문다"가 되어 허용하지 않는다.
	const float SafeMultiplier = FMath::Max(DrainMultiplierAtMaxNeed, 1.f);

	const float Urgency = Needs.GetMaxWeightedUrgency(Priority);
	const float Multiplier = 1.f + (SafeMultiplier - 1.f) * Urgency;

	Result.ConsumedStayBudgetMinutes = ElapsedMinutes * Multiplier;
	Result.ExtraStayBudgetFromNeed = Result.ConsumedStayBudgetMinutes - ElapsedMinutes;
	return Result;
}

FFF_NeedState FFF_NeedRules::MakeInitialNeeds(
	int32 GlobalSimulationSeed,
	int32 StableAgentId,
	int32 SpawnGeneration,
	const FFF_NeedRiseRates& RiseRates,
	float ModeFractionOfLimit,
	float MaxFractionOfLimit)
{
	FFF_NeedState Result;

	// 상한 0 이하면 흩뿌리기를 끈 것이다 — 전원 0 에서 시작한다.
	// 1.0 을 넘기면 입장 즉시 한계인 개체가 나오므로 clamp 한다.
	const float SafeMax = FMath::Clamp(MaxFractionOfLimit, 0.f, 1.f);
	if (SafeMax <= 0.f)
	{
		return Result;
	}
	// Mode 가 Max 를 넘으면 삼각분포가 성립하지 않는다.
	const float SafeMode = FMath::Clamp(ModeFractionOfLimit, 0.f, SafeMax);

	for (int32 Index = 0; Index < NeedTypeCount; ++Index)
	{
		const EFF_NeedType NeedType = static_cast<EFF_NeedType>(Index);

		// 안 쌓이는 Need 에 초기값을 주면 영원히 그 값에 머문다 — Advance 가 올리지도,
		// 시설이 없으면 Relieve 가 내리지도 않기 때문이다. 0 에서 시작한다.
		if (RiseRates.GetRisePerMinute(NeedType) <= 0.f)
		{
			continue;
		}

		const uint32 TraitSeed = FFF_DeterministicRandom::MakeTraitSeed(
			GlobalSimulationSeed,
			StableAgentId,
			GetInitialNeedTraitKey(NeedType),
			SpawnGeneration,
			TEXT("NeedInitial"));

		// Need 는 한계까지 0→1 로 선형이므로 "한계 대비 비율"이 곧 Need 값이다.
		const float InitialNeed = FFF_DeterministicRandom::SampleTriangularFromFraction(
			0.f, SafeMode, SafeMax,
			FFF_DeterministicRandom::MakeUniformFraction(TraitSeed));

		SetNeedComponent(Result, NeedType, FMath::Clamp(InitialNeed, 0.f, 1.f));
	}

	return Result;
}

FFF_NeedRiseRates FFF_NeedRules::LoadRiseRates(const UDataTable* UtilityCoefficientTable)
{
	// 기본값은 구조체 선언과 같게 유지한다 — 테이블이 없는 경로에서도 같은 행동이 나와야 한다.
	FFF_NeedRiseRates Rates;
	const auto GetCoefficient = [UtilityCoefficientTable](FName CoefficientId, float Fallback)
	{
		return FFF_UtilityCoefficientHelpers::GetCoefficient(UtilityCoefficientTable, CoefficientId, Fallback);
	};

	Rates.HungerMinutesToLimit = GetCoefficient(
		TEXT("need_hunger_minutes_to_limit"), Rates.HungerMinutesToLimit);
	Rates.RestroomMinutesToLimit = GetCoefficient(
		TEXT("need_restroom_minutes_to_limit"), Rates.RestroomMinutesToLimit);
	Rates.ThirstMinutesToLimit = GetCoefficient(
		TEXT("need_thirst_minutes_to_limit"), Rates.ThirstMinutesToLimit);
	Rates.FatigueMinutesToLimit = GetCoefficient(
		TEXT("need_fatigue_minutes_to_limit"), Rates.FatigueMinutesToLimit);
	Rates.StimulationMinutesToLimit = GetCoefficient(
		TEXT("need_stimulation_minutes_to_limit"), Rates.StimulationMinutesToLimit);

	return Rates;
}

float FFF_NeedRules::LoadStayDrainMultiplierAtMaxNeed(const UDataTable* UtilityCoefficientTable)
{
	// 기본값 1.0 = 기능 꺼짐. 근거 있는 값이 없어서 중립값을 둔다 — FF_NeedRules.h ConsumeStay 주석 참조.
	return FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("stay_drain_multiplier_at_max_need"), 1.0f);
}

FFF_NeedReliefAmounts FFF_NeedRules::LoadReliefAmounts(const UDataTable* UtilityCoefficientTable)
{
	FFF_NeedReliefAmounts Amounts;
	const auto GetCoefficient = [UtilityCoefficientTable](FName CoefficientId, float Fallback)
	{
		return FFF_UtilityCoefficientHelpers::GetCoefficient(UtilityCoefficientTable, CoefficientId, Fallback);
	};

	Amounts.Hunger = GetCoefficient(TEXT("need_hunger_relief_amount"), Amounts.Hunger);
	Amounts.Restroom = GetCoefficient(TEXT("need_restroom_relief_amount"), Amounts.Restroom);
	Amounts.Thirst = GetCoefficient(TEXT("need_thirst_relief_amount"), Amounts.Thirst);
	Amounts.Fatigue = GetCoefficient(TEXT("need_fatigue_relief_amount"), Amounts.Fatigue);
	Amounts.Stimulation = GetCoefficient(TEXT("need_stimulation_relief_amount"), Amounts.Stimulation);

	return Amounts;
}

void FFF_NeedRules::LoadInitialNeedFractions(
	const UDataTable* UtilityCoefficientTable,
	float& OutModeFractionOfLimit,
	float& OutMaxFractionOfLimit)
{
	// 기본값은 MakeInitialNeeds 선언의 기본 인자와 같게 유지한다 —
	// 테이블이 없는 경로에서도 같은 행동이 나와야 한다.
	OutModeFractionOfLimit = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_initial_fraction_mode"), 0.15f);
	OutMaxFractionOfLimit = FFF_UtilityCoefficientHelpers::GetCoefficient(
		UtilityCoefficientTable, TEXT("need_initial_fraction_max"), 0.50f);
}
