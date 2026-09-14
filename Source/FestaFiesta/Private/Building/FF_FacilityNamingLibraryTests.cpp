// FF_FacilityNamingLibraryTests.cpp

// FacilityDefinitionId 규칙 표·고유 토큰 보존·거부 조건과 Zone Bridge 호환성을 검증한다.
// 흐름: 규칙 표 열거 -> 정상 ID 파싱 -> 위반 입력 거부 -> Bridge 반환값 회귀 확인.
// 불변식: 공개 토큰과 underscore 보존 규칙을 실제 콘텐츠 식별자 계약으로 고정한다.
// 실패/성능: 대표 성공·실패 입력을 작은 순수 테스트로 분리해 깨진 규칙을 즉시 식별한다.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Building/FF_FacilityNamingLibrary.h"
#include "NPC/Integration/FF_ZoneSystemBridge.h"

// 네이밍 규칙 표가 현재 지원하는 열 대분류와 대표 소분류를 모두 노출하는지 검증한다.
// 이 테스트가 깨지면 enum 추가/변경과 규칙 표 갱신이 어긋났거나, 기존 데이터 에셋의 공개 토큰이
// 의도치 않게 바뀌었다는 뜻이다. ProductFilter로 등록해 개발 자동화 실행에서 회귀를 빠르게 찾는다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityNamingLibrary_CategoryTables,
	"FestaFiesta.Building.FacilityNaming.CategoryTablesExposeTenMajors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityNamingLibrary_CategoryTables::RunTest(const FString& Parameters)
{
	// 대분류 토큰은 데이터 에셋 ID의 첫 부분이므로 모든 카테고리를 정확한 영문 표기로 고정한다.
	TestEqual(TEXT("Booth 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Booth), FName(TEXT("Booth")));
	TestEqual(TEXT("Toilet 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Toilet), FName(TEXT("Toilet")));
	TestEqual(TEXT("Info 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Info), FName(TEXT("Info")));
	TestEqual(TEXT("Rest 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Rest), FName(TEXT("Rest")));
	TestEqual(TEXT("Performance 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Performance), FName(TEXT("Performance")));
	TestEqual(TEXT("Safety 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Safety), FName(TEXT("Safety")));
	TestEqual(TEXT("PhotoSpot 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::PhotoSpot), FName(TEXT("PhotoSpot")));
	TestEqual(TEXT("Lighting 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Lighting), FName(TEXT("Lighting")));
	TestEqual(TEXT("Lighting 카테고리는 기존 저장값 뒤에 추가된다"),
		static_cast<uint8>(EFFZSFacilityCategory::Lighting), static_cast<uint8>(7));
	TestEqual(TEXT("Road 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Road), FName(TEXT("Road")));
	TestEqual(TEXT("Road 카테고리는 기존 저장값 뒤에 추가된다"),
		static_cast<uint8>(EFFZSFacilityCategory::Road), static_cast<uint8>(8));
	TestTrue(TEXT("Road 바닥 소분류"),
		UFF_FacilityNamingLibrary::GetKnownMinorTokens(EFFZSFacilityCategory::Road).Contains(TEXT("Surface")));
	TestEqual(TEXT("Tile 대분류"),
		UFF_FacilityNamingLibrary::GetMajorToken(EFFZSFacilityCategory::Tile), FName(TEXT("Tile")));
	TestEqual(TEXT("Tile 카테고리는 기존 저장값 뒤에 추가된다"),
		static_cast<uint8>(EFFZSFacilityCategory::Tile), static_cast<uint8>(9));
	TestTrue(TEXT("Tile 바닥 소분류"),
		UFF_FacilityNamingLibrary::GetKnownMinorTokens(EFFZSFacilityCategory::Tile).Contains(TEXT("Surface")));

	// Booth는 소비자 취향 매핑에 직접 사용되는 대표 카테고리다. 개수와 양 끝의 대표 토큰을 함께 검사해
	// 목록이 통째로 비거나 일부 항목이 우연히 누락되는 회귀를 포착한다.
	const TArray<FName> BoothMinors =
		UFF_FacilityNamingLibrary::GetKnownMinorTokens(EFFZSFacilityCategory::Booth);
	TestEqual(TEXT("Booth 소분류 수"), BoothMinors.Num(), 5);
	TestTrue(TEXT("Booth FoodStall 소분류"), BoothMinors.Contains(TEXT("FoodStall")));
	TestTrue(TEXT("Booth Nightlife 소분류"), BoothMinors.Contains(TEXT("Nightlife")));
	const TArray<FName> LightingMinors =
		UFF_FacilityNamingLibrary::GetKnownMinorTokens(EFFZSFacilityCategory::Lighting);
	TestEqual(TEXT("Lighting 소분류 수"), LightingMinors.Num(), 3);
	TestTrue(TEXT("Lighting 장식 조명 소분류"), LightingMinors.Contains(TEXT("Decorative")));
	TestTrue(TEXT("Lighting 펜던트 조명 소분류"), LightingMinors.Contains(TEXT("Pendant")));
	TestTrue(TEXT("Lighting 가로등 소분류"), LightingMinors.Contains(TEXT("Streetlamp")));
	return true;
}


// 정상 ID 파싱의 출력 계약을 검증한다. 기본 3토큰뿐 아니라 고유이름 내부 underscore를 허용해야
// 기존 에셋의 상세 이름을 손실 없이 Zone/NPC 소비 코드에 전달할 수 있다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityNamingLibrary_ParsesUniqueToken,
	"FestaFiesta.Building.FacilityNaming.ParsesAndPreservesUniqueToken",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityNamingLibrary_ParsesUniqueToken::RunTest(const FString& Parameters)
{
	FFF_FacilityNameParts Parts;
	FString FailureReason;

	// 가장 단순한 `Major_Minor_Unique`가 각 출력 필드로 정확히 분리되고 성공 시 오류가 비는지 확인한다.
	TestTrue(TEXT("기본 3토큰 ID 파싱"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Booth,
			TEXT("Booth_FoodStall_DadaPochaFoodTruck"),
			Parts,
			FailureReason));
	TestEqual(TEXT("대분류 토큰"), Parts.MajorToken, FName(TEXT("Booth")));
	TestEqual(TEXT("소분류 토큰"), Parts.MinorToken, FName(TEXT("FoodStall")));
	TestEqual(TEXT("고유이름 토큰"), Parts.UniqueToken, FString(TEXT("DadaPochaFoodTruck")));
	TestTrue(TEXT("성공 시 실패 이유는 비어 있음"), FailureReason.IsEmpty());

	// 세 번째 뒤의 토큰들은 새로운 분류 단계가 아니라 UniqueToken의 일부다. 재결합 시 구분자까지 보존한다.
	TestTrue(TEXT("고유이름 내부 underscore 허용"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Performance,
			TEXT("Performance_Busking_Street_Stage_North"),
			Parts,
			FailureReason));
	TestEqual(TEXT("추가 underscore를 보존한 고유이름"),
		Parts.UniqueToken, FString(TEXT("Street_Stage_North")));
	return true;
}


// 잘못된 ID가 조용히 허용되지 않고 호출자가 진단할 수 있는 실패 이유를 제공하는지 검증한다.
// 단, NAME_None은 로딩/연결 전의 정상 중간 상태이므로 false만 반환하고 경고 문구는 만들지 않는
// 의도적인 예외 계약도 함께 고정한다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityNamingLibrary_RejectsConventionViolations,
	"FestaFiesta.Building.FacilityNaming.RejectsConventionViolations",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityNamingLibrary_RejectsConventionViolations::RunTest(const FString& Parameters)
{
	FFF_FacilityNameParts Parts;
	FString FailureReason;

	// 고유이름이 없는 2토큰 ID는 최소 형식 자체를 만족하지 않는다.
	TestFalse(TEXT("3토큰 미만 거부"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall"), Parts, FailureReason));
	TestTrue(TEXT("3토큰 오류 이유"), FailureReason.Contains(TEXT("3개 이상")));

	// 데이터 정의의 enum과 ID 첫 토큰이 다르면 UI 분류와 NPC 로직이 달라지므로 반드시 거부한다.
	TestFalse(TEXT("Category와 대분류 불일치 거부"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Rest, TEXT("Booth_FoodStall_Test"), Parts, FailureReason));
	TestTrue(TEXT("대분류 불일치 오류 이유"), FailureReason.Contains(TEXT("대분류")));

	// 표에 없는 소분류는 후속 취향/효과 매핑이 정의되지 않았으므로 실패 이유에 문제 토큰을 포함한다.
	TestFalse(TEXT("알 수 없는 소분류 거부"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Booth, TEXT("Booth_Unknown_Test"), Parts, FailureReason));
	TestTrue(TEXT("소분류 오류 이유"), FailureReason.Contains(TEXT("소분류 'Unknown'")));

	// 미연결 상태는 규칙 위반 로그 대상이 아니다. Bridge의 warning-once 정책이 불필요하게 발동하지 않게 한다.
	TestFalse(TEXT("NAME_None은 파싱하지 않음"),
		UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
			EFFZSFacilityCategory::Booth, NAME_None, Parts, FailureReason));
	TestTrue(TEXT("NAME_None은 경고 대상이 아니므로 이유 없음"), FailureReason.IsEmpty());
	return true;
}


// 파서 자체의 출력뿐 아니라 실제 소비자인 ZoneSystemBridge의 의미 매핑을 끝단에서 검증한다.
// 네이밍 표나 파싱 방식이 변경되어도 FoodStall의 취향 키와 Cooling의 복수 욕구 해소 효과가
// 유지되어야 하므로, 단위 테스트가 놓칠 수 있는 통합 회귀를 이 테스트가 막는다.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFF_FacilityNamingLibrary_BridgeRegression,
	"FestaFiesta.Building.FacilityNaming.BridgeMappingRemainsStable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFF_FacilityNamingLibrary_BridgeRegression::RunTest(const FString& Parameters)
{
	// Booth/FoodStall 파싱 결과가 NPC 취향 시스템의 snake_case 키로 안정적으로 변환되는지 확인한다.
	TestEqual(TEXT("FoodStall 취향 매핑"),
		FFF_ZoneSystemBridge::MapToPreferenceCategory(
			EFFZSFacilityCategory::Booth, TEXT("Booth_FoodStall_DadaPochaFoodTruck")),
		FName(TEXT("food_stall")));

	// Cooling 하나가 피로와 갈증 두 플래그를 동시에 켜는 복합 효과를 보존해야 한다.
	const FFF_NeedReliefFlags CoolingFlags = FFF_ZoneSystemBridge::GetNeedReliefFlags(
		EFFZSFacilityCategory::Rest, TEXT("Rest_Cooling_Oasis"));
	TestTrue(TEXT("Cooling은 피로 해소"), CoolingFlags.bRelievesFatigue);
	TestTrue(TEXT("Cooling은 갈증 해소"), CoolingFlags.bRelievesThirst);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
