// Copyright Epic Games, Inc. All Rights Reserved.

#include "Economy/FF_FacilityCostResolver.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "DataTables/FF_FacilityCostRow.h"
#include "Engine/DataTable.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityCostResolverTest,
	"FestaFiesta.Economy.FacilityCostResolver.SingleSourceAndOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_FacilityCostResolverTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FFFZSFacilitySpec Spec;
	Spec.Category = EFFZSFacilityCategory::Booth;
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_DadaPochaFoodTruck");

	FName Key;
	FString Failure;
	TestTrue(TEXT("정의 ID에서 비용 키를 만든다"),
		FFF_FacilityCostResolver::ResolveCostKey(Spec, Key, Failure));
	const FName ExactKey(TEXT("Cost.Booth.FoodStall.DadaPochaFoodTruck"));
	const FName LegacyMinorKey(TEXT("Cost.Booth.FoodStall"));
	const FName MajorDefaultKey(TEXT("Cost.Booth.Default"));
	const FName OverrideKey(TEXT("Cost.Special.EventPrice"));
	TestEqual(TEXT("대·소분류·고유이름 키"), Key, ExactKey);

	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow ExactRow;
	ExactRow.InstallCost = 700;
	Table->AddRow(ExactKey, ExactRow);
	FFF_FacilityCostRow LegacyMinorRow;
	LegacyMinorRow.InstallCost = 500;
	Table->AddRow(LegacyMinorKey, LegacyMinorRow);
	FFF_FacilityCostRow DefaultRow;
	DefaultRow.InstallCost = 400;
	Table->AddRow(MajorDefaultKey, DefaultRow);

	int32 Cost = 0;
	FName MatchedRow;
	TestTrue(TEXT("4단 시설별 키를 가장 먼저 찾는다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("시설별 비용"), Cost, 700);
	TestEqual(TEXT("시설별 행"), MatchedRow, ExactKey);

	Table->RemoveRow(ExactKey);
	TestTrue(TEXT("4단 행이 없으면 기존 3단 소분류 행으로 폴백한다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("소분류 폴백 비용"), Cost, 500);
	TestEqual(TEXT("소분류 폴백 행"), MatchedRow, LegacyMinorKey);

	Table->RemoveRow(LegacyMinorKey);
	TestTrue(TEXT("3단 소분류 행도 없으면 대분류 Default로 폴백한다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("대분류 폴백 비용"), Cost, 400);
	TestEqual(TEXT("대분류 폴백 행"), MatchedRow, MajorDefaultKey);

	Table->AddRow(ExactKey, ExactRow);
	Table->AddRow(LegacyMinorKey, LegacyMinorRow);
	FFF_FacilityCostRow OverrideRow;
	OverrideRow.InstallCost = 900;
	Table->AddRow(OverrideKey, OverrideRow);
	Spec.InstallCostKey = OverrideKey;
	TestTrue(TEXT("명시 키는 예외 override로 허용한다"),
		FFF_FacilityCostResolver::ResolveCostKey(Spec, Key, Failure));
	TestEqual(TEXT("override 키 생성 우선"), Key, OverrideKey);
	TestTrue(TEXT("명시 override 행이 자동 파생 행들보다 우선한다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("override 비용"), Cost, 900);
	TestEqual(TEXT("override 행"), MatchedRow, OverrideKey);

	Table->RemoveRow(OverrideKey);
	TestTrue(TEXT("없는 override는 자동 파생 행을 쓰지 않고 카테고리 Default로 폴백한다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("없는 override의 폴백 비용"), Cost, 400);
	TestEqual(TEXT("없는 override의 폴백 행"), MatchedRow, MajorDefaultKey);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityCostResolverFreeAndInvalidPricesTest,
	"FestaFiesta.Economy.FacilityCostResolver.FreeAndInvalidPrices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_FacilityCostResolverFreeAndInvalidPricesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FFFZSFacilitySpec Spec;
	Spec.Category = EFFZSFacilityCategory::Booth;
	Spec.FacilityDefinitionId = TEXT("Booth_FoodStall_FreeFacility");
	const FName ExactKey(TEXT("Cost.Booth.FoodStall.FreeFacility"));
	const FName MinorKey(TEXT("Cost.Booth.FoodStall"));
	const FName DefaultKey(TEXT("Cost.Booth.Default"));
	const FName OverrideKey(TEXT("Cost.Special.FreeFacility"));
	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FFF_FacilityCostRow::StaticStruct();

	int32 Cost = 0;
	FName MatchedRow;
	FString Failure;
	// Every supported lookup path shares the same free/invalid-price contract.
	for (const FName Key : { ExactKey, MinorKey, DefaultKey, OverrideKey })
	{
		Table->EmptyTable();
		Spec.InstallCostKey = Key == OverrideKey ? OverrideKey : NAME_None;
		if (Key != DefaultKey)
		{
			FFF_FacilityCostRow FallbackRow;
			FallbackRow.InstallCost = 400;
			Table->AddRow(DefaultKey, FallbackRow);
		}
		FFF_FacilityCostRow Row;
		Row.InstallCost = 0;
		Table->AddRow(Key, Row);
		TestTrue(*FString::Printf(TEXT("Authored zero resolves through %s"), *Key.ToString()),
			FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
		TestEqual(TEXT("A free row is not replaced by a paid fallback"), Cost, 0);
		TestEqual(TEXT("A free row retains its identity"), MatchedRow, Key);
		TestTrue(TEXT("A free quote has no failure reason"), Failure.IsEmpty());

		Row.InstallCost = -1;
		Table->AddRow(Key, Row);
		Cost = 999;
		TestFalse(TEXT("An invalid authored row is rejected instead of falling back"),
			FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
		TestEqual(TEXT("A failed lookup clears the previous cost"), Cost, 0);
		TestEqual(TEXT("A failed lookup clears the matched row"), MatchedRow, NAME_None);
		TestTrue(TEXT("Invalid-price diagnostics identify the authored row"), Failure.Contains(Key.ToString()));
	}
	Table->EmptyTable();
	TestFalse(TEXT("A missing price row is not a free facility"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestFalse(TEXT("A missing price table is not a free facility"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, nullptr, Cost, MatchedRow, Failure));
	TestEqual(TEXT("Free facilities cannot produce a demolition refund"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(0, 0.5f), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityDemolitionRefundCalculationTest,
	"FestaFiesta.Economy.FacilityCostResolver.DemolitionRefundCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFF_FacilityDemolitionRefundCalculationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestEqual(TEXT("A zero refund rate returns zero Gold"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(400, 0.0f), 0);
	TestEqual(TEXT("The configured half refund returns 200 of 400 Gold"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(400, 0.5f), 200);
	TestEqual(TEXT("Half-Gold results use the authoritative nearest-integer rounding"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(401, 0.5f), 201);
	TestEqual(TEXT("Rates above one cannot refund more than the install cost"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(400, 1.5f), 400);
	TestEqual(TEXT("Negative rates cannot create a negative refund"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(400, -0.5f), 0);
	TestEqual(TEXT("Negative install costs cannot create a refund"),
		FFF_FacilityCostResolver::CalculateDemolitionRefund(-400, 0.5f), 0);
	return true;
}

#endif
