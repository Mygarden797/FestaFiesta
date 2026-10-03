// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Economy/Rules/FF_DayLedger.h"
#include "Economy/Rules/FF_VisitorForecastRules.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Festival/FF_FestivalDefinition.h"
#include "Festival/FF_FestivalProgressionSubsystem.h"
#include "Save/FF_SaveTypes.h"

/**
 * 축제 종료 직전까지의 연동: 평판 배수, 끝난 날 원장 보관, 저장 복원, 평가 동결, 최고 기록.
 * 월드를 만들지 않는다. FF_LedgerRowsTests.cpp 와 같은 방식이다.
 */
namespace FFF_FestivalEndTestHelpers
{
	struct FEndHarness
	{
		bool Initialize()
		{
			GameInstance = NewObject<UGameInstance>(GetTransientPackage());
			if (GameInstance == nullptr) return false;
			Progression = NewObject<UFF_FestivalProgressionSubsystem>(GameInstance);
			Definition = NewObject<UFF_FestivalDefinition>(GameInstance);
			UDataTable* CostTable = NewObject<UDataTable>(GameInstance);
			if (!Progression || !Definition || !CostTable) return false;

			CostTable->RowStruct = FFF_FacilityCostRow::StaticStruct();
			CostTable->AddRow(TEXT("Cost.Booth.FoodStall"), FFF_FacilityCostRow());
			Definition->FacilityCostTable = CostTable;
			Definition->FestivalId = TEXT("FestivalEndAutomationFestival");
			Definition->InitialGold = 10000;
			Definition->BaseDailyVisitors = 500;
			Progression->StartFestival(Definition);
			return true;
		}

		UGameInstance* GameInstance = nullptr;
		UFF_FestivalProgressionSubsystem* Progression = nullptr;
		UFF_FestivalDefinition* Definition = nullptr;
	};

	int32 SumAmounts(const TArray<FFF_LedgerRow>& Rows)
	{
		int32 Sum = 0;
		for (const FFF_LedgerRow& Row : Rows) Sum += Row.Amount;
		return Sum;
	}
}

// 평판 배수: 만족도 50 에서 1.0, 0 에서 하한, 100 에서 상한. 범위 밖은 잘린다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_ReputationCurve, "FestaFiesta.Economy.FestivalEnd.ReputationCurve", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_ReputationCurve::RunTest(const FString& Parameters)
{
	auto Mult = [](float Satisfaction)
	{
		return FFF_VisitorForecastRules::CalcReputationMultiplier(Satisfaction, 0.88f, 1.08f);
	};
	TestEqual(TEXT("중립 50 은 ×1.00"), Mult(50.f), 1.f, 1e-4f);
	TestEqual(TEXT("0 은 하한 ×0.88"), Mult(0.f), 0.88f, 1e-4f);
	TestEqual(TEXT("100 은 상한 ×1.08"), Mult(100.f), 1.08f, 1e-4f);
	TestEqual(TEXT("25 는 하한과 중립의 중간"), Mult(25.f), 0.94f, 1e-4f);
	TestEqual(TEXT("범위 밖은 잘린다"), Mult(-30.f), 0.88f, 1e-4f);
	TestEqual(TEXT("범위 밖은 잘린다(위)"), Mult(250.f), 1.08f, 1e-4f);
	// 상한이 1 미만이거나 하한이 1 초과로 잘못 저작돼도 1.0 을 가로지르지 않는다.
	TestEqual(TEXT("하한이 1 을 넘으면 1 로 본다"),
		FFF_VisitorForecastRules::CalcReputationMultiplier(0.f, 1.3f, 1.08f), 1.f, 1e-4f);
	return true;
}

// 첫날은 평판 배수가 1.0 이고, 예보에 곱해진다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_ReputationScalesForecast, "FestaFiesta.Economy.FestivalEnd.ReputationScalesForecast", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_ReputationScalesForecast::RunTest(const FString& Parameters)
{
	FFF_FestivalEndTestHelpers::FEndHarness Harness;
	if (!TestTrue(TEXT("픽스처"), Harness.Initialize())) return false;

	const FFF_VisitorForecast Day1 = Harness.Progression->GetVisitorForecast();
	TestEqual(TEXT("첫날 평판 배수는 1.0"), Day1.ReputationMultiplier, 1.f, 1e-4f);

	const FFF_VisitorForecast Boosted = FFF_VisitorForecastRules::BuildForecast(
		*Harness.Definition, EFF_PromotionGrade::None, Harness.Definition->AdmissionFeeGold, 50.f, 1.08f);
	const FFF_VisitorForecast Plain = FFF_VisitorForecastRules::BuildForecast(
		*Harness.Definition, EFF_PromotionGrade::None, Harness.Definition->AdmissionFeeGold, 50.f, 1.f);
	TestTrue(TEXT("평판이 좋으면 방문객이 는다"), Boosted.ExpectedVisitors > Plain.ExpectedVisitors);
	TestEqual(TEXT("배수가 예보에 그대로 실린다"), Boosted.ReputationMultiplier, 1.08f, 1e-4f);
	return true;
}

// 끝난 날의 원장을 모두 보관하고, 축제 전체 합계가 날짜별 합과 같다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_LedgerHistoryAndTotals, "FestaFiesta.Economy.FestivalEnd.LedgerHistoryAndTotals", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_LedgerHistoryAndTotals::RunTest(const FString& Parameters)
{
	FFF_FestivalEndTestHelpers::FEndHarness Harness;
	if (!TestTrue(TEXT("픽스처"), Harness.Initialize())) return false;

	Harness.Progression->AddRevenue(100, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(2);
	Harness.Progression->AddRevenue(50, EFF_EarnReason::AdmissionFee);
	Harness.Progression->BeginNewDay(3);
	Harness.Progression->AddRevenue(25, EFF_EarnReason::AdmissionFee);

	TestTrue(TEXT("Day1 원장이 남아 있다"), Harness.Progression->HasLedgerForDay(1));
	TestEqual(TEXT("Day1 매출"), FFF_FestivalEndTestHelpers::SumAmounts(Harness.Progression->GetEarnRowsForDay(1)), 100);
	TestEqual(TEXT("Day2 매출"), FFF_FestivalEndTestHelpers::SumAmounts(Harness.Progression->GetEarnRowsForDay(2)), 50);
	TestEqual(TEXT("Day3 매출"), FFF_FestivalEndTestHelpers::SumAmounts(Harness.Progression->GetEarnRowsForDay(3)), 25);
	TestEqual(TEXT("축제 전체 매출은 3일 합계"),
		FFF_FestivalEndTestHelpers::SumAmounts(Harness.Progression->GetEarnRowsForFestival()), 175);
	return true;
}

// 원장 장부 저장 표현이 왕복해도 값이 그대로다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_LedgerExportImport, "FestaFiesta.Economy.FestivalEnd.LedgerExportImport", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_LedgerExportImport::RunTest(const FString& Parameters)
{
	FFF_DayLedger Source;
	Source.RecordEarn(EFF_EarnReason::FacilityCommission, 300, FName(TEXT("Booth.A")));
	Source.RecordEarn(EFF_EarnReason::AdmissionFee, 120);
	Source.RecordSpend(EFF_SpendReason::StaffHire, 150);
	Source.RecordRefund(40);

	TArray<int32> Spend, Earn, FacilityRevenue;
	TArray<FName> FacilityIds;
	int32 Refund = 0;
	Source.Export(Spend, Earn, Refund, FacilityIds, FacilityRevenue);

	FFF_DayLedger Copy;
	Copy.Import(Spend, Earn, Refund, FacilityIds, FacilityRevenue);
	TestEqual(TEXT("수수료"), Copy.GetEarnByReason(EFF_EarnReason::FacilityCommission), 300);
	TestEqual(TEXT("입장료"), Copy.GetEarnByReason(EFF_EarnReason::AdmissionFee), 120);
	TestEqual(TEXT("고용 지출"), Copy.GetSpendByReason(EFF_SpendReason::StaffHire), 150);
	TestEqual(TEXT("환급"), Copy.GetTotalRefund(), 40);
	TestEqual(TEXT("부스별 표"), Copy.GetRevenueByFacility().FindRef(FName(TEXT("Booth.A"))), 300);

	// 칸 수가 모자란 옛 저장도 겹치는 칸만 옮기고 크래시하지 않는다.
	FFF_DayLedger FromShort;
	FromShort.Import({10}, {20}, 0, {}, {});
	TestEqual(TEXT("짧은 배열도 받는다"), FromShort.GetSpendByReason(EFF_SpendReason::Promotion), 0);
	TestEqual(TEXT("첫 칸은 옮겨진다"), FromShort.GetEarnByReason(EFF_EarnReason::AdmissionFee), 20);
	return true;
}

// 이어하기: 끝난 날 원장과 시설별 누적 매출이 저장을 건너 살아남는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_SaveRestoresLedgers, "FestaFiesta.Economy.FestivalEnd.SaveRestoresLedgers", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_SaveRestoresLedgers::RunTest(const FString& Parameters)
{
	FFF_FestivalEndTestHelpers::FEndHarness Source;
	if (!TestTrue(TEXT("원본 픽스처"), Source.Initialize())) return false;

	const FGuid FacilityId = FGuid::NewGuid();
	Source.Progression->AddFacilityRevenue(
		400, EFF_EarnReason::FacilityCommission, FName(TEXT("Booth.A")), FacilityId);
	Source.Progression->BeginNewDay(2);

	TArray<uint8> Bytes;
	Source.Progression->WriteSaveData(Bytes);

	FFF_FestivalEndTestHelpers::FEndHarness Loaded;
	if (!TestTrue(TEXT("복원 픽스처"), Loaded.Initialize())) return false;
	Loaded.Progression->ReadSaveData(Bytes);

	TestTrue(TEXT("Day1 원장이 복원된다"), Loaded.Progression->HasLedgerForDay(1));
	TestEqual(TEXT("Day1 매출"), FFF_FestivalEndTestHelpers::SumAmounts(Loaded.Progression->GetEarnRowsForDay(1)), 400);
	TestEqual(TEXT("끝난 날 원장 스냅샷"),
		Loaded.Progression->GetEndedDayLedger().GetEarnByReason(EFF_EarnReason::FacilityCommission), 400);

	FFF_FacilityInstanceRevenue Revenue;
	TestTrue(TEXT("시설별 장부가 복원된다"), Loaded.Progression->GetFacilityInstanceRevenue(FacilityId, Revenue));
	TestEqual(TEXT("시설 누적 매출"), Revenue.TotalGold, 400);
	TestEqual(TEXT("하루 경계로 오늘 값은 0"), Revenue.TodayGold, 0);
	return true;
}

// 축제가 끝나면 평가가 굳는다 - 이후 만족도·방문객이 바뀌어도 최종 화면의 값이 변하지 않는다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_EvaluationFreezes, "FestaFiesta.Economy.FestivalEnd.EvaluationFreezes", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_EvaluationFreezes::RunTest(const FString& Parameters)
{
	FFF_FestivalEndTestHelpers::FEndHarness Harness;
	if (!TestTrue(TEXT("픽스처"), Harness.Initialize())) return false;

	Harness.Progression->BeginNewDay(2);
	Harness.Progression->BeginNewDay(3);
	TestFalse(TEXT("끝나기 전에는 굳지 않는다"), Harness.Progression->IsFestivalEvaluationFrozen());

	UFunction* EndedHandler = Harness.Progression->FindFunction(TEXT("HandleFestivalEnded"));
	if (!TestNotNull(TEXT("종료 핸들러"), EndedHandler)) return false;
	Harness.Progression->ProcessEvent(EndedHandler, nullptr);

	TestTrue(TEXT("종료 후 굳는다"), Harness.Progression->IsFestivalEvaluationFrozen());
	TestEqual(TEXT("마지막 날이 날짜별 기록에 들어간다"), Harness.Progression->GetDailySatisfactionHistory().Num(), 3);

	const FFF_FestivalEvaluation Frozen = Harness.Progression->GetFestivalEvaluation();
	Harness.Progression->RecordAdmittedVisitorsForDailyGoal(400);
	const FFF_FestivalEvaluation After = Harness.Progression->GetFestivalEvaluation();
	TestEqual(TEXT("종료 후 방문객이 늘어도 총점이 같다"), After.TotalScore, Frozen.TotalScore);
	TestEqual(TEXT("방문객 점수도 같다"), After.VisitorScore, Frozen.VisitorScore);

	// 새 축제를 시작하면 풀린다.
	Harness.Progression->StartFestival(Harness.Definition);
	TestFalse(TEXT("새 축제에서 풀린다"), Harness.Progression->IsFestivalEvaluationFrozen());
	return true;
}

// 최고 기록은 더 높은 총점과 그때의 등급을 함께 남긴다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_FestivalEnd_RecordKeepsBestScore, "FestaFiesta.Economy.FestivalEnd.RecordKeepsBestScore", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_FestivalEnd_RecordKeepsBestScore::RunTest(const FString& Parameters)
{
	FFF_FestivalRecord Stored;
	FFF_FestivalRecord First;
	First.BestScore = 62;
	First.BestGrade = EFF_FestivalEvaluationGrade::B;
	First.PlayCount = 1;
	TestTrue(TEXT("첫 기록은 갱신이다"), Stored.MergeBest(First));

	FFF_FestivalRecord Lower;
	Lower.BestScore = 40;
	Lower.BestGrade = EFF_FestivalEvaluationGrade::D;
	Lower.PlayCount = 1;
	Stored.MergeBest(Lower);
	TestEqual(TEXT("낮은 점수는 기록을 내리지 않는다"), Stored.BestScore, 62);
	TestTrue(TEXT("등급도 그대로"), Stored.BestGrade == EFF_FestivalEvaluationGrade::B);

	FFF_FestivalRecord Higher;
	Higher.BestScore = 91;
	Higher.BestGrade = EFF_FestivalEvaluationGrade::S;
	Higher.PlayCount = 1;
	Stored.MergeBest(Higher);
	TestEqual(TEXT("높은 점수로 갱신"), Stored.BestScore, 91);
	TestTrue(TEXT("그때의 등급이 함께 바뀐다"), Stored.BestGrade == EFF_FestivalEvaluationGrade::S);
	TestEqual(TEXT("맡은 횟수"), Stored.PlayCount, 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
