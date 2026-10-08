#include "Economy/Rules/FF_FestivalEvaluationRules.h"

#define LOCTEXT_NAMESPACE "FestivalEvaluation"

namespace
{
	int32 Clamp100(const float Value)
	{
		return FMath::Clamp(FMath::RoundToInt32(Value), 0, 100);
	}

	FText HeadlineFor(const EFF_FestivalEvaluationGrade Grade)
	{
		switch (Grade)
		{
		case EFF_FestivalEvaluationGrade::S: return LOCTEXT("HeadlineS", "전설로 남을 축제였다");
		case EFF_FestivalEvaluationGrade::A: return LOCTEXT("HeadlineA", "모두가 다시 오고 싶어 하는 축제였다");
		case EFF_FestivalEvaluationGrade::B: return LOCTEXT("HeadlineB", "무난하게 성공한 축제였다");
		case EFF_FestivalEvaluationGrade::C: return LOCTEXT("HeadlineC", "아쉬움이 남는 축제였다");
		default:                             return LOCTEXT("HeadlineD", "다음엔 더 잘할 수 있다");
		}
	}

	FText CommentFor(const EFF_FestivalEvaluationAxis Axis)
	{
		switch (Axis)
		{
		case EFF_FestivalEvaluationAxis::Satisfaction: return LOCTEXT("WeakSatisfaction", "손님들이 불편을 많이 겪었다");
		case EFF_FestivalEvaluationAxis::Visitors:     return LOCTEXT("WeakVisitors", "생각보다 손님이 적었다");
		case EFF_FestivalEvaluationAxis::Finance:      return LOCTEXT("WeakFinance", "남는 돈이 많지 않았다");
		default:                                       return LOCTEXT("WeakBuzz", "입소문이 크게 나지 않았다");
		}
	}
}

EFF_FestivalEvaluationGrade FFF_FestivalEvaluationRules::GradeForScore(const int32 TotalScore)
{
	if (TotalScore >= 85) return EFF_FestivalEvaluationGrade::S;
	if (TotalScore >= 70) return EFF_FestivalEvaluationGrade::A;
	if (TotalScore >= 55) return EFF_FestivalEvaluationGrade::B;
	if (TotalScore >= 40) return EFF_FestivalEvaluationGrade::C;
	return EFF_FestivalEvaluationGrade::D;
}

int32 FFF_FestivalEvaluationRules::CalcFinanceScore(const int32 InitialGold, const int32 FinalGold, const int32 TargetProfitGold)
{
	// 원금 대비 비율로 재면 입장료만으로 원금이 두 배가 돼 아무것도 안 해도 만점이었다.
	// 목표 이익을 따로 두어 투자해서 번 플레이와 돈만 아낀 플레이가 갈리게 한다.
	const int32 Target = TargetProfitGold > 0 ? TargetProfitGold : InitialGold;
	if (Target <= 0)
	{
		return 0;
	}
	const float Ratio = static_cast<float>(FinalGold - InitialGold) / static_cast<float>(Target);
	return Clamp100(100.f * Ratio);
}

FFF_FestivalEvaluation FFF_FestivalEvaluationRules::Evaluate(const FFF_FestivalEvaluationInput& Input)
{
	FFF_FestivalEvaluation Result;

	float SatisfactionAverage = 50.f;
	if (Input.DailySatisfaction.Num() > 0)
	{
		int64 Sum = 0;
		for (const int32 Value : Input.DailySatisfaction)
		{
			Sum += FMath::Clamp(Value, 0, 100);
		}
		SatisfactionAverage = static_cast<float>(Sum) / static_cast<float>(Input.DailySatisfaction.Num());
	}
	Result.SatisfactionScore = Clamp100(SatisfactionAverage + Input.FinaleSatisfactionBonus);

	const int64 GoalTotal = Input.VisitorGoalTotal > 0
		? static_cast<int64>(Input.VisitorGoalTotal)
		: static_cast<int64>(FMath::Max(Input.DailyVisitorGoal, 0)) * FMath::Max(Input.DaysPlayed, 0);
	Result.VisitorScore = GoalTotal > 0
		? Clamp100(100.f * static_cast<float>(FMath::Max(Input.TotalAdmittedVisitors, 0)) / static_cast<float>(GoalTotal))
		: 50;

	Result.FinanceScore = CalcFinanceScore(Input.InitialGold, Input.FinalGold, Input.TargetProfitGold);
	Result.BuzzScore = Clamp100(Input.Buzz);

	const float Weights[4] = {
		FMath::Max(Input.WeightSatisfaction, 0.f), FMath::Max(Input.WeightVisitors, 0.f),
		FMath::Max(Input.WeightFinance, 0.f), FMath::Max(Input.WeightBuzz, 0.f) };
	const int32 Scores[4] = { Result.SatisfactionScore, Result.VisitorScore, Result.FinanceScore, Result.BuzzScore };

	float WeightSum = 0.f;
	float Weighted = 0.f;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		WeightSum += Weights[Index];
		Weighted += Weights[Index] * Scores[Index];
	}
	Result.TotalScore = WeightSum > 0.f
		? Clamp100(Weighted / WeightSum)
		: Clamp100((Scores[0] + Scores[1] + Scores[2] + Scores[3]) / 4.f);

	int32 Strongest = 0;
	int32 Weakest = 0;
	for (int32 Index = 1; Index < 4; ++Index)
	{
		if (Scores[Index] > Scores[Strongest]) Strongest = Index;
		if (Scores[Index] < Scores[Weakest]) Weakest = Index;
	}
	Result.StrongestAxis = static_cast<EFF_FestivalEvaluationAxis>(Strongest);
	Result.WeakestAxis = static_cast<EFF_FestivalEvaluationAxis>(Weakest);

	Result.Grade = GradeForScore(Result.TotalScore);
	Result.Headline = HeadlineFor(Result.Grade);
	if (Result.Grade != EFF_FestivalEvaluationGrade::S)
	{
		Result.WeakestAxisComment = CommentFor(Result.WeakestAxis);
	}
	return Result;
}

#undef LOCTEXT_NAMESPACE
