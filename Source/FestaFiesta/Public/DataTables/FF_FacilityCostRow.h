// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FF_FacilityCostRow.generated.h"

/**
 * 시설 1종의 설치비. data/unreal/facility_costs_v0.csv 에서 Import as DataTable.
 * RowName 은 한광훈 님이 FFFZSFacilitySpec::InstallCostKey 에 저작한 키를 그대로 쓴다 (파싱 X)
 * 조회 실패 시: 정확한 키 → 카테고리 폴백("Cost.<Category>.Default") → 설치 거부 + Error.
 *
 * [참고] MorningCheck 에 시설 이동 기능이 없고, Setup 중 재배치는 추가 비용 없이 허용한다. 
 */
USTRUCT(BlueprintType)
struct FFF_FacilityCostRow : public FTableRowBase
{
	GENERATED_BODY()

	// 설치비 (Gold). 0 이하면 저작 오류 (공짜 시설은 없다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cost", meta = (ClampMin = "1"))
	int32 InstallCost = 0;
};
