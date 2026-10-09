// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"

// UFF_SatisfactionMetricsSubsystem 자체는 GameInstance를 Outer로 요구하므로 테스트에서
// 만들 수 없다. 그래서 산출 계산을 FFF_SatisfactionAggregator(순수 구조체)로 분리했고
// 여기서는 그것만 검증한다.

namespace FFF_SatisfactionMetricsTestHelpers
{
	// 환경 축(대기+밀도)만 살리고 개인 부합도 축을 꺼서, 대기·밀도 입력만 골라 확인할 때 쓴다
	FFF_SatisfactionCoefficients MakeEnvironmentOnlyCoefficients(float WeightWait, float WeightDensity)
	{
		FFF_SatisfactionCoefficients Coefficients;
		Coefficients.WeightEnvironment = 1.f;
		Coefficients.WeightPersonalFit = 0.f;
		Coefficients.WeightEnvironmentWait = WeightWait;
		Coefficients.WeightEnvironmentDensity = WeightDensity;
		return Coefficients;
	}

	// 개인 부합도 축(취향+Need)만 살리고 환경 축을 꺼서, 취향·Need 입력만 골라 확인할 때 쓴다
	FFF_SatisfactionCoefficients MakePersonalFitOnlyCoefficients(float WeightPreference, float WeightNeed)
	{
		FFF_SatisfactionCoefficients Coefficients;
		Coefficients.WeightEnvironment = 0.f;
		Coefficients.WeightPersonalFit = 1.f;
		Coefficients.WeightPersonalFitPreference = WeightPreference;
		Coefficients.WeightPersonalFitNeed = WeightNeed;
		return Coefficients;
	}
}

using namespace FFF_SatisfactionMetricsTestHelpers;

// 표본을 넣기 전에는 중립값 50이고, Commit 전까지는 값이 바뀌지 않는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_StartsAtNeutralNotDummy, "FestaFiesta.NPC.SatisfactionMetrics.StartsAtNeutralNotDummy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_StartsAtNeutralNotDummy::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;

	TestEqual(TEXT("Commit 전 중립값은 50이다 — 이전 더미(72)를 물려받지 않는다"), Metrics.GetSimSatisfaction(), 50);
	TestEqual(TEXT("표본 수는 0이다"), Metrics.GetSampleCount(), 0);
	return true;
}

// 대기 항 단독 — 대기시간이 기준(WaitRefSeconds)의 절반이면 점수도 절반
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_WaitScoreScalesWithSeconds, "FestaFiesta.NPC.SatisfactionMetrics.WaitScoreScalesWithSeconds", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_WaitScoreScalesWithSeconds::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(1.f, 0.f);
	Coefficients.WaitRefSeconds = 300.f;

	Metrics.Begin();
	Metrics.AddWaitSample(150.f); // 기준의 절반
	// 150초/300초 → 대기 점수 50이 되는데, 이는 우연히 Commit 전 중립값(50)과 같다 —
	// 그래서 여기서는 "바뀌었는가"가 아니라 실제 값만 확인한다
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("150초/300초 대기 → 대기 점수 50"), Metrics.GetWaitScore(), 50);
	TestEqual(TEXT("환경 축만 켰으므로 최종 점수도 50"), Metrics.GetSimSatisfaction(), 50);
	TestEqual(TEXT("표본 수 1"), Metrics.GetSampleCount(), 1);

	// 기준을 넘는 대기는 0으로 바닥나고, 음수로 안 내려간다
	Metrics.Begin();
	Metrics.AddWaitSample(900.f);
	TestTrue(TEXT("50에서 0으로 바뀌었으니 Commit이 true를 돌려준다"), Metrics.Commit(Coefficients));
	TestEqual(TEXT("기준을 훌쩍 넘는 대기는 0으로 클램프된다"), Metrics.GetWaitScore(), 0);
	return true;
}

// 큐 성공률이 대기 점수에 곱해진다 — 생존자 편향 보정(2026-09-15)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_QueueSuccessRateMultipliesWaitScore, "FestaFiesta.NPC.SatisfactionMetrics.QueueSuccessRateMultipliesWaitScore", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_QueueSuccessRateMultipliesWaitScore::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(1.f, 0.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 완료자는 대기 0초 → 원점수 100
	Metrics.SetQueueSuccessRate(0.5f); // 이번 창엔 절반만 성공적으로 가입했다
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("원점수 100 x 성공률 0.5 = 50"), Metrics.GetWaitScore(), 50);
	return true;
}

// 완료자가 이번 창에 하나도 없어도(전원 거절) 성공률만으로 대기 점수를 낸다 —
// "생존자 편향" 그 자체를 고치는 케이스
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_SuccessRateAloneCoversAllRejectedWindow, "FestaFiesta.NPC.SatisfactionMetrics.SuccessRateAloneCoversAllRejectedWindow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_SuccessRateAloneCoversAllRejectedWindow::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(1.f, 0.f);

	Metrics.Begin();
	// AddWaitSample을 한 번도 안 부른다 — 이번 창은 전부 거절/이탈이었다는 뜻
	Metrics.SetQueueSuccessRate(0.3f);
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("완료자가 없어도 낙관적 기본값(100) x 성공률(0.3) = 30"), Metrics.GetWaitScore(), 30);
	TestEqual(TEXT("성공률은 비율이라 표본 수 집계에는 안 잡힌다"), Metrics.GetSampleCount(), 0);
	return true;
}

// 밀도 항 단독 — 사람 수로 가중한 평균이어야 한다 (셀 평균이 아니라)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_DensityScoreIsPersonWeighted, "FestaFiesta.NPC.SatisfactionMetrics.DensityScoreIsPersonWeighted", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_DensityScoreIsPersonWeighted::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(0.f, 1.f);

	Metrics.Begin();
	// A등급(100점) 셀에 1명, F등급(0점) 셀에 9명 — 셀 단순평균(50)이 아니라
	// 사람 가중평균(10점)이 나와야 한다
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(0), 1);
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(5), 9);
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("사람 수로 가중한 평균이지 셀 단순평균이 아니다"), Metrics.GetDensityScore(), 10);
	TestEqual(TEXT("표본 수는 인원 합계(10)다"), Metrics.GetSampleCount(), 10);
	return true;
}

// 인원이 0인 셀은 무시된다 — 빈 셀이 밀도 점수를 흔들면 안 된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_EmptyCellsAreIgnored, "FestaFiesta.NPC.SatisfactionMetrics.EmptyCellsAreIgnored", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_EmptyCellsAreIgnored::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(0.f, 1.f);

	Metrics.Begin();
	TestFalse(TEXT("가중치(인원) 0인 셀은 표본으로 안 들어간다"), Metrics.AddDensitySample(0.f, 0));
	TestFalse(TEXT("음수 가중치도 거부한다"), Metrics.AddDensitySample(0.f, -1));
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(0), 5);
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("빈 셀은 무시되고 실제 셀만 반영된다"), Metrics.GetDensityScore(), 100);
	return true;
}

// 취향매칭 항 단독 — -1~1을 0~100으로 선형 매핑한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_PreferenceScoreMapsMinusOneToOne, "FestaFiesta.NPC.SatisfactionMetrics.PreferenceScoreMapsMinusOneToOne", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_PreferenceScoreMapsMinusOneToOne::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakePersonalFitOnlyCoefficients(1.f, 0.f);

	Metrics.Begin();
	Metrics.AddPreferenceSample(1.f); // 완전 선호
	Metrics.AddPreferenceSample(-1.f); // 완전 비선호
	Metrics.Commit(Coefficients);

	// 평균 0 → 중앙값 50
	TestEqual(TEXT("선호(+1)와 비선호(-1)의 평균은 중앙값(50)이다"), Metrics.GetPreferenceScore(), 50);
	return true;
}

// Need 충족도 항 단독 — 여러 성분의 평균이다 (2026-09-15 신설, gap 보완)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_NeedFulfillmentIsAverageOfComponents, "FestaFiesta.NPC.SatisfactionMetrics.NeedFulfillmentIsAverageOfComponents", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_NeedFulfillmentIsAverageOfComponents::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakePersonalFitOnlyCoefficients(0.f, 1.f);

	Metrics.Begin();
	Metrics.AddNeedFulfillmentSample(80.f); // Hunger 충족도
	Metrics.AddNeedFulfillmentSample(60.f); // Restroom 충족도
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("두 성분의 평균(70)이 나온다"), Metrics.GetNeedFulfillmentScore(), 70);
	TestEqual(TEXT("개인 부합도 축만 켰으므로 최종 점수도 70"), Metrics.GetSimSatisfaction(), 70);
	return true;
}

// 2단 계층 전체 블렌드 — 환경 축과 개인 부합도 축을 각각 만들고 최종에서 다시 섞는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_BlendsTwoTierHierarchy, "FestaFiesta.NPC.SatisfactionMetrics.BlendsTwoTierHierarchy", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_BlendsTwoTierHierarchy::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	// 전부 기본 가중치(축 0.5/0.5, 축 내부 0.5/0.5)를 쓴다
	const FFF_SatisfactionCoefficients Coefficients;

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 대기 점수 100
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(5), 1); // 밀도 점수 0 (F등급)
	Metrics.AddPreferenceSample(0.f); // 취향 점수 50 (중립)
	Metrics.AddNeedFulfillmentSample(100.f); // Need 충족도 100
	Metrics.Commit(Coefficients);

	// 환경 = (100*0.5 + 0*0.5) = 50, 개인부합 = (50*0.5 + 100*0.5) = 75
	TestEqual(TEXT("환경 축 = 대기·밀도 평균 = 50"), Metrics.GetEnvironmentScore(), 50);
	TestEqual(TEXT("개인 부합도 축 = 취향·Need 평균 = 75"), Metrics.GetPersonalFitScore(), 75);
	// 최종 = (50*0.5 + 75*0.5) = 62.5 -> 반올림 63
	TestEqual(TEXT("두 축을 다시 0.5/0.5로 섞으면 63"), Metrics.GetSimSatisfaction(), 63);
	return true;
}

// 축이 통째로 비어도(예: 이번 창에 취향·Need 표본이 하나도 없음) 남은 축 하나로만 정규화된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_MissingAxisIsExcludedNotZeroed, "FestaFiesta.NPC.SatisfactionMetrics.MissingAxisIsExcludedNotZeroed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_MissingAxisIsExcludedNotZeroed::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients; // 기본 0.5/0.5

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 환경 축만 채운다. 개인 부합도 축(취향·Need)은 이번 창에 없음
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("환경 축 = 대기 100점 하나뿐(밀도 없음) = 100"), Metrics.GetEnvironmentScore(), 100);
	TestEqual(TEXT("개인 부합도 축이 통째로 비었으니 환경 축 하나로만 정규화된 100이 최종값이다"), Metrics.GetSimSatisfaction(), 100);
	return true;
}

// 가중치 합이 1이 아니어도(CSV 오기입) 정규화돼서 0~100 범위를 벗어나지 않는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_WeightsAreNormalizedEvenIfMisconfigured, "FestaFiesta.NPC.SatisfactionMetrics.WeightsAreNormalizedEvenIfMisconfigured", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_WeightsAreNormalizedEvenIfMisconfigured::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	// 최상위 가중치 합이 1이 아니라 4다(오기입 상황을 흉내). 정규화되면 4배가 아니라
	// 정상 범위 안의 값이 나와야 한다
	FFF_SatisfactionCoefficients Coefficients;
	Coefficients.WeightEnvironment = 2.f;
	Coefficients.WeightPersonalFit = 2.f;

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 환경 축(대기만) = 100
	Metrics.AddPreferenceSample(0.f); // 개인 부합도 축(취향만) = 50
	Metrics.Commit(Coefficients);

	// 가중치가 같으므로(2:2) 결과는 단순 평균과 같다: (100+50)/2 = 75
	TestEqual(TEXT("최상위 가중치 합이 1이 아니어도 비율만 맞으면 정상 범위로 정규화된다"), Metrics.GetSimSatisfaction(), 75);
	return true;
}

// 네 입력이 전부 비어 있으면 확정하지 않고 직전 값을 유지한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_AllInputsMissingKeepsLastValue, "FestaFiesta.NPC.SatisfactionMetrics.AllInputsMissingKeepsLastValue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_AllInputsMissingKeepsLastValue::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients;

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 환경 축만으로 100
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("첫 확정: 환경 축 하나로 100"), Metrics.GetSimSatisfaction(), 100);

	// 두 번째 주기 — 아무 표본도 안 들어온다 (Live 시작 직후 등)
	Metrics.Begin();
	TestFalse(TEXT("표본이 하나도 없으면 Commit이 '안 바뀜'을 알린다"), Metrics.Commit(Coefficients));
	TestEqual(TEXT("직전 값(100)을 유지한다 — 0으로 되돌리면 '전원 불만족'처럼 보인다"), Metrics.GetSimSatisfaction(), 100);
	TestEqual(TEXT("표본 수는 0이라 '이번 주기는 데이터 없음'이 드러난다"), Metrics.GetSampleCount(), 0);
	return true;
}

// Begin() 없이 Add*/Set*를 부르면 거부된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_AddSampleWithoutBeginIsRejected, "FestaFiesta.NPC.SatisfactionMetrics.AddSampleWithoutBeginIsRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_AddSampleWithoutBeginIsRejected::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;

	TestFalse(TEXT("Begin() 없이 넣은 대기 표본은 거부된다"), Metrics.AddWaitSample(10.f));
	TestFalse(TEXT("Begin() 없이 설정한 성공률은 거부된다"), Metrics.SetQueueSuccessRate(0.5f));
	TestFalse(TEXT("Begin() 없이 넣은 밀도 표본은 거부된다"), Metrics.AddDensitySample(50.f, 1));
	TestFalse(TEXT("Begin() 없이 넣은 Need 충족도 표본은 거부된다"), Metrics.AddNeedFulfillmentSample(50.f));

	TestFalse(TEXT("Begin() 없이 넣은 감점 표본은 거부된다"), Metrics.AddPenaltySample(10.f));

	Metrics.Begin();
	TestTrue(TEXT("Begin() 후에는 받아들인다"), Metrics.AddWaitSample(10.f));
	return true;
}

// 취향 표본은 결정이 확정되는 집계 창 밖에서 들어온다. 거부하지 않고 다음 창이 이어받는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_PreferenceOutsideWindowCarriesToNextWindow, "FestaFiesta.NPC.SatisfactionMetrics.PreferenceOutsideWindowCarriesToNextWindow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_PreferenceOutsideWindowCarriesToNextWindow::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakePersonalFitOnlyCoefficients(1.f, 1.f);

	// 집계 창이 열리기 전에 확정된 결정. 실제 게임에서는 항상 이 순서다
	TestTrue(TEXT("창 밖의 취향 표본도 받는다"), Metrics.AddPreferenceSample(1.f));

	Metrics.Begin();
	Metrics.AddNeedFulfillmentSample(100.f);
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("모아 둔 표본(선호 1.0)이 이번 창에 반영된다 → 취향 100점"), Metrics.GetPreferenceScore(), 100);
	TestEqual(TEXT("취향 100 + Need 100 → 100"), Metrics.GetSimSatisfaction(), 100);

	// 다음 창에는 새 취향 표본이 없다. 지난 표본을 또 쓰면 안 된다
	Metrics.Begin();
	Metrics.AddNeedFulfillmentSample(50.f);
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("이어받은 표본은 한 번만 쓰인다 → 개인 부합도는 Need 50만"), Metrics.GetPersonalFitScore(), 50);
	TestEqual(TEXT("최종 50"), Metrics.GetSimSatisfaction(), 50);

	// 창 안에서 넣은 표본과 창 밖의 표본이 함께 평균된다
	Metrics.AddPreferenceSample(1.f);  // 창 밖
	Metrics.Begin();
	Metrics.AddPreferenceSample(-1.f); // 창 안
	Metrics.AddNeedFulfillmentSample(50.f);
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("선호 1.0과 비선호 -1.0의 평균 0 → 취향 50점"), Metrics.GetPreferenceScore(), 50);
	return true;
}

// 감점은 방문객 전체 평균으로 최종 점수에서 빠진다 — 일부만 깎이면 그 비율만큼만 내려간다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_PenaltyIsAveragedOverAllVisitors, "FestaFiesta.NPC.SatisfactionMetrics.PenaltyIsAveragedOverAllVisitors", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_PenaltyIsAveragedOverAllVisitors::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(1.f, 0.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 원점수 100
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("감점 표본이 없으면 감점 없이 100"), Metrics.GetSimSatisfaction(), 100);
	TestEqual(TEXT("감점 없음"), Metrics.GetPenaltyPoints(), 0);

	// 방문객 5명 중 1명이 50점 감점 → 평균 10점
	Metrics.Begin();
	Metrics.AddWaitSample(0.f);
	Metrics.AddPenaltySample(50.f);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Metrics.AddPenaltySample(0.f);
	}
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("평균 감점 10점"), Metrics.GetPenaltyPoints(), 10);
	TestEqual(TEXT("100 - 10 = 90"), Metrics.GetSimSatisfaction(), 90);
	TestEqual(TEXT("대기 점수 자체는 오염되지 않는다"), Metrics.GetWaitScore(), 100);
	return true;
}

// 재료가 없는 주기에도 감점이 창마다 쌓이지 않는다 — 이미 감점된 값에서 또 빼면 안 된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_PenaltyDoesNotStackOnEmptyWindow, "FestaFiesta.NPC.SatisfactionMetrics.PenaltyDoesNotStackOnEmptyWindow", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_PenaltyDoesNotStackOnEmptyWindow::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeEnvironmentOnlyCoefficients(1.f, 0.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f);
	Metrics.AddPenaltySample(20.f);
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("100 - 20 = 80"), Metrics.GetSimSatisfaction(), 80);

	// 재료는 없고 감점 표본만 있는 주기가 이어진다
	for (int32 Window = 0; Window < 3; ++Window)
	{
		Metrics.Begin();
		Metrics.AddPenaltySample(20.f);
		Metrics.Commit(Coefficients);
	}
	TestEqual(TEXT("3번 지나도 80이다 — 매 창마다 20씩 더 빠지지 않는다"), Metrics.GetSimSatisfaction(), 80);

	// 회복: 감점 표본이 0이 되면 원래 점수로 돌아온다
	Metrics.Begin();
	Metrics.AddPenaltySample(0.f);
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("감점이 사라지면 100으로 돌아온다"), Metrics.GetSimSatisfaction(), 100);
	return true;
}

// 시설 커버리지 배수: 목표 대비 비율, 하한과 1 사이로 자른다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_NeedCoverageMultiplierClampsToFloor, "FestaFiesta.NPC.SatisfactionMetrics.NeedCoverageMultiplierClampsToFloor", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_NeedCoverageMultiplierClampsToFloor::RunTest(const FString& Parameters)
{
	FFF_SatisfactionCoefficients Coefficients;
	Coefficients.NeedCoverageFloor = 0.3f;

	TestEqual(TEXT("시설이 없으면 하한"), Coefficients.NeedCoverageMultiplier(0, 3.f), 0.3f);
	TestEqual(TEXT("하한보다 큰 비율은 그대로"), Coefficients.NeedCoverageMultiplier(1, 2.f), 0.5f);
	TestEqual(TEXT("목표를 채우면 1"), Coefficients.NeedCoverageMultiplier(3, 3.f), 1.f);
	TestEqual(TEXT("목표를 넘어도 1"), Coefficients.NeedCoverageMultiplier(7, 3.f), 1.f);
	TestEqual(TEXT("목표가 0이면 보지 않는다"), Coefficients.NeedCoverageMultiplier(0, 0.f), 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
