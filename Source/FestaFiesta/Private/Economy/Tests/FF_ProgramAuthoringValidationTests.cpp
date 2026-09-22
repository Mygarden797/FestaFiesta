// FF_ProgramAuthoringValidationTests.cpp
// 역할: 프로그램 작성 오류와 무료 입장·개최 비용·정원의 허용 경계를 자동 검사한다.
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include <limits>

#if WITH_DEV_AUTOMATION_TESTS

#include "Economy/FF_ProgramAuthoringValidationLibrary.h"
#include "Festival/FF_FestivalDefinition.h"

namespace FFF_ProgramAuthoringValidationTestHelpers
{
	bool ContainsError(const TArray<FText>& Errors, const TCHAR* Fragment)
	{
		return Errors.ContainsByPredicate([Fragment](const FText& Error)
		{
			return Error.ToString().Contains(Fragment);
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramAuthoringValidation_DefaultCatalogIsValid,
	"FestaFiesta.Economy.ProgramAuthoring.DefaultCatalogIsValid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_ProgramAuthoringValidation_DefaultCatalogIsValid::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_FestivalDefinition* Definition =
		NewObject<UFF_FestivalDefinition>(GetTransientPackage());
	TArray<FText> Errors;
	TestTrue(TEXT("기본 프로그램 카탈로그는 독립 저작 검증을 통과한다"),
		UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog(
			Definition->ProgramCatalog, Errors));
	TestEqual(TEXT("기본 카탈로그에는 저작 오류가 없다"), Errors.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramAuthoringValidation_ReportsAllErrors,
	"FestaFiesta.Economy.ProgramAuthoring.ReportsAllErrors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_ProgramAuthoringValidation_ReportsAllErrors::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramAuthoringValidationTestHelpers;
	(void)Parameters;

	const UFF_FestivalDefinition* Definition =
		NewObject<UFF_FestivalDefinition>(GetTransientPackage());
	TArray<FFF_ProgramCard> Catalog = Definition->ProgramCatalog;
	if (!TestTrue(TEXT("검사용 기본 카드가 두 장 이상 있다"), Catalog.Num() >= 2))
	{
		return false;
	}

	Catalog[1].ProgramId = Catalog[0].ProgramId;
	Catalog[1].DisplayName = FText::FromString(TEXT("   "));
	Catalog[1].CostGold = 0;
	Catalog[1].Capacity = 0;
	Catalog[1].FeePerPerson = -1;
	Catalog[1].StartMinutes = std::numeric_limits<float>::quiet_NaN();
	Catalog[1].EndMinutes = std::numeric_limits<float>::infinity();
	Catalog[1].AllowedFacilityTypes = {
		TEXT("Performance_ExperienceStage_Extra"),
		TEXT("Performance_ExperienceStage_Extra")
	};

	TArray<FText> Errors;
	TestFalse(TEXT("깨진 카탈로그는 거절된다"),
		UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog(Catalog, Errors));
	TestTrue(TEXT("중복 ProgramId를 설명한다"), ContainsError(Errors, TEXT("ProgramId")));
	TestTrue(TEXT("공백 표시 이름을 설명한다"), ContainsError(Errors, TEXT("화면 표시 이름")));
	TestTrue(TEXT("0 이하 경제값을 설명한다"), ContainsError(Errors, TEXT("CostGold")));
	TestTrue(TEXT("NaN/Infinity 시간 범위를 설명한다"), ContainsError(Errors, TEXT("시간 범위")));
	TestTrue(TEXT("시설 타입 형식을 설명한다"), ContainsError(Errors, TEXT("대분류_소분류")));
	TestTrue(TEXT("중복 시설 타입을 설명한다"), ContainsError(Errors, TEXT("중복")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramAuthoringValidation_FreeProgramsAreValid,
	"FestaFiesta.Economy.ProgramAuthoring.FreeProgramsAreValid",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramAuthoringValidation_FreeProgramsAreValid::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>();
	TArray<FFF_ProgramCard> Catalog = Definition->ProgramCatalog;
	if (!TestFalse(TEXT("The default catalog contains a card for boundary checks"), Catalog.IsEmpty()))
	{
		return false;
	}
	for (FFF_ProgramCard& Card : Catalog)
	{
		Card.FeePerPerson = 0;
		TestTrue(TEXT("An intentionally free program remains a valid schedulable card"), Card.IsValid());
		TestEqual(TEXT("Free attendance cannot generate projected revenue"), Card.CalcMaxRevenue(), 0);
	}
	TArray<FText> Errors;
	TestTrue(TEXT("The editor authoring contract accepts zero admission fees"),
		UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog(Catalog, Errors));
	TestEqual(TEXT("Free admission adds no authoring errors"), Errors.Num(), 0);
	Catalog[0].FeePerPerson = -1;
	Errors.Reset();
	TestFalse(TEXT("Negative fees remain invalid after introducing free programs"),
		UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog(Catalog, Errors));
	TestFalse(TEXT("Negative fees cannot pass the runtime card guard"), Catalog[0].IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramAuthoringValidation_EconomyBoundaries,
	"FestaFiesta.Economy.ProgramAuthoring.EconomyBoundaries",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_ProgramAuthoringValidation_EconomyBoundaries::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UFF_FestivalDefinition* Definition = NewObject<UFF_FestivalDefinition>();
	if (!TestFalse(TEXT("A default program exists"), Definition->ProgramCatalog.IsEmpty()))
	{
		return false;
	}

	// 각 항목만 경계 밖으로 옮겨 다른 오류가 잘못된 값의 허용을 가리지 않게 한다.
	struct FBoundaryCase
	{
		const TCHAR* Label;
		int32 Cost;
		int32 Capacity;
		int32 Fee;
		bool bExpectedValid;
	};
	const FBoundaryCase Cases[] = {
		{ TEXT("Free admission with positive cost and capacity"), 1, 1, 0, true },
		{ TEXT("Paid admission with positive cost and capacity"), 1, 1, 1, true },
		{ TEXT("Negative admission fee"), 1, 1, -1, false },
		{ TEXT("Zero hosting cost is still invalid"), 0, 1, 0, false },
		{ TEXT("Negative hosting cost is still invalid"), -1, 1, 0, false },
		{ TEXT("Zero capacity is still invalid"), 1, 0, 0, false },
		{ TEXT("Negative capacity is still invalid"), 1, -1, 0, false },
	};
	for (const FBoundaryCase& Boundary : Cases)
	{
		FFF_ProgramCard Card = Definition->ProgramCatalog[0];
		Card.CostGold = Boundary.Cost;
		Card.Capacity = Boundary.Capacity;
		Card.FeePerPerson = Boundary.Fee;
		TArray<FText> Errors;
		TestEqual(*FString::Printf(TEXT("Runtime: %s"), Boundary.Label),
			Card.IsValid(), Boundary.bExpectedValid);
		TestEqual(*FString::Printf(TEXT("Authoring: %s"), Boundary.Label),
			UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog({ Card }, Errors), Boundary.bExpectedValid);
		TestEqual(*FString::Printf(TEXT("Diagnostic: %s"), Boundary.Label),
			Errors.IsEmpty(), Boundary.bExpectedValid);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_ProgramAuthoringValidation_EmptyCatalogIsRejected,
	"FestaFiesta.Economy.ProgramAuthoring.EmptyCatalogIsRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_ProgramAuthoringValidation_EmptyCatalogIsRejected::RunTest(const FString& Parameters)
{
	using namespace FFF_ProgramAuthoringValidationTestHelpers;
	(void)Parameters;
	TArray<FText> Errors;
	TestFalse(TEXT("빈 카탈로그는 거절된다"),
		UFF_ProgramAuthoringValidationLibrary::ValidateProgramCatalog({}, Errors));
	TestTrue(TEXT("ProgramCatalog가 비었다고 설명한다"),
		ContainsError(Errors, TEXT("ProgramCatalog")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
