// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Metrics/FF_NpcNeedMetricsSubsystem.h"

// UFF_NpcNeedMetricsSubsystem 자체는 GameInstance를 Outer로 요구하므로 테스트에서 만들 수 없다.
// 그래서 집계 계산을 FFF_NeedMetricsAggregator(순수 구조체)로 분리했고 여기서는 그것만 검증한다.
// 서브시스템은 이 구조체를 감싸 델리게이트만 붙이는 얇은 층이다.

namespace FFF_NpcNeedMetricsTestHelpers
{
	FFF_NeedState MakeNeeds(float Hunger, float Restroom)
	{
		FFF_NeedState Needs;
		Needs.Hunger = Hunger;
		Needs.Restroom = Restroom;
		return Needs;
	}
}

using namespace FFF_NpcNeedMetricsTestHelpers;

// 표본이 없으면 0을 돌려주고, 표본 수로 "데이터 없음"을 구분할 수 있어야 한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NpcNeedMetrics_EmptyIsDistinguishableFromZero, "FestaFiesta.NPC.NeedMetrics.EmptyIsDistinguishableFromZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NpcNeedMetrics_EmptyIsDistinguishableFromZero::RunTest(const FString& Parameters)
{
	FFF_NeedMetricsAggregator Metrics;
	TArray<EFF_NeedType> Changed;

	TestEqual(TEXT("표본을 넣기 전 미해소율은 0이다"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 0.f);
	TestEqual(TEXT("표본을 넣기 전 표본 수는 0이다 — UI가 이걸로 '데이터 없음'을 구분한다"), Metrics.GetSampleCount(), 0);

	// 전원 Need 0 → 미해소율 0%이지만 표본 수는 0이 아니다
	Metrics.Begin();
	Metrics.AddSample(MakeNeeds(0.f, 0.f));
	Metrics.AddSample(MakeNeeds(0.f, 0.f));
	Metrics.Commit(Changed);

	TestEqual(TEXT("전원 해소 상태면 미해소율 0%"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 0.f);
	TestEqual(TEXT("이때 표본 수는 2다 — 0%와 '데이터 없음'이 구분된다"), Metrics.GetSampleCount(), 2);
	TestEqual(TEXT("값이 안 바뀌었으므로 변경 알림이 없다"), Changed.Num(), 0);
	return true;
}

// 미해소율은 임계값 이상인 개체의 비율(퍼센트)이다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NpcNeedMetrics_UnmetRatioIsPercentOverThreshold, "FestaFiesta.NPC.NeedMetrics.UnmetRatioIsPercentOverThreshold", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NpcNeedMetrics_UnmetRatioIsPercentOverThreshold::RunTest(const FString& Parameters)
{
	FFF_NeedMetricsAggregator Metrics;
	TArray<EFF_NeedType> Changed;
	TestEqual(TEXT("기본 임계값은 0.7"), Metrics.GetUnmetThreshold(), 0.7f);

	// Hunger: 4명 중 1명만 임계값 이상 → 25%
	// Restroom: 4명 중 3명이 임계값 이상 → 75%
	Metrics.Begin();
	Metrics.AddSample(MakeNeeds(0.9f, 0.8f));
	Metrics.AddSample(MakeNeeds(0.6f, 0.7f));  // 0.7은 경계 — 이상이므로 미해소로 센다
	Metrics.AddSample(MakeNeeds(0.1f, 0.95f));
	Metrics.AddSample(MakeNeeds(0.0f, 0.2f));
	Metrics.Commit(Changed);

	TestEqual(TEXT("Hunger 미해소율 25% (0~100 스케일)"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 25.f, 0.01f);
	TestEqual(TEXT("Restroom 미해소율 75% — 임계값과 같은 값도 미해소로 센다"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Restroom), 75.f, 0.01f);
	TestEqual(TEXT("Hunger 평균은 0~1 스케일"), Metrics.GetAverage(EFF_NeedType::Hunger), 0.4f, 0.01f);
	TestEqual(TEXT("표본을 안 넣은 Need는 0"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Stimulation), 0.f);
	TestEqual(TEXT("Hunger·Restroom 두 개만 바뀌었다고 알린다"), Changed.Num(), 2);
	return true;
}

// Commit 전에는 이전 확정값이 유지된다 (집계 중 값이 흔들려 보이지 않게)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NpcNeedMetrics_PendingIsNotVisibleUntilCommit, "FestaFiesta.NPC.NeedMetrics.PendingIsNotVisibleUntilCommit", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NpcNeedMetrics_PendingIsNotVisibleUntilCommit::RunTest(const FString& Parameters)
{
	FFF_NeedMetricsAggregator Metrics;
	TArray<EFF_NeedType> Changed;

	Metrics.Begin();
	Metrics.AddSample(MakeNeeds(0.9f, 0.f));
	Metrics.Commit(Changed);
	TestEqual(TEXT("첫 집계 확정: 100%"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 100.f, 0.01f);

	// 두 번째 주기 — Commit 전까지는 이전 값(100%)이 보여야 한다
	Metrics.Begin();
	Metrics.AddSample(MakeNeeds(0.f, 0.f));
	TestEqual(TEXT("Commit 전에는 이전 확정값이 유지된다"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 100.f, 0.01f);

	Metrics.Commit(Changed);
	TestEqual(TEXT("Commit 후 새 값으로 바뀐다"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 0.f, 0.01f);
	TestTrue(TEXT("Hunger가 바뀌었다고 알린다"), Changed.Contains(EFF_NeedType::Hunger));
	return true;
}

// Begin() 없이 AddSample을 부르면 무시하고 false를 돌려준다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NpcNeedMetrics_AddSampleWithoutBeginIsRejected, "FestaFiesta.NPC.NeedMetrics.AddSampleWithoutBeginIsRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NpcNeedMetrics_AddSampleWithoutBeginIsRejected::RunTest(const FString& Parameters)
{
	FFF_NeedMetricsAggregator Metrics;

	TestFalse(TEXT("Begin() 없이 넣은 표본은 거부된다"), Metrics.AddSample(MakeNeeds(0.9f, 0.9f)));
	TestEqual(TEXT("거부된 표본은 집계에 안 들어간다"), Metrics.GetSampleCount(), 0);

	Metrics.Begin();
	TestTrue(TEXT("Begin() 후에는 받아들인다"), Metrics.AddSample(MakeNeeds(0.9f, 0.9f)));
	return true;
}

// 임계값을 바꾸면 미해소율 판정이 따라 바뀐다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_NpcNeedMetrics_ThresholdChangesClassification, "FestaFiesta.NPC.NeedMetrics.ThresholdChangesClassification", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_NpcNeedMetrics_ThresholdChangesClassification::RunTest(const FString& Parameters)
{
	FFF_NeedMetricsAggregator Metrics;
	TArray<EFF_NeedType> Changed;

	Metrics.SetUnmetThreshold(0.5f);
	TestEqual(TEXT("임계값이 0.5로 바뀐다"), Metrics.GetUnmetThreshold(), 0.5f);

	Metrics.Begin();
	Metrics.AddSample(MakeNeeds(0.6f, 0.f));  // 0.7 기준이면 해소, 0.5 기준이면 미해소
	Metrics.AddSample(MakeNeeds(0.1f, 0.f));
	Metrics.Commit(Changed);

	TestEqual(TEXT("임계값 0.5에서 0.6은 미해소로 센다 → 50%"), Metrics.GetUnmetRatioPercent(EFF_NeedType::Hunger), 50.f, 0.01f);

	// 범위 밖 값은 클램프된다
	Metrics.SetUnmetThreshold(5.f);
	TestEqual(TEXT("임계값은 0~1로 클램프된다"), Metrics.GetUnmetThreshold(), 1.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
