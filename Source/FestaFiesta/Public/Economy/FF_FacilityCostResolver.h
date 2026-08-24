// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FF_FFZSTypes.h"

class UDataTable;

// 시설 정의의 단일 식별 원천에서 경제 설치비 키와 값을 결정하는 순수 조회기.
class FESTAFIESTA_API FFF_FacilityCostResolver
{
public:
	// 명시 InstallCostKey가 있으면 기존 예외 override 계약대로 그 키를 우선한다.
	// 없으면 FacilityDefinitionId의 대·소분류·고유이름으로 표준 4-part 키를 만든다.
	static bool ResolveCostKey(const FFFZSFacilitySpec& Spec, FName& OutCostKey, FString& OutFailureReason);

	// 자동 키는 표준 4-part → 구형 3-part → Cost.<대분류>.Default 순으로 조회한다.
	// 명시 InstallCostKey는 가장 먼저 조회하며, 없으면 자동 키로 바꾸지 않고 기존 동작대로
	// Cost.<대분류>.Default로 폴백한다. 상태를 변경하지 않는다.
	static bool FindInstallCost(
		const FFFZSFacilitySpec& Spec,
		const UDataTable* CostTable,
		int32& OutInstallCost,
		FName& OutMatchedRow,
		FString& OutFailureReason);

	// 설치비와 저작된 철거 환급률을 실제 지급에 사용할 정수 Gold로 변환한다.
	// 호출자가 달라도 같은 clamp/round 규칙을 사용하도록 경제 도메인이 계산을 소유한다.
	static int32 CalculateDemolitionRefund(int32 InstallCost, float RefundRate);
};
