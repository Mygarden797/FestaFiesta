// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Economy/FF_FestivalEconomyTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FF_ProgramAuthoringValidationLibrary.generated.h"

/** 기존 FestivalDefinition을 수정하지 않는 프로그램 카탈로그 저작 검증기. */
UCLASS()
class FESTAFIESTA_API UFF_ProgramAuthoringValidationLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 모든 오류를 한 번에 모아 기획자가 한 번의 수정 주기로 고칠 수 있게 한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program|Authoring")
	static bool ValidateProgramCatalog(
		const TArray<FFF_ProgramCard>& ProgramCatalog,
		TArray<FText>& OutErrors);

	/** AllowedFacilityTypes 한 항목이 정확한 `대분류_소분류` 형식인지 검사한다. */
	UFUNCTION(BlueprintPure, Category = "FestaFiesta|Paid Program|Authoring")
	static bool IsAllowedFacilityTypeTokenValid(FName FacilityType);
};
