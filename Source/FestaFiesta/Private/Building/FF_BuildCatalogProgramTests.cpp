#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_BuildCatalogEconomyReadModel.h"
#include "Building/FF_BuildCatalogModel.h"
#include "Engine/GameInstance.h"
#include <limits>

namespace FFBuildCatalogProgramTests
{
	FFF_BuildCatalogEntry MakeWorkshopEntry()
	{
		FFF_BuildCatalogEntry Entry;
		Entry.FacilityDefinitionId = TEXT("Booth_CraftExperience_TestWorkshop");
		Entry.Category = EFFZSFacilityCategory::Booth;
		return Entry;
	}

	FFF_ProgramCard MakeProgram(const FName Id, const FName AllowedType, const TCHAR* Name)
	{
		FFF_ProgramCard Card;
		Card.ProgramId = Id;
		Card.DisplayName = FText::FromString(Name);
		Card.CostGold = 200;
		Card.RequiredStaff = 1;
		Card.Capacity = 12;
		Card.StartMinutes = 30.f;
		Card.EndMinutes = 60.f;
		Card.AllowedFacilityTypes = { AllowedType };
		return Card;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildCatalogProgramCompatibilityTest,
	"FestaFiesta.Building.Catalog.ProgramHighlights.Compatibility",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogProgramCompatibilityTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildCatalogProgramTests;
	const FFF_ProgramCard Workshop = MakeProgram(TEXT("Program_Workshop"), TEXT("Booth_CraftExperience"), TEXT("바다 공방"));
	const FFF_ProgramCard Yoga = MakeProgram(TEXT("Program_Yoga"), TEXT("Performance_ExperienceStage"), TEXT("선셋 요가"));
	FFF_ProgramCard AnotherWorkshop = MakeProgram(TEXT("Program_OtherWorkshop"), TEXT("Booth_CraftExperience"), TEXT("공예 체험"));
	FFF_BuildCatalogEntry Entry = MakeWorkshopEntry();
	Entry.MinorToken = TEXT("ExperienceStage");
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Workshop, Yoga, AnotherWorkshop, Workshop });
	TestEqual(TEXT("호환 프로그램은 중복 ID를 제외하고 센다"), Entry.CompatibleProgramCount, 2);
	TestEqual(TEXT("실제 정의 ID로 호환 이름만 순서대로 안내한다"), Entry.CompatibleProgramNames.ToString(), FString(TEXT("바다 공방, 공예 체험")));

	FFF_ProgramCard Unrestricted = Workshop;
	Unrestricted.AllowedFacilityTypes.Reset();
	Entry.Category = EFFZSFacilityCategory::Toilet;
	Entry.FacilityDefinitionId = TEXT("Toilet_Standard_Test");
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Unrestricted });
	TestEqual(TEXT("빈 허용 종류는 도메인 규칙대로 제한 없음이다"), Entry.CompatibleProgramCount, 1);

	Entry = MakeWorkshopEntry();
	Entry.Category = EFFZSFacilityCategory::Performance;
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Unrestricted });
	TestEqual(TEXT("종류 제한이 없어도 잘못된 대분류의 정의는 제외한다"), Entry.CompatibleProgramCount, 0);
	Entry = MakeWorkshopEntry();
	Entry.FacilityDefinitionId = TEXT("Booth_CraftExperience_");
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Unrestricted });
	TestEqual(TEXT("무효 정의 이름은 제외한다"), Entry.CompatibleProgramCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildCatalogProgramValidityTest,
	"FestaFiesta.Building.Catalog.ProgramHighlights.Validity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogProgramValidityTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildCatalogProgramTests;
	const FFF_ProgramCard Valid = MakeProgram(TEXT("Program_Valid"), TEXT("Booth_CraftExperience"), TEXT("체험 프로그램"));
	FFF_BuildCatalogEntry Entry = MakeWorkshopEntry();
	TArray<FFF_ProgramCard> InvalidCards;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		FFF_ProgramCard& Card = InvalidCards.Add_GetRef(Valid);
		Card.ProgramId = FName(*FString::Printf(TEXT("Program_Invalid%d"), Index));
	}
	InvalidCards[0].CostGold = 0;
	InvalidCards[1].StartMinutes = -1.f;
	InvalidCards[2].EndMinutes = std::numeric_limits<float>::infinity();
	InvalidCards[3].StartMinutes = std::numeric_limits<float>::quiet_NaN();
	InvalidCards[4].ProgramId = NAME_None;
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, InvalidCards);
	TestEqual(TEXT("편성할 수 없는 저작 데이터에는 프로그램 배지를 붙이지 않는다"), Entry.CompatibleProgramCount, 0);

	FFF_ProgramCard InvalidFirst = Valid;
	InvalidFirst.Capacity = 0;
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { InvalidFirst, Valid });
	TestEqual(TEXT("중복의 첫 카드가 무효면 실제 FindCard 편성 판정과 같이 제외한다"), Entry.CompatibleProgramCount, 0);
	FFF_ProgramCard IncompatibleFirst = Valid;
	IncompatibleFirst.AllowedFacilityTypes = { TEXT("Performance_ExperienceStage") };
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { IncompatibleFirst, Valid });
	TestEqual(TEXT("중복의 첫 카드가 다른 시설용이면 뒤 카드로 호환을 우회하지 않는다"), Entry.CompatibleProgramCount, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_BuildCatalogProgramRefreshTest,
	"FestaFiesta.Building.Catalog.ProgramHighlights.RefreshAndPriceIndependence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_BuildCatalogProgramRefreshTest::RunTest(const FString& Parameters)
{
	using namespace FFBuildCatalogProgramTests;
	const FFF_ProgramCard Program = MakeProgram(TEXT("Program_Workshop"), TEXT("Booth_CraftExperience"), TEXT("바다 공방"));
	FFF_BuildCatalogEntry Entry = MakeWorkshopEntry();
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Program });
	FString Failure;
	TestFalse(TEXT("설치비가 없어도 별도로 실패를 반환한다"),
		FFBuildCatalogModel::ApplyFacilityCosts(Entry, nullptr, nullptr, 0.f, Failure));
	TestEqual(TEXT("설치비 실패가 프로그램 기능을 지우지 않는다"), Entry.CompatibleProgramCount, 1);
	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, {});
	TestTrue(TEXT("프로그램 카탈로그가 비면 개수와 이름을 모두 초기화한다"),
		Entry.CompatibleProgramCount == 0 && Entry.CompatibleProgramNames.IsEmpty());

	FFBuildCatalogModel::ApplyCompatiblePrograms(Entry, { Program });
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UFF_BuildCatalogEconomyReadModel* ReadModel = NewObject<UFF_BuildCatalogEconomyReadModel>(GameInstance);
	TestFalse(TEXT("현재 축제가 없는 읽기 모델은 가격을 확정하지 않는다"),
		ReadModel->TryProjectFacilityCosts(Entry, nullptr, Failure));
	TestTrue(TEXT("축제를 확인할 수 없으면 이전 프로그램 표시도 제거한다"),
		Entry.CompatibleProgramCount == 0 && Entry.CompatibleProgramNames.IsEmpty());
	return true;
}

#endif
