// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/Rules/FF_FestivalEvaluationRules.h"
#include "Economy/Rules/FF_VisitorForecastRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"

namespace FFF_VisitorForecastTestHelpers
{
	struct FForecastHarness
	{
		bool Initialize()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			UDataTable* CostTable = NewObject<UDataTable>(GameInstance);
			if (!GameInstance || !Progression || !Definition || !CostTable) return false;

			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;
			Definition->FestivalId = TEXT("ForecastAutomationFestival");
			Definition->InitialGold = 10000;
			Definition->BaseDailyVisitors = 500;
	Definition->AdmissionFeeGold = 5; // 수요 곡선 산술을 기준값 5 로 검증한다

			Progression->StartFestival(Definition);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
	};
}

// 입장료 수요 곡선 — 기획서 표(0 / 5 / 10 / 15 Gold → ×1.15 / 1.00 / 0.75 / 0.50)를 고정한다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Forecast_AdmissionFeeCurve, "FestaFiesta.Economy.Forecast.AdmissionFeeCurve", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Forecast_AdmissionFeeCurve::RunTest(const FString& Parameters)
{
	auto Mult = [](int32 Fee) { return FFF_VisitorForecastRules::CalcAdmissionFeeMultiplier(Fee, 5, 0.05f, 0.5f, 1.15f); };
	TestEqual(TEXT("0 Gold 는 상한 ×1.15 에서 잘린다"), Mult(0), 1.15f, 1e-4f);
	TestEqual(TEXT("기준값 5 Gold 는 ×1.0"), Mult(5), 1.f, 1e-4f);
	TestEqual(TEXT("10 Gold 는 ×0.75"), Mult(10), 0.75f, 1e-4f);
	TestEqual(TEXT("15 Gold 는 ×0.5"), Mult(15), 0.5f, 1e-4f);
	TestEqual(TEXT("범위 밖은 하한에서 잘린다"), Mult(40), 0.5f, 1e-4f);
	return true;
}

// 화제성 — 배수, 하루 식음, SNS 글 한 건의 크기.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Forecast_BuzzRules, "FestaFiesta.Economy.Forecast.BuzzRules", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Forecast_BuzzRules::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("중립 50 은 ×1.0"), FFF_VisitorForecastRules::CalcBuzzMultiplier(50.f, 0.15f), 1.f, 1e-4f);
	TestEqual(TEXT("100 은 ×1.15"), FFF_VisitorForecastRules::CalcBuzzMultiplier(100.f, 0.15f), 1.15f, 1e-4f);
	TestEqual(TEXT("0 은 ×0.85"), FFF_VisitorForecastRules::CalcBuzzMultiplier(0.f, 0.15f), 0.85f, 1e-4f);
	TestEqual(TEXT("범위 밖 값은 잘린 뒤 계산한다"), FFF_VisitorForecastRules::CalcBuzzMultiplier(250.f, 0.15f), 1.15f, 1e-4f);

	TestEqual(TEXT("하루에 중립과의 거리 25% 를 줄인다"), FFF_BuzzRules::ApplyDailyDecay(90.f, 0.25f), 80.f, 1e-4f);
	TestEqual(TEXT("낮은 쪽도 중립으로 올라온다"), FFF_BuzzRules::ApplyDailyDecay(10.f, 0.25f), 20.f, 1e-4f);

	TestEqual(TEXT("긍정 글은 +, 강도에 비례"), FFF_BuzzRules::CalcSnsPostDelta(true, 50.f, 0.6f), 0.3f, 1e-4f);
	TestEqual(TEXT("부정 글은 -"), FFF_BuzzRules::CalcSnsPostDelta(false, 100.f, 0.6f), -0.6f, 1e-4f);
	return true;
}

// 예보 한 벌 — 입장료 때문에 몇 명 덜 오는지, 입장료 수입이 얼마인지까지.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Forecast_BuildForecast, "FestaFiesta.Economy.Forecast.BuildForecast", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Forecast_BuildForecast::RunTest(const FString& Parameters)
{
	UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>(GetTransientPackage());
	Definition->BaseDailyVisitors = 500;
	Definition->AdmissionFeeGold = 5; // 수요 곡선 산술을 기준값 5 로 검증한다

	const FFF_VisitorForecast Expensive = FFF_VisitorForecastRules::BuildForecast(
		*Definition, EFF_PromotionGrade::Basic, 10, 50.f);
	TestEqual(TEXT("지역 SNS · 10 Gold · 중립 화제성 = 375명"), Expensive.ExpectedVisitors, 375);
	TestEqual(TEXT("기준 입장료였다면 500명"), Expensive.VisitorsAtReferenceFee, 500);
	TestEqual(TEXT("입장료 때문에 125명 덜 온다"), Expensive.VisitorDeltaFromFee, -125);
	TestEqual(TEXT("입장료 수입 375 × 10"), Expensive.ExpectedAdmissionRevenue, 3750);

	const FFF_VisitorForecast NoPromotion = FFF_VisitorForecastRules::BuildForecast(
		*Definition, EFF_PromotionGrade::None, 5, 50.f);
	TestEqual(TEXT("홍보 없음은 ×0.85 라 손해다"), NoPromotion.ExpectedVisitors, 425);

	const FFF_VisitorForecast Influencer = FFF_VisitorForecastRules::BuildForecast(
		*Definition, EFF_PromotionGrade::Influencer, 5, 50.f);
	TestEqual(TEXT("인플루언서는 ×1.25"), Influencer.ExpectedVisitors, 625);

	const FFF_VisitorForecast Hot = FFF_VisitorForecastRules::BuildForecast(
		*Definition, EFF_PromotionGrade::Focused, 5, 100.f);
	TestEqual(TEXT("전국 홍보 × 화제성 최대 = 500 × 1.12 × 1.15"), Hot.ExpectedVisitors, 644);
	return true;
}

// 입장료 변경 API — 범위 밖 거부, 변경 즉시 예보에 반영, 새 축제면 기준값으로 돌아간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Forecast_SetAdmissionFee, "FestaFiesta.Economy.Forecast.SetAdmissionFee", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Forecast_SetAdmissionFee::RunTest(const FString& Parameters)
{
	using namespace FFF_VisitorForecastTestHelpers;
	FForecastHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize())) return false;
	UFF_FestivalProgressionSubsystem* Progression = Harness.Progression;

	TestEqual(TEXT("시작 입장료는 기준값"), Progression->GetAdmissionFee(), Harness.Definition->AdmissionFeeGold);
	TestEqual(TEXT("범위 밖은 거부"), Progression->TrySetAdmissionFee(99), EFF_AdmissionFeeResult::OutOfRange);
	TestEqual(TEXT("거부하면 그대로"), Progression->GetAdmissionFee(), Harness.Definition->AdmissionFeeGold);

	const int32 Before = Progression->GetExpectedVisitors();
	TestEqual(TEXT("10 Gold 로 바뀐다"), Progression->TrySetAdmissionFee(10), EFF_AdmissionFeeResult::Success);
	TestEqual(TEXT("현재 입장료가 바뀐다"), Progression->GetAdmissionFee(), 10);
	TestTrue(TEXT("비싸지면 예상 방문객이 준다"), Progression->GetExpectedVisitors() < Before);
	TestEqual(TEXT("예보도 같은 값"), Progression->GetVisitorForecast().ExpectedVisitors, Progression->GetExpectedVisitors());

	const FFF_VisitorForecast Preview = Progression->PreviewVisitorForecast(EFF_PromotionGrade::None, 0);
	TestEqual(TEXT("미리보기는 상태를 안 바꾼다"), Progression->GetAdmissionFee(), 10);
	TestEqual(TEXT("미리보기 입장료가 실린다"), Preview.AdmissionFeeGold, 0);

	Progression->StartFestival(Harness.Definition);
	TestEqual(TEXT("새 축제는 기준값으로 돌아간다"), Progression->GetAdmissionFee(), Harness.Definition->AdmissionFeeGold);
	return true;
}

// 인플루언서 등급을 살 수 있고, 가장 비싸고 가장 효과가 크다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Forecast_InfluencerTier, "FestaFiesta.Economy.Forecast.InfluencerTier", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Forecast_InfluencerTier::RunTest(const FString& Parameters)
{
	using namespace FFF_VisitorForecastTestHelpers;
	FForecastHarness Harness;
	if (!TestTrue(TEXT("픽스처가 구성된다"), Harness.Initialize())) return false;

	const UFF_FestivalDefinition* Def = Harness.Definition;
	TestTrue(TEXT("인플루언서가 가장 비싸다"), Def->PromotionInfluencer.CostGold > Def->PromotionFocused.CostGold);
	TestTrue(TEXT("인플루언서가 가장 효과가 크다"), Def->PromotionInfluencer.VisitorMultiplier > Def->PromotionFocused.VisitorMultiplier);

	const int32 Gold = Harness.Progression->GetGold();
	TestEqual(TEXT("인플루언서 홍보가 구매된다"),
		Harness.Progression->TryPurchasePromotion(EFF_PromotionGrade::Influencer), EFF_PromotionPurchaseResult::Success);
	TestEqual(TEXT("비용이 빠진다"), Harness.Progression->GetGold(), Gold - Def->PromotionInfluencer.CostGold);
	TestEqual(TEXT("예상 방문객 = 기본 × 1.25"), Harness.Progression->GetExpectedVisitors(), 625);
	TestEqual(TEXT("티어 조회가 화제성 시작값을 준다"),
		Harness.Progression->GetPromotionTier(EFF_PromotionGrade::Influencer).StartingBuzz, Def->PromotionInfluencer.StartingBuzz);
	TestEqual(TEXT("홍보 없음 티어는 0 Gold · 0.85"),
		Harness.Progression->GetPromotionTier(EFF_PromotionGrade::None).VisitorMultiplier, Def->NoPromotionVisitorMultiplier, 1e-4f);
	return true;
}

// 축제 평가 — 가중 평균, 등급 경계, 가장 못한 항목.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_Evaluation_Rules, "FestaFiesta.Economy.Evaluation.Rules", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_Evaluation_Rules::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("본전이면 재무 0"), FFF_FestivalEvaluationRules::CalcFinanceScore(10000, 10000, 6000), 0);
	TestEqual(TEXT("목표 이익의 절반이면 50"), FFF_FestivalEvaluationRules::CalcFinanceScore(10000, 13000, 6000), 50);
	TestEqual(TEXT("목표 이익 이상이면 100"), FFF_FestivalEvaluationRules::CalcFinanceScore(10000, 30000, 6000), 100);
	TestEqual(TEXT("목표가 없으면 시작 자금이 목표"), FFF_FestivalEvaluationRules::CalcFinanceScore(10000, 15000, 0), 50);
	TestEqual(TEXT("손해면 0"), FFF_FestivalEvaluationRules::CalcFinanceScore(10000, 0, 6000), 0);

	TestEqual(TEXT("85 는 S"), FFF_FestivalEvaluationRules::GradeForScore(85), EFF_FestivalEvaluationGrade::S);
	TestEqual(TEXT("84 는 A"), FFF_FestivalEvaluationRules::GradeForScore(84), EFF_FestivalEvaluationGrade::A);
	TestEqual(TEXT("39 는 D"), FFF_FestivalEvaluationRules::GradeForScore(39), EFF_FestivalEvaluationGrade::D);

	FFF_FestivalEvaluationInput Input;
	Input.DailySatisfaction = { 70, 80, 90 };
	Input.FinaleSatisfactionBonus = 5;
	Input.TotalAdmittedVisitors = 1200;
	Input.DailyVisitorGoal = 500;
	Input.DaysPlayed = 3;
	Input.InitialGold = 10000;
	Input.FinalGold = 13000;
	Input.TargetProfitGold = 6000;
	Input.Buzz = 60.f;
	const FFF_FestivalEvaluation Result = FFF_FestivalEvaluationRules::Evaluate(Input);

	TestEqual(TEXT("만족도 = 평균 80 + 피날레 5"), Result.SatisfactionScore, 85);
	TestEqual(TEXT("방문객 = 1200 / 1500"), Result.VisitorScore, 80);
	TestEqual(TEXT("재무 = 100 × 3000 / 6000"), Result.FinanceScore, 50);
	TestEqual(TEXT("화제성 그대로"), Result.BuzzScore, 60);
	// 0.4×85 + 0.25×80 + 0.2×50 + 0.15×60 = 34 + 20 + 10 + 9 = 73
	TestEqual(TEXT("가중 평균"), Result.TotalScore, 73);
	TestEqual(TEXT("73 은 A"), Result.Grade, EFF_FestivalEvaluationGrade::A);
	TestEqual(TEXT("가장 잘한 것은 만족도"), Result.StrongestAxis, EFF_FestivalEvaluationAxis::Satisfaction);
	TestEqual(TEXT("가장 못한 것은 재무"), Result.WeakestAxis, EFF_FestivalEvaluationAxis::Finance);
	TestFalse(TEXT("S 가 아니면 보완 한 줄이 있다"), Result.WeakestAxisComment.IsEmpty());
	TestFalse(TEXT("한 줄 평이 있다"), Result.Headline.IsEmpty());

	const FFF_FestivalEvaluation Empty = FFF_FestivalEvaluationRules::Evaluate(FFF_FestivalEvaluationInput());
	TestEqual(TEXT("기록이 없으면 만족도 중립 50"), Empty.SatisfactionScore, 50);
	return true;
}

#endif
