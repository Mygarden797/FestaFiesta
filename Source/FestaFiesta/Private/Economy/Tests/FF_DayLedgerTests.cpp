// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/Rules/FF_DayLedger.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_ConstructionCorrections,
	"FestaFiesta.Economy.DayLedger.ConstructionCorrections",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_ConstructionCorrections::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;
	Ledger.RecordSpend(EFF_SpendReason::FacilityInstall, 700);
	Ledger.RecordSpend(EFF_SpendReason::Cleanup, 125);
	Ledger.RecordEarn(EFF_EarnReason::AdmissionFee, 50);
	Ledger.RecordRefund(350);
	TestTrue(TEXT("Undo original construction entries"), Ledger.AdjustConstruction(-700, -350));
	TestEqual(TEXT("Unrelated spending is preserved"), Ledger.GetSpendByReason(EFF_SpendReason::Cleanup), 125);
	TestEqual(TEXT("Unrelated revenue is preserved"), Ledger.GetEarnByReason(EFF_EarnReason::AdmissionFee), 50);
	TestFalse(TEXT("Cannot undo the same spending twice"), Ledger.AdjustConstruction(-1, 0));
	TestFalse(TEXT("Cannot undo the same refund twice"), Ledger.AdjustConstruction(0, -1));
	TestFalse(TEXT("Reject aggregate overflow without narrowing"), Ledger.AdjustConstruction(int64(MAX_int32) + 1, 0));
	TestEqual(TEXT("Rejection preserves zero construction spending"), Ledger.GetSpendByReason(EFF_SpendReason::FacilityInstall), 0);
	TestTrue(TEXT("Redo restores exact entries"), Ledger.AdjustConstruction(700, 350));
	TestEqual(TEXT("Exact refund restored"), Ledger.GetTotalRefund(), 350);
	return true;
}

// 원인별로 정확히 누적되고, 다른 원인끼리는 서로 안 섞인다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_RecordSpendAccumulatesByReason, "FestaFiesta.Economy.DayLedger.RecordSpendAccumulatesByReason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_RecordSpendAccumulatesByReason::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;

	Ledger.RecordSpend(EFF_SpendReason::FacilityInstall, 400);
	Ledger.RecordSpend(EFF_SpendReason::FacilityInstall, 500);
	Ledger.RecordSpend(EFF_SpendReason::StaffHire, 1500);

	TestEqual(TEXT("같은 원인은 누적된다"),
		Ledger.GetSpendByReason(EFF_SpendReason::FacilityInstall), 900);
	TestEqual(TEXT("다른 원인은 서로 안 섞인다"),
		Ledger.GetSpendByReason(EFF_SpendReason::StaffHire), 1500);
	TestEqual(TEXT("기록한 적 없는 원인은 0이다"),
		Ledger.GetSpendByReason(EFF_SpendReason::Cleanup), 0);

	return true;
}

// 매출도 같은 방식으로 원인별 누적된다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_RecordEarnAccumulatesByReason, "FestaFiesta.Economy.DayLedger.RecordEarnAccumulatesByReason", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_RecordEarnAccumulatesByReason::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;

	Ledger.RecordEarn(EFF_EarnReason::AdmissionFee, 2500);
	Ledger.RecordEarn(EFF_EarnReason::AdmissionFee, 2500);
	Ledger.RecordEarn(EFF_EarnReason::DailyGoalBonus, 300);

	TestEqual(TEXT("같은 원인은 누적된다"),
		Ledger.GetEarnByReason(EFF_EarnReason::AdmissionFee), 5000);
	TestEqual(TEXT("다른 원인은 서로 안 섞인다"),
		Ledger.GetEarnByReason(EFF_EarnReason::DailyGoalBonus), 300);

	return true;
}

// FacilityId가 있는 매출만 부스별 표에 반영된다. 입장료처럼 특정 부스에 안 걸리는 매출은 안 들어간다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_RecordEarnWithFacilityIdUpdatesRevenueByFacility, "FestaFiesta.Economy.DayLedger.RecordEarnWithFacilityIdUpdatesRevenueByFacility", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_RecordEarnWithFacilityIdUpdatesRevenueByFacility::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;
	const FName FoodStall(TEXT("Booth_FoodStall_DadaPochaFoodTruck"));
	const FName MarketGoods(TEXT("Booth_MarketGoods_FestivalGoods"));

	Ledger.RecordEarn(EFF_EarnReason::FacilityCommission, 48, FoodStall);
	Ledger.RecordEarn(EFF_EarnReason::FacilityCommission, 48, FoodStall);
	Ledger.RecordEarn(EFF_EarnReason::FacilityCommission, 60, MarketGoods);
	Ledger.RecordEarn(EFF_EarnReason::AdmissionFee, 2500); // FacilityId 없음 — 부스별 표에 안 들어간다

	const TMap<FName, int32>& RevenueByFacility = Ledger.GetRevenueByFacility();
	TestEqual(TEXT("부스 2개만 표에 있다"), RevenueByFacility.Num(), 2);
	TestEqual(TEXT("같은 부스는 누적된다"), RevenueByFacility.FindRef(FoodStall), 96);
	TestEqual(TEXT("다른 부스는 따로 쌓인다"), RevenueByFacility.FindRef(MarketGoods), 60);

	return true;
}

// 음수는 ensure로 막고 상태를 전혀 바꾸지 않는다 — FFF_FestivalFunds::AddRevenue와 같은 규칙(§4.3).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_NegativeRecordsDoNotMutate, "FestaFiesta.Economy.DayLedger.NegativeRecordsDoNotMutate", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_NegativeRecordsDoNotMutate::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;

	// ensure가 배너·조건식·콜스택을 여러 줄로 찍고 실행마다 Error/Verbose 배치가 달라진다(실측).
	// 특정 문구로 못 고정하니 빈 문자열로 이 테스트의 모든 Error를 통과시킨다.
	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);
	Ledger.RecordSpend(EFF_SpendReason::FacilityInstall, -400);
	TestEqual(TEXT("음수 지출 기록은 반영되지 않는다"),
		Ledger.GetSpendByReason(EFF_SpendReason::FacilityInstall), 0);

	AddExpectedError(TEXT(""), EAutomationExpectedErrorFlags::Contains, 0);
	Ledger.RecordEarn(EFF_EarnReason::AdmissionFee, -100, FName(TEXT("Booth_FoodStall_DadaPochaFoodTruck")));
	TestEqual(TEXT("음수 매출 기록은 반영되지 않는다"),
		Ledger.GetEarnByReason(EFF_EarnReason::AdmissionFee), 0);
	TestEqual(TEXT("음수 매출은 부스별 표도 안 건드린다"),
		Ledger.GetRevenueByFacility().Num(), 0);

	return true;
}

// 하루 경계(Reset)는 지출·매출·부스별 표를 전부 비운다.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_DayLedger_ResetClearsEverything, "FestaFiesta.Economy.DayLedger.ResetClearsEverything", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_DayLedger_ResetClearsEverything::RunTest(const FString& Parameters)
{
	FFF_DayLedger Ledger;
	const FName FoodStall(TEXT("Booth_FoodStall_DadaPochaFoodTruck"));

	Ledger.RecordSpend(EFF_SpendReason::FacilityInstall, 400);
	Ledger.RecordEarn(EFF_EarnReason::FacilityCommission, 48, FoodStall);

	Ledger.Reset();

	TestEqual(TEXT("지출이 리셋된다"), Ledger.GetSpendByReason(EFF_SpendReason::FacilityInstall), 0);
	TestEqual(TEXT("매출이 리셋된다"), Ledger.GetEarnByReason(EFF_EarnReason::FacilityCommission), 0);
	TestEqual(TEXT("부스별 표도 리셋된다"), Ledger.GetRevenueByFacility().Num(), 0);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
