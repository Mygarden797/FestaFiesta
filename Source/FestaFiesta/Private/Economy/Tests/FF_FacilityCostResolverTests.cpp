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
	TestEqual(TEXT("대·소분류 키"), Key, FName(TEXT("Cost.Booth.FoodStall")));

	Spec.InstallCostKey = TEXT("Cost.Special.EventPrice");
	TestTrue(TEXT("명시 키는 예외 override로 허용한다"),
		FFF_FacilityCostResolver::ResolveCostKey(Spec, Key, Failure));
	TestEqual(TEXT("override 우선"), Key, Spec.InstallCostKey);

	UDataTable* Table = NewObject<UDataTable>();
	Table->RowStruct = FFF_FacilityCostRow::StaticStruct();
	FFF_FacilityCostRow DefaultRow;
	DefaultRow.InstallCost = 400;
	Table->AddRow(TEXT("Cost.Booth.Default"), DefaultRow);

	int32 Cost = 0;
	FName MatchedRow;
	TestTrue(TEXT("없는 override는 카테고리 Default로 폴백한다"),
		FFF_FacilityCostResolver::FindInstallCost(Spec, Table, Cost, MatchedRow, Failure));
	TestEqual(TEXT("폴백 비용"), Cost, 400);
	TestEqual(TEXT("폴백 행"), MatchedRow, FName(TEXT("Cost.Booth.Default")));
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
