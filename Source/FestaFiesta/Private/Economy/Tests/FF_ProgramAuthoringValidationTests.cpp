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
	Catalog[1].RequiredStaff = 0;
	Catalog[1].Capacity = 0;
	Catalog[1].FeePerPerson = 0;
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
