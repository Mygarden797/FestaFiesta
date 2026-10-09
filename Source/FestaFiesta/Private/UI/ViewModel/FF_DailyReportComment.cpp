// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/ViewModel/FF_DailyReportComment.h"

#define LOCTEXT_NAMESPACE "FFDailyReportComment"

namespace
{
	FText NeedShortageAdvice(const FFF_DailyReportCommentInputs& Inputs)
	{
		// 가장 모자란 시설을 짚는다. 커버리지가 같으면 화장실 → 음식 → 즐길 거리 순
		float Lowest = 1.f;
		FText Advice;
		auto Consider = [&Lowest, &Advice](float Coverage, const FText& Text)
		{
			if (Coverage < Lowest)
			{
				Lowest = Coverage;
				Advice = Text;
			}
		};
		Consider(Inputs.RestroomCoverage, LOCTEXT("NeedRestroom", "화장실이 모자랐어요. 내일은 화장실을 더 지어 보세요."));
		Consider(Inputs.HungerCoverage, LOCTEXT("NeedFood", "먹을 곳이 모자랐어요. 음식 부스를 더 지어 보세요."));
		Consider(Inputs.StimulationCoverage, LOCTEXT("NeedFun", "즐길 거리가 모자랐어요. 체험 부스나 포토존을 더해 보세요."));
		return Advice;
	}

	FText ComplaintLine(const FFF_DailyReportCommentInputs& Inputs)
	{
		if (!Inputs.bHasSatisfaction)
		{
			return FText::GetEmpty();
		}

		struct FAxis { int32 Score; FText Text; };
		const FText NeedText = NeedShortageAdvice(Inputs);
		const FAxis Axes[] =
		{
			{ Inputs.NeedScore, NeedText.IsEmpty()
				? LOCTEXT("NeedGeneric", "필요한 시설을 제때 못 찾은 손님이 많았어요. 시설을 고루 지어 보세요.") : NeedText },
			{ Inputs.WaitScore, LOCTEXT("Wait", "줄이 길다는 불만이 가장 컸어요. 인기 시설 옆에 같은 종류를 하나 더 지어 보세요.") },
			{ Inputs.DensityScore, LOCTEXT("Density", "너무 붐빈다는 불만이 가장 컸어요. 시설을 넓게 떨어뜨려 지어 보세요.") },
			{ Inputs.PreferenceScore, LOCTEXT("Preference", "취향에 맞는 곳이 없다는 손님이 많았어요. 다른 종류의 시설을 더해 보세요.") },
		};

		const FAxis* Worst = nullptr;
		for (const FAxis& Axis : Axes)
		{
			if (Axis.Score >= 0 && Axis.Score < FFF_DailyReportCommentRules::ComplaintScoreBelow
				&& (!Worst || Axis.Score < Worst->Score))
			{
				Worst = &Axis;
			}
		}
		return Worst ? Worst->Text : LOCTEXT("NoComplaint", "손님들이 대체로 만족했어요. 이대로만 가면 돼요!");
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
}

FText FFF_DailyReportCommentRules::Build(const FFF_DailyReportCommentInputs& Inputs)
{
	TArray<FText> Lines;
	if (!Inputs.TopFacilityName.IsEmpty() && Inputs.TopFacilityGold > 0)
	{
		Lines.Add(FText::Format(LOCTEXT("TopFacility", "오늘은 {0}에서 {1} Gold로 가장 많이 벌었어요."),
			Inputs.TopFacilityName, FText::AsNumber(Inputs.TopFacilityGold)));
	}
	const FText Complaint = ComplaintLine(Inputs);
	if (!Complaint.IsEmpty())
	{
		Lines.Add(Complaint);
	}
	const FText Goal = GoalLine(Inputs);
	if (!Goal.IsEmpty())
	{
		Lines.Add(Goal);
	}
	return FText::Join(FText::FromString(TEXT("\n")), Lines);
}

#undef LOCTEXT_NAMESPACE
