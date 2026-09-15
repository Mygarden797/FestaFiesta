// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/Metrics/FF_SatisfactionMetricsSubsystem.h"

// UFF_SatisfactionMetricsSubsystem 자체는 GameInstance를 Outer로 요구하므로 테스트에서
// 만들 수 없다. 그래서 산출 계산을 FFF_SatisfactionAggregator(순수 구조체)로 분리했고
// 여기서는 그것만 검증한다. 서브시스템은 이 구조체를 감싸 델리게이트만 붙이는 얇은 층이다.

namespace FFF_SatisfactionMetricsTestHelpers
{
	// 세 항의 가중치가 각각 하나만 켜진 계수 — 나머지 두 항이 결과에 안 섞이는지
	// 확인할 때 쓴다. 밀도 점수표는 기본값(A=100 ~ F=0)을 그대로 둔다
	FFF_SatisfactionCoefficients MakeCoefficients(float WeightWait, float WeightDensity, float WeightPreference)
	{
		FFF_SatisfactionCoefficients Coefficients;
		Coefficients.WeightWait = WeightWait;
		Coefficients.WeightDensity = WeightDensity;
		Coefficients.WeightPreference = WeightPreference;
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
	FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(1.f, 0.f, 0.f);
	Coefficients.WaitRefSeconds = 300.f;

	Metrics.Begin();
	Metrics.AddWaitSample(150.f); // 기준의 절반
	// 150초/300초 → 대기 점수 50이 되는데, 이는 우연히 Commit 전 중립값(50)과 같다 —
	// 그래서 여기서는 "바뀌었는가"가 아니라 실제 값만 확인한다. bChanged는 아래
	// 900초 케이스(값이 확실히 달라짐)에서 검증한다
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("150초/300초 대기 → 대기 점수 50"), Metrics.GetWaitScore(), 50);
	TestEqual(TEXT("가중치를 대기에만 줬으므로 최종 점수도 50"), Metrics.GetSimSatisfaction(), 50);
	TestEqual(TEXT("표본 수 1"), Metrics.GetSampleCount(), 1);

	// 기준을 넘는 대기는 0으로 바닥나고, 음수로 안 내려간다. 50 → 0은 확실히 달라지므로
	// 여기서 bChanged도 함께 검증한다
	Metrics.Begin();
	Metrics.AddWaitSample(900.f);
	TestTrue(TEXT("50에서 0으로 바뀌었으니 Commit이 true를 돌려준다"), Metrics.Commit(Coefficients));
	TestEqual(TEXT("기준을 훌쩍 넘는 대기는 0으로 클램프된다"), Metrics.GetWaitScore(), 0);
	return true;
}

// 밀도 항 단독 — 사람 수로 가중한 평균이어야 한다 (셀 평균이 아니라)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_DensityScoreIsPersonWeighted, "FestaFiesta.NPC.SatisfactionMetrics.DensityScoreIsPersonWeighted", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_DensityScoreIsPersonWeighted::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(0.f, 1.f, 0.f);

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
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(0.f, 1.f, 0.f);

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
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(0.f, 0.f, 1.f);

	Metrics.Begin();
	Metrics.AddPreferenceSample(1.f); // 완전 선호
	Metrics.AddPreferenceSample(-1.f); // 완전 비선호
	Metrics.Commit(Coefficients);

	// 평균 0 → 중앙값 50
	TestEqual(TEXT("선호(+1)와 비선호(-1)의 평균은 중앙값(50)이다"), Metrics.GetPreferenceScore(), 50);
	return true;
}

// 세 입력이 모두 있을 때 가중합이 맞아야 한다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_BlendsThreeInputsByWeight, "FestaFiesta.NPC.SatisfactionMetrics.BlendsThreeInputsByWeight", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_BlendsThreeInputsByWeight::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	// 대기 100점, 밀도 0점, 취향 50점을 각각 0.5/0.3/0.2로 섞으면
	// 100*0.5 + 0*0.3 + 50*0.2 = 60
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(0.5f, 0.3f, 0.2f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 대기 0초 → 점수 100
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(5), 1); // F등급(0점)
	Metrics.AddPreferenceSample(0.f); // 중립 → 50
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("가중합 100*0.5 + 0*0.3 + 50*0.2 = 60"), Metrics.GetSimSatisfaction(), 60);
	return true;
}

// 가중치 합이 1이 아니어도(CSV 오기입) 정규화돼서 0~100 범위를 벗어나지 않는다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_WeightsAreNormalizedEvenIfMisconfigured, "FestaFiesta.NPC.SatisfactionMetrics.WeightsAreNormalizedEvenIfMisconfigured", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_WeightsAreNormalizedEvenIfMisconfigured::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	// 합이 1이 아니라 4다 — 정규화하지 않으면 400점이 나와야 정상이라는 뜻이므로,
	// 클램프가 아니라 정규화로 60이 나오는지 확인한다
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(2.f, 1.f, 1.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 100점
	Metrics.AddDensitySample(Coefficients.ScoreForLOSGrade(5), 1); // 0점
	Metrics.AddPreferenceSample(0.f); // 50점
	Metrics.Commit(Coefficients);

	// (100*2 + 0*1 + 50*1) / 4 = 62.5 → 반올림 63
	TestEqual(TEXT("가중치 합으로 나눠 정규화한다 (오기입에도 0~100을 벗어나지 않는다)"), Metrics.GetSimSatisfaction(), 63);
	return true;
}

// 이번 주기에 표본이 없는 입력은 가중합에서 빠지고, 나머지 입력만으로 정규화된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_MissingInputIsExcludedNotZeroed, "FestaFiesta.NPC.SatisfactionMetrics.MissingInputIsExcludedNotZeroed", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_MissingInputIsExcludedNotZeroed::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(1.f, 1.f, 1.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 100점. 밀도·취향은 이번 주기에 표본 없음
	Metrics.Commit(Coefficients);

	TestEqual(TEXT("밀도·취향이 빠졌으니 대기 점수(100) 하나로만 정해진다 — 0으로 안 섞인다"), Metrics.GetSimSatisfaction(), 100);
	return true;
}

// 세 입력이 전부 비어 있으면 확정하지 않고 직전 값을 유지한다 — Need 집계와 다른 정책이다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_AllInputsMissingKeepsLastValue, "FestaFiesta.NPC.SatisfactionMetrics.AllInputsMissingKeepsLastValue", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_AllInputsMissingKeepsLastValue::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;
	const FFF_SatisfactionCoefficients Coefficients = MakeCoefficients(1.f, 1.f, 1.f);

	Metrics.Begin();
	Metrics.AddWaitSample(0.f); // 100점
	Metrics.Commit(Coefficients);
	TestEqual(TEXT("첫 확정: 100"), Metrics.GetSimSatisfaction(), 100);

	// 두 번째 주기 — 아무 표본도 안 들어온다 (Live 시작 직후 등)
	Metrics.Begin();
	TestFalse(TEXT("표본이 하나도 없으면 Commit이 '안 바뀜'을 알린다"), Metrics.Commit(Coefficients));
	TestEqual(TEXT("직전 값(100)을 유지한다 — 0으로 되돌리면 '전원 불만족'처럼 보인다"), Metrics.GetSimSatisfaction(), 100);
	TestEqual(TEXT("표본 수는 0이라 '이번 주기는 데이터 없음'이 드러난다"), Metrics.GetSampleCount(), 0);
	return true;
}

// Begin() 없이 Add*를 부르면 거부된다
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_SatisfactionMetrics_AddSampleWithoutBeginIsRejected, "FestaFiesta.NPC.SatisfactionMetrics.AddSampleWithoutBeginIsRejected", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_SatisfactionMetrics_AddSampleWithoutBeginIsRejected::RunTest(const FString& Parameters)
{
	FFF_SatisfactionAggregator Metrics;

	TestFalse(TEXT("Begin() 없이 넣은 대기 표본은 거부된다"), Metrics.AddWaitSample(10.f));
	TestFalse(TEXT("Begin() 없이 넣은 밀도 표본은 거부된다"), Metrics.AddDensitySample(50.f, 1));
	TestFalse(TEXT("Begin() 없이 넣은 취향 표본은 거부된다"), Metrics.AddPreferenceSample(0.f));

	Metrics.Begin();
	TestTrue(TEXT("Begin() 후에는 받아들인다"), Metrics.AddWaitSample(10.f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
