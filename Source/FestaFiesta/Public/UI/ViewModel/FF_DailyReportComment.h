// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NPC/Party/FF_NeedState.h"

/** 결산 비서 한마디에 들어가는 오늘의 값. 모르는 값은 INDEX_NONE·빈 문구로 둔다. */
struct FFF_DailyReportCommentInputs
{
	FText TopFacilityName;
	int32 TopFacilityGold = 0;

	bool bHasSatisfaction = false;
	int32 WaitScore = INDEX_NONE;
	int32 DensityScore = INDEX_NONE;
	int32 PreferenceScore = INDEX_NONE;
	int32 NeedScore = INDEX_NONE;

	// 항목이 만족도에서 실제로 깎은 점수(FFF_SatisfactionBreakdown). 알면 불만 1위를 원점수가 아니라 이 값으로 고른다.
	// 비중이 큰 항목은 점수가 덜 낮아도 더 많이 깎는다 — 결산의 "점수가 깎인 이유" 맨 위 줄과 같은 말을 해야 납득된다
	int32 NeedLoss = INDEX_NONE;
	int32 WaitLoss = INDEX_NONE;
	int32 DensityLoss = INDEX_NONE;
	int32 PreferenceLoss = INDEX_NONE;

	// 시설 커버리지(하한~1). 1 미만인 욕구가 있으면 불만 원인으로 그 시설을 짚는다
	float HungerCoverage = 1.f;
	float RestroomCoverage = 1.f;
	float StimulationCoverage = 1.f;

	int32 AdmittedVisitors = INDEX_NONE;
	int32 DailyVisitorGoal = INDEX_NONE;

	// 마지막 날이면 "내일" 대신 축제 전체를 돌아본다. 아래 값은 마지막 날에만 쓴다
	bool bFinalDay = false;
	int32 DaysPlayed = 0;
	FText FestivalTopFacilityName;
	int32 FestivalTopFacilityGold = 0;
	int32 FirstDaySatisfaction = INDEX_NONE;
	int32 FinalDaySatisfaction = INDEX_NONE;
	// 기록된 날 중 방문객 목표를 넘긴 날 수. 모르면 INDEX_NONE
	int32 GoalDaysMet = INDEX_NONE;
};

/** 불만 1위로 고른 만족도 항목. */
enum class EFF_DailyComplaintAxis : uint8
{
	None,
	Need,
	Wait,
	Density,
	Preference
};

/** 하루의 판정. 결산 한마디와 다음 날 아침 비서 브리핑이 같은 판정을 쓴다. */
struct FFF_DailyComplaint
{
	// 만족도를 몰라 판정하지 못했으면 거짓. 참인데 Axis 가 None 이면 모든 항목이 기준 이상이다
	bool bJudged = false;
	EFF_DailyComplaintAxis Axis = EFF_DailyComplaintAxis::None;

	// 커버리지가 가장 모자란 필요(화장실 · 배고픔 · 자극 중). 모두 충분하면 거짓
	bool bHasNeedShortage = false;
	EFF_NeedType NeedShortage = EFF_NeedType::Hunger;
};

/**
 * 결산 화면의 비서 한마디. 매출 1위 시설 · 가장 낮은 만족도 항목(불만 1위)과 고칠 방법 · 방문객 목표를 한 줄씩 말한다.
 * 마지막 날은 축제 전체 매출 1위 · 만족도 흐름(첫날 → 마지막 날) · 목표 달성 일수 · 다음 축제에 대한 조언으로 바뀐다.
 * 숫자를 다시 계산하지 않고 결산 표시 모델에 이미 있는 값만 문장으로 바꾼다.
 */
struct FESTAFIESTA_API FFF_DailyReportCommentRules
{
	static FText Build(const FFF_DailyReportCommentInputs& Inputs);

	// 불만 1위 판정. 기준 점수 미만인 항목 중 깎은 점수(모르면 원점수가 가장 낮은 것)가 가장 큰 것.
	// 같으면 필요 → 줄 → 붐빔 → 취향 순으로 앞의 것을 고른다
	static FFF_DailyComplaint FindComplaint(const FFF_DailyReportCommentInputs& Inputs);

	// 이 점수 미만인 항목만 불만으로 짚는다. 모두 넘으면 칭찬한다
	static constexpr int32 ComplaintScoreBelow = 70;

	// 마지막 날 만족도 흐름에서 이만큼 이상 바뀌어야 "올랐다/내려갔다"고 말한다
	static constexpr int32 SatisfactionTrendAtLeast = 3;
};
