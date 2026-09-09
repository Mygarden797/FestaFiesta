#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"
#include "FF_RequiredFacilityTypes.generated.h"

class UFFZSFacilityDefinition;

// Setup 완료 전에 설치해야 하는 시설 종류와 최소 개수. 자동 배치는 하지 않는다.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacility
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup")
	TSoftObjectPtr<UFFZSFacilityDefinition> FacilityDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Festival|Setup", meta = (ClampMin = "1"))
	int32 MinimumCount = 1;
};

// 카탈로그 카드/카테고리 느낌표와 설치 안내가 함께 사용하는 한 행.
USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacilityRow
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FName FacilityDefinitionId;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText DisplayName;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	EFFZSFacilityCategory Category = EFFZSFacilityCategory::Booth;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 MinimumCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 InstalledCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	int32 MissingCount = 0;
	// false이면 Category도 미확정이다. 임의의 카테고리에 느낌표를 붙이지 않는다.
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	bool bDefinitionValid = false;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText Guidance;
};

USTRUCT(BlueprintType)
struct FESTAFIESTA_API FFF_RequiredFacilitiesStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	TArray<FFF_RequiredFacilityRow> Facilities;
	// 정의/월드가 없거나 저작 오류가 있으면 false. 빈 필수 목록은 유효하다.
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	bool bCanProceed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Festival|Setup")
	FText BlockingReason;
};
