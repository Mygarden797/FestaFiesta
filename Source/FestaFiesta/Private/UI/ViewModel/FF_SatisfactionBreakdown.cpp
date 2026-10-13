// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_SatisfactionBreakdown.h"

#define LOCTEXT_NAMESPACE "FFSatisfactionBreakdown"

namespace
{
	using FInputs = FFF_SatisfactionBreakdownInputs;

	FText NeedName(const int32 NeedIndex)
	{
		switch (NeedIndex)
		{
		case 0: return LOCTEXT("NeedFood", "음식");
		case 1: return LOCTEXT("NeedRestroom", "화장실");
		case 3: return LOCTEXT("NeedDrink", "음료");
		case 4: return LOCTEXT("NeedRest", "휴게");
		default: return LOCTEXT("NeedFun", "즐길 거리");
		}
	}

	// 목적격 조사까지 붙인 이름. "음식을 · 화장실을 · 즐길 거리를"
	FText NeedObject(const int32 NeedIndex)
	{
		switch (NeedIndex)
		{
		case 0: return LOCTEXT("NeedFoodObject", "음식을");
		case 1: return LOCTEXT("NeedRestroomObject", "화장실을");
		case 3: return LOCTEXT("NeedDrinkObject", "마실 곳을");
		case 4: return LOCTEXT("NeedRestObject", "쉴 곳을");
		default: return LOCTEXT("NeedFunObject", "즐길 거리를");
		}
	}

	FText Percent(const float Ratio01)
	{
		return FText::AsNumber(FMath::RoundToInt(FMath::Clamp(Ratio01, 0.f, 1.f) * 100.f));
	}

	float Coverage(const FInputs& Inputs, const int32 NeedIndex)
	{
		const float Target = Inputs.CoverageTarget[NeedIndex];
		if (Target <= 0.f || Inputs.ReliefFacilityCount[NeedIndex] < 0)
		{
			return 1.f;
		}
		return FMath::Clamp(Inputs.ReliefFacilityCount[NeedIndex] / Target, FMath::Clamp(Inputs.CoverageFloor, 0.f, 1.f), 1.f);
	}

	FText JoinParts(const TArray<FText>& Parts)
	{
		return FText::Join(FText::FromString(TEXT(" · ")), Parts);
	}

	// 켜진 욕구 수. 꺼진 욕구는 못 채운 비율이 음수로 들어온다
	int32 ActiveNeedCount(const FInputs& Inputs)
	{
		int32 Count = 0;
		for (int32 Index = 0; Index < FInputs::NeedCount; ++Index)
		{
			Count += Inputs.UnmetPercent[Index] >= 0.f ? 1 : 0;
		}
		return FMath::Max(Count, 1);
	}

	// 욕구 점수는 켜진 욕구들의 (100 - 미충족률) × 커버리지 평균이다. 그 욕구가 100 에서 모자란 만큼
	float NeedDeficit(const FInputs& Inputs, const int32 NeedIndex)
	{
		const float Unmet = FMath::Max(Inputs.UnmetPercent[NeedIndex], 0.f);
		return 100.f - (100.f - Unmet) * Coverage(Inputs, NeedIndex);
	}

	// 욕구 감점을 가장 많이 만든 욕구. 시설이 모자라서든 있어도 못 찾아서든 같은 잣대로 본다
	int32 WorstNeed(const FInputs& Inputs)
	{
		int32 Worst = INDEX_NONE;
		for (int32 Index = 0; Index < FInputs::NeedCount; ++Index)
		{
			if (NeedDeficit(Inputs, Index) > 0.5f && (Worst == INDEX_NONE || NeedDeficit(Inputs, Index) > NeedDeficit(Inputs, Worst)))
			{
				Worst = Index;
			}
		}
		return Worst;
	}

	FText FacilityCount(const FInputs& Inputs, const int32 NeedIndex)
	{
		return Coverage(Inputs, NeedIndex) < 1.f
			? FText::Format(LOCTEXT("NeedShort", "{0} 시설 {1}/{2}곳"), NeedName(NeedIndex),
				FText::AsNumber(Inputs.ReliefFacilityCount[NeedIndex]), FText::AsNumber(FMath::CeilToInt(Inputs.CoverageTarget[NeedIndex])))
			: FText::Format(LOCTEXT("NeedEnough", "{0} 시설 {1}곳"), NeedName(NeedIndex), FText::AsNumber(Inputs.ReliefFacilityCount[NeedIndex]));
	}

	FText NeedEvidence(const FInputs& Inputs)
	{
		const int32 Worst = WorstNeed(Inputs);
		if (Worst == INDEX_NONE)
		{
			return LOCTEXT("NeedFine", "필요한 시설은 충분했어요");
		}
		// 가장 많이 깎은 욕구를 먼저: 시설 수와 못 찾은 손님. 시설이 목표를 채웠는데도 못 찾았다면 거리나 줄이 문제다
		TArray<FText> Parts;
		if (Inputs.ReliefFacilityCount[Worst] >= 0)
		{
			Parts.Add(FacilityCount(Inputs, Worst));
		}
		if (Inputs.UnmetPercent[Worst] >= 1.f)
		{
			Parts.Add(FText::Format(LOCTEXT("NeedUnmet", "{0} 못 찾은 손님 {1}%"), NeedObject(Worst),
				FText::AsNumber(FMath::RoundToInt(Inputs.UnmetPercent[Worst]))));
		}
		for (int32 Index = 0; Index < FInputs::NeedCount; ++Index)
		{
			if (Index != Worst && Coverage(Inputs, Index) < 1.f)
			{
				Parts.Add(FacilityCount(Inputs, Index));
			}
		}
		return JoinParts(Parts);
	}

	FText WaitEvidence(const FInputs& Inputs)
	{
		TArray<FText> Parts;
		if (Inputs.AverageWaitSeconds >= 0.f)
		{
			Parts.Add(FText::Format(LOCTEXT("WaitAverage", "평균 {0}초 기다림(기준 {1}초)"),
				FText::AsNumber(FMath::RoundToInt(Inputs.AverageWaitSeconds)), FText::AsNumber(FMath::RoundToInt(Inputs.WaitRefSeconds))));
		}
		if (Inputs.QueueSuccessRate >= 0.f && Inputs.QueueSuccessRate < 1.f)
		{
			Parts.Add(FText::Format(LOCTEXT("WaitTurnedAway", "줄에서 돌아선 손님 {0}%"), Percent(1.f - Inputs.QueueSuccessRate)));
		}
		if (!Inputs.LongestWaitFacility.IsEmpty())
		{
			Parts.Add(FText::Format(LOCTEXT("WaitLongest", "가장 오래 기다린 곳 ‘{0}’"), Inputs.LongestWaitFacility));
		}
		return JoinParts(Parts);
	}

	FText DensityEvidence(const FInputs& Inputs)
	{
		return Inputs.CrowdedShare >= 0.f
			? FText::Format(LOCTEXT("DensityCrowded", "붐비는 곳에 있던 손님 {0}%"), Percent(Inputs.CrowdedShare))
			: FText::GetEmpty();
	}

	FText PreferenceEvidence(const FInputs& Inputs)
	{
		return Inputs.PreferenceMatchShare >= 0.f && Inputs.PreferenceSampleCount >= FFF_SatisfactionBreakdownRules::MinPreferenceSamples
			? FText::Format(LOCTEXT("PreferenceMatched", "취향에 맞는 곳을 찾은 손님 {0}%"), Percent(Inputs.PreferenceMatchShare))
			: FText::GetEmpty();
	}

	// 욕구 하나의 시설을 목표까지 채우면 오르는 만족도. Need 점수는 켜진 욕구들의 (100 - 미충족률) × 커버리지 평균이다
	float NeedGain(const FInputs& Inputs, const int32 NeedIndex, const float NeedWeight)
	{
		const float Current = Coverage(Inputs, NeedIndex);
		if (Current >= 1.f || Inputs.UnmetPercent[NeedIndex] < 0.f)
		{
			return 0.f;
		}
		return NeedWeight * (100.f - Inputs.UnmetPercent[NeedIndex]) * (1.f - Current) / ActiveNeedCount(Inputs);
	}

	FText MakeHint(const FInputs& Inputs, const EFF_SatisfactionFactor Top, const float NeedWeight)
	{
		switch (Top)
		{
		case EFF_SatisfactionFactor::Need:
		{
			const int32 Worst = WorstNeed(Inputs);
			if (Worst == INDEX_NONE)
			{
				return LOCTEXT("HintNeedGeneric", "손님이 필요한 시설을 제때 못 찾았어요. 먹을 곳 · 마실 곳 · 쉴 곳 · 화장실 · 즐길 거리를 고루 두세요.");
			}
			const float Gain = NeedGain(Inputs, Worst, NeedWeight);
			if (Coverage(Inputs, Worst) < 1.f && FMath::RoundToInt(Gain) >= FFF_SatisfactionBreakdownRules::MinHintGain)
			{
				const int32 Target = FMath::CeilToInt(Inputs.CoverageTarget[Worst]);
				return FText::Format(LOCTEXT("HintNeed", "{0} 시설을 {1}곳 더 지으면(목표 {2}곳) 만족도가 약 {3}점 올라요."),
					NeedName(Worst), FText::AsNumber(Target - Inputs.ReliefFacilityCount[Worst]),
					FText::AsNumber(Target), FText::AsNumber(FMath::RoundToInt(Gain)));
			}
			// 시설 수는 채웠는데 못 찾았다: 손님이 그 시설까지 가기엔 멀거나 줄이 길다
			return FText::Format(LOCTEXT("HintNeedReach", "{0} 시설은 있는데 손님이 제때 못 찾았어요. 손님이 몰리는 곳 가까이 옮기거나 하나 더 두세요."),
				NeedName(Worst));
		}
		case EFF_SatisfactionFactor::Wait:
			return Inputs.LongestWaitFacility.IsEmpty()
				? LOCTEXT("HintWait", "줄이 긴 시설 옆에 같은 시설을 하나 더 두면 대기 감점이 줄어요.")
				: FText::Format(LOCTEXT("HintWaitFacility", "‘{0}’ 옆에 같은 시설을 하나 더 두면 대기 감점이 줄어요."), Inputs.LongestWaitFacility);
		case EFF_SatisfactionFactor::Density:
			return LOCTEXT("HintDensity", "시설을 넓게 떨어뜨려 손님을 흩으면 밀도 감점이 줄어요.");
		case EFF_SatisfactionFactor::Preference:
			return LOCTEXT("HintPreference", "손님 취향은 제각각이에요. 다른 종류의 시설을 섞으면 취향 감점이 줄어요.");
		case EFF_SatisfactionFactor::Penalty:
			return LOCTEXT("HintPenalty", "사건에 스태프를 빨리 보내면 사건 영향이 줄어요.");
		default:
			return FText::GetEmpty();
		}
	}
}

int32 FFF_SatisfactionBreakdown::GetLoss(const EFF_SatisfactionFactor Factor) const
{
	for (const FFF_SatisfactionLoss& Loss : Losses)
	{
		if (Loss.Factor == Factor)
		{
			return Loss.Points;
		}
	}
	return INDEX_NONE;
}

FText FFF_SatisfactionBreakdown::GetFactorLabel(const EFF_SatisfactionFactor Factor)
{
	switch (Factor)
	{
	case EFF_SatisfactionFactor::Need: return LOCTEXT("LabelNeed", "욕구 충족");
	case EFF_SatisfactionFactor::Wait: return LOCTEXT("LabelWait", "대기");
	case EFF_SatisfactionFactor::Density: return LOCTEXT("LabelDensity", "밀도");
	case EFF_SatisfactionFactor::Preference: return LOCTEXT("LabelPreference", "취향 매칭");
	case EFF_SatisfactionFactor::Penalty: return LOCTEXT("LabelPenalty", "사건 영향");
	default: return FText::GetEmpty();
	}
}

FFF_SatisfactionBreakdown FFF_SatisfactionBreakdownRules::Build(const FFF_SatisfactionBreakdownInputs& Inputs)
{
	FFF_SatisfactionBreakdown Result;
	if (Inputs.Satisfaction < 0 || Inputs.BaseSatisfaction < 0 || Inputs.WaitScore < 0 || Inputs.DensityScore < 0
		|| Inputs.PreferenceScore < 0 || Inputs.NeedScore < 0)
	{
		return Result;
	}

	// 만족도 집계와 같은 2단 정규화로 항목별 실제 비중을 낸다
	const float Top = FMath::Max(Inputs.WeightEnvironment + Inputs.WeightPersonalFit, KINDA_SMALL_NUMBER);
	const float Environment = FMath::Max(Inputs.WeightWait + Inputs.WeightDensity, KINDA_SMALL_NUMBER);
	const float PersonalFit = FMath::Max(Inputs.WeightPreference + Inputs.WeightNeed, KINDA_SMALL_NUMBER);
	const float NeedWeight = Inputs.WeightPersonalFit / Top * Inputs.WeightNeed / PersonalFit;
	struct FRaw { EFF_SatisfactionFactor Factor; float Points; };
	TArray<FRaw, TInlineAllocator<4>> Raw = {
		{ EFF_SatisfactionFactor::Need, NeedWeight * (100 - Inputs.NeedScore) },
		{ EFF_SatisfactionFactor::Wait, Inputs.WeightEnvironment / Top * Inputs.WeightWait / Environment * (100 - Inputs.WaitScore) },
		{ EFF_SatisfactionFactor::Density, Inputs.WeightEnvironment / Top * Inputs.WeightDensity / Environment * (100 - Inputs.DensityScore) },
		{ EFF_SatisfactionFactor::Preference, Inputs.WeightPersonalFit / Top * Inputs.WeightPreference / PersonalFit * (100 - Inputs.PreferenceScore) },
	};

	// 반올림과 일부 입력이 빈 창 때문에 생기는 차이는 비율대로 맞추고, 남는 1점들은 소수점이 큰 항목부터 준다(최대 나머지 방식).
	// 그래야 화면의 항목 점수를 더한 값이 정확히 100 - 감점 전 점수가 된다
	const int32 BaseLoss = FMath::Clamp(100 - Inputs.BaseSatisfaction, 0, 100);
	float RawSum = 0.f;
	for (const FRaw& Item : Raw)
	{
		RawSum += Item.Points;
	}
	TArray<int32, TInlineAllocator<4>> Points;
	TArray<float, TInlineAllocator<4>> Remainders;
	int32 Assigned = 0;
	for (const FRaw& Item : Raw)
	{
		const float Scaled = RawSum > 0.f ? Item.Points * BaseLoss / RawSum : 0.f;
		Points.Add(FMath::FloorToInt(Scaled));
		Remainders.Add(Scaled - Points.Last());
		Assigned += Points.Last();
	}
	for (int32 Left = BaseLoss - Assigned; Left > 0; --Left)
	{
		int32 Best = 0;
		for (int32 Index = 1; Index < Remainders.Num(); ++Index)
		{
			if (Remainders[Index] > Remainders[Best])
			{
				Best = Index;
			}
		}
		++Points[Best];
		Remainders[Best] = -1.f;
	}

	for (int32 Index = 0; Index < Raw.Num(); ++Index)
	{
		FFF_SatisfactionLoss& Loss = Result.Losses.AddDefaulted_GetRef();
		Loss.Factor = Raw[Index].Factor;
		Loss.Points = Points[Index];
		switch (Loss.Factor)
		{
		case EFF_SatisfactionFactor::Need: Loss.Evidence = NeedEvidence(Inputs); break;
		case EFF_SatisfactionFactor::Wait: Loss.Evidence = WaitEvidence(Inputs); break;
		case EFF_SatisfactionFactor::Density: Loss.Evidence = DensityEvidence(Inputs); break;
		default: Loss.Evidence = PreferenceEvidence(Inputs); break;
		}
	}
	const int32 Penalty = FMath::Max(Inputs.BaseSatisfaction - Inputs.Satisfaction, 0);
	if (Penalty > 0)
	{
		FFF_SatisfactionLoss& Loss = Result.Losses.AddDefaulted_GetRef();
		Loss.Factor = EFF_SatisfactionFactor::Penalty;
		Loss.Points = Penalty;
		Loss.Evidence = LOCTEXT("PenaltyEvidence", "사건이 난 곳 근처 손님들의 불만");
	}

	// 많이 깎은 순. 같으면 욕구 → 대기 → 밀도 → 취향 → 사건(결산 한마디의 순서와 같다)
	Result.Losses.StableSort([](const FFF_SatisfactionLoss& A, const FFF_SatisfactionLoss& B) { return A.Points > B.Points; });
	Result.Satisfaction = Inputs.Satisfaction;
	Result.bValid = true;
	if (Result.Losses[0].Points > 0)
	{
		Result.Hint = MakeHint(Inputs, Result.Losses[0].Factor, NeedWeight);
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
