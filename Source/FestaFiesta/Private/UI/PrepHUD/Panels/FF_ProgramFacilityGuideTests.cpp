// FF_ProgramFacilityGuideTests.cpp
// 역할: 프로그램의 필요 시설 안내의 동작과 저장된 화면 연결을 자동 검증한다.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "UI/PrepHUD/Panels/FF_ProgramFacilityGuide.h"

namespace FFProgramFacilityGuideTests
{
	FFF_BuildCatalogEntry MakeEntry(
		const FName Id, const EFFZSFacilityCategory Category, const TCHAR* DisplayName)
	{
		FFF_BuildCatalogEntry Entry;
		Entry.FacilityDefinitionId = Id;
		Entry.Category = Category;
		Entry.DisplayName = FText::FromString(DisplayName);
		return Entry;
	}

	FFF_ProgramCard MakeWorkshop()
	{
		FFF_ProgramCard Card;
		Card.ProgramId = TEXT("Program_Workshop");
		Card.AllowedFacilityTypes = { TEXT("Booth_CraftExperience") };
		return Card;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramFacilityGuide_Compatibility,
	"FestaFiesta.UI.PrepProgram.FacilityGuide.Compatibility",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramFacilityGuide_Compatibility::RunTest(const FString& Parameters)
{
	using namespace FFProgramFacilityGuideTests;
	const FFF_BuildCatalogEntry Workshop = MakeEntry(
		TEXT("Booth_CraftExperience_SeaWorkshop"), EFFZSFacilityCategory::Booth, TEXT("바다 공방"));
	const FFF_BuildCatalogEntry Yoga = MakeEntry(
		TEXT("Performance_ExperienceStage_Yoga"), EFFZSFacilityCategory::Performance, TEXT("요가 무대"));
	FFF_BuildCatalogEntry StaleProjection = Yoga;
	StaleProjection.MinorToken = TEXT("CraftExperience");
	const TArray<FFF_BuildCatalogEntry> Entries = {
		Yoga, Workshop, StaleProjection,
		MakeEntry(TEXT("Booth_CraftExperience_"), EFFZSFacilityCategory::Booth, TEXT("무효 이름")),
		MakeEntry(TEXT("Booth_CraftExperience_WrongCategory"), EFFZSFacilityCategory::Performance, TEXT("잘못된 분류"))
	};

	const TArray<FFF_BuildCatalogEntry> Matches =
		FFF_ProgramFacilityGuide::FindCompatibleEntries(MakeWorkshop(), Entries);
	TestEqual(TEXT("실제 정의 ID가 편성 규칙에 맞는 시설만 안내한다"), Matches.Num(), 1);
	if (Matches.Num() == 1)
	{
		TestEqual(TEXT("공방 프로그램은 공방 시설을 가리킨다"),
			Matches[0].FacilityDefinitionId, Workshop.FacilityDefinitionId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramFacilityGuide_Unrestricted,
	"FestaFiesta.UI.PrepProgram.FacilityGuide.Unrestricted",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramFacilityGuide_Unrestricted::RunTest(const FString& Parameters)
{
	using namespace FFProgramFacilityGuideTests;
	FFF_ProgramCard Card = MakeWorkshop();
	Card.AllowedFacilityTypes.Reset();
	const TArray<FFF_BuildCatalogEntry> Entries = {
		MakeEntry(TEXT("Performance_ExperienceStage_Yoga"), EFFZSFacilityCategory::Performance, TEXT("요가 무대")),
		MakeEntry(NAME_None, EFFZSFacilityCategory::Booth, TEXT("정의 없음")),
		MakeEntry(TEXT("Booth_CraftExperience_Workshop"), EFFZSFacilityCategory::Booth, TEXT("공방"))
	};
	const TArray<FFF_BuildCatalogEntry> Matches = FFF_ProgramFacilityGuide::FindCompatibleEntries(Card, Entries);
	TestEqual(TEXT("빈 허용 목록은 종류 제한 없음이며 무효 정의는 제외한다"), Matches.Num(), 2);
	if (Matches.Num() == 2)
	{
		TestEqual(TEXT("원래 건축목록의 첫 항목 순서를 유지한다"), Matches[0].FacilityDefinitionId, Entries[0].FacilityDefinitionId);
		TestEqual(TEXT("원래 건축목록의 다음 항목 순서를 유지한다"), Matches[1].FacilityDefinitionId, Entries[2].FacilityDefinitionId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramFacilityGuide_ReadableNotice,
	"FestaFiesta.UI.PrepProgram.FacilityGuide.ReadableNotice",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramFacilityGuide_ReadableNotice::RunTest(const FString& Parameters)
{
	using namespace FFProgramFacilityGuideTests;
	const FFF_BuildCatalogEntry Workshop = MakeEntry(
		TEXT("Booth_CraftExperience_SeaWorkshop"), EFFZSFacilityCategory::Booth, TEXT("바다 공방"));
	const FFF_BuildCatalogEntry Yoga = MakeEntry(
		TEXT("Performance_ExperienceStage_Yoga"), EFFZSFacilityCategory::Performance, TEXT("요가 무대"));
	TestEqual(TEXT("공방의 위치는 실제 건축목록 한글 분류와 같다"),
		FFF_ProgramFacilityGuide::GetCatalogPath(Workshop).ToString(), FString(TEXT("건축목록 > 부스 > 공예·제작 체험")));
	TestEqual(TEXT("요가는 참여형 무대를 안내한다"),
		FFF_ProgramFacilityGuide::GetCatalogPath(Yoga).ToString(), FString(TEXT("건축목록 > 공연 > 참여형 무대")));

	const FString MissingNotice = FFF_ProgramFacilityGuide::BuildFacilityNotice(MakeWorkshop(), { Workshop, Yoga }, 0).ToString();
	TestTrue(TEXT("설치할 실제 시설 이름을 알려 준다"), MissingNotice.Contains(TEXT("바다 공방")));
	TestTrue(TEXT("설치가 필요한 경우 한 곳을 설치하도록 안내한다"), MissingNotice.Contains(TEXT("한 곳을 설치")));
	TestTrue(TEXT("시설을 찾을 위치를 알려 준다"), MissingNotice.Contains(TEXT("건축목록 > 부스 > 공예·제작 체험")));
	TestFalse(TEXT("프로그램과 무관한 요가는 안내하지 않는다"), MissingNotice.Contains(TEXT("요가 무대")));
	TestFalse(TEXT("시설 ID를 사용자에게 노출하지 않는다"), MissingNotice.Contains(Workshop.FacilityDefinitionId.ToString()));

	const FString InstalledNotice = FFF_ProgramFacilityGuide::BuildFacilityNotice(MakeWorkshop(), { Workshop }, 2).ToString();
	TestTrue(TEXT("기존 시설이 있으면 편성 가능 개수를 안내한다"), InstalledNotice.Contains(TEXT("2곳")));
	TestFalse(TEXT("설치된 시설이 있는데 다시 설치하도록 요구하지 않는다"), InstalledNotice.Contains(TEXT("설치한 뒤")));
	const FString UnknownNotice = FFF_ProgramFacilityGuide::BuildFacilityNotice(MakeWorkshop(), { Workshop }, INDEX_NONE).ToString();
	TestFalse(TEXT("설치 수를 모르는 상태에서 시설이 없다고 단정하지 않는다"), UnknownNotice.Contains(TEXT("설치한 뒤")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFF_ProgramFacilityGuide_MissingCatalogAndDisplayName,
	"FestaFiesta.UI.PrepProgram.FacilityGuide.MissingCatalogAndDisplayName",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FFF_ProgramFacilityGuide_MissingCatalogAndDisplayName::RunTest(const FString& Parameters)
{
	using namespace FFProgramFacilityGuideTests;
	FFF_ProgramCard Card = MakeWorkshop();
	Card.AllowedFacilityTypes = { TEXT("Unpublished_InternalType") };
	const FString MissingNotice = FFF_ProgramFacilityGuide::BuildFacilityNotice(Card, {}, 0).ToString();
	TestTrue(TEXT("대응 시설이 없으면 건축목록에서 찾지 못했다고 알린다"), MissingNotice.Contains(TEXT("찾지 못했습니다")));
	TestFalse(TEXT("알 수 없는 허용 종류 토큰을 표시하지 않는다"), MissingNotice.Contains(TEXT("Unpublished_InternalType")));
	Card.AllowedFacilityTypes.Reset();
	TestTrue(TEXT("제한 없는 프로그램도 빈 건축목록을 안전하게 안내한다"),
		FFF_ProgramFacilityGuide::BuildFacilityNotice(Card, {}, 0).ToString().Contains(TEXT("시설 종류 제한은 없지만")));

	FFF_BuildCatalogEntry Entry = MakeEntry(
		TEXT("Booth_CraftExperience_NoLabel"), EFFZSFacilityCategory::Booth, TEXT(""));
	const FString NamelessNotice = FFF_ProgramFacilityGuide::BuildFacilityNotice(MakeWorkshop(), { Entry }, 0).ToString();
	TestFalse(TEXT("표시 이름이 비어도 ID로 대체하지 않는다"), NamelessNotice.Contains(TEXT("NoLabel")));
	TestTrue(TEXT("표시 이름이 비어도 건축목록 경로가 남는다"), NamelessNotice.Contains(TEXT("공예·제작 체험")));
	Entry.DisplayName = FText::FromName(Entry.FacilityDefinitionId);
	TestFalse(TEXT("ID를 표시 이름으로 설정한 항목도 내부 식별자를 숨긴다"),
		FFF_ProgramFacilityGuide::BuildFacilityNotice(MakeWorkshop(), { Entry }, 0).ToString().Contains(TEXT("NoLabel")));
	return true;
}

#endif
