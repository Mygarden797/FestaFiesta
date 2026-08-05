// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NPC/FF_PreferenceVectorBuilder.h"
#include "DataTables/FF_ArchetypeRow.h"
#include "DataTables/FF_CompanionModifierRow.h"
#include "Engine/DataTable.h"

namespace
{
	UDataTable* MakeArchetypeTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_ArchetypeRow::StaticStruct();
		FFF_ArchetypeRow Row;
		Row.ArchetypeId = TEXT("life_stage_teen");
		Row.AmbiencePhotoZoneDeltaPp = -15.80f;
		Row.FoodStallDeltaPp = -9.40f;
		Table->AddRow(TEXT("life_stage_teen"), Row);
		return Table;
	}

	UDataTable* MakeCompanionTable()
	{
		UDataTable* Table = NewObject<UDataTable>();
		Table->RowStruct = FFF_CompanionModifierRow::StaticStruct();
		FFF_CompanionModifierRow Row;
		Row.CompanionType = TEXT("group_3_plus");
		Row.AmbiencePhotoZoneDeltaPp = -1.00f;
		Row.FoodStallDeltaPp = -0.60f;
		Table->AddRow(TEXT("group_3_plus"), Row);
		return Table;
	}
}

// 아키타입 delta + 동반유형 delta를 더하고 preference_strength_multiplier를 곱하는지 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PreferenceVectorBuilder_SumsAndScales, "FestaFiesta.NPC.PreferenceVectorBuilder.SumsArchetypeAndCompanionThenScales", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PreferenceVectorBuilder_SumsAndScales::RunTest(const FString& Parameters)
{
	UDataTable* ArchetypeTable = MakeArchetypeTable();
	UDataTable* CompanionTable = MakeCompanionTable();

	const TMap<FName, float> Result = FFF_PreferenceVectorBuilder::Build(
		TEXT("life_stage_teen"), TEXT("group_3_plus"), 1.4f, ArchetypeTable, CompanionTable);

	// (-15.80 + -1.00) * 1.4 = -23.52
	TestTrue(TEXT("ambience_photo_zone은 아키타입+동반유형 delta 합에 multiplier를 곱한 값"),
		FMath::IsNearlyEqual(Result.FindRef(TEXT("ambience_photo_zone")), -23.52f, 0.01f));
	// (-9.40 + -0.60) * 1.4 = -14.00
	TestTrue(TEXT("food_stall도 동일한 방식으로 계산된다"),
		FMath::IsNearlyEqual(Result.FindRef(TEXT("food_stall")), -14.00f, 0.01f));
	// 값을 안 채운 카테고리는 0 + 0으로 처리되어야 한다
	TestTrue(TEXT("데이터 없는 카테고리는 0이다"),
		FMath::IsNearlyEqual(Result.FindRef(TEXT("market_goods")), 0.f, 0.01f));
	return true;
}

// Row를 못 찾아도(nullptr) 크래시 없이 0으로 처리되는지 확인
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_PreferenceVectorBuilder_MissingRowFallsBackToZero, "FestaFiesta.NPC.PreferenceVectorBuilder.MissingRowFallsBackToZero", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_PreferenceVectorBuilder_MissingRowFallsBackToZero::RunTest(const FString& Parameters)
{
	const TMap<FName, float> Result = FFF_PreferenceVectorBuilder::Build(
		TEXT("does_not_exist"), TEXT("does_not_exist"), 1.0f, nullptr, nullptr);

	TestTrue(TEXT("테이블이 없어도 크래시 없이 0을 반환한다"),
		FMath::IsNearlyEqual(Result.FindRef(TEXT("ambience_photo_zone")), 0.f, 0.01f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
