#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_PrepRecommendedFacilities.generated.h"

UENUM(BlueprintType)
enum class EFF_PrepRecommendedFacilityKind : uint8
{
	Lighting, Food, Toilet, Entertainment, Rest, Drink, FirstAid, FireSafety
};

/** 음료 판매 시설은 NPC의 갈증 해소(모든 음식점)와 구분한다. 새 음료 시설은 점검 Defaults에서 추가한다. */
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepRecommendedFacilityRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
	TArray<FName> DrinkFacilityIds = {
		TEXT("Booth_FoodStall_CafeTruck"), TEXT("Booth_FoodStall_FruitAdeBar") };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facilities")
	TArray<FName> DrinkBoothMinorTokens = { TEXT("Nightlife") };

	bool Matches(EFF_PrepRecommendedFacilityKind Kind, EFFZSFacilityCategory Category, FName DefinitionId) const;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_PrepRecommendedFacilitiesView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Facilities") bool bAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "Facilities") TArray<EFF_PrepRecommendedFacilityKind> MissingKinds;
};

namespace FFPrepRecommendedFacilities
{
	/** 준비 단계의 설치 상태를 읽는다. 운영 중 수용량·만족도 부족 판정과는 별개다. */
	FESTAFIESTA_API FFF_PrepRecommendedFacilitiesView Evaluate(
		const FFFZSReadModel& Model, const FFF_PrepRecommendedFacilityRules& Rules, bool bAvailable);
}
