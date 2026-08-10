// FF_FacilityNamingLibrary.h

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_FacilityNamingLibrary.generated.h"

//`대분류_소분류_고유이름` FacilityDefinitionId를 UI와 게임 로직이 함께 쓰는 형태로 분해한 값이다.
// UniqueToken은 세 번째 토큰부터 끝까지를 원래 underscore와 함께 보존한다.
// 파싱 실패 시 구조체 전체가 기본값으로 초기화되므로 이전 호출의 일부 결과가 남지 않는다.
 
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_FacilityNameParts
{
	GENERATED_BODY()

	// 첫 번째 토큰. 시설 카테고리와 1:1로 대응하며 예: Booth, Rest, Safety. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Naming")
	FName MajorToken = NAME_None;

	// 두 번째 토큰. 카테고리별 허용 표에 등록된 기능 분류이며 예: FoodStall, Cooling. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Naming")
	FName MinorToken = NAME_None;

	// 세 번째 이후 토큰을 underscore로 다시 연결한 시설 고유 부분. 표시/에셋 식별을 위해 원형을 보존한다. 
	UPROPERTY(BlueprintReadOnly, Category = "FestaFiesta|Building|Naming")
	FString UniqueToken;
};

//시설 네이밍 규칙의 단일 진실 공급원.

//이 라이브러리는 파싱과 검증만 하고 로그를 남기지 않는다. 반복 평가 시 로그 폭증을 막는
// Warning-once 정책은 실제 소비자인 ZoneSystemBridge가 소유한다.
// 표에 없는 enum 값은 예외를 던지지 않고 빈 결과/실패 이유로 반환해, 신규 카테고리가 추가되었지만
// 규칙 표가 아직 갱신되지 않은 상황을 호출자가 안전하게 진단할 수 있게 한다.
 
UCLASS()
class FESTAFIESTA_API UFF_FacilityNamingLibrary final : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// EFFZSFacilityCategory에 대응하는 네이밍 규칙의 대분류 토큰을 반환한다.
	// 입력 카테고리가 표에 없으면 예외 없이 NAME_None을 반환한다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Naming")
	static FName GetMajorToken(EFFZSFacilityCategory Category);

	// 입력 카테고리에서 허용하는 소분류 토큰의 값 복사본을 반환한다.
	// 호출자는 결과를 자유롭게 수정할 수 있으며 내부 규칙 표에는 영향이 없다. 표에 없으면 빈 배열이다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Naming")
	static TArray<FName> GetKnownMinorTokens(EFFZSFacilityCategory Category);

	//또는 개발 UI에 표시하도록 허용 소분류를 `A, B, C` 형식으로 연결한다.
	// 카테고리가 표에 없으면 빈 문자열 대신 진단 가능한 `(표 없음)`을 반환한다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Naming")
	static FString JoinKnownMinorTokens(EFFZSFacilityCategory Category);

	//FacilityDefinitionId를 `대분류_소분류_고유이름`으로 파싱하고 ExpectedCategory와 대분류가
	// 일치하는지, 소분류가 규칙 표에 있는지 검증한다. 세 번째 이후 underscore는 UniqueToken에
	// 그대로 포함한다. NAME_None은 아직 정의가 연결되지 않은 상태이므로 실패하되 오류 문자열은 비운다.
	 
	// @param ExpectedCategory 데이터 정의가 선언한 카테고리. 첫 토큰 및 허용 소분류 표를 선택한다.
	// @param FacilityDefinitionId 검사할 FName ID. 문자열로 변환한 대분류/소분류 표기가 규칙 표와 일치해야 한다.
	// @param OutParts 성공 시 세 부분을 채우며, 실패 시 항상 기본값으로 비운다.
	// @param OutFailureReason 규칙 위반의 개발자용 이유. 성공 및 NAME_None 실패에서는 빈 문자열이다.
	// @return 카테고리/토큰 수/대분류/소분류/고유이름 검증을 모두 통과했을 때만 true.
	// 이 함수는 전역 상태를 변경하거나 로그를 남기지 않아 검증 루프에서 반복 호출해도 부작용이 없다.
	 
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Building|Naming")
	static bool TryParseFacilityDefinitionId(
		EFFZSFacilityCategory ExpectedCategory,
		FName FacilityDefinitionId,
		FFF_FacilityNameParts& OutParts,
		FString& OutFailureReason);
};
