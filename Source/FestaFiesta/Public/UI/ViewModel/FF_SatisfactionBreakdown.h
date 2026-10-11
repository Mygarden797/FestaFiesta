// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/** 만족도를 깎은 항목. 결산 화면의 "점수가 깎인 이유" 한 줄이다. */
enum class EFF_SatisfactionFactor : uint8
{
	Need,
	Wait,
	Density,
	Preference,
	// 사건 주변 손님에게 들어간 감점
	Penalty,
	Count
};

/** 분해에 쓰는 값. 결산 시점의 만족도 집계 · 계수 · 시설 수를 그대로 옮긴다. 모르는 값은 음수로 둔다. */
struct FFF_SatisfactionBreakdownInputs
{
	// 최종 만족도와 감점 전 점수(0~100)
	int32 Satisfaction = INDEX_NONE;
	int32 BaseSatisfaction = INDEX_NONE;

	// 하위 점수(0~100)
	int32 WaitScore = INDEX_NONE;
	int32 DensityScore = INDEX_NONE;
	int32 PreferenceScore = INDEX_NONE;
	int32 NeedScore = INDEX_NONE;

	// 만족도 계수의 가중치. 축(환경 · 개인 부합) 안에서 먼저 섞고 두 축을 다시 섞는다
	float WeightEnvironment = 0.4f;
	float WeightPersonalFit = 0.6f;
	float WeightWait = 0.5f;
	float WeightDensity = 0.5f;
	float WeightPreference = 0.35f;
	float WeightNeed = 0.65f;

	// 대기 근거: 평균 완료 대기(초) · 대기 점수가 0 이 되는 기준(초) · 줄 가입 성공률(0~1)
	float AverageWaitSeconds = -1.f;
	float WaitRefSeconds = 300.f;
	float QueueSuccessRate = -1.f;
	FText LongestWaitFacility;

	// 밀도 근거: 붐비는 칸(D 등급 이하)에 있던 손님 비율(0~1)
	float CrowdedShare = -1.f;

	// 취향 근거: 취향에 맞는 곳을 고른 결정 비율(0~1)과 그 결정 수. 결정이 적으면 비율이 출렁여 근거로 쓰지 않는다
	float PreferenceMatchShare = -1.f;
	int32 PreferenceSampleCount = 0;

	// 욕구 근거. 순서는 음식(Hunger) · 화장실(Restroom) · 즐길 거리(Stimulation)
	static constexpr int32 NeedCount = 3;
	int32 ReliefFacilityCount[NeedCount] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	float CoverageTarget[NeedCount] = { 0.f, 0.f, 0.f };
	float CoverageFloor = 0.3f;
	// 그 욕구를 못 채운 손님 비율(0~100)
	float UnmetPercent[NeedCount] = { -1.f, -1.f, -1.f };
};

/** 항목 하나가 깎은 점수와 그 근거. */
struct FFF_SatisfactionLoss
{
	EFF_SatisfactionFactor Factor = EFF_SatisfactionFactor::Need;
	int32 Points = 0;
	FText Evidence;
};

/**
 * 결산 만족도를 "100 에서 항목별로 깎인 점수"로 나눈 것.
 * 항목별 점수를 더하면 정확히 100 - 만족도가 된다. 플레이어가 화면의 숫자만으로 합을 맞춰 볼 수 있어야 점수를 납득한다.
 */
struct FESTAFIESTA_API FFF_SatisfactionBreakdown
{
	bool bValid = false;
	int32 Satisfaction = INDEX_NONE;

	// 많이 깎은 순. 욕구 · 대기 · 밀도 · 취향은 0 점이어도 넣고, 사건 영향은 0 보다 클 때만 넣는다
	TArray<FFF_SatisfactionLoss> Losses;

	// 가장 많이 깎은 항목을 줄이는 방법. 욕구는 시설 수로 오를 점수를 계산해 말한다
	FText Hint;

	// 그 항목이 깎은 점수. 분해가 없거나 항목이 없으면 INDEX_NONE
	int32 GetLoss(EFF_SatisfactionFactor Factor) const;

	static FText GetFactorLabel(EFF_SatisfactionFactor Factor);
};

struct FESTAFIESTA_API FFF_SatisfactionBreakdownRules
{
	static FFF_SatisfactionBreakdown Build(const FFF_SatisfactionBreakdownInputs& Inputs);

	// 힌트로 말할 만큼 오르는 최소 점수. 이보다 작으면 시설 수 대신 방향만 말한다
	static constexpr int32 MinHintGain = 1;

	// 취향 비율을 근거로 보여 줄 최소 결정 수
	static constexpr int32 MinPreferenceSamples = 10;
};
