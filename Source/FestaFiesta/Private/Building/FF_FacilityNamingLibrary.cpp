// FF_FacilityNamingLibrary.cpp

// FacilityDefinitionId의 대분류·소분류 토큰 표와 엄격한 파싱 규칙을 구현한다.
// 흐름: 카테고리 토큰 조회 -> 소분류 허용 여부 확인 -> 세 번째 이후 토큰을 고유 이름으로 결합.
// 불변식: 토큰 비교는 대소문자를 구분하지 않지만 결과는 규칙 표의 정규 이름으로 반환한다.
// 실패/성능: 무효 형식은 출력 초기화 뒤 false를 반환하고 정적 규칙 표는 최초 호출에 한 번만 만든다.

#include "Building/FF_FacilityNamingLibrary.h"

namespace
{
	// Zone System의 enum 카테고리를 FacilityDefinitionId 첫 토큰으로 변환하는 규칙 표다.
	// 함수 지역 static을 사용해 최초 호출 시 한 번만 구성하고, const 참조만 노출해 실행 중 변경을 막는다.
	// 카테고리를 추가할 때 아래 소분류 표도 함께 추가해야 TryParseFacilityDefinitionId가 성공한다.
	
	const TMap<EFFZSFacilityCategory, FString>& GetCategoryToMajorToken()
	{
		static const TMap<EFFZSFacilityCategory, FString> Table = {
			{ EFFZSFacilityCategory::Booth,       TEXT("Booth") },
			{ EFFZSFacilityCategory::Toilet,      TEXT("Toilet") },
			{ EFFZSFacilityCategory::Info,        TEXT("Info") },
			{ EFFZSFacilityCategory::Rest,        TEXT("Rest") },
			{ EFFZSFacilityCategory::Performance, TEXT("Performance") },
			{ EFFZSFacilityCategory::Safety,      TEXT("Safety") },
			{ EFFZSFacilityCategory::PhotoSpot,   TEXT("PhotoSpot") },
			{ EFFZSFacilityCategory::Lighting,    TEXT("Lighting") },
			{ EFFZSFacilityCategory::Road,        TEXT("Road") },
			{ EFFZSFacilityCategory::Tile,        TEXT("Tile") },
		};
		return Table;
	}

	
	// 카테고리마다 두 번째 토큰으로 허용할 기능 분류 목록이다. 문자열로 보관하는 이유는 파싱한
	// FString과 정확히 비교하고, 진단 메시지용 Join을 별도 변환 없이 수행하기 위해서다. 
	 
	const TMap<EFFZSFacilityCategory, TArray<FString>>& GetCategoryToKnownMinorTokens()
	{
		static const TMap<EFFZSFacilityCategory, TArray<FString>> Table = {
			{ EFFZSFacilityCategory::Booth,       { TEXT("FoodStall"), TEXT("MarketGoods"), TEXT("CraftExperience"), TEXT("ActivityGame"), TEXT("Nightlife") } },
			{ EFFZSFacilityCategory::Toilet,      { TEXT("Standard"), TEXT("Accessible"), TEXT("Family"), TEXT("Temporary") } },
			{ EFFZSFacilityCategory::Info,        { TEXT("GeneralInfo"), TEXT("Ticketing"), TEXT("LostFound"), TEXT("Translation") } },
			{ EFFZSFacilityCategory::Rest,        { TEXT("Seating"), TEXT("Shade"), TEXT("Picnic"), TEXT("Cooling") } },
			{ EFFZSFacilityCategory::Performance, { TEXT("MainStage"), TEXT("SubStage"), TEXT("Busking"), TEXT("ExperienceStage") } },
			{ EFFZSFacilityCategory::Safety,      { TEXT("FirstAid"), TEXT("Security"), TEXT("FireSafety"), TEXT("Evacuation") } },
			{ EFFZSFacilityCategory::PhotoSpot,   { TEXT("Landmark"), TEXT("Theme"), TEXT("Scenic"), TEXT("Interactive") } },
			{ EFFZSFacilityCategory::Lighting,    { TEXT("Decorative"), TEXT("Pendant"), TEXT("Streetlamp") } },
			{ EFFZSFacilityCategory::Road,        { TEXT("Surface") } },
			{ EFFZSFacilityCategory::Tile,        { TEXT("Surface") } },
		};
		return Table;
	}
}

FName UFF_FacilityNamingLibrary::GetMajorToken(const EFFZSFacilityCategory Category)
{
	// 알 수 없는 enum 값은 assert하지 않고 NAME_None으로 돌려 호출자가 누락된 표를 처리하게 한다.
	const FString* Token = GetCategoryToMajorToken().Find(Category);
	return Token ? FName(**Token) : NAME_None;
}

TArray<FName> UFF_FacilityNamingLibrary::GetKnownMinorTokens(const EFFZSFacilityCategory Category)
{
	// 반환 배열은 호출자 소유의 값 복사본이다. 내부 static 표를 Blueprint나 외부 코드가 변경할 수 없다.
	TArray<FName> Result;
	const TArray<FString>* Tokens = GetCategoryToKnownMinorTokens().Find(Category);
	if (!Tokens)
	{
		return Result;
	}

	// 정확한 크기를 미리 확보해 FString -> FName 변환 중 배열 재할당을 피한다.
	Result.Reserve(Tokens->Num());
	for (const FString& Token : *Tokens)
	{
		Result.Emplace(*Token);
	}
	return Result;
}

FString UFF_FacilityNamingLibrary::JoinKnownMinorTokens(const EFFZSFacilityCategory Category)
{
	// `(표 없음)`은 정상적으로 소분류가 0개인 것과 규칙 표 누락을 개발 UI에서 구분하기 위한 폴백 문구다.
	const TArray<FString>* Tokens = GetCategoryToKnownMinorTokens().Find(Category);
	return Tokens ? FString::Join(*Tokens, TEXT(", ")) : TEXT("(표 없음)");
}

bool UFF_FacilityNamingLibrary::TryParseFacilityDefinitionId(
	const EFFZSFacilityCategory ExpectedCategory,
	const FName FacilityDefinitionId,
	FFF_FacilityNameParts& OutParts,
	FString& OutFailureReason)
{
	// Out 매개변수는 성공 여부와 무관하게 결정적인 상태를 보장한다. 같은 변수를 여러 ID 파싱에 재사용해도
	// 이전 성공 결과나 실패 이유가 다음 호출 결과로 새어 나오지 않는다.
	OutParts = FFF_FacilityNameParts{};
	OutFailureReason.Reset();

	if (FacilityDefinitionId.IsNone())
	{
		// NAME_None은 시설 정의가 아직 연결되지 않은 정상적인 중간 상태일 수 있다. 파싱은 실패하지만
		// 규칙 위반 경고를 만들 근거는 없으므로 OutFailureReason도 의도적으로 비워 둔다.
		return false;
	}

	// ExpectedCategory 자체가 표에 없으면 ID 형식을 판정할 기준이 없다. 코드 표 갱신이 필요한 오류로 구분한다.
	const FString* ExpectedMajor = GetCategoryToMajorToken().Find(ExpectedCategory);
	if (!ExpectedMajor)
	{
		OutFailureReason = TEXT("이 시설 카테고리에 대응하는 대분류 토큰이 코드 표에 없다(FF_FacilityNamingLibrary 갱신 필요).");
		return false;
	}

	TArray<FString> Tokens;
	// bCullEmpty=false로 빈 토큰도 보존한다. 예를 들어 `Booth__Name`이나 `Booth_FoodStall_`가
	// 조용히 다른 토큰 수로 축약되지 않고 아래 소분류/고유이름 검증에서 명확히 실패하게 한다.
	FacilityDefinitionId.ToString().ParseIntoArray(Tokens, TEXT("_"), false);
	if (Tokens.Num() < 3)
	{
		OutFailureReason = FString::Printf(TEXT("토큰이 %d개뿐이다(3개 이상 필요)."), Tokens.Num());
		return false;
	}

	if (Tokens[0] != *ExpectedMajor)
	{
		// ID 첫 토큰과 데이터 정의의 enum 카테고리가 다르면 소비 시스템마다 분류가 갈릴 수 있어 거부한다.
		OutFailureReason = FString::Printf(
			TEXT("대분류가 '%s'인데 이 시설의 카테고리는 '%s'다."),
			*Tokens[0], **ExpectedMajor);
		return false;
	}

	const TArray<FString>* KnownMinors = GetCategoryToKnownMinorTokens().Find(ExpectedCategory);
	if (!KnownMinors || !KnownMinors->Contains(Tokens[1]))
	{
		// 임의 소분류를 허용하지 않아 취향 매핑과 욕구 해소 규칙이 항상 알려진 분류 집합을 보게 한다.
		OutFailureReason = FString::Printf(TEXT("소분류 '%s'가 규칙 표에 없다."), *Tokens[1]);
		return false;
	}

	// 세 번째 이후는 의미를 더 해석하지 않는다. 고유이름 내부 underscore를 그대로 재조립해
	// `Street_Stage_North` 같은 기존 에셋 ID의 식별 정보를 손실하지 않는다.
	FString UniqueToken = Tokens[2];
	for (int32 TokenIndex = 3; TokenIndex < Tokens.Num(); ++TokenIndex)
	{
		UniqueToken += TEXT("_");
		UniqueToken += Tokens[TokenIndex];
	}
	if (UniqueToken.IsEmpty())
	{
		// 최소 3토큰이어도 마지막 토큰이 빈 `Major_Minor_` 형태는 고유 시설을 식별할 수 없어 실패한다.
		OutFailureReason = TEXT("고유이름 토큰이 비어 있다.");
		return false;
	}

	// 모든 검증을 통과한 뒤 한 번에 결과를 채운다. 따라서 false 반환 시 부분적으로 채워진 구조체가 없다.
	OutParts.MajorToken = FName(*Tokens[0]);
	OutParts.MinorToken = FName(*Tokens[1]);
	OutParts.UniqueToken = MoveTemp(UniqueToken);
	return true;
}
