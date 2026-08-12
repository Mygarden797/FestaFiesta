// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"

class UDataTable;

// 시설 정의의 단일 식별 원천에서 경제 설치비 키와 값을 결정하는 순수 조회기.
class FESTAFIESTA_API FFF_FacilityCostResolver
{
public:
	// 명시 override가 있으면 우선하고, 없으면 FacilityDefinitionId의 대·소분류로 키를 만든다.
	static bool ResolveCostKey(const FFFZSFacilitySpec& Spec, FName& OutCostKey, FString& OutFailureReason);

	// 정확한 키를 먼저 찾고 Cost.<대분류>.Default로 한 번 폴백한다. 상태를 변경하지 않는다.
	static bool FindInstallCost(
		const FFFZSFacilitySpec& Spec,
		const UDataTable* CostTable,
		int32& OutInstallCost,
		FName& OutMatchedRow,
		FString& OutFailureReason);
};
