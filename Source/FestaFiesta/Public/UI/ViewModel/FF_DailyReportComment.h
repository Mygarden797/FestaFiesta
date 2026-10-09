// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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

	// 시설 커버리지(하한~1). 1 미만인 욕구가 있으면 불만 원인으로 그 시설을 짚는다
	float HungerCoverage = 1.f;
	float RestroomCoverage = 1.f;
	float StimulationCoverage = 1.f;

	int32 AdmittedVisitors = INDEX_NONE;
	int32 DailyVisitorGoal = INDEX_NONE;
};

/**
 * 결산 화면의 비서 한마디. 매출 1위 시설 · 가장 낮은 만족도 항목(불만 1위)과 고칠 방법 · 방문객 목표를 한 줄씩 말한다.
 * 숫자를 다시 계산하지 않고 결산 표시 모델에 이미 있는 값만 문장으로 바꾼다.
 */
struct FESTAFIESTA_API FFF_DailyReportCommentRules
{
	static FText Build(const FFF_DailyReportCommentInputs& Inputs);

	// 이 점수 미만인 항목만 불만으로 짚는다. 모두 넘으면 칭찬한다
	static constexpr int32 ComplaintScoreBelow = 70;
};
