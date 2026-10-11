// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_DailyReportComment.h"

#define LOCTEXT_NAMESPACE "FFDailyReportComment"

namespace
{
	// 조언 하나. 마지막 날은 "내일"이 없으므로 다음 축제에 대한 말로 바꾼다
	struct FAdvice
	{
		FText Today;
		FText Final;
	};

	const FText& Pick(const FAdvice& Advice, bool bFinalDay)
	{
		return bFinalDay ? Advice.Final : Advice.Today;
	}

	FAdvice NeedAdvice(const FFF_DailyComplaint& Complaint)
	{
		if (Complaint.bHasNeedShortage)
		{
			switch (Complaint.NeedShortage)
			{
			case EFF_NeedType::Restroom:
				return {
					LOCTEXT("NeedRestroom", "화장실이 모자랐어요. 내일은 화장실을 더 지어 보세요."),
					LOCTEXT("NeedRestroomFinal", "가장 아쉬운 점은 화장실이 모자랐던 거예요. 다음 축제에서는 화장실부터 넉넉히 지어 보세요.") };
			case EFF_NeedType::Hunger:
				return {
					LOCTEXT("NeedFood", "먹을 곳이 모자랐어요. 음식 부스를 더 지어 보세요."),
					LOCTEXT("NeedFoodFinal", "가장 아쉬운 점은 먹을 곳이 모자랐던 거예요. 다음 축제에서는 음식 부스를 더 지어 보세요.") };
			case EFF_NeedType::Stimulation:
				return {
					LOCTEXT("NeedFun", "즐길 거리가 모자랐어요. 체험 부스나 포토존을 더해 보세요."),
					LOCTEXT("NeedFunFinal", "가장 아쉬운 점은 즐길 거리가 모자랐던 거예요. 다음 축제에서는 체험 부스나 포토존을 더해 보세요.") };
			default:
				break;
			}
		}
		return {
			LOCTEXT("NeedGeneric", "필요한 시설을 제때 못 찾은 손님이 많았어요. 시설을 고루 지어 보세요."),
			LOCTEXT("NeedGenericFinal", "가장 아쉬운 점은 필요한 시설을 제때 못 찾은 손님이 많았던 거예요. 다음 축제에서는 시설을 고루 지어 보세요.") };
	}

	FText ComplaintLine(const FFF_DailyReportCommentInputs& Inputs)
	{
		const FFF_DailyComplaint Complaint = FFF_DailyReportCommentRules::FindComplaint(Inputs);
		if (!Complaint.bJudged)
		{
			return FText::GetEmpty();
		}

		switch (Complaint.Axis)
		{
		case EFF_DailyComplaintAxis::Need:
			return Pick(NeedAdvice(Complaint), Inputs.bFinalDay);
		case EFF_DailyComplaintAxis::Wait:
			return Inputs.bFinalDay
				? LOCTEXT("WaitFinal", "가장 아쉬운 점은 긴 줄이었어요. 다음 축제에서는 인기 시설을 하나 더 지어 줄을 나눠 보세요.")
				: LOCTEXT("Wait", "줄이 길다는 불만이 가장 컸어요. 인기 시설 옆에 같은 종류를 하나 더 지어 보세요.");
		case EFF_DailyComplaintAxis::Density:
			return Inputs.bFinalDay
				? LOCTEXT("DensityFinal", "가장 아쉬운 점은 붐빔이었어요. 다음 축제에서는 시설을 넓게 떨어뜨려 지어 보세요.")
				: LOCTEXT("Density", "너무 붐빈다는 불만이 가장 컸어요. 시설을 넓게 떨어뜨려 지어 보세요.");
		case EFF_DailyComplaintAxis::Preference:
			return Inputs.bFinalDay
				? LOCTEXT("PreferenceFinal", "가장 아쉬운 점은 취향에 맞는 곳이 적었던 거예요. 다음 축제에서는 여러 종류의 시설을 섞어 보세요.")
				: LOCTEXT("Preference", "취향에 맞는 곳이 없다는 손님이 많았어요. 다른 종류의 시설을 더해 보세요.");
		default:
			return Inputs.bFinalDay
				? LOCTEXT("NoComplaintFinal", "손님들이 대체로 만족한 축제였어요. 정말 수고하셨어요!")
				: LOCTEXT("NoComplaint", "손님들이 대체로 만족했어요. 이대로만 가면 돼요!");
		}
	}

	FText GoalLine(const FFF_DailyReportCommentInputs& Inputs)
	{
		if (Inputs.AdmittedVisitors < 0 || Inputs.DailyVisitorGoal <= 0)
		{
			return FText::GetEmpty();
		}
		if (Inputs.AdmittedVisitors >= Inputs.DailyVisitorGoal)
		{
			return LOCTEXT("GoalMet", "방문객 목표도 넘겼어요. 소문이 나서 내일은 더 많이 올 거예요.");
		}
		return FText::Format(LOCTEXT("GoalMissed", "방문객 목표까지 {0}명 모자랐어요. 아침에 홍보를 해 보는 건 어때요?"),
			FText::AsNumber(Inputs.DailyVisitorGoal - Inputs.AdmittedVisitors));
	}

	// 마지막 날: 축제 전체를 돌아본다 — 3일 매출 1위, 만족도 흐름, 목표 달성 일수
	void AddFinalDayLines(const FFF_DailyReportCommentInputs& Inputs, TArray<FText>& Lines)
	{
		if (!Inputs.FestivalTopFacilityName.IsEmpty() && Inputs.FestivalTopFacilityGold > 0)
		{
			Lines.Add(FText::Format(LOCTEXT("FestivalTop", "축제 내내 {0}에서 {1} Gold로 가장 많이 벌었어요."),
				Inputs.FestivalTopFacilityName, FText::AsNumber(Inputs.FestivalTopFacilityGold)));
		}
		if (Inputs.FirstDaySatisfaction >= 0 && Inputs.FinalDaySatisfaction >= 0 && Inputs.DaysPlayed >= 2)
		{
			const int32 Delta = Inputs.FinalDaySatisfaction - Inputs.FirstDaySatisfaction;
			const FText First = FText::AsNumber(Inputs.FirstDaySatisfaction);
			const FText Last = FText::AsNumber(Inputs.FinalDaySatisfaction);
			if (Delta >= FFF_DailyReportCommentRules::SatisfactionTrendAtLeast)
			{
				Lines.Add(FText::Format(LOCTEXT("TrendUp", "손님 만족도가 첫날 {0}점에서 마지막 날 {1}점으로 올랐어요."), First, Last));
			}
			else if (Delta <= -FFF_DailyReportCommentRules::SatisfactionTrendAtLeast)
			{
				Lines.Add(FText::Format(LOCTEXT("TrendDown", "손님 만족도가 첫날 {0}점에서 마지막 날 {1}점으로 내려갔어요."), First, Last));
			}
			else
			{
				Lines.Add(FText::Format(LOCTEXT("TrendFlat", "손님 만족도는 첫날({0}점)과 비슷한 {1}점으로 마무리했어요."), First, Last));
			}
		}
		if (Inputs.DaysPlayed > 0 && Inputs.GoalDaysMet >= 0)
		{
			Lines.Add(Inputs.GoalDaysMet >= Inputs.DaysPlayed
				? FText::Format(LOCTEXT("GoalAll", "방문객 목표는 {0}일 모두 달성했어요."), FText::AsNumber(Inputs.DaysPlayed))
				: FText::Format(LOCTEXT("GoalSome", "방문객 목표는 {0}일 중 {1}일 달성했어요."),
					FText::AsNumber(Inputs.DaysPlayed), FText::AsNumber(Inputs.GoalDaysMet)));
		}
	}
}

FFF_DailyComplaint FFF_DailyReportCommentRules::FindComplaint(const FFF_DailyReportCommentInputs& Inputs)
{
	FFF_DailyComplaint Complaint;

	// 가장 모자란 시설. 커버리지가 같으면 화장실 → 음식 → 즐길 거리 순
	float Lowest = 1.f;
	const TPair<EFF_NeedType, float> Coverages[] =
	{
		{ EFF_NeedType::Restroom, Inputs.RestroomCoverage },
		{ EFF_NeedType::Hunger, Inputs.HungerCoverage },
		{ EFF_NeedType::Stimulation, Inputs.StimulationCoverage },
	};
	for (const TPair<EFF_NeedType, float>& Coverage : Coverages)
	{
		if (Coverage.Value < Lowest)
		{
			Lowest = Coverage.Value;
			Complaint.bHasNeedShortage = true;
			Complaint.NeedShortage = Coverage.Key;
		}
	}

	if (!Inputs.bHasSatisfaction)
	{
		return Complaint;
	}
	Complaint.bJudged = true;

	const TPair<EFF_DailyComplaintAxis, int32> Axes[] =
	{
		{ EFF_DailyComplaintAxis::Need, Inputs.NeedScore },
		{ EFF_DailyComplaintAxis::Wait, Inputs.WaitScore },
		{ EFF_DailyComplaintAxis::Density, Inputs.DensityScore },
		{ EFF_DailyComplaintAxis::Preference, Inputs.PreferenceScore },
	};
	int32 WorstScore = ComplaintScoreBelow;
	for (const TPair<EFF_DailyComplaintAxis, int32>& Axis : Axes)
	{
		if (Axis.Value >= 0 && Axis.Value < WorstScore)
		{
			WorstScore = Axis.Value;
			Complaint.Axis = Axis.Key;
		}
	}
	return Complaint;
}

FText FFF_DailyReportCommentRules::Build(const FFF_DailyReportCommentInputs& Inputs)
{
	TArray<FText> Lines;
	if (Inputs.bFinalDay)
	{
		AddFinalDayLines(Inputs, Lines);
	}
	else if (!Inputs.TopFacilityName.IsEmpty() && Inputs.TopFacilityGold > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("TopFacility", "오늘은 {0}에서 {1} Gold로 가장 많이 벌었어요."),
			Inputs.TopFacilityName, FText::AsNumber(Inputs.TopFacilityGold)));
	}
	const FText Complaint = ComplaintLine(Inputs);
	if (!Complaint.IsEmpty())
	{
		Lines.Add(Complaint);
	}
	if (!Inputs.bFinalDay)
	{
		const FText Goal = GoalLine(Inputs);
		if (!Goal.IsEmpty())
		{
			Lines.Add(Goal);
		}
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE
